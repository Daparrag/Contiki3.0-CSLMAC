/*
 * Copyright (c) 2014, Swedish Institute of Computer Science.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */
/**
 * \file
 *         Example file using RPL for a data collection.
 *         Can be deployed in the Indriya or Twist testbeds.
 *
 * \author Simon Duquennoy <simonduq@sics.se>
 */

#include "contiki-conf.h"
#include "net/netstack.h"
#include "net/mac/tsch/tsch-schedule.h"
#include "net/rpl/rpl-private.h"
#include "net/rpl/rpl-ns.h"
#include "net/mac/tsch/tsch-schedule.h"
#include "lib/random.h"
#include "net/mac/tsch/tsch-rpl.h"
#include "deployment.h"
#include "simple-udp.h"
#include <stdio.h>
//#include "orchestra.h"

#define DEBUG DEBUG_PRINT
#include "net/ip/uip-debug.h"

#if IN_COOJA
#define START_DELAY (60 * CLOCK_SECOND)
#define SEND_INTERVAL   (CLOCK_SECOND)
#else
#define START_DELAY (5 * 60 * CLOCK_SECOND)
#define SEND_INTERVAL   (60 * CLOCK_SECOND)
#endif

#define WITH_PONG 0
#define UDP_PORT 1234

static struct simple_udp_connection unicast_connection;

/*---------------------------------------------------------------------------*/
PROCESS(unicast_sender_process, "Any-to-any Application");
AUTOSTART_PROCESSES(&unicast_sender_process);

/*---------------------------------------------------------------------------*/
static void
print_network_status(void)
{
  int i;
  uint8_t state;
  uip_ds6_defrt_t *default_route;

  PRINTF("--- Network status ---\n");

  /* Our IPv6 addresses */
  PRINTF("NetStatus: Server IPv6 addresses:\n");
  for(i = 0; i < UIP_DS6_ADDR_NB; i++) {
    state = uip_ds6_if.addr_list[i].state;
    if(uip_ds6_if.addr_list[i].isused &&
       (state == ADDR_TENTATIVE || state == ADDR_PREFERRED)) {
      PRINTF("NetStatus: ");
      PRINT6ADDR(&uip_ds6_if.addr_list[i].ipaddr);
      PRINTF("\n");
    }
  }

  /* Our default route */
  PRINTF("- Default route:\n");
  default_route = uip_ds6_defrt_lookup(uip_ds6_defrt_choose());
  if(default_route != NULL) {
    PRINTF("NetStatus: ");
    PRINT6ADDR(&default_route->ipaddr);;
    PRINTF(" (lifetime: %lu seconds)\n", (unsigned long)default_route->lifetime.interval);
  } else {
    PRINTF("\n");
  }

#if RPL_WITH_STORING
  /* Our routing entries */
  uip_ds6_route_t *route;
  PRINTF("- Routing entries (%u in total):\n", uip_ds6_route_num_routes());
  route = uip_ds6_route_head();
  while(route != NULL) {
    PRINTF("NetStatus: ");
    PRINT6ADDR(&route->ipaddr);
    PRINTF(" via ");
    PRINT6ADDR(uip_ds6_route_nexthop(route));
    PRINTF(" (lifetime: %lu seconds)\n", (unsigned long)route->state.lifetime);
    route = uip_ds6_route_next(route);
  }
#endif

#if RPL_WITH_NON_STORING
  /* Our routing links */
  rpl_ns_node_t *link;
  PRINTF("- Routing links (%u in total):\n", rpl_ns_num_nodes());
  link = rpl_ns_node_head();
  while(link != NULL) {
    if(link->parent != NULL) {
      uip_ipaddr_t child_ipaddr;
      uip_ipaddr_t parent_ipaddr;
      rpl_ns_get_node_global_addr(&child_ipaddr, link);
      rpl_ns_get_node_global_addr(&parent_ipaddr, link->parent);
      PRINTF("NetStatus: ");
      PRINT6ADDR(&child_ipaddr);
      PRINTF(" to ");
      PRINT6ADDR(&parent_ipaddr);
      PRINTF(" (lifetime: %lu seconds)\n", (unsigned long)link->lifetime);
    }
    link = rpl_ns_node_next(link);
  }
#endif

  PRINTF("----------------------\n");
}

/*---------------------------------------------------------------------------*/
int app_send_to(uint16_t id, uint32_t seqno, int ping);
static void
receiver(struct simple_udp_connection *c,
         const uip_ipaddr_t *sender_addr,
         uint16_t sender_port,
         const uip_ipaddr_t *receiver_addr,
         uint16_t receiver_port,
         const uint8_t *dataptr,
         uint16_t datalen)
{
  struct app_data data;
  appdata_copy((void *)&data, (void *)dataptr);
  if(data.ping) {
#if WITH_PONG
    uint32_t seqno = (UIP_HTONL(data.seqno) & 0x00ffffff) | (((uint32_t)node_id << 24) & 0xff000000);
    LOGA((struct app_data *)dataptr, "App: received ping");
    app_send_to(UIP_HTONS(data.src), seqno, 0);
#else
    LOGA((struct app_data *)dataptr, "App: received");
#endif
  } else {
    LOGA((struct app_data *)dataptr, "App: received pong");
  }
}
/*---------------------------------------------------------------------------*/
int
can_send_to(uip_ipaddr_t *ipaddr) {
  return uip_ds6_is_addr_onlink(ipaddr)
      || uip_ds6_route_lookup(ipaddr)
#if RPL_WITH_NON_STORING
      || rpl_ns_is_node_reachable(default_instance->current_dag, ipaddr)
#endif
;
}
/*---------------------------------------------------------------------------*/
int
app_send_to(uint16_t id, uint32_t seqno, int ping)
{
  struct app_data data;
  uip_ipaddr_t dest_ipaddr;

  data.magic = UIP_HTONL(LOG_MAGIC);
  data.seqno = UIP_HTONL(seqno);
  data.src = UIP_HTONS(node_id);
  data.dest = UIP_HTONS(id);
  data.hop = 0;
  data.ping = ping;

  set_ipaddr_from_id(&dest_ipaddr, id);

  if(default_instance != NULL) {
#if WITH_PONG
    if(ping) {
      LOGA(&data, "App: sending ping");
    } else {
      LOGA(&data, "App: sending pong");
    }
#else
    LOGA(&data, "App: sending");
#endif
    simple_udp_sendto(&unicast_connection, &data, sizeof(data), &dest_ipaddr);
    return 1;
  } else {
    data.seqno = UIP_HTONL(seqno);
    LOGA(&data, "App: could not send");
    return 0;
  }
}
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(unicast_sender_process, ev, data)
{
  static struct etimer periodic_timer;
  uip_ipaddr_t global_ipaddr;
  static uint32_t cnt = 0;
  static uint32_t seqno;

  PROCESS_BEGIN();

  if(deployment_init(&global_ipaddr, NULL, ROOT_ID)) {
    //LOG("App: %u start\n", node_id);
  } else {
    etimer_set(&periodic_timer, 60 * CLOCK_SECOND);
    while(1) {
      printf("Info: Not running. My MAC address: ");
      net_debug_lladdr_print((const uip_lladdr_t *)&linkaddr_node_addr);
      printf("\n");
      PROCESS_WAIT_UNTIL(etimer_expired(&periodic_timer));
      etimer_reset(&periodic_timer);
    }
  }

  simple_udp_register(&unicast_connection, UDP_PORT,
                      NULL, UDP_PORT, receiver);

#if WITH_TSCH
#if WITH_ORCHESTRA
  orchestra_init();
#else
  tsch_schedule_create_minimal();
#endif
#endif

  if((node_id % 5) == 0) {
    unsigned short r, r2;
    etimer_set(&periodic_timer, START_DELAY);
    PROCESS_WAIT_UNTIL(etimer_expired(&periodic_timer));

    r = ((node_id * random_rand() >> 4) % MAX_NODES);
    r2 = r;

    while(r2-- != 0) {
      get_next_node_id();
    }

    printf("Init %u: %u\n", r, get_next_node_id());

    etimer_set(&periodic_timer, SEND_INTERVAL);
    while(1) {
      if(default_instance != NULL) {
        uint16_t target_id;
        uip_ipaddr_t target_ipaddr;
        do {
          target_id = get_next_node_id();
          set_ipaddr_from_id(&target_ipaddr, target_id);
        } while(target_id == node_id);
        deployment_set_seen(target_id, 1);
        seqno = (((uint32_t)node_id << 24) & 0xff000000) | (cnt & 0x00ffffff);
        app_send_to(target_id, seqno, 1);
        cnt++;
        if((cnt % 60) == 0) {
          print_network_status();
        }
      } else {
        LOG("App: no DODAG\n");
        print_network_status();
      }

      PROCESS_WAIT_UNTIL(etimer_expired(&periodic_timer));
      etimer_reset(&periodic_timer);
    }
  } else {
    etimer_set(&periodic_timer, 60 * CLOCK_SECOND);
    while(1) {
      LOG("App: running\n");
      PROCESS_WAIT_UNTIL(etimer_expired(&periodic_timer));
      etimer_reset(&periodic_timer);
      print_network_status();
    }
  }

  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
