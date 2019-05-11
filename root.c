#include "contiki.h"
#include "lib/list.h"
#include "lib/memb.h"
#include "lib/random.h"
#include "net/rime/rime.h"

#include <stdio.h>
#include <stdlib.h>



struct broadcast_msg {
  uint8_t type;
  int16_t dist;
};
struct runicast_msg {
  int8_t temperature;
  uint8_t src_ID;
};
/* These are the types of broadcast messages that we can send. */
enum {
  BROADCAST_TYPE_DISCOVER,
  BROADCAST_TYPE_CONFIG
};

static struct broadcast_conn broadcast;
static struct runicast_conn runicast;

/*---------------------------------------------------------------------------*/
PROCESS(broadcast_process, "Broadcast process");
PROCESS(runicast_process, "Runicast process");
AUTOSTART_PROCESSES(&broadcast_process, &runicast_process);

/*---------------------------------------------------------------------------*/
static const struct broadcast_callbacks broadcast_call = {};
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(broadcast_process, ev, data) {
  static struct etimer et;
  struct broadcast_msg msg;
  msg.type = BROADCAST_TYPE_DISCOVER;
  msg.dist = 1;

  PROCESS_EXITHANDLER(broadcast_close(&broadcast);)
  PROCESS_BEGIN();
  broadcast_open(&broadcast, 129, &broadcast_call);

  while(1) {
    /* Send a broadcast every 4 - 8 seconds */
    etimer_set(&et, CLOCK_SECOND * 4 + random_rand() % (CLOCK_SECOND * 4));

    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
    packetbuf_copyfrom(&msg, sizeof(struct broadcast_msg));
    broadcast_send(&broadcast);
  }

  PROCESS_END();
}



/*---------------------------RUNICAST-----------------------------------------*/
static void runicast_recv(struct runicast_conn *c, const linkaddr_t *from, uint8_t seqno) {
  struct runicast_msg *msg;
  msg = packetbuf_dataptr();

  printf("DATAS message received on ROOT: %d/temperature/%d#\n",
  msg->src_ID, msg->temperature)
}
static const struct runicast_callbacks runicast_callbacks = {runicast_recv};
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(runicast_process, ev, data) {
  static struct etimer et;
  struct runicast_msg msg;
  msg.src_ID = node_id;

  PROCESS_EXITHANDLER(runicast_close(&runicast);)
  PROCESS_BEGIN();
  runicast_open(&runicast, 144, &runicast_callbacks);

  while(1) {
    // Delay between 16 and 32 seconds
    etimer_set(&et, CLOCK_SECOND * 16 + random_rand() % (CLOCK_SECOND * 16));

    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
  }

  PROCESS_END();
}
