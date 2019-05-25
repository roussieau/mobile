#include "contiki.h"
#include "node-id.h"
#include "lib/list.h"
#include "lib/memb.h"
#include "lib/random.h"
#include "net/rime/rime.h"
#include "sys/timer.h"
#include "sys/clock.h"
#include "powertrace.h"

#include <stdio.h>
#include <stdlib.h>
#include "message.h"

#define MAX_RETRANSMISSIONS 4

struct runicast_msg {
    int8_t temperature;
    uint8_t src_ID;
};

struct node {
    int16_t distToRoot;
    uint8_t addr[2];
};

struct data {
	int humidity;
	int temperature;
};

static struct broadcast_conn broadcast;
static struct runicast_conn runicast;
static struct timer lastUpdate;
static struct timer aggregation;
static struct node *parent;
static int16_t config;
static int mode = 1;
static char aggregate_datas[128];
static struct data newValue = {0, 0}; 
static struct data oldValue = {0, 0}; 

/*---------------------------------------------------------------------------*/
PROCESS(broadcast_process, "Broadcast process");
PROCESS(runicast_process, "Runicast process");
PROCESS(gen_process, "Generate process");
AUTOSTART_PROCESSES(&broadcast_process, &runicast_process, &gen_process);

/*---------------------------------------------------------------------------*/
static void broadcast_recv(struct broadcast_conn *c, const linkaddr_t *from) {
    struct broadcast_msg *msg;
    msg = packetbuf_dataptr();
    // Discover packet
    if(msg->type == BROADCAST_TYPE_DISCOVER) {
        // DISCOVER MESSAGE ==> We need to construct the tree
        if(parent->distToRoot == msg->info && parent->addr[0] == from->u8[0]
                && parent->addr[1] == from->u8[1]) {
            // Chosed parent is still up ==> restart timer
            timer_restart(&lastUpdate);
        }

        if(timer_expired(&lastUpdate)) {
            // Chosed parent has timeout
            parent->distToRoot = -1;
        }

        if(parent->distToRoot < 0 || msg->info < parent->distToRoot) {
            // We have no parent OR we found a better one
            parent->distToRoot = msg->info;
            parent->addr[0] = from->u8[0];
            parent->addr[1] = from->u8[1];
            timer_restart(&lastUpdate);
            printf("New parent found:%d.%d dist:%d \n",
                    from->u8[0], from->u8[1], msg->info);
        }
    } 
	else if (msg-> type == BROADCAST_TYPE_SIGNALLOST
            && from->u8[0] == parent->addr[0]
            && from->u8[1] == parent->addr[1]) {
        // Signal to root is lost
        printf("Lost signal received !\n");
        parent->distToRoot = -1;
		packetbuf_copyfrom(msg, sizeof(struct broadcast_msg));
        broadcast_send(&broadcast);
    } 
	else if (msg->type == BROADCAST_TYPE_CONFIG
            && from->u8[0] == parent->addr[0]
            && from->u8[1] == parent->addr[1]) {
        // CONFIG MESSAGE only allowed from parent IF we have one
        // CONFIG MESSAGE ==> We need to forward it downstream
        printf("CONFIG message %d received from parent\n", msg->info);
		if (msg->info == BROADCAST_CONFIG_AGGREGATE 
			|| msg->info == BROADCAST_CONFIG_INSTANT) {
			config = msg->info;
		}
		else {
			mode = msg->info;
		}
        packetbuf_copyfrom(msg, sizeof(struct broadcast_msg));
        broadcast_send(&broadcast);
    }
}
static const struct broadcast_callbacks broadcast_call = {broadcast_recv};
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(broadcast_process, ev, data) {
    static struct etimer et;
    struct broadcast_msg msg;

    PROCESS_EXITHANDLER(broadcast_close(&broadcast);)
    PROCESS_BEGIN();
    powertrace_start(CLOCK_SECOND*5);

    config = BROADCAST_CONFIG_AGGREGATE; 
    parent = (struct node*)malloc(sizeof(struct node));
    parent->distToRoot = -1;
    timer_set(&lastUpdate, CLOCK_SECOND * 40);

    broadcast_open(&broadcast, 129, &broadcast_call);

    while(1) {
        /* Send a broadcast every 16 - 32 seconds */
        etimer_set(&et, CLOCK_SECOND * 4 + random_rand() % (CLOCK_SECOND * 4));

        PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
        if(parent->distToRoot > 0 && timer_expired(&lastUpdate)) {
            parent->distToRoot = -1;
            printf("Timer expired\n");
            // We warn the neighbors that we have lost the signal to root
            msg.type = BROADCAST_TYPE_SIGNALLOST;
            packetbuf_copyfrom(&msg, sizeof(struct broadcast_msg));
            broadcast_send(&broadcast);
        }
        else if(parent->distToRoot > 0) {
            // We know a path to the route
            msg.type = BROADCAST_TYPE_DISCOVER;
            msg.info = parent->distToRoot + 1;
            packetbuf_copyfrom(&msg, sizeof(struct broadcast_msg));
            broadcast_send(&broadcast);
        }
    }

  free(parent);
  PROCESS_END();
}



/*---------------------------RUNICAST-----------------------------------------*/
static void runicast_send_bulk(struct runicast_conn *c) {
  linkaddr_t addr;
  addr.u8[0] = parent->addr[0];
  addr.u8[1] = parent->addr[1];

  size_t size = strlen(aggregate_datas);
  if(size > 0) {
    packetbuf_copyfrom(aggregate_datas, size+1);
    runicast_send(c, &addr, MAX_RETRANSMISSIONS);
  }
}
static void append_msg(struct runicast_conn *c, char *datas) {
  if(timer_expired(&aggregation) || strlen(aggregate_datas) + strlen(datas) > 100) {
    runicast_send_bulk(c);
    timer_restart(&aggregation);
    strcpy(aggregate_datas, "");
  }

  strcat(aggregate_datas, datas);
}
static void runicast_recv(struct runicast_conn *c, const linkaddr_t *from, uint8_t seqno) {
    // If root or not connected to parent, we do not forward packets
    if(parent->distToRoot < 0) return;

  // When receiving a runicast packet, forward it to the parent
  char tmp[strlen(packetbuf_dataptr())+1];
  strcpy(tmp, packetbuf_dataptr());
  tmp[strlen(packetbuf_dataptr())] = '\0';
  append_msg(c, tmp);
  if(config == BROADCAST_CONFIG_INSTANT) {
    runicast_send_bulk(c);
    //printf("Sending: %s(%d) to %d.%d\n", (char *)packetbuf_dataptr(), seqno, parent->addr[0], parent->addr[1]);
    strcpy(aggregate_datas, "");
  }
}
static const struct runicast_callbacks runicast_callbacks = {runicast_recv};

/*---------------------------------------------------------------------------*/
PROCESS_THREAD(runicast_process, ev, data) {
  char temperature[25], humidity[25];

  PROCESS_EXITHANDLER(runicast_close(&runicast);)
  PROCESS_BEGIN();
  powertrace_start(CLOCK_SECOND*5);
  timer_set(&aggregation, CLOCK_SECOND * 120 + random_rand() % 20);
  runicast_open(&runicast, 144, &runicast_callbacks);

  while(1) {
    static struct etimer et;

    etimer_set(&et, CLOCK_SECOND * 10);

    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
    if(parent->distToRoot > 0 && !runicast_is_transmitting(&runicast)) {
	  if( mode == BROADCAST_CONFIG_PERIODIC
	    || oldValue.temperature != newValue.temperature 
	    || oldValue.humidity != oldValue.humidity) {
		  memset(temperature, ' ', 25);
		  memset(humidity, ' ', 25);
		  sprintf(temperature, "!%d/0:%d;", node_id, newValue.temperature);
		  sprintf(humidity, "!%d/1:%d;", node_id, newValue.humidity);
      }
	  oldValue = newValue;
      append_msg(&runicast, temperature);
      append_msg(&runicast, humidity);
      if(config == BROADCAST_CONFIG_INSTANT) {
        runicast_send_bulk(&runicast);
        strcpy(aggregate_datas, "");
      }
	}
  }

  PROCESS_END();
}

// Random data generation
PROCESS_THREAD(gen_process, ev, data) {

  PROCESS_EXITHANDLER(runicast_close(&runicast);)
  PROCESS_BEGIN();

  while(1) {
    static struct etimer et;

    // Delay between 100 and 120 seconds
    etimer_set(&et, CLOCK_SECOND * 100 + random_rand() % (CLOCK_SECOND * 20));

    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
	newValue.temperature = (random_rand() % 40) - 10;
	newValue.humidity = (random_rand() % 100);
  }

  PROCESS_END();
}
