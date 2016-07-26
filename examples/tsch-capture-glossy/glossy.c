/*
 * Copyright (c) 2016, Inria.
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
 * This file is part of the Contiki operating system.
 *
 */

/**
 * \file
 *         An example of Rime/TSCH
 * \author
 *         Simon Duquennoy <simon.duquennoy@inria.fr>
 *
 */

#include <stdio.h>
#include "contiki-conf.h"
#include "net/netstack.h"
#include "net/net-debug.h"
#include "net/rime/rime.h"
#include "net/mac/tsch/tsch.h"
#include "deployment.h"

const linkaddr_t coordinator_addr =    { { 1, 0 } };
static void recv_bc(struct broadcast_conn *c, const linkaddr_t *from);
static void sent_bc(struct broadcast_conn *ptr, int status, int num_tx);
static const struct broadcast_callbacks broadcast_callback = { recv_bc, sent_bc };
static struct broadcast_conn bc;

static int gloss_tx_count;
static struct ctimer glossy_timer;

#define TX_COUNT 3
static unsigned char payload[128];
static unsigned payload_len;
static unsigned glossy_round = 0;

#define START_DELAY (30 * CLOCK_SECOND)

/*---------------------------------------------------------------------------*/
PROCESS(unicast_test_process, "Rime Glossy Node");
AUTOSTART_PROCESSES(&unicast_test_process);

/*---------------------------------------------------------------------------*/
static void
glossy_send()
{
  packetbuf_copyfrom(payload, payload_len);
  broadcast_send(&bc);
  if(--gloss_tx_count) {
    ctimer_set(&glossy_timer, 0, glossy_send, NULL);
  }
}
/*---------------------------------------------------------------------------*/
void
glossy_start()
{
  *((unsigned*)payload) = glossy_round;
  payload_len = 32;
  gloss_tx_count = TX_COUNT;
  glossy_send();
}
/*---------------------------------------------------------------------------*/
static void
recv_bc(struct broadcast_conn *c, const linkaddr_t *from)
{
  packetbuf_copyto(payload);
  if(*((unsigned*)payload) > glossy_round) {
    glossy_round = *((unsigned*)payload);
    glossy_start();
    printf("App: bc message received from %u, round %u\n", LOG_ID_FROM_LINKADDR(from), glossy_round);
  }
}
/*---------------------------------------------------------------------------*/
static void
sent_bc(struct broadcast_conn *ptr, int status, int num_tx)
{
}
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(unicast_test_process, ev, data)
{
  static struct etimer et;
  PROCESS_BEGIN();

  if(deployment_init(ROOT_ID)) {
  } else {
    etimer_set(&et, 60 * CLOCK_SECOND);
    while(1) {
      printf("Info: Not running. My MAC address: ");
      net_debug_lladdr_print((const uip_lladdr_t *)&linkaddr_node_addr);
      printf("\n");
      PROCESS_WAIT_UNTIL(etimer_expired(&et));
      etimer_reset(&et);
    }
  }

  printf("App: my node id %u %u\n", node_id, LOG_ID_FROM_LINKADDR(&linkaddr_node_addr));

  tsch_set_coordinator(linkaddr_cmp(&coordinator_addr, &linkaddr_node_addr));
  NETSTACK_MAC.on();

  broadcast_open(&bc, 146, &broadcast_callback);

  etimer_set(&et, START_DELAY);
  PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));

  if(linkaddr_cmp(&coordinator_addr, &linkaddr_node_addr)) {
    glossy_round++;
    glossy_start();
  }

  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
