#include "contiki.h"
#include "lib/list.h"
#include "lib/memb.h"
#include "lib/random.h"
#include "net/rime/rime.h"

#include <stdio.h>
#include <stdlib.h>


/* These hold the broadcast and unicast structures, respectively. */
static struct broadcast_conn broadcast;
struct broadcast_msg {
    int dist;
};

struct node {
    int distToRoot;
    uint8_t addr[2];
    unsigned long lastUpdate;
};

static struct node *parent;

/*---------------------------------------------------------------------------*/
/* We first declare our two processes. */
PROCESS(broadcast_process, "Broadcast process");
//PROCESS(unicast_process, "Example unicast");

/* The AUTOSTART_PROCESSES() definition specifices what processes to
   start when this module is loaded. We put both our processes
   there. */
AUTOSTART_PROCESSES(&broadcast_process);

/*---------------------------------------------------------------------------*/
/* This function is called whenever a broadcast message is received. */
    static void
broadcast_recv(struct broadcast_conn *c, const linkaddr_t *from)
{
    if (parent->distToRoot == 0) return;
    struct broadcast_msg *msg;
    msg = packetbuf_dataptr();
    if(parent->distToRoot == msg->dist 
	&& parent->addr[0] == from->u8[0] 
	&& parent->addr[1] == from->u8[1]) {
	parent->lastUpdate = clock_seconds();
    }
 	
    if(parent->distToRoot < 0 
       || msg->dist < parent->distToRoot
       || clock_seconds() - parent->lastUpdate > 10) {
	parent->distToRoot = msg->dist;
	parent->addr[0] = from->u8[0];
	parent->addr[1] = from->u8[1];
	parent->lastUpdate = clock_seconds();
	printf("New parent found:%d.%d dist:%d \n",
	    from->u8[0], from->u8[1], msg->dist);

    }
}
/* This is where we define what function to be called when a broadcast
   is received. We pass a pointer to this structure in the
   broadcast_open() call below. */
static const struct broadcast_callbacks broadcast_call = {broadcast_recv};
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(broadcast_process, ev, data)
{
    static struct etimer et;
    struct broadcast_msg msg;

    PROCESS_EXITHANDLER(broadcast_close(&broadcast);)

    PROCESS_BEGIN();

    parent = (struct node*)malloc(sizeof(struct node));
    parent->distToRoot = -1;
    parent->lastUpdate = clock_seconds();

    broadcast_open(&broadcast, 129, &broadcast_call);

    while(1) {

	/* Send a broadcast every 16 - 32 seconds */
	etimer_set(&et, CLOCK_SECOND * 4 + random_rand() % (CLOCK_SECOND * 4));

	PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
	if(parent->distToRoot >= 0) {  
	    msg.dist = parent->distToRoot +1;
	    packetbuf_copyfrom(&msg, sizeof(struct broadcast_msg));
	    broadcast_send(&broadcast);
	}
    }

    PROCESS_END();
}



/******************************************************************************
*UNICAST
*/

//static struct unicast_conn uc;
/*---------------------------------------------------------------------------*/
//PROCESS_THREAD(unicast_process, ev, data)
//{
//  PROCESS_EXITHANDLER(unicast_close(&uc);)
//    
//  PROCESS_BEGIN();
//
//  unicast_open(&uc, 146, &unicast_callbacks);
//
//  while(1) {
//    static struct etimer et;
//    linkaddr_t addr;
//    
//    etimer_set(&et, CLOCK_SECOND);
//    
//    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
//
//    if (parent->distToRoot > 0) {
//	packetbuf_copyfrom("Hello", 5);
//	addr.u8[0] = parent->addr[0];
//	addr.u8[1] = parent->addr[1];
//	unicast_send(&uc, &addr);
//    }
//
//  }
//
//  PROCESS_END();
//}
