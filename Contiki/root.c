#include "contiki.h"
#include "lib/list.h"
#include "lib/memb.h"
#include "lib/random.h"
#include "net/rime/rime.h"

#include "dev/serial-line.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "stdbool.h"
#include "message.h"

struct runicast_msg {
  int8_t temperature;
  uint8_t src_ID;
};

static struct broadcast_conn broadcast;
static struct runicast_conn runicast;
static bool powerOn = 1;

/*---------------------------------------------------------------------------*/
PROCESS(broadcast_process, "Broadcast process");
PROCESS(runicast_process, "Runicast process");
PROCESS(main_process, "main process");
AUTOSTART_PROCESSES(&broadcast_process, &runicast_process, &main_process);

/*---------------------------------------------------------------------------*/
static const struct broadcast_callbacks broadcast_call = {};
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(broadcast_process, ev, data) {
  static struct etimer et;
  struct broadcast_msg msg;
  msg.type = BROADCAST_TYPE_DISCOVER;
  msg.info = 1;

  PROCESS_EXITHANDLER(broadcast_close(&broadcast);)
  PROCESS_BEGIN();
  broadcast_open(&broadcast, 129, &broadcast_call);

  while(1) {
    /* Send a broadcast every 4 - 8 seconds */
    etimer_set(&et, CLOCK_SECOND * 4 + random_rand() % (CLOCK_SECOND * 4));

    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
    if(powerOn) {
        packetbuf_copyfrom(&msg, sizeof(struct broadcast_msg));
        broadcast_send(&broadcast);
    }
  }

  PROCESS_END();
}



/*---------------------------RUNICAST-----------------------------------------*/
static void runicast_recv(struct runicast_conn *c, const linkaddr_t *from, uint8_t seqno) {
  printf("%s\n", (char*)packetbuf_dataptr());
}
static const struct runicast_callbacks runicast_callbacks = {runicast_recv};
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(runicast_process, ev, data) {
  static struct etimer et;

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

PROCESS_THREAD(main_process, ev, data)
{
    PROCESS_BEGIN();
	struct broadcast_msg msg;
    for(;;) {
        PROCESS_WAIT_EVENT();
        if (ev == serial_line_event_message && data != NULL) {
            printf("got input string: '%s'\n", (const char *) data);
            if (strcmp("power on", data) == 0) {
                powerOn = 1;
                printf("Power on !\n");
            }
            else if (strcmp("power off", data) == 0) {
                powerOn = 0;
                printf("Power off !\n");
				msg.type = BROADCAST_TYPE_SIGNALLOST;
                packetbuf_copyfrom(&msg, sizeof(struct broadcast_msg));
                broadcast_send(&broadcast);
       }
            else if(strcmp("config instant", data) == 0) {
                msg.type = BROADCAST_TYPE_CONFIG;
                msg.info = BROADCAST_CONFIG_INSTANT;
                packetbuf_copyfrom(&msg, sizeof(struct broadcast_msg));
                broadcast_send(&broadcast);
            }
            else if(strcmp("config aggregate", data) == 0) {
              msg.type = BROADCAST_TYPE_CONFIG;
              msg.info = BROADCAST_CONFIG_AGGREGATE;
              packetbuf_copyfrom(&msg, sizeof(struct broadcast_msg));
              broadcast_send(&broadcast);
            }

        }
    }
    PROCESS_END();
}

