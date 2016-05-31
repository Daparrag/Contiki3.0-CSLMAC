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
 * \author Simon Duquennoy <simonduq@sics.se>
 */

#ifndef __PROJECT_CONF_H__
#define __PROJECT_CONF_H__

#include "deployment-def.h"

//#define WITH_POLL_RPL_PROCESS 1

#if CONFIG == CONFIG_TSCH

#undef WITH_TSCH
#define WITH_TSCH 1

/* TSCH and RPL callbacks */
#define RPL_CALLBACK_PARENT_SWITCH tsch_rpl_callback_parent_switch
#define RPL_CALLBACK_NEW_DIO_INTERVAL tsch_rpl_callback_new_dio_interval
#define TSCH_CALLBACK_JOINING_NETWORK tsch_rpl_callback_joining_network
#define TSCH_CALLBACK_LEAVING_NETWORK tsch_rpl_callback_leaving_network

/* Needed for IoT-LAB M3 nodes */
#undef RF2XX_SOFT_PREPARE
#define RF2XX_SOFT_PREPARE 0
#undef RF2XX_WITH_TSCH
#define RF2XX_WITH_TSCH 1
/* Needed for cc2420 platforms only */
/* Disable DCO calibration (uses timerB) */
#undef DCOSYNCH_CONF_ENABLED
#define DCOSYNCH_CONF_ENABLED            0
/* Enable SFD timestamps (uses timerB) */
#undef CC2420_CONF_SFD_TIMESTAMPS
#define CC2420_CONF_SFD_TIMESTAMPS       1

/* TSCH logging. 0: disabled. 1: basic log. 2: with delayed
 * log messages from interrupt */
#undef TSCH_LOG_CONF_LEVEL
#define TSCH_LOG_CONF_LEVEL 1

/* Don't log broadcast Rx */
#undef TSCH_LOG_CONF_ALL_RX
#define TSCH_LOG_CONF_ALL_RX 0

/* IEEE802.15.4 PANID */
#undef IEEE802154_CONF_PANID
#define IEEE802154_CONF_PANID 0xbe43

#undef TSCH_CONF_JOIN_MY_PANID_ONLY
#define TSCH_CONF_JOIN_MY_PANID_ONLY 1

/* Do not start TSCH at init, wait for NETSTACK_MAC.on() */
#undef TSCH_CONF_AUTOSTART
#define TSCH_CONF_AUTOSTART 0

/* 6TiSCH minimal schedule length.
 * Larger values result in less frequent active slots: reduces capacity and saves energy. */
#undef TSCH_SCHEDULE_CONF_DEFAULT_LENGTH
#define TSCH_SCHEDULE_CONF_DEFAULT_LENGTH 1

#undef TSCH_CONF_ADAPTIVE_TIMESYNC
#define TSCH_CONF_ADAPTIVE_TIMESYNC 1

#undef TSCH_CONF_KEEPALIVE_TIMEOUT
#define TSCH_CONF_KEEPALIVE_TIMEOUT (20 * CLOCK_SECOND)

#undef TSCH_CONF_MAX_KEEPALIVE_TIMEOUT
#define TSCH_CONF_MAX_KEEPALIVE_TIMEOUT (60 * CLOCK_SECOND)

#undef TSCH_CONF_EB_PERIOD
#define TSCH_CONF_EB_PERIOD (16 * CLOCK_SECOND)

#undef TSCH_CONF_MAX_EB_PERIOD
#define TSCH_CONF_MAX_EB_PERIOD (50 * CLOCK_SECOND)

#undef NETSTACK_CONF_MAC
#define NETSTACK_CONF_MAC     tschmac_driver
#undef NETSTACK_CONF_RDC
#define NETSTACK_CONF_RDC     nordc_driver

#undef TSCH_CONF_DEFAULT_HOPPING_SEQUENCE
#if TSCH_CONF_NCHANNELS == 4
#define TSCH_CONF_DEFAULT_HOPPING_SEQUENCE TSCH_HOPPING_SEQUENCE_4_4
#endif
#if TSCH_CONF_NCHANNELS == 16
#define TSCH_CONF_DEFAULT_HOPPING_SEQUENCE TSCH_HOPPING_SEQUENCE_16_16
#endif

#undef TSCH_PACKET_CONF_EACK_WITH_DEST_ADDR
#define TSCH_PACKET_CONF_EACK_WITH_DEST_ADDR CONF_SMARTDUP

#undef NETSTACK_CONF_MAC_SEQNO_MAX_AGE
#if CONF_SMARTDUP
#define NETSTACK_CONF_MAC_SEQNO_MAX_AGE (10 * CLOCK_SECOND)
#else
#define NETSTACK_CONF_MAC_SEQNO_MAX_AGE 0
#endif

/* IEEE802.15.4 frame version */
#undef FRAME802154_CONF_VERSION
#define FRAME802154_CONF_VERSION FRAME802154_IEEE802154E_2012

#elif CONFIG == CONFIG_NULLRDC

/* Netstack layers */
#undef NETSTACK_CONF_MAC
#define NETSTACK_CONF_MAC     csma_driver
#undef NETSTACK_CONF_RDC
#define NETSTACK_CONF_RDC     nullrdc_driver

#undef RF_CHANNEL
#define RF_CHANNEL 15
#undef CC2420_CONF_CHANNEL
#define CC2420_CONF_CHANNEL RF_CHANNEL
#undef MICROMAC_CONF_CHANNEL
#define MICROMAC_CONF_CHANNEL RF_CHANNEL

#undef NULLRDC_CONF_802154_AUTOACK
#define NULLRDC_CONF_802154_AUTOACK 1

#else

#error CONFIG not supported!

#endif

#undef NETSTACK_CONF_FRAMER
#define NETSTACK_CONF_FRAMER  framer_802154

#if RPL_CONFIG == CONFIG_STORING

#undef RPL_CONF_WITH_STORING
#define RPL_CONF_WITH_STORING 1

#undef RPL_CONF_WITH_NON_STORING
#define RPL_CONF_WITH_NON_STORING 0

#undef RPL_CONF_MOP
#define RPL_CONF_MOP RPL_MOP_STORING_NO_MULTICAST

#undef UIP_CONF_MAX_ROUTES
#define UIP_CONF_MAX_ROUTES  MAX_NODES

#undef RPL_NS_CONF_LINK_NUM
#define RPL_NS_CONF_LINK_NUM 0

#undef RPL_CONF_DIO_REFRESH_DAO_ROUTES
#define RPL_CONF_DIO_REFRESH_DAO_ROUTES 1

#elif RPL_CONFIG == CONFIG_NON_STORING

#undef RPL_CONF_WITH_STORING
#define RPL_CONF_WITH_STORING 0

#undef RPL_CONF_WITH_NON_STORING
#define RPL_CONF_WITH_NON_STORING 1

#undef RPL_CONF_MOP
#define RPL_CONF_MOP RPL_MOP_NON_STORING

#undef UIP_CONF_MAX_ROUTES
#define UIP_CONF_MAX_ROUTES  0

#undef RPL_NS_CONF_LINK_NUM
#define RPL_NS_CONF_LINK_NUM MAX_NODES

#undef RPL_CONF_DIO_REFRESH_DAO_ROUTES
#define RPL_CONF_DIO_REFRESH_DAO_ROUTES 1

#else

#error RPL_CONF not supported!

#endif

#undef QUEUEBUF_CONF_NUM
#define QUEUEBUF_CONF_NUM 24

#undef TSCH_QUEUE_CONF_NUM_PER_NEIGHBOR
#define TSCH_QUEUE_CONF_NUM_PER_NEIGHBOR 16

#undef TSCH_CONF_MAX_INCOMING_PACKETS
#define TSCH_CONF_MAX_INCOMING_PACKETS 16

#if RSSI_BASED_ETX
#undef LINK_STATS_CONF_INIT_ETX
#define LINK_STATS_CONF_INIT_ETX(stats) guess_etx_from_rssi(stats)
#endif

/* Disable UDP checksum, needed as we have mutable fields (hop count) in the data packet */
#undef UIP_CONF_UDP_CHECKSUMS
#define UIP_CONF_UDP_CHECKSUMS   0
#undef RDC_CONF_WITH_DUPLICATE_DETECTION
#define RDC_CONF_WITH_DUPLICATE_DETECTION 1

/* Use no DIO suppression */
#undef RPL_CONF_DIO_REDUNDANCY
#define RPL_CONF_DIO_REDUNDANCY 0xff

#undef RPL_CONF_DIO_INTERVAL_MIN
#define RPL_CONF_DIO_INTERVAL_MIN 15 /* 2^15 ms = 32.768 s */
//#define RPL_CONF_DIO_INTERVAL_MIN 14

#undef RPL_CONF_DIO_INTERVAL_DOUBLINGS
#define RPL_CONF_DIO_INTERVAL_DOUBLINGS 5 /* 2^(15+5) ms = 1048.576 s */
//#define RPL_CONF_DIO_INTERVAL_DOUBLINGS 6

#undef RPL_CONF_PROBING_INTERVAL
#define RPL_CONF_PROBING_INTERVAL (60 * CLOCK_SECOND)

#if RPL_CONF_WITH_DAO_ACK

#undef RPL_CONF_DAO_DELAY
#define RPL_CONF_DAO_DELAY (30 * CLOCK_SECOND)

#undef RPL_CONF_DAO_RETRANSMISSION_TIMEOUT
#define RPL_CONF_DAO_RETRANSMISSION_TIMEOUT (60 * CLOCK_SECOND)

#endif

/* Save some space  */
#undef UIP_CONF_TCP
#define UIP_CONF_TCP 0
#undef UIP_CONF_ND6_SEND_NA
#define UIP_CONF_ND6_SEND_NA 0
#undef SICSLOWPAN_CONF_FRAG
#define SICSLOWPAN_CONF_FRAG 0

#define TSCH_LOG_CONF_ID_FROM_LINKADDR(addr) LOG_ID_FROM_LINKADDR(addr)

#if DEPLOYMENT == DEPLOYMENT_COOJA
/* Save some space to fit the limited RAM of the z1 */
#undef UIP_CONF_TCP
#define UIP_CONF_TCP 0
#undef QUEUEBUF_CONF_NUM
#define QUEUEBUF_CONF_NUM 4
#undef TSCH_CONF_MAX_INCOMING_PACKETS
#define TSCH_CONF_MAX_INCOMING_PACKETS 4
#undef UIP_CONF_MAX_ROUTES
#define UIP_CONF_MAX_ROUTES  4
#undef NBR_TABLE_CONF_MAX_NEIGHBORS
#define NBR_TABLE_CONF_MAX_NEIGHBORS 4
#undef RPL_NS_CONF_LINK_NUM
#define RPL_NS_CONF_4
#undef UIP_CONF_ND6_SEND_NA
#define UIP_CONF_ND6_SEND_NA 0
#undef SICSLOWPAN_CONF_FRAG
#define SICSLOWPAN_CONF_FRAG 0
#undef TSCH_LOG_CONF_LEVEL
#define TSCH_LOG_CONF_LEVEL 0
#undef TSCH_CONF_EB_PERIOD
#define TSCH_CONF_EB_PERIOD (4 * CLOCK_SECOND)
#undef TSCH_CONF_MAX_EB_PERIOD
#define TSCH_CONF_MAX_EB_PERIOD (4 * CLOCK_SECOND)

#elif DEPLOYMENT == DEPLOYMENT_NESTESTBED

#undef NBR_TABLE_CONF_MAX_NEIGHBORS
#define NBR_TABLE_CONF_MAX_NEIGHBORS 25

#elif DEPLOYMENT == DEPLOYMENT_IOTLAB
#undef NBR_TABLE_CONF_MAX_NEIGHBORS
#define NBR_TABLE_CONF_MAX_NEIGHBORS 130

#endif

#define WITH_RPL 1
#define WITH_DEPLOYMENT 1
#define WITH_LOG 1
#define WITH_LOG_HOP_COUNT 1
#if WITH_LOG
#include "deployment.h"
#include "deployment-log.h"
#endif

#endif /* __PROJECT_CONF_H__ */
