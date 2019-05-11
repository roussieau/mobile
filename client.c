#include "contiki.h"
#include "lib/list.h"
#include "lib/memb.h"
#include "lib/random.h"
#include "net/rime/rime.h"
#include "sys/timer.h"

#include <stdio.h>
#include <stdlib.h>


/* These hold the broadcast and unicast structures, respectively. */
static struct broadcast_conn broadcast;
static struct unicast_conn unicast;
static struct timer lastUpdate;

struct broadcast_msg {
    uint8_t type;
    int dist;
};
struct unicast_msg {
    int temperature;
};
/* These are the types of broadcast messages that we can send. */
enum {
    BROADCAST_TYPE_DISCOVER,
    BROADCAST_TYPE_CONFIG
};

struct node {
    int distToRoot;
    uint8_t addr[2];
};

static struct node *parent;

/*---------------------------------------------------------------------------*/
/* We first declare our two processes. */
PROCESS(broadcast_process, "Broadcast process");
PROCESS(unicast_process, "Unicast process");

/* The AUTOSTART_PROCESSES() definition specifices what processes to
   start when this module is loaded. We put both our processes
   there. */
AUTOSTART_PROCESSES(&broadcast_process, &unicast_process);

/*---------------------------------------------------------------------------*/
/* This function is called whenever a broadcast message is received. */
static void broadcast_recv(struct broadcast_conn *c, const linkaddr_t *from) {
    struct broadcast_msg *msg;
    msg = packetbuf_dataptr();

    if(parent->distToRoot == 0) {
        return;
    } else if(msg->type == BROADCAST_TYPE_DISCOVER) {
        // DISCOVER MESSAGE ==> We need to construct the tree
        if(parent->distToRoot == msg->dist && parent->addr[0] == from->u8[0]
            && parent->addr[1] == from->u8[1]) {
              // Chosed parent is still up ==> update timestamp
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
/* This is where we define what function to be called when a broadcast
   is received. We pass a pointer to this structure in the
   broadcast_open() call below. */
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
      if(parent->distToRoot >= 0) {
          // We know a path to the route
          msg.dist = parent->distToRoot + 1;
          packetbuf_copyfrom(&msg, sizeof(struct broadcast_msg));
          broadcast_send(&broadcast);
      }

      if(random_rand() % 1000 == 0 && parent->distToRoot == 0) {
       struct broadcast_msg config;
        config.type = BROADCAST_TYPE_CONFIG;
        packetbuf_copyfrom(&config, sizeof(struct broadcast_msg));
        broadcast_send(&broadcast);
      }
    }

    PROCESS_END();
}



/*---------------------------UNICAST------------------------------------------*/
static void unicast_recv(struct unicast_conn *c, const linkaddr_t *from) {
    // If root or not connected to parent, we do not forward packets
    if(parent->distToRoot <= 0) return;


    // When receiving a unicast packet, forward it to the parent
    struct unicast_msg *msg;
    linkaddr_t addr;

    msg = packetbuf_dataptr();
    addr.u8[0] = parent->addr[0];
    addr.u8[1] = parent->addr[1];

    printf("DATAS message received from children (%d)\n", msg->temperature);

    packetbuf_copyfrom(msg, sizeof(struct unicast_msg));
    unicast_send(c, &addr);
}
static const struct unicast_callbacks unicast_callbacks = {unicast_recv};
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(unicast_process, ev, data) {
    struct unicast_msg msg;
    msg.temperature = random_rand() % 1000;

    PROCESS_EXITHANDLER(unicast_close(&unicast);)

    PROCESS_BEGIN();

    unicast_open(&unicast, 146, &unicast_callbacks);

    while(1) {
        static struct etimer et;
        linkaddr_t addr;

        etimer_set(&et, CLOCK_SECOND * 16 + random_rand() % (CLOCK_SECOND * 16));

        PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));

        if(parent->distToRoot > 0) {
            packetbuf_copyfrom(&msg, sizeof(struct unicast_msg));
            addr.u8[0] = parent->addr[0];
            addr.u8[1] = parent->addr[1];
            unicast_send(&unicast, &addr);
        }
    }

    PROCESS_END();
}
