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
/* These are the types of broadcast messages that we can send. */
enum {
  BROADCAST_TYPE_DISCOVER,
  BROADCAST_TYPE_CONFIG
};

static struct broadcast_conn broadcast;

/*---------------------------------------------------------------------------*/
PROCESS(broadcast_process, "Broadcast process");
AUTOSTART_PROCESSES(&broadcast_process);

/*---------------------------------------------------------------------------*/
static const struct broadcast_callbacks broadcast_call = {};
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(broadcast_process, ev, data) {
  static struct etimer et;
  struct broadcast_msg msg;
  msg.type = BROADCAST_TYPE_DISCOVER;
  msg.dist = 0;

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
