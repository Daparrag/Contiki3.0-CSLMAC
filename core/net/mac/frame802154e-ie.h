/*
 * Copyright (c) 2015, SICS Swedish ICT.
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
 *         IEEE 802.15.4e Information Element (IE) creation and parsing.
 * \author
 *         Simon Duquennoy <simonduq@sics.se>
 */

#ifndef FRAME_802154E_H
#define FRAME_802154E_H

#include "contiki.h"
/* We need definitions from tsch-private.h for TSCH-specific information elements */
#include "net/mac/tsch/tsch-private.h"

#define FRAME802154E_IE_MAX_LINKS       4

#define WRITE16(buf, val) \
  do { ((uint8_t *)(buf))[0] = (val) & 0xff; \
       ((uint8_t *)(buf))[1] = ((val) >> 8) & 0xff; } while(0);


#define READ16(buf, var) \
  (var) = ((uint8_t *)(buf))[0] | ((uint8_t *)(buf))[1] << 8

#define WRITE32(buf, val) \
  do { ((uint8_t *)(buf))[0] = (val) & 0xff; \
       ((uint8_t *)(buf))[1] = ((val) >> 8) & 0xff;\
       ((uint8_t *)(buf))[2] = ((val) >> 8) & 0xff;\
       ((uint8_t *)(buf))[3] = ((val) >> 8) & 0xff;} while(0);



/* c.f. IEEE 802.15.4e Table 4b */
enum ieee802154e_header_ie_id {
	//0x00-0x19 unmanaged for user propose
  HEADER_IE_LE_CSL = 0x1a,
  HEADER_IE_LE_RIT,
  HEADER_IE_DSME_PAN_DESCRIPTOR,
  HEADER_IE_RZ_TIME,
  HEADER_IE_ACK_NACK_TIME_CORRECTION,
  HEADER_IE_GACK,
  HEADER_IE_LOW_LATENCY_NETWORK_INFO,
  //0x21-0x7d reserved
  HEADER_IE_LIST_TERMINATION_1 = 0x7e,
  HEADER_IE_LIST_TERMINATION_2 = 0x7f,
  //0x80-0xff reserved
};

/* c.f. IEEE 802.15.4e Table 4c */
enum ieee802154e_payload_gp_id {
  PAYLOAD_IE_ESDU = 0,
  PAYLOAD_IE_MLME,
  //0x2-0x9 unmanagement for user propouse.
  //0xa-0xe reserved.
  PAYLOAD_IE_LIST_TERMINATION = 0xf,
};

/* c.f. IEEE 802.15.4e Table 4d */
enum ieee802154e_mlme_short_subie_id {
  MLME_SHORT_IE_TSCH_SYNCHRONIZATION = 0x1a,
  MLME_SHORT_IE_TSCH_SLOFTRAME_AND_LINK,
  MLME_SHORT_IE_TSCH_TIMESLOT,
  MLME_SHORT_IE_TSCH_HOPPING_TIMING,
  MLME_SHORT_IE_TSCH_EB_FILTER,
  MLME_SHORT_IE_TSCH_MAC_METRICS_1,
  MLME_SHORT_IE_TSCH_MAC_METRICS_2,
};

/* c.f. IEEE 802.15.4e Table 4e */
enum ieee802154e_mlme_long_subie_id {
  MLME_LONG_IE_TSCH_CHANNEL_HOPPING_SEQUENCE = 0x9,
};



/* Structures used for the Slotframe and Links information element */
struct tsch_slotframe_and_links_link {
  uint16_t timeslot;
  uint16_t channel_offset;
  uint8_t link_options;
};
struct tsch_slotframe_and_links {
  uint8_t num_slotframes; /* We support only 0 or 1 slotframe in this IE */
  uint8_t slotframe_handle;
  uint16_t slotframe_size;
  uint8_t num_links;
  struct tsch_slotframe_and_links_link links[FRAME802154E_IE_MAX_LINKS];
};

/* The information elements that we currently support */
struct ieee802154_ies {
	/* Header RZTime */
	uint16_t ie_rz_time;
	/* Header IEs */
  int16_t ie_time_correction;
  uint8_t ie_is_nack;
  /*Header IE ACK_CSL_Phase*/
  uint16_t CSL_phase;
  /*Header IE ACK_CSL_period*/
    uint16_t CSL_period;



  /* Payload MLME */
  uint8_t ie_payload_ie_offset;
  uint16_t ie_mlme_len;
  /* Payload Short MLME IEs */
  uint8_t ie_tsch_synchronization_offset;
  struct asn_t ie_asn;
  uint8_t ie_join_priority;
  uint8_t ie_tsch_timeslot_id;
  uint16_t ie_tsch_timeslot[tsch_ts_elements_count];
  struct tsch_slotframe_and_links ie_tsch_slotframe_and_link;
  /* Payload Long MLME IEs */
  uint8_t ie_channel_hopping_sequence_id;
  /* We include and parse only the sequence len and list and omit unused fields */
  uint16_t ie_hopping_sequence_len;
  uint8_t ie_hopping_sequence_list[TSCH_HOPPING_SEQUENCE_MAX_LEN];
};

/** Insert various Information Elements **/
/* Header IE. ACK/NACK time correction. Used in enhanced ACKs */
int frame80215e_create_ie_header_ack_nack_time_correction(uint8_t *buf, int len,
    struct ieee802154_ies *ies);
/* Header IE. List termination 1 (Signals the end of the Header IEs when
 * followed by payload IEs) */
int frame80215e_create_ie_header_list_termination_1(uint8_t *buf, int len,
    struct ieee802154_ies *ies);
/* Header IE. List termination 2 (Signals the end of the Header IEs when
 * followed by an unformatted payload) */
int frame80215e_create_ie_header_list_termination_2(uint8_t *buf, int len,
    struct ieee802154_ies *ies);
/* Payload IE. List termination */
int frame80215e_create_ie_payload_list_termination(uint8_t *buf, int len,
    struct ieee802154_ies *ies);
/* Payload IE. MLME. Used to nest sub-IEs */
int frame80215e_create_ie_mlme(uint8_t *buf, int len,
    struct ieee802154_ies *ies);
/* MLME sub-IE. TSCH synchronization. Used in EBs: ASN and join priority */
int frame80215e_create_ie_tsch_synchronization(uint8_t *buf, int len,
    struct ieee802154_ies *ies);
/* MLME sub-IE. TSCH slotframe and link. Used in EBs: initial schedule */
int frame80215e_create_ie_tsch_slotframe_and_link(uint8_t *buf, int len,
    struct ieee802154_ies *ies);
/* MLME sub-IE. TSCH timeslot. Used in EBs: timeslot template (timing) */
int frame80215e_create_ie_tsch_timeslot(uint8_t *buf, int len,
    struct ieee802154_ies *ies);
/* MLME sub-IE. TSCH channel hopping sequence. Used in EBs: hopping sequence */
int frame80215e_create_ie_tsch_channel_hopping_sequence(uint8_t *buf, int len,
    struct ieee802154_ies *ies);
/* RZ_time IE creation*/
int frame80215e_create_ie_rztime(uint8_t *buf, int len, 
    struct ieee802154_ies *ies);
/* Parse all Information Elements of a frame */
int frame802154e_parse_information_elements(const uint8_t *buf, uint8_t buf_size,
    struct ieee802154_ies *ies);
/*create the header of le_csl IE frame.*/
int frame80215e_create_ie_header_le_csl(uint8_t *buf, int len, 
    struct ieee802154_ies *ies);


#endif /* FRAME_802154E_H */
