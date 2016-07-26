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
 */

#include "contiki.h"
#include "orchestra.h"
#include "net/packetbuf.h"

static uint16_t slotframe_handle = 0;
static uint16_t channel_offset = 0;
static struct tsch_slotframe *sf;

/*---------------------------------------------------------------------------*/
static uint16_t
get_node_timeslot(const linkaddr_t *addr)
{
#if ORCHESTRA_UNICAST_PERIOD > 0
  return addr != NULL ? (ORCHESTRA_LINKADDR_HASH(addr) % ORCHESTRA_UNICAST_PERIOD) : -1;
#else
  return 0xffff;
#endif
}
/*---------------------------------------------------------------------------*/
static int
select_packet(uint16_t *slotframe, uint16_t *timeslot)
{
  /* Select unicast only */
  const linkaddr_t *dest = packetbuf_addr(PACKETBUF_ADDR_RECEIVER);
  if(!linkaddr_cmp(dest, &linkaddr_null)) {
    if(slotframe != NULL) {
      *slotframe = slotframe_handle;
    }
    if(timeslot != NULL) {
      *timeslot = get_node_timeslot(dest);
    }
    return 1;
  }
  return 0;
}
/*---------------------------------------------------------------------------*/
static void
new_time_source(const struct tsch_neighbor *old, const struct tsch_neighbor *new)
{
  uint16_t old_ts = old != NULL ? get_node_timeslot(&old->addr) : 0xffff;
  uint16_t new_ts = new != NULL ? get_node_timeslot(&new->addr) : 0xffff;

  printf("Orchestra schedule new ts %p %p  %u %u\n", old, new, old_ts, new_ts);
  if(new_ts == old_ts) {
    return;
  }

  if(old_ts != 0xffff) {
    /* Stop tx to the old time source */
    if(old_ts == get_node_timeslot(&linkaddr_node_addr)) {
      tsch_schedule_add_link(sf,
                             LINK_OPTION_RX,
                             LINK_TYPE_NORMAL, &tsch_broadcast_address,
                             old_ts, channel_offset);
    } else {
      tsch_schedule_remove_link_by_timeslot(sf, old_ts);
    }
  }
  if(new_ts != 0xffff) {
    /* Tx to the new time source */
    uint8_t link_options = LINK_OPTION_TX | LINK_OPTION_SHARED;
    if(new_ts == get_node_timeslot(&linkaddr_node_addr)) {
      /* Same timeslot as us */
      link_options |= LINK_OPTION_RX;
    }
    tsch_schedule_add_link(sf,
                           link_options,
                           LINK_TYPE_NORMAL, &tsch_broadcast_address,
                           new_ts, channel_offset);
  }
}
/*---------------------------------------------------------------------------*/
static void
init(uint16_t sf_handle)
{
  slotframe_handle = sf_handle;
  channel_offset = sf_handle;
  sf = tsch_schedule_add_slotframe(slotframe_handle, ORCHESTRA_UNICAST_PERIOD);
  /* Rx link */
  tsch_schedule_add_link(sf,
                         LINK_OPTION_RX,
                         LINK_TYPE_NORMAL, &tsch_broadcast_address,
                         get_node_timeslot(&linkaddr_node_addr), channel_offset);
}
/*---------------------------------------------------------------------------*/
struct orchestra_rule unicast_per_time_source = {
  init,
  new_time_source,
  select_packet,
  NULL,
  NULL,
};
