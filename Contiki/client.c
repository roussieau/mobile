#include "contiki.h"
#include "node-id.h"
#include "lib/list.h"
#include "lib/memb.h"
#include "lib/random.h"
#include "net/rime/rime.h"
#include "sys/timer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_RETRANSMISSIONS 4



struct broadcast_msg {
  uint8_t type;
  int16_t dist;
};
/* These are the types of broadcast messages that we can send. */
enum {
  BROADCAST_TYPE_DISCOVER,
  BROADCAST_TYPE_CONFIG
};

struct node {
  int16_t distToRoot;
  uint8_t addr[2];
};


static struct broadcast_conn broadcast;
static struct runicast_conn runicast;
static struct timer lastUpdate;
static struct timer aggregation;
static struct node *parent;
static char aggregate_datas[128];

/*---------------------------------------------------------------------------*/
PROCESS(broadcast_process, "Broadcast process");
PROCESS(runicast_process, "Runicast process");
AUTOSTART_PROCESSES(&broadcast_process, &runicast_process);

/*---------------------------------------------------------------------------*/
static void broadcast_recv(struct broadcast_conn *c, const linkaddr_t *from) {
  struct broadcast_msg *msg;
  msg = packetbuf_dataptr();

  if(msg->type == BROADCAST_TYPE_DISCOVER) {
    // DISCOVER MESSAGE ==> We need to construct the tree
    if(parent->distToRoot == msg->dist && parent->addr[0] == from->u8[0]
      && parent->addr[1] == from->u8[1]) {
      // Chosed parent is still up ==> restart timer
      timer_restart(&lastUpdate);
    }

    if(timer_expired(&lastUpdate)) {
      // Chosed parent has timeout
      parent->distToRoot = -1;
    }

    if(parent->distToRoot < 0 || msg->dist < parent->distToRoot) {
      // We have no parent OR we found a better one
      parent->distToRoot = msg->dist;
      parent->addr[0] = from->u8[0];
      parent->addr[1] = from->u8[1];
      timer_restart(&lastUpdate);
      printf("New parent found:%d.%d dist:%d \n",
              from->u8[0], from->u8[1], msg->dist);
    }
  } else if(msg->type == BROADCAST_TYPE_CONFIG && parent->distToRoot >= 0
              && from->u8[0] == parent->addr[0]
              && from->u8[1] == parent->addr[1]) {
    // CONFIG MESSAGE only allowed from parent IF we have one
    // CONFIG MESSAGE ==> We need to forward it downstream
    printf("CONFIG message received from parent\n");

    packetbuf_copyfrom(msg, sizeof(struct broadcast_msg));
    broadcast_send(&broadcast);
  }
}
static const struct broadcast_callbacks broadcast_call = {broadcast_recv};
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(broadcast_process, ev, data) {
  static struct etimer et;
  struct broadcast_msg msg;
  msg.type = BROADCAST_TYPE_DISCOVER;

  PROCESS_EXITHANDLER(broadcast_close(&broadcast);)
  PROCESS_BEGIN();

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
    } else if(parent->distToRoot > 0) {
      // We know a path to the route
      msg.dist = parent->distToRoot + 1;
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
    packetbuf_copyfrom(aggregate_datas, size);
    while(runicast_is_transmitting(c)) {}
    runicast_send(c, &addr, MAX_RETRANSMISSIONS);
  }
}
static void runicast_recv(struct runicast_conn *c, const linkaddr_t *from, uint8_t seqno) {
  // If root or not connected to parent, we do not forward packets
  if(parent->distToRoot < 0) return;

  // When receiving a runicast packet, forward it to the parent
  char *datas;
  datas = packetbuf_dataptr();
  char tmp[strlen(datas)];
  strncpy(tmp, datas, strlen(datas));

  if(timer_expired(&aggregation) || strlen(aggregate_datas) + strlen(datas) > 100) {
    runicast_send_bulk(c);
    timer_restart(&aggregation);
    strncpy(aggregate_datas, "", strlen(aggregate_datas));
  }

  strcat(aggregate_datas, tmp);
}
static const struct runicast_callbacks test = {};
static const struct runicast_callbacks runicast_callbacks = {runicast_recv};
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(runicast_process, ev, data) {
  char datas[25];

  PROCESS_EXITHANDLER(runicast_close(&runicast);)
  PROCESS_BEGIN();
  timer_set(&aggregation, CLOCK_SECOND * 120 + random_rand() % 20);
  runicast_open(&runicast, 144, &runicast_callbacks);

  while(1) {
    static struct etimer et;
    linkaddr_t addr;

    // Delay between 16 and 32 seconds
    etimer_set(&et, CLOCK_SECOND * 16 + random_rand() % (CLOCK_SECOND * 16));

    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
    if(parent->distToRoot > 0 && !runicast_is_transmitting(&runicast)) {
      memset(datas, ' ', 25);
      sprintf(datas, "%d/temperature:%d;", node_id, (random_rand() % 40) - 10);
      packetbuf_copyfrom(datas, strlen(datas));
      addr.u8[0] = parent->addr[0];
      addr.u8[1] = parent->addr[1];
      runicast_send(&runicast, &addr, MAX_RETRANSMISSIONS);
    }
  }

  PROCESS_END();
}

