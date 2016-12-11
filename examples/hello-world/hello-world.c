/*
 * Copyright (c) 2006, Swedish Institute of Computer Science.
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
 *         A very simple Contiki application showing how Contiki programs look
 * \author
 *         Adam Dunkels <adam@sics.se>
 */
#include "dev/leds.h"
#include "dev/radio.h"
#include "dev/watchdog.h"
#include "net/netstack.h"
#include "lib/random.h"
#include "net/mac/cxmac/cxmac.h"
#include "net/rime/rime.h"
#include "net/rime/timesynch.h"
#include "sys/compower.h"
#include "sys/pt.h"
#include "sys/rtimer.h"
#include "net/mac/phase.h"
#include "net/ipv6/uip-ds6.h"
#include "net/ip/uip-debug.h"
#include "net/mac/frame802154.h"
#include "net/packetbuf.h"
#include "net/mac/tsch/tsch-slot-operation.h"

#include "contiki-conf.h"
#include "sys/cc.h"



#ifdef EXPERIMENT_SETUP
#include "experiment-setup.h"
#endif

#include <string.h>

	uint8_t test_buf [PACKETBUF_SIZE];
	struct ieee802154_ies test_ies;
	struct ieee802154_ies test_ies_rx;
/*---------------------------------------------------------------------------*/
PROCESS(hello_world_process, "Hello world process");
AUTOSTART_PROCESSES(&hello_world_process);
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(hello_world_process, ev, data)
{

	PROCESS_BEGIN();

	int _len,len_rx;
	test_ies.CSL_period=0x00ff & 0xffff;
	test_ies.CSL_phase=0xdddd & 0xffff;

	_len = frame80215e_create_ie_header_le_csl(test_buf,sizeof(test_buf),&test_ies);
	frame80215e_create_ie_header_list_termination_2(test_buf + _len, sizeof(test_buf) - _len, &test_ies);
	
	len_rx=frame802154e_parse_information_elements(test_buf,sizeof(test_buf),&test_ies_rx);
	printf("CSL_period (%04x) , CSL_phase(%04x)\n",test_ies_rx.CSL_period,test_ies_rx.CSL_phase);



	PROCESS_END();
}
/*---------------------------------------------------------------------------*/
