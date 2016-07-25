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
 *         Tools for logging RPL state and tracing data packets
 *
 * \author Simon Duquennoy <simonduq@sics.se>
 */

#include "contiki.h"
#include "net/packetbuf.h"
#include "net/queuebuf.h"
#include "deployment-log.h"
#include "simple-energest.h"
#include <stdio.h>
#include <string.h>

#if WITH_DEPLOYMENT
#include "deployment.h"
#endif /* WITH_DEPLOYMENT */

#if WITH_LOG

/* Copy an appdata to another with no assumption that the addresses are aligned */
void
appdata_copy(void *dst, void *src)
{
  if(dst != NULL) {
    if(src != NULL) {
      memcpy(dst, src, sizeof(struct app_data));
    } else {
      memset(dst, 0, sizeof(struct app_data));
    }
  }
}
/* Get dataptr from a buffer */
struct app_data *
appdataptr_from_buffer(const void *b, int len)
{
  void *ptr;
  struct app_data data;
  if(len < sizeof(struct app_data)) {
    return NULL;
  }
  ptr = (char*)b + ((len - sizeof(struct app_data)));
  appdata_copy(&data, ptr);
  if(data.magic == UIP_HTONL(LOG_MAGIC)) {
    return ptr;
  } else {
    return NULL;
  }
}
/* Get dataptr from the current packetbuf */
struct app_data *
appdataptr_from_packetbuf()
{
  return appdataptr_from_buffer(packetbuf_dataptr(), packetbuf_datalen());
}
/* Get dataptr from a queuebuf */
struct app_data *
appdataptr_from_queuebuf(const void *q)
{
  return appdataptr_from_buffer(queuebuf_dataptr((struct queuebuf *)q), queuebuf_datalen((struct queuebuf *)q));
}
/* Log information about a data packet along with RPL routing information */
void
log_appdataptr(void *dataptr)
{
  struct app_data data;

  if(dataptr) {
    appdata_copy(&data, dataptr);

    if(data.magic == UIP_HTONL(LOG_MAGIC)) {

      printf(" [%lx %u %u->%u]",
          (unsigned long)UIP_HTONL(data.seqno),
          data.hop,
          UIP_HTONS(data.src),
          UIP_HTONS(data.dest)
      );

    }
  }

  printf("\n");

}
PROCESS(log_process, "Logging process");
/* Starts logging process */
void
log_start()
{
  process_start(&log_process, NULL);
}
/* The logging process */
PROCESS_THREAD(log_process, ev, data)
{
  static struct etimer periodic;
  PROCESS_BEGIN();
#if !MIN_LOG
  etimer_set(&periodic, 4 * 60 * CLOCK_SECOND);
#else
  etimer_set(&periodic, 60 * 60 * CLOCK_SECOND);
#endif
  simple_energest_init();

  while(1) {
    PROCESS_WAIT_UNTIL(etimer_expired(&periodic));
    etimer_reset(&periodic);
    simple_energest_step(1);
  }

  PROCESS_END();
}

#endif /* WITH_LOG */
