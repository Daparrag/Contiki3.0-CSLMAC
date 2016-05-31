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
#include "net/mac/tsch/tsch.h"
#include "net/mac/tsch/tsch-private.h"
#include "deployment.h"
#include "simple-udp.h"
#include <stdio.h>
//#include "orchestra.h"

#define DEBUG DEBUG_PRINT
#include "net/ip/uip-debug.h"

#define START_DELAY (2 * 60 * CLOCK_SECOND)
//#define START_DELAY (2 * 5 * CLOCK_SECOND)
#define ASN_STEP (100 * 60)

static struct asn_t next_asn;
static int count = 0;

/*---------------------------------------------------------------------------*/
PROCESS(app_rpl_process, "RPL Application");
AUTOSTART_PROCESSES(&app_rpl_process);

/*---------------------------------------------------------------------------*/
static void
print_network_status(int count)
{
  uip_ds6_defrt_t *default_route;

  PRINTF("Stat: [%u] --- Network status --- (asn %lu %lu)\n", count, current_asn.ls4b, next_asn.ls4b);

  /* Our default route */
  PRINTF("Stat: [%u] - Default route:\n", count);
  default_route = uip_ds6_defrt_lookup(uip_ds6_defrt_choose());
  if(default_route != NULL) {
    PRINTF("Stat: [%u] NetStatus: ", count);
    PRINT6ADDR(&default_route->ipaddr);;
    PRINTF(" (lifetime: %lu seconds)\n", (unsigned long)default_route->lifetime.interval);
  } else {
    PRINTF("Stat: [%u]\n", count);
  }

#if RPL_WITH_STORING
  /* Our routing entries */
  uip_ds6_route_t *route;
  PRINTF("Stat: [%u] - Routing entries (%u in total):\n", count, uip_ds6_route_num_routes());
  route = uip_ds6_route_head();
  while(route != NULL) {
    PRINTF("Stat: [%u] NetStatus: ", count);
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
  PRINTF("Stat: [%u] - Routing links (%u in total):\n", count, rpl_ns_num_nodes());
  link = rpl_ns_node_head();
  while(link != NULL) {
    if(link->parent != NULL) {
      uip_ipaddr_t child_ipaddr;
      uip_ipaddr_t parent_ipaddr;
      rpl_ns_get_node_global_addr(&child_ipaddr, link);
      rpl_ns_get_node_global_addr(&parent_ipaddr, link->parent);
      PRINTF("Stat: [%u] NetStatus: ", count);
      PRINT6ADDR(&child_ipaddr);
      PRINTF(" to ");
      PRINT6ADDR(&parent_ipaddr);
      PRINTF(" (lifetime: %lu seconds)\n", (unsigned long)link->lifetime);
    }
    link = rpl_ns_node_next(link);
  }
#endif

  PRINTF("Stat: [%u] ----------------------\n", count);
}

/*---------------------------------------------------------------------------*/
PROCESS_THREAD(app_rpl_process, ev, data)
{
  static struct etimer periodic_timer;
  uip_ipaddr_t global_ipaddr;

  PROCESS_BEGIN();

  etimer_set(&periodic_timer, START_DELAY);
  PROCESS_WAIT_UNTIL(etimer_expired(&periodic_timer));

  if(deployment_init(&global_ipaddr, NULL, ROOT_ID)) {
    LOG("App: %u start\n", node_id);
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

#if WITH_TSCH
#if WITH_ORCHESTRA
  orchestra_init();
#else
  tsch_schedule_create_minimal();
#endif
#endif

  while(!tsch_is_associated) {
    PROCESS_YIELD();
  }

  ASN_INIT(next_asn, 0, 0);

  while(((int32_t)ASN_DIFF(current_asn, next_asn)) < 0) {
    ASN_INC(next_asn, ASN_STEP);
  }

  while(1) {
    ASN_INC(next_asn, ASN_STEP);
    PROCESS_WAIT_UNTIL(((int32_t)ASN_DIFF(current_asn, next_asn)) >= 0);
    print_network_status(count++);
  }

  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
