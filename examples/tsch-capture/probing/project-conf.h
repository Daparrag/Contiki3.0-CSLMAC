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
 *         Project config file
 * \author
 *         Simon Duquennoy <simon.duquennoy@inria.fr>
 *
 */

#ifndef __PROJECT_CONF_H__
#define __PROJECT_CONF_H__

#include "deployment-def.h"

#define CONFIG_SYNC_ONLY                1
#define CONFIG_CAPTURE_DEDICATED        2
#define CONFIG_CAPTURE_SHARED           3

//#define CONFIG CONFIG_SYNC_ONLY
//#define CONFIG CONFIG_CAPTURE_DEDICATED
#define CONFIG CONFIG_CAPTURE_SHARED

/* Netstack layers */
#undef NETSTACK_CONF_MAC
#define NETSTACK_CONF_MAC     tschmac_driver
#undef NETSTACK_CONF_RDC
#define NETSTACK_CONF_RDC     nordc_driver
#undef NETSTACK_CONF_FRAMER
#define NETSTACK_CONF_FRAMER  framer_802154

/* IEEE802.15.4 frame version */
#undef FRAME802154_CONF_VERSION
#define FRAME802154_CONF_VERSION FRAME802154_IEEE802154E_2012

#undef TSCH_CONF_AUTOSELECT_TIME_SOURCE
#define TSCH_CONF_AUTOSELECT_TIME_SOURCE 1

/* Needed for IoT-LAB M3 nodes */
#undef RF2XX_SOFT_PREPARE
#define RF2XX_SOFT_PREPARE 0
#undef RF2XX_WITH_TSCH
#define RF2XX_WITH_TSCH 1

/* Needed for CC2538 platforms only */
/* For TSCH we have to use the more accurate crystal oscillator
 * by default the RC oscillator is activated */
#undef SYS_CTRL_CONF_OSC32K_USE_XTAL
#define SYS_CTRL_CONF_OSC32K_USE_XTAL 1

/* Needed for cc2420 platforms only */
/* Disable DCO calibration (uses timerB) */
#undef DCOSYNCH_CONF_ENABLED
#define DCOSYNCH_CONF_ENABLED 0
/* Enable SFD timestamps (uses timerB) */
#undef CC2420_CONF_SFD_TIMESTAMPS
#define CC2420_CONF_SFD_TIMESTAMPS 1

/* TSCH logging. 0: disabled. 1: basic log. 2: with delayed
 * log messages from interrupt */
#undef TSCH_LOG_CONF_LEVEL
#define TSCH_LOG_CONF_LEVEL 2

/* IEEE802.15.4 PANID */
#undef IEEE802154_CONF_PANID
#define IEEE802154_CONF_PANID 0xaeda

#undef TSCH_CONF_JOIN_MY_PANID_ONLY
#define TSCH_CONF_JOIN_MY_PANID_ONLY 1

#define LINK_STATS_CONF_INIT_ETX(stats) guess_etx_from_rssi(stats)

/* Do not start TSCH at init, wait for NETSTACK_MAC.on() */
#undef TSCH_CONF_AUTOSTART
#define TSCH_CONF_AUTOSTART 0

#undef TSCH_CONF_ADAPTIVE_TIMESYNC
#define TSCH_CONF_ADAPTIVE_TIMESYNC 1

#undef TSCH_LOG_CONF_ID_FROM_LINKADDR
#define TSCH_LOG_CONF_ID_FROM_LINKADDR(addr) LOG_ID_FROM_LINKADDR(addr)

/* See apps/orchestra/README.md for more Orchestra configuration options */
#define TSCH_SCHEDULE_CONF_WITH_6TISCH_MINIMAL 0 /* No 6TiSCH minimal schedule */
#define TSCH_CONF_WITH_LINK_SELECTOR 1 /* Orchestra requires per-packet link selection */
/* Orchestra callbacks */
#define TSCH_CALLBACK_NEW_TIME_SOURCE orchestra_callback_new_time_source
#define TSCH_CALLBACK_PACKET_READY orchestra_callback_packet_ready
#define NETSTACK_CONF_ROUTING_NEIGHBOR_ADDED_CALLBACK orchestra_callback_child_added
#define NETSTACK_CONF_ROUTING_NEIGHBOR_REMOVED_CALLBACK orchestra_callback_child_removed

#undef WITH_RSSI_LOG
#define WITH_RSSI_LOG 1

#if DEPLOYMENT == DEPLOYMENT_COOJA

#undef TSCH_CONF_DEFAULT_HOPPING_SEQUENCE
#define TSCH_CONF_DEFAULT_HOPPING_SEQUENCE TSCH_HOPPING_SEQUENCE_4_4

#else

#undef TSCH_CONF_DEFAULT_HOPPING_SEQUENCE
#define TSCH_CONF_DEFAULT_HOPPING_SEQUENCE TSCH_HOPPING_SEQUENCE_16_16

#undef NBR_TABLE_CONF_MAX_NEIGHBORS
#define NBR_TABLE_CONF_MAX_NEIGHBORS MAX_NODES

#endif

#undef TSCH_CONF_SYNC_ON_EB
#define TSCH_CONF_SYNC_ON_EB 0

#undef TSCH_CONF_KEEPALIVE_TIMEOUT
#define TSCH_CONF_KEEPALIVE_TIMEOUT (12 * CLOCK_SECOND)

#undef TSCH_CONF_MAX_KEEPALIVE_TIMEOUT
#define TSCH_CONF_MAX_KEEPALIVE_TIMEOUT (60 * CLOCK_SECOND)

#undef TSCH_CONF_MAC_MAX_BE
#define TSCH_CONF_MAC_MAX_BE 4

#undef TSCH_CONF_MAC_MAX_FRAME_RETRIES
#define TSCH_CONF_MAC_MAX_FRAME_RETRIES 4

#undef ORCHESTRA_CONF_LINKADDR_HASH
#define ORCHESTRA_CONF_LINKADDR_HASH(addr)             (node_id_from_linkaddr(addr) * 67)

#if CONFIG == CONFIG_SYNC_ONLY

#undef TSCH_CONF_EB_PERIOD
#define TSCH_CONF_EB_PERIOD (32 * CLOCK_SECOND)

#undef ORCHESTRA_CONF_RULES
#define ORCHESTRA_CONF_RULES { &unicast_per_time_source, &eb_common }

#undef ORCHESTRA_CONF_EBSF_PERIOD
#define ORCHESTRA_CONF_EBSF_PERIOD 101

#undef ORCHESTRA_CONF_UNICAST_PERIOD
#define ORCHESTRA_CONF_UNICAST_PERIOD 61

#elif CONFIG == CONFIG_CAPTURE_DEDICATED

#undef TSCH_CONF_EB_PERIOD
#define TSCH_CONF_EB_PERIOD (2 * CLOCK_SECOND)

#undef ORCHESTRA_CONF_RULES
#define ORCHESTRA_CONF_RULES { &unicast_per_time_source, &eb_per_time_source_listen_all }

#undef ORCHESTRA_CONF_EBSF_PERIOD
#define ORCHESTRA_CONF_EBSF_PERIOD 401

#undef ORCHESTRA_CONF_UNICAST_PERIOD
#define ORCHESTRA_CONF_UNICAST_PERIOD 61

#elif CONFIG == CONFIG_CAPTURE_SHARED

#undef ORCHESTRA_CONF_RULES
#define ORCHESTRA_CONF_RULES { &unicast_per_time_source, &eb_common }

#undef TSCH_CONF_EB_PERIOD
#define TSCH_CONF_EB_PERIOD (4 * CLOCK_SECOND)

#undef ORCHESTRA_CONF_EBSF_PERIOD
#define ORCHESTRA_CONF_EBSF_PERIOD 1

#undef ORCHESTRA_CONF_UNICAST_PERIOD
#define ORCHESTRA_CONF_UNICAST_PERIOD 61

#endif

#define WITH_DEPLOYMENT 1
#define WITH_LOG 1
#if WITH_LOG
#include "deployment.h"
#include "deployment-log.h"
#endif

#endif /* __PROJECT_CONF_H__ */
