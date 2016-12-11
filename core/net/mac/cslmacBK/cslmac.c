/*
 * cslmac.c
 *
 *  Created on: Jul 6, 2016
 *      Author: Homero
 */


#include "dev/leds.h"
#include "dev/radio.h"
#include "dev/watchdog.h"
#include "net/netstack.h"
#include "lib/random.h"
#include "net/mac/cslmac/clsmac.h"
#include "net/rime/rime.h"
#include "net/rime/timesynch.h"
#include "sys/compower.h"
#include "sys/pt.h"
#include "sys/rtimer.h"

#include "contiki-conf.h"
#include "sys/cc.h"
#include "net/mac/phase.h"

#include "net/mac/tsch/tsch-packet.h"
#include "net/mac/cslmac/clsmac.h"

#ifdef EXPERIMENT_SETUP
#include "experiment-setup.h"
#endif

#include <string.h>


uint16_t macCSLMaxPeriod=0; /* Maximum CSL sampled listening period in unit of 10 symbols in the entire PAN. This
determines the length of the wake-up sequence when communicating to a device whose CSL listen period is unknown. NHL
may set this attribute to zero to stop sending wake-up sequences with proper coordination with neighboring devices.*/

uint32_t macCSLChannelMask;/* 32-bit bitmap relative to phyCurrentPage of channels. It represents the list of channels
CSL operates on. Zero  means CSL operates on phyCurrentChannel of phyCurrentPage */

rtimer_clock_t macCSLFrame_PendingWaitT; /*Number of symbols to keep the receiver on after receiving a payload frame
 with Frame Control field frame pe nding bit set to one.*/

static volatile unsigned char radio_is_on = 0; /*indicates if the radio is on of off*/

static volatile unsigned char wait_framer = 0; /*indicates if the radio is on of off*/

static volatile unsigned char sending_frame = 0; /*indicates if the radio is on and it is sending frames*/

static volatile unsigned char keep_radio_on = 0; /*indicates if the radio is on of off*/
static volatile unsigned char someone_is_sending=0; /*used for indicate if the channel is busy*/



static struct rtimer rt;
static struct pt pt;


struct cslmac_config clsmac_config = {
		EACK_WAIT_DURATION,
		CLS_PERIOD,
		macCSLMaxPeriod
};

#include <stdio.h>


/*---------------------------------------------------------------------------*/
/*																			 */
/* 					csl_mac_init: initialize the clsmac-driver 			  	 */
/*																			 */
/*---------------------------------------------------------------------------*/

void
clsmac_init(void)
{
	radio_is_on = 0;
	wait_framer = 0;
	  PT_INIT(&pt);
	  macCSLMaxPeriod = CLS_PERIOD;
	  init_csl_config();
//start the phase tables
	  phase_init();
//Scheduler the wakeup and listen Phase phase
	  CSCHEDULE_POWERCYCLE(RTIMER_NOW() + CLS_PERIOD);
	 /* rtimer_set(&rt, RTIMER_NOW() + CLS_PERIOD, 1, cschedule_powercycle, NULL);*/

}

/*---------------------------------------------------------------------------*/
/* 							on: turn on the radio 							 */
/*---------------------------------------------------------------------------*/
static void
on(void)
{
  if(macCSLMaxPeriod > 0 && radio_is_on == 0) {
    radio_is_on = 1;
    NETSTACK_RADIO.on();
	LEDS_ON(LEDS_RED);
  }
}


/*---------------------------------------------------------------------------*/
/*							off: turn off the radio 						 */
/*---------------------------------------------------------------------------*/
static void
off(void)
{
  if(radio_is_on != 0 &&
		  wait_framer == 0 && keep_radio_on==0) {
    radio_is_on = 0;
    NETSTACK_RADIO.off();
	 LEDS_OFF(LEDS_RED);
  }
}

/*---------------------------------------------------------------------------*/
/*																			 */
/* 	powercycle_turn_radio_off: turn_off the radio checking sending_frame &   */
/*							wait_framer										 */
/*---------------------------------------------------------------------------*/

static void
powercycle_turn_radio_off(void)
{
  if(sending_frame == 0 &&
		  wait_framer == 0) {
    off();
  }

  //who knows
  /*
#if CXMAC_CONF_COMPOWER
  compower_accumulate(&compower_idle_activity);
#endif // CXMAC_CONF_COMPOWER //
  */
}

/*---------------------------------------------------------------------------*/
/*																			 */
/* 	powercycle_turn_radio_on: turn_on the radio checking sending_frame &     */
/*							  wait_framer									 */
/*---------------------------------------------------------------------------*/
static void
powercycle_turn_radio_on(void)
{
  if(sending_frame == 0 &&
		  wait_framer == 0) {
    on();
  }
}


/*---------------------------------------------------------------------------*/
/*																			 */
/* 	register_neighbor: introduce a new_neigbor_time & phase entry       	 */
/*																			 */
/*---------------------------------------------------------------------------*/
static void
register_neighbor(
		const linkaddr_t *neighbor, void * neighbor_time,
		uint16_t neighbor_time_phase){

		rtimer_clock_t _rtime = *(rtimer_clock_t ) neighbor_time;
		rtimer_clock_t _rphase = *(rtimer_clock_t ) neighbor_time_phase;

		phase_update(neighbor,_rtime,MAC_TX_OK,_rphase);

}

/*---------------------------------------------------------------------------*/
/*																			 */
/* 	update_neighbor:  update the neighbor time & phase entry       	 		 */
/*																			 */
/*---------------------------------------------------------------------------*/


static void
update_neighbor(const linkaddr_t *neighbor, void * neighbor_time,
		void * neighbor_time_phase,int mac_status){

	rtimer_clock_t _rtime = *(rtimer_clock_t ) neighbor_time;
	rtimer_clock_t _phase = *(rtimer_clock_t ) neighbor_time_phase;

	phase_update(neighbor,_rtime,mac_status,_phase);
}

/*---------------------------------------------------------------------------*/
static struct ctimer cpowercycle_ctimer;
#define CSCHEDULE_POWERCYCLE(rtime) cslchedule_powercycle((1ul * CLOCK_SECOND * (rtime)) / RTIMER_ARCH_SECOND)//to understand
static void csl_update_state(ieee802154e_cls_mod newmode);
static void recv_burst_off(void *ptr);
static char Someone_Sending(void);

/*---------------------------------------------------------------------------*/
static void csl_update_Wakeup_time(rtimer_clock_t *sample_update){

	cslchedule_powercycle((1ul * CLOCK_SECOND * (*(sample_update))) / RTIMER_ARCH_SECOND);
}


/*---------------------------------------------------------------------------*/
static void csl_update_state(ieee802154e_cls_mod newmode){
	clsmac_config.cls_operation_mode = newmode;
}


/*---------------------------------------------------------------------------*/
static void csl_update_sub_RX_state(ieee802154e_cls_RX_Sub_States newRXmode){
	clsmac_config.cls_RX_submode = newRXmode;
}

/*---------------------------------------------------------------------------*/
static void csl_update_sub_TX_state(ieee802154e_cls_RX_Sub_States newTXmode){

	clsmac_config.cls_TX_submode = newTXmode;
}
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
/*																			 */
/* 	cschedule_powercycle:  perform the wake-up period for listen a possible  */
/*                         package in the air       	 				     */
/*																			 */
/*---------------------------------------------------------------------------*/

cslchedule_powercycle(clock_time_t time)
{

  if(clsmac_config.macCSLPeriod > 0) {
    if(time == 0) {
      time = 1; //set up the clock time to 1 if it is zero
    }

    ctimer_set(&cpowercycle_ctimer, time,
               (void (*)(void *))cslpowercycle, NULL);//after ctime expired the function cpowercycle is called;
  }
}



/*---------------------------------------------------------------------------*/
/*																			 */
/* 	CSLpowercycle:  performs the ON/OFF scheduling for  data transmission    */
/*																			 */
/*---------------------------------------------------------------------------*/

static char
cslpowercycle(void *ptr){
			rtimer_clock_t _next_wakeup_detection;
			rtimer_clock_t _package_detection_period;
			rtimer_clock_t _nextSamplePeriod;
			static uint8_t packet_seen;
			_nextSamplePeriod=RTIMER_NOW()+clsmac_config.macCSLPeriod;

			if(clsmac_config.macCSLPeriod==0){
				//if macCSLPeriod is set up to zero then the radio must to be active always
				powercycle_turn_radio_on();
				return WRONG_CSL_CONFIG;
			}

	PT_BEGIN(&pt);
	powercycle_turn_radio_on();
	while(RTIMER_CLOCK_LT(RTIMER_NOW(),_nextSamplePeriod)&&radio_is_on){ //1.0 during the CLS_period
		_next_wakeup_detection = RTIMER_NOW() + WAKEUP_FRAME_TIME;
		packet_seen=0;
		while(RTIMER_CLOCK_LT(RTIMER_NOW(),_next_wakeup_detection)){
			//2.0 during the wake_up phase


				if(NETSTACK_RADIO.channel_clear() == 0 || Someone_Sending()) {
					//2.1 detect a package
						packet_seen = 1;
						break;
				}
		}
			if(!packet_seen){
				//2.2 if not package is detected then then we turn off the radio until the next wakeup sample.
				powercycle_turn_radio_off();
				_next_wakeup_detection= RTIMER_NOW() + INTER_PACKET_DEADLINE;
				CSCHEDULE_POWERCYCLE(_next_wakeup_detection);
				PT_YIELD(&pt);
			}else{
				//2.2a if package is detected wait until all package is received
				_package_detection_period=RTIMER_NOW()+TIME_AFTER_PACKET_DETECTED;
				while(RTIMER_CLOCK_LT(RTIMER_NOW(),_package_detection_period)
						&&radio_is_on && NETSTACK_RADIO.receiving_packet()){
					if(NETSTACK_RADIO.pending_packet()){
						//wait until reception of the package complete.
						break;
					}
				}
				if(!NETSTACK_RADIO.pending_packet()){
					//if not package is received sleep until the next wakeup period
					_next_wakeup_detection= RTIMER_NOW() + INTER_PACKET_DEADLINE;
					CSCHEDULE_POWERCYCLE(_next_wakeup_detection);
					PT_YIELD(&pt);
				}else{

					// parse the recieved frame.

				}
			}
	}
	PT_END(&pt);
}

/*---------------------------------------------------------------------------*/
/* 			Someone_Sending: Processing the input packages	 	 			 */
/*---------------------------------------------------------------------------*/


static char
Someone_Sending(void){
/*simple test for recognize if some one is sending or it is interference*/
	if(NETSTACK_RADIO.pending_packet()) {
		static int len = NETSTACK_RADIO.read(packetbuf_dataptr(), PACKETBUF_SIZE);
		if (len){
			PRINTF("SOMEONE_SENDING");
			return 1;

		}
	}else if (!(NETSTACK_RADIO.receiving_packet() || NETSTACK_RADIO.channel_clear())){
		PRINTF("INTEFERENCE");
		return 0;
	}
}

/*---------------------------------------------------------------------------*/
/* 				input_packet: Processing the input packages 	 			 */
/*---------------------------------------------------------------------------*/
static void
input_packet(void)
{
	frame802154_t rframe;
		struct ieee802154_ies ries;
		uint8_t rseqno;
		uint8_t rhdr_len=1;
		int ret;
		linkaddr_t linkaddr_node_dest;
		frame802154_t * hdr;
		hdr = packetbuf_dataptr();

	switch(hdr->fcf.frame_type){
		case FRAME802154_MULTPROFRAME:
		/*evaluate the destination of the frame*/
			if(linkaddr_cmp(packetbuf_addr(PACKETBUF_ADDR_RECEIVER),
		                                     &linkaddr_node_addr) ||
			 linkaddr_cmp(packetbuf_addr(PACKETBUF_ADDR_RECEIVER),
		                      &linkaddr_null)){

			/*Depending of the RX we perform different operations*/
			switch(clsmac_config.cls_RX_submode){
			case WAIT_WAKEUP:
			/*parsel the wake up frame*/
				if((ret = tsch_packet_parse_wake_up(packetbuf_dataptr(),packetbuf_datalen(),rseqno,&rframe,&ries,&rhdr_len)) == 0){
				PRINTF("there is not a wakeup frame")
				break;
				}
				rtimer_clock_t _package_detection_period;
			/*if the mote parse correctly the frame then go to sleep until data frame TX*/
					csl_update_sub_RX_state(WAIT_DATAFRAME);
					_package_detection_period= RTIMER_NOW() + ries.ie_rz_time;
					csl_update_Wakeup_time(&_package_detection_period);
				break;
			}
			}else{
				/*the destination address is different and this is not a wakeup frame then it is necessary process different*/
				if((ret = tsch_packet_parse_wake_up(packetbuf_dataptr(),packetbuf_datalen(),rseqno,
													 &rframe,&ries,&rhdr_len)) == 0){
								PRINTF("ERROR PARSE THE WAKEUP-FRAME OR NOT IS A WAKEUP-FRAME")
					break;
				}
				/*if this is a wakeup frame to another destination address then we go to sleep until the Transmision to the next node*/
					rtimer_clock_t _nextSamplePeriod;
					csl_update_sub_RX_state(WAIT_WAKEUP);
					_nextSamplePeriod =  RTIMER_NOW() + ries.ie_rz_time + TIME_AFTER_PACKET_DETECTED + EACK_WAIT_DURATION;
					csl_update_Wakeup_time(&_nextSamplePeriod);
				break;

			}
		break;
		case FRAME802154_DATAFRAME:
			/*there is a data frame to process*/
					if(linkaddr_cmp(packetbuf_addr(PACKETBUF_ADDR_RECEIVER),
		                                     &linkaddr_node_addr) ||
									linkaddr_cmp(packetbuf_addr(PACKETBUF_ADDR_RECEIVER),
									&linkaddr_null)) {
			/*if there is a dataframe to the mote and the  it is  waiting for a dataframe*/
			/* This is a regular packet that is destined to us or to the
			 broadcast address. */
						static struct ctimer ct; //in case of multiple framers
						int duplicate = 0;

			/*generate variables needed for ack frame*/
						int original_datalen;
						uint8_t *original_dataptr;
						original_datalen = packetbuf_datalen();
						original_dataptr = packetbuf_dataptr();
						wait_framer = packetbuf_attr(PACKETBUF_ATTR_PENDING);
			/*detecting duplicate*/
			duplicate = mac_sequence_is_duplicate();
				 	 if(duplicate){
				 		/* Drop the packet. */
				 		 PRINTF("CLS-MAC: Drop duplicate\n");
				 	 }else{
				 		 mac_sequence_register_seqno();
				 	 }
			/* oooo-----------CONTIKIMAC_CONF_COMPOWER--------------oooooo*/

			/*create and sent EACK*/
				csl_update_sub_RX_state(PREPARE_EACK_FRAME);
				frame802154_t info154;
		 	        frame802154_parse(original_dataptr, original_datalen, &info154);
		 	        	if(info154.fcf.frame_type == FRAME802154_DATAFRAME &&
		 	                   info154.fcf.ack_required != 0 &&
		 	                   linkaddr_cmp((linkaddr_t *)&info154.dest_addr,
		 	                       &linkaddr_node_addr)){

							static uint8_t ack_buf[TSCH_PACKET_MAX_LEN];
		 	        		static int32_t estimated_drift;
		 	        		static int ack_len;
		 	        		int do_nack = 0;
		 	        		ack_len = tsch_packet_create_eack(ack_buf, sizeof(ack_buf),
		 	        		                  &info154.dest_addr,&info154.seq , (int16_t)RTIMERTICKS_TO_US(estimated_drift), do_nack,
		 	        		                  WAKE_UP_EACK,clsmac_config.macCSLPeriod,NULL);
		 	        		csl_update_sub_RX_state(EACK_READY_TO_TX);
		 	        		NETSTACK_RADIO.prepare((const void *)ack_buf, ack_len);
		 	        		csl_update_sub_RX_state(TRASMITING_EACK);
		 	        		NETSTACK_RADIO.send(ack_buf,sizeof(ack_buf));
		 	        		csl_update_sub_RX_state(EACK_TX_SUCESSFUL);
						}
			/*ending and sent EACK*/
				csl_update_sub_RX_state(RECIVED_DATAFRAME); /*it means that the package it is received */
				NETSTACK_MAC.input();

				if(wait_framer){
							/*in case of burst or multiple packets*/
							/*the deivice must to be awake until FRAME_PENDING_TIME*/
										/*setup the clock for recive each package*/
										on();
										macCSLFrame_PendingWaitT = RTIMER_NOW() + FRAME_PENDING_TIME;
										ctimer_set(&ct,macCSLFrame_PendingWaitT,recv_burst_off,NULL);
							/*ctimer_set(&ct, INTER_PACKET_DEADLINE, recv_burst_off, NULL);*/
				}else{
							/*otherwise simple goes to sleep until the next sample time*/
									off();
									rtimer_clock_t _nextSamplePeriod;
									_nextSamplePeriod =  RTIMER_NOW()+clsmac_config.macCSLPeriod;
									csl_update_Wakeup_time(&_nextSamplePeriod);
									ctimer_stop(&ct);
				}
				break;
		}else{
			/*if the mote are received a dataframe for another destination it means that something it is worng since at this point
			the mote must to be sleeping*/
			PRINTF("ERROR: THE MOTE RECEIVED A DATAFRAMER FOR ANOTHER NODE");
		}
	break;
	case FRAME802154_ACKFRAME:
		/*in the case that a ackframe it is received */
		switch (clsmac_config.cls_TX_submode){
		case WAIT_EACKFRAME:
		/*check the destination_address */
		if(linkaddr_cmp(packetbuf_addr(PACKETBUF_ADDR_RECEIVER),
		                                     &linkaddr_node_addr)){
			/*EACKframe it is received...*/
			uint8_t ackbuf[TSCH_PACKET_MAX_LEN];
			int ack_len;
			uint16_t seqno;
			//is_time_source = 0;
			uint8_t ack_hdrlen;
			struct ieee802154_ies ack_ies;
			frame802154_t frame;

            ack_len = NETSTACK_RADIO.read((void *)ackbuf, sizeof(ackbuf));

			/* The radio driver should return 0 if no valid packets are in the rx buffer */
			//is_time_source = 0;
              if(ack_len > 0) {
                if(tsch_packet_parse_eack(ackbuf, ack_len, seqno,
                    &frame, &ack_ies, &ack_hdrlen) == 0) {
                  ack_len = 0;
				  PRINTF("ERROR: PROCESSING EACK-FRAME");
				  break;
                }
			/*valid EACK frame*/
				/*update the neigbor phase and CLS_period*/
				linkaddr_cmp(packetbuf_addr(PACKETBUF_ADDR_SENDER),
		                                     &linkaddr_node_dest);

				update_neighbor(&linkaddr_node_dest,&ack_ies.CSL_period,&ack_ies.CSL_phase,MAC_TX_OK);
			 }
		}else{
			PRINTF("ERROR: RECEIVED DIFERENT DESTINATION EACK_FRAME");
		}
		break;
		}
	break;
	}
}

/*---------------------------------------------------------------------------*/
/* Timer callback triggered when receiving a burst, after having
   waited for a next packet for a too long time. Turns the radio off
   and leaves burst reception mode */
static void
recv_burst_off(void *ptr)
{
  off();
  wait_framer = 0;
}
/*---------------------------------------------------------------------------*/
/*																			 */
/*			cls_send_list:  prepare the packages to send.					 */
/*																			 */
/*---------------------------------------------------------------------------*/

static void
cls_send_list(mac_callback_t sent,void *ptr,struct rdc_buf_list *buf_list)
{
	 if(buf_list != NULL) {
		 queuebuf_to_packetbuf(buf_list->buf);
		 cls_qsend_packet(sent, ptr);

	 }

}

/*---------------------------------------------------------------------------*/
/*																			 */
/*			cls_qsend_packet: Evaluate the state of the network before to 	 */
/* 								TX the package 					 			 */
/*																			 */
/*---------------------------------------------------------------------------*/

static void
cls_qsend_packet(mac_callback_t sent, void *ptr)
{
  int ret;
  if(someone_is_sending) {
    PRINTF("cxmac: should queue packet, now just dropping %d %d %d %d.\n",
    		wait_framer, someone_is_sending, sending_frame, radio_is_on);

    /*if some one is sending we have to wakeup again in the next wakeup interval*/

    RIMESTATS_ADD(sendingdrop);
    ret = MAC_TX_COLLISION;
  } else {
    PRINTF("cxmac: send immediately.\n");
    ret = send_packet();
  }

  mac_call_sent_callback(sent, ptr, ret, 1);
}
/*---------------------------------------------------------------------------*/


/*---------------------------------------------------------------------------*/
/* 				send_packet:  Transmit the package in the air	 			 */
/*																			 */
/*---------------------------------------------------------------------------*/
/*CLS-MAC SEND IMPLEMENTATION*/
static int
send_packet(void)
{
	/*This is the implementation of package send of the cls-RDC protocol*/
	/*unicast and broadcast */

	/*This implementation Consider Three Phases*/

	/*Phase 1: Idicate the next time for the next Frame*/
	/*1.1 Synchornous TX or 1.2 Asynchornous TX*/

	/*Phase 2: send the data frame*/

	/*Phase 3: wait for the eACK and update the period and face of the neighbor*/

	/* In case broadcast of broadcast TX always the sender consider async Tx the destination address it is always 0xFFFF the higher level could include IE like LE CSL IE for propagate the iformation of phase and period to all the neighbors*/

	/**/


	/*-------------------------_Phase 1_--------------------------------*/
powercycle_turn_radio_on();

char is_broadcast = 0;
struct phase *e;
const linkaddr_t * dest_address; //Destination Address.
const linkaddr_t * src_address;	//Source Address.
//uint8_t ackbuf[ACK_LEN];


	if((someone_is_sending=Someone_Sending())==1){
		/*if there is some one sending goto sleep until the next wakeup time */
		powercycle_turn_radio_off();
		return MAC_TX_COLLISION;
	}else{
			/*check if for the destination there is a entry in the phase table, if yes then we are in a sync mode, else async*/
			dest_address = packetbuf_addr(PACKETBUF_ADDR_RECEIVER);//geting the dest address form the buffer
			src_address = packetbuf_addr(PACKETBUF_ADDR_SENDER);//geting the source address form the buffer
		if((dest_address != 0xFFFF) && (e= is_a_phase_neighbor_entry (dest_address) != 0)){
/*synchonous Transmission*/
				/*unicast: the wakeup phase consist of the preparation and TX of the wakeup frames with:
				a. The destionation short address 2 octects
				b. The time until the next frame tx*/
				//preparing the wakeup frame

			is_broadcast = 0;
#if NETSTACK_CONF_WITH_IPV6
		    PRINTDEBUG("cxmac: send unicast to %02x%02x:%02x%02x:%02x%02x:%02x%02x\n",
		           packetbuf_addr(PACKETBUF_ADDR_RECEIVER)->u8[0],
		           packetbuf_addr(PACKETBUF_ADDR_RECEIVER)->u8[1],
		           packetbuf_addr(PACKETBUF_ADDR_RECEIVER)->u8[2],
		           packetbuf_addr(PACKETBUF_ADDR_RECEIVER)->u8[3],
		           packetbuf_addr(PACKETBUF_ADDR_RECEIVER)->u8[4],
		           packetbuf_addr(PACKETBUF_ADDR_RECEIVER)->u8[5],
		           packetbuf_addr(PACKETBUF_ADDR_RECEIVER)->u8[6],
		           packetbuf_addr(PACKETBUF_ADDR_RECEIVER)->u8[7]);
#else
		    PRINTDEBUG("cxmac: send unicast to %u.%u\n",
		           packetbuf_addr(PACKETBUF_ADDR_RECEIVER)->u8[0],
		           packetbuf_addr(PACKETBUF_ADDR_RECEIVER)->u8[1]);
#endif /* NETSTACK_CONF_WITH_IPV6 */

//now we have to create the wakeupframe and transmited until the end of the wakeup phase
			rtimer_clock_t _nextSamplePeriod;
			rtimer_clock_t rztime = e->time;
			_nextSamplePeriod= RTIMER_NOW() + rztime;
				while(RTIMER_CLOCK_LT(RTIMER_NOW(),_nextSamplePeriod)&&radio_is_on){
					csl_update_sub_TX_state(PREPARE_WAKEUP_FRAME);


					static uint8_t wakeup_buf[TSCH_PACKET_MAX_LEN];
					static int wakeup_len;
					uint8_t seqno = generate_seq_num(); //generate a seq number.
					uint8_t more_frame= packetbuf_attr(PACKETBUF_ATTR_PENDING); //gertting the package pending bit from the buffer.
					uint16_t rztime = _nextSamplePeriod - RTIMER_NOW();

//creation of the wakeup frame
					wakeup_len= tsch_packet_create_wakeup(wakeup_buf,dest_address,sizeof(wakeup_buf),seqno,more_frame,&rztime);
					if(wakeup_len>0 ) {
//prepare and transmission of the wakeup-frame
						if(rztime > MAX_WAKEUP_FRAME_TIME){
						NETSTACK_RADIO.prepare((const void *)wakeup_buf, wakeup_len);
//transmission of the wakeup-frame
						NETSTACK_RADIO.send(wakeup_buf,sizeof(wakeup_buf));
						}else{
							/*if the time to TX the wakeup frame is not sufficient according to the windows tx size then the mote sleep until the
							data frame transmission.
							*/
						powercycle_turn_radio_off();
							//scheduler the dataTX
						}
					}else{
						PRINTF("ERROR GENERATED A WAKEUP FRAME Wrong Wakeuplength \n",wakeup_len)
					}

				}
/*-------------------------_Phase 2_--------------------------------*/
				//call the dataTX procedure
					sending_frame = 1;
					int res = send_Data_packet();
					if(res != MAC_TX_OK){
					//Colission occours
					PRINTF("csl: error transmiting the frame ");
					return MAC_TX_COLLISION;
					}else{
						sending_frame = 0;
						PRINTF("csl: dataframe_TX successful now wait for ACK");
/*-------------------------_Phase 3_--------------------------------*/
		//wait for ACK and Update the neigbor phase and period.
						rtimer_clock_t _package_detection_period;
						_package_detection_period= RTIMER_NOW() + EACK_WAIT_DURATION;
							while(RTIMER_CLOCK_LT(RTIMER_NOW(),_package_detection_period)){
								wait_framer=1;
								packetbuf_clear();
								uint16_t seqno;
								//is_time_source = 0;
								uint8_t ack_hdrlen;
								struct ieee802154_ies ack_ies;
								frame802154_t frame;
								int len = NETSTACK_RADIO.read(packetbuf_dataptr(), PACKETBUF_SIZE);
									if(len>0){
		//test if the ACK destination it is the adrress of the mote
										if(linkaddr_cmp(packetbuf_addr(PACKETBUF_ADDR_RECEIVER),
		                                    &linkaddr_node_addr)){
/*EACKframe it is received...*/
											static uint8_t ackbuf[TSCH_PACKET_MAX_LEN];
											static int ack_len;

											if(tsch_packet_parse_eack(ackbuf, ack_len, seqno,
																	&frame, &ack_ies, &ack_hdrlen) == 0){
												PRINTF("ERROR: PROCESSING EACK-FRAME");
												break;
											}
/*valid EACK frame*/
/*update the neigbor phase and CLS_period*/
										wait_framer=0;
											linkaddr_cmp(packetbuf_addr(PACKETBUF_ADDR_SENDER),
														&dest_address);
											update_neighbor(&dest_address,&ack_ies.CSL_period,&ack_ies.CSL_phase,MAC_TX_OK);
										}else{
											PRINTF("ERROR: RECEIVED DIFERENT DESTINATION EACK_FRAME");
										return MAC_TX_COLLISION;
										}
									}else{
										PRINTF("CSL-ERROR: reading the radio package_buffer ");
										return MAC_TX_COLLISION;
									}
							}
								if(wait_framer){
									wait_framer=0;
									PRINTF("CSL-ERROR: EACK_Reception_ERROR");
									return MAC_TX_COLLISION;
								}
					}

			}else{
			/*asynchronous TX:
			we don't know the wakeup phase of the receiver
			broadcast TX.
			*/
/*we create the wakeup frame as usually with the dest_adrress*/
					rtimer_clock_t _nextSamplePeriod;
					rtimer_clock_t _NextTransmissionFrame;
					_nextSamplePeriod = RTIMER_NOW() + CLS_MAX_PERIOD;
//while the maxclsPeriod the sender create a wakeup frame and transmited.
					while(RTIMER_CLOCK_LT(RTIMER_NOW(),_nextSamplePeriod)&&radio_is_on){
					static uint8_t wakeup_buf[TSCH_PACKET_MAX_LEN];//instance of the wakeup buf
					static int wakeup_len; //used for evaluate the length of the wakeup-frame
					uint8_t seqno = generate_seq_num(); //generate a seq number.
					uint8_t more_frame= packetbuf_attr(PACKETBUF_ATTR_PENDING); //gertting the package pending bit from the buffer.
//creation of the wakeup frame
					_NextTransmissionFrame= _nextSamplePeriod - RTIMER_NOW();
					wakeup_len= tsch_packet_create_wakeup(wakeup_buf,dest_address,sizeof(wakeup_buf),seqno,more_frame,&_NextTransmissionFrame);
						if(wakeup_len > 0){
//prepare and transmission of the wakeup-frame
							if(_NextTransmissionFrame > MAX_WAKEUP_FRAME_TIME){
								NETSTACK_RADIO.prepare((const void *)wakeup_buf, wakeup_len);
//transmission of the wakeup-frame
								sending_frame = 1;
								NETSTACK_RADIO.send(wakeup_buf,sizeof(wakeup_buf));
							}else{
							/*if the time to TX the wakeup frame is not sufficient according to the windows tx size then the mote sleep until the
						data frame transmission.*/
								powercycle_turn_radio_off();
								//scheduler the dataTX

							}
						}else{
						//Error in the wakeup frame generation
						PRINTF("ERROR GENERATED A WAKEUP FRAME Wrong Wakeuplength \n",wakeup_len);
						}
/*-------------------------_Phase 2_--------------------------------*/
				//call the dataTX procedure
						sending_frame = 1;
						int res = send_Data_packet();
						if(res != MAC_TX_OK){
							//Colission occours
							PRINTF("csl: error transmitting the frame ");
							return MAC_TX_COLLISION;
						}else{
							sending_frame = 0;
							PRINTF("csl: dataframe_TX successful now wait for ACK");
/*-------------------------_Phase 3_--------------------------------*/
		//wait for ACK and Update the neigbor phase and period.
//wait for ACK and Update the neigbor phase and period.
							if(dest_address!=0xFFFF){
								rtimer_clock_t _package_detection_period;
								_package_detection_period= RTIMER_NOW() + EACK_WAIT_DURATION;
									while(RTIMER_CLOCK_LT(RTIMER_NOW(),_package_detection_period)){
										wait_framer=1;
										packetbuf_clear();
										uint16_t seqno;
										//is_time_source = 0;
										uint8_t ack_hdrlen;
										struct ieee802154_ies ack_ies;
										frame802154_t frame;
										int len = NETSTACK_RADIO.read(packetbuf_dataptr(), PACKETBUF_SIZE);
										if(len>0){
											//test if the ACK destination it is the adrress of the mote
											if(linkaddr_cmp(packetbuf_addr(PACKETBUF_ADDR_RECEIVER),
															&linkaddr_node_addr)){

/*EACKframe it is received...*/
												static uint8_t ackbuf[TSCH_PACKET_MAX_LEN];
												static int ack_len;

												if(tsch_packet_parse_eack(ackbuf, ack_len, seqno,
													&frame, &ack_ies, &ack_hdrlen) == 0){
													PRINTF("ERROR: PROCESSING EACK-FRAME");
													break;
												}
/*valid EACK frame*/
/*update the neigbor phase and CLS_period*/
											wait_framer=0;
											linkaddr_cmp(packetbuf_addr(PACKETBUF_ADDR_SENDER),
														&dest_address);
											update_neighbor(&dest_address,&ack_ies.CSL_period,&ack_ies.CSL_phase,MAC_TX_OK);
											}else{
												PRINTF("ERROR: RECEIVED DIFERENT DESTINATION EACK_FRAME");
											}
										}else{
											PRINTF("CSL-ERROR: reading the radio package_buffer ");
										}
								}
								if(wait_framer){
									wait_framer=0;
									PRINTF("CSL-ERROR: EACK_RX_ERROR");
									return MAC_TX_COLLISION;
									}

							}else{
								/*this is a broadcastTX we dont wait for EACK*/
								wait_framer=0;
								PRINTF("CSL: broadcastTX");
								return MAC_TX_COLLISION;
							}
				return 	MAC_TX_OK;
						}/*end frame transmission and ack phase 2 & 3*/

			}/*end async TX*/

		}/*end send process*/
}



static int
send_Data_packet(void)
{

	int len;
	struct queuebuf *packet;
	rtimer_clock_t csl_period;
	const linkaddr_t * dest_address;
	/*find if there is a entry for the the neighbor in the phase/period Table*/
	dest_address = packetbuf_addr(PACKETBUF_ADDR_RECEIVER);//geting the dest address form the buffer

	if(is_a_phase_neighbor_entry(dest_address)!=0){

		/*sync TX*/
		//if there is a entry for the destination address, then we send the frame at the time indicated in the neighbor_table.

		/*1. Create The framer to TX */


		  /* Create the X-MAC header for the data packet. */
#if !NETSTACK_CONF_BRIDGE_MODE
	  /* If NETSTACK_CONF_BRIDGE_MODE is set, assume PACKETBUF_ADDR_SENDER is already set. */
		packetbuf_set_addr(PACKETBUF_ADDR_SENDER, &linkaddr_node_addr);
#endif
		  if(packetbuf_holds_broadcast()) {
		    is_broadcast = 1;
		    PRINTDEBUG("cxmac: send broadcast\n");
		  } else {
#if NETSTACK_CONF_WITH_IPV6
		    PRINTDEBUG("cxmac: send unicast to %02x%02x:%02x%02x:%02x%02x:%02x%02x\n",
		           packetbuf_addr(PACKETBUF_ADDR_RECEIVER)->u8[0],
		           packetbuf_addr(PACKETBUF_ADDR_RECEIVER)->u8[1],
		           packetbuf_addr(PACKETBUF_ADDR_RECEIVER)->u8[2],
		           packetbuf_addr(PACKETBUF_ADDR_RECEIVER)->u8[3],
		           packetbuf_addr(PACKETBUF_ADDR_RECEIVER)->u8[4],
		           packetbuf_addr(PACKETBUF_ADDR_RECEIVER)->u8[5],
		           packetbuf_addr(PACKETBUF_ADDR_RECEIVER)->u8[6],
		           packetbuf_addr(PACKETBUF_ADDR_RECEIVER)->u8[7]);
#else
		    PRINTDEBUG("cxmac: send unicast to %u.%u\n",
		           packetbuf_addr(PACKETBUF_ADDR_RECEIVER)->u8[0],
		           packetbuf_addr(PACKETBUF_ADDR_RECEIVER)->u8[1]);
#endif /* NETSTACK_CONF_WITH_IPV6 */
		  }
		  len = NETSTACK_FRAMER.create();
		  if(len<0){/* Failed to send */
			   PRINTF("cxmac: send failed, error creating a data frame\n");
			    return MAC_TX_ERR_FATAL;
		  }
	 //compact the package to sent
		packetbuf_compact();
		packet = queuebuf_new_from_packetbuf();

		if(packet == NULL) {
		     /* No buffer available */
		  PRINTF("cxmac: send failed, no queue buffer available (of %u)\n",
		          QUEUEBUF_CONF_NUM);
			      return MAC_TX_ERR;
		}

		/*2. get the phase of the neighbor and Transmit it at correct time*/
		off();
		csl_period =*((rtimer_clock_t *) sget_period(dest_address));
	}

}

/*---------------------------------------------------------------------------*/
static unsigned short
channel_check_interval(void)
{
  return 0;
}
/*---------------------------------------------------------------------------*/


/*---------------------------------------------------------------------------*/
static int
turn_off(int keep_radio_on)
{
  //cxmac_is_on = 0;
  if(radio_is_on) {
    return NETSTACK_RADIO.on();
  } else {
    return NETSTACK_RADIO.off();
  }
}
/*---------------------------------------------------------------------------*/


static int
turn_on(void)
{
  //clsmac_is_on = 1;
  /*  rtimer_set(&rt, RTIMER_NOW() + cxmac_config.off_time, 1,
      (void (*)(struct rtimer *, void *))powercycle, NULL);*/
  CSCHEDULE_POWERCYCLE(CLS_PERIOD);
  return 1;
}


/*---------------------------------------------------------------------------*/
static unsigned short
channel_check_interval(void)
{
  return (1ul * CLOCK_SECOND * DEFAULT_PERIOD) / RTIMER_ARCH_SECOND;
}



/*---------------------------------------------------------------------------*/
const struct rdc_driver cslmac_driver =
  {
    "ClS-MAC",
    clsmac_init,
    cls_qsend_packet,
    cls_send_list,
    input_packet,
    turn_on,
    turn_off,
    channel_check_interval,
  };

