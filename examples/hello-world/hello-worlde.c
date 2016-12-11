/**
 * \file
 *         A simple power saving MAC protocol based on CLS-MAC [SenSys 2016]
 * \author
 *         Adam Dunkels <adam@sics.se>
 *         Niclas Finne <nfi@sics.se>
 *         Joakim Eriksson <joakime@sics.se>
 */
/**/
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
#include "net/mac/tsch/tsch.h"
#include  "net/mac/tsch/tsch-packet.h"


#include "contiki-conf.h"
#include "sys/cc.h"



#ifdef EXPERIMENT_SETUP
#include "experiment-setup.h"
#endif

#include <string.h>

//#define DEFAULT_PERIOD_TIME (RTIMER_ARCH_SECOND / 160)
//#define MAC_CLS_PERIOD ((DEFAULT_PERIOD_TIME) + (RTIMER_ARCH_SECOND / NETSTACK_RDC_CHANNEL_CHECK_RATE - (DEFAULT_PERIOD_TIME)))

#ifdef MAC_CLS_PERIOD
#if MAC_CLS_PERIOD==0
#define DEFAULT_ON_TIME (MAC_CLS_PERIOD)
#define DEFAULT_OFF_TIME 0
#else
#define DEFAULT_ON_TIME (MAC_CLS_PERIOD / 10)
#define DEFAULT_OFF_TIME(9 * MAC_CLS_PERIOD / 10)
#endif
#else
#define DEFAULT_ON_TIME (RTIMER_ARCH_SECOND / 160)
#define DEFAULT_OFF_TIME (RTIMER_ARCH_SECOND / NETSTACK_RDC_CHANNEL_CHECK_RATE - DEFAULT_ON_TIME)
#endif

#define DEFAULT_CLS_PERIOD (DEFAULT_OFF_TIME + DEFAULT_ON_TIME)

#define WAIT_TIME_BEFORE_STROBE_ACK RTIMER_ARCH_SECOND / 1000

#define DEFAULT_STROBE_WAIT_TIME (7 * DEFAULT_ON_TIME / 8)


#include "lib/list.h"
#include "lib/memb.h"

#include <stdio.h>




struct neighbor_element {
  struct neighbor_element *next;
  linkaddr_t neighbor;
  rtimer_clock_t sample_channel; //frecuency for sample the channel i.e Time_on + Time_sleep.
  rtimer_clock_t phase; //case o multiplechannels.	
};

#define MAX_NEIGHBORS 4
LIST(encounter_list);
MEMB(encounter_memb, struct neighbor_element, MAX_NEIGHBORS);




static struct pt pt;
static struct pt pt2;

static volatile uint8_t cslmac_is_on = 0;

static volatile unsigned char waiting_for_packet = 0;
static volatile unsigned char someone_is_sending = 0;
static volatile unsigned char we_are_sending = 0;
static volatile unsigned char radio_is_on = 0;
static volatile unsigned char is_listening=0;

#undef LEDS_ON
#undef LEDS_OFF
#undef LEDS_TOGGLE

#define LEDS_ON(x) leds_on(x)
#define LEDS_OFF(x) leds_off(x)
#define LEDS_TOGGLE(x) leds_toggle(x)
#define DEBUG 1
#if DEBUG
#include <stdio.h>
#define PRINTF(...) printf(__VA_ARGS__)
#define PRINTDEBUG(...) printf(__VA_ARGS__)
#else
#undef LEDS_ON
#undef LEDS_OFF
#undef LEDS_TOGGLE
#define LEDS_ON(x)
#define LEDS_OFF(x)
#define LEDS_TOGGLE(x)
#define PRINTF(...)
#define PRINTDEBUG(...)
#endif

/*variables for test*/
int len;
uint8_t wakeupbuf [TSCH_PACKET_MAX_LEN];
linkaddr_t dest_addr= {0xff,0xff, 0, 0, 0, 0, 0, 0};
uint8_t seqno=0x1d;
uint8_t more_frame=1;
uint16_t rztime =0xaaaa;
/*------------------------*/



static uint8_t is_streaming;
static uint8_t next_sync_wakeup;
static linkaddr_t is_streaming_to, is_streaming_to_too;
static rtimer_clock_t stream_until;

static uint8_t wakeack_seqno;


struct cslmac_config cslmac_config = {
  DEFAULT_ON_TIME,
  DEFAULT_OFF_TIME,
  4 * DEFAULT_ON_TIME + DEFAULT_OFF_TIME,
  DEFAULT_STROBE_WAIT_TIME,
 2 * DEFAULT_ON_TIME + DEFAULT_OFF_TIME
};


/*---------------------------------------------------------------------------*/
/* 				on: turn on the radio 		             */
/*---------------------------------------------------------------------------*/
static void
on(void)
{
  if(cslmac_is_on && radio_is_on == 0) {
    radio_is_on = 1;
    NETSTACK_RADIO.on();
    LEDS_ON(LEDS_RED);
  }
}
/*---------------------------------------------------------------------------*/
/*				off: turn off the radio 	             */
/*---------------------------------------------------------------------------*/
static void
off(void)
{
  if(cslmac_is_on && radio_is_on != 0 && is_listening == 0 &&
     is_streaming == 0) {
    radio_is_on = 0;
    NETSTACK_RADIO.off();
    LEDS_OFF(LEDS_RED);
  }
}
/*-------------------------------------------------------------------------------*/
/*	                   						         */
/* 	powercycle_turn_radio_off: turn_off the radio checking sending_frame &   */
/*							wait_framer		 */
/*-------------------------------------------------------------------------------*/
static void
powercycle_turn_radio_on(void)
{
  if(we_are_sending == 0 &&
     waiting_for_packet == 0) {
    on();
  }
}
/*---------------------------------------------------------------------------*/
/*	     								     */
/* 	powercycle_turn_radio_on: turn_on the radio checking sending_frame & */
/*							  wait_framer	     */
/*---------------------------------------------------------------------------*/
static void
powercycle_turn_radio_off(void)
{
  if(we_are_sending == 0 &&
     waiting_for_packet == 0) {
    off();
  }
#if CSLMAC_CONF_COMPOWER
  compower_accumulate(&compower_idle_activity);
#endif /* CSLMAC_CONF_COMPOWER */
}


/*---------------------------------------------------------------------------*/
/* 			Someone_Sending: Processing the input packages	 	 			 */
/*---------------------------------------------------------------------------*/


static char
Channel_Busy(void){
char len;
/*simple test for recognize if some one is sending or it is interference*/
	if(NETSTACK_RADIO.pending_packet()) {
		   len = NETSTACK_RADIO.read(packetbuf_dataptr(), PACKETBUF_SIZE);
		if (len){
			PRINTF("cslmac: someone sending %u\n",len);
			return 1;

		}
	}else if (!(NETSTACK_RADIO.receiving_packet() || NETSTACK_RADIO.channel_clear())){
		PRINTF("cslmac: interference detected %u\n",len);
		return 0;
	}
return 0;
PRINTF("cslmac: channel it is free \n");
}


/*---------------------------------------------------------------------------*/
/* 			Is_broadcast: used for retrieve if the destination is broadcast  */
/*			or unicast										 	 			 */
/*---------------------------------------------------------------------------*/
static char
Is_Broadcast(const linkaddr_t * address){
	if((address->u8[0]==255)&&(address->u8[1]==255))return 1;
	return 0;
	
}

/*----------------------------------------------------------------------------*/
/* 			Generate_ack_seqno:used for generate a sequncial number fo wakeup */
/*			or unicast										 	 			  */
/*----------------------------------------------------------------------------*/
static char
Generate_ack_seqno(void){
	 wakeack_seqno=random_rand() % 256;
	 return wakeack_seqno;
}

/*---------------------------------------------------------------------------*/
/* 			Get_ack_seqno:used for generate a sequncial number fo wakeup     */
/*			or unicast										 	 			 */
/*---------------------------------------------------------------------------*/
static char
Get_ack_seqno(void){
	return wakeack_seqno;
}
/*---------------------------------------------------------------------------*/
static struct ctimer cpowercycle_ctimer;
static struct timer timer_on;
static struct timer timer_off;
uip_ipaddr_t ipaddr;

static char cpowercycle(void *ptr);
static void sync_powercycle(void*ptr);
static void cschedule_powercycle(clock_time_t time);
static void cschedule_sync_powercycle(clock_time_t time);
#define CSCHEDULE_POWERCYCLE(rtime) cschedule_powercycle((1ul * CLOCK_SECOND * (rtime)) / RTIMER_ARCH_SECOND)
#define CSCHEDULE_SYNCPOWERCYCLE(rtime) cschedule_sync_powercycle((1ul * CLOCK_SECOND * (rtime)) / RTIMER_ARCH_SECOND)



/*---------------------------------------------------------------------------*/
/*									     									 */
/* 		csl_mac_init: initialize the clsmac-driver 	             			 */
/*									     									 */
/*---------------------------------------------------------------------------*/
void
clsmac_init(void)
{
  radio_is_on = 0;
  waiting_for_packet = 0;
  PT_INIT(&pt2);
  
  cslmac_is_on = 1;
  
  //start the neighbor list
   list_init(encounter_list);
  memb_init(&encounter_memb);
  //Scheduler the wakeup and listen Phase phase
  CSCHEDULE_SYNCPOWERCYCLE(DEFAULT_OFF_TIME);
}

/*---------------------------------------------------------------------------*/
/*	       								     								 */
/* 	register_neighbor: introduce a new_neigbor_time & phase entry        	 */
/*									     									 */
/*---------------------------------------------------------------------------*/
static void
register_neighbor(const linkaddr_t *neighbor, rtimer_clock_t time, rtimer_clock_t phase)
{
  struct neighbor_element *e;

  /* If we have an entry for this neighbor already, we renew it. */
  for(e = list_head(encounter_list); e != NULL; e = list_item_next(e)) {
    if(linkaddr_cmp(neighbor, &e->neighbor)) {
      e->sample_channel = time;
	  e->phase = phase;
      break;
    }
  }
  /* No matching encounter was found, so we allocate a new one. */
  if(e == NULL) {
    e = memb_alloc(&encounter_memb);
    if(e == NULL) {
      /* We could not allocate memory for this encounter, so we just drop it. */
      return;
    }
    linkaddr_copy(&e->neighbor, neighbor);
    e->sample_channel = time;
    list_add(encounter_list, e);
  }
}

/*---------------------------------------------------------------------------*/
/* is_neighbor_in_list: returns if a neighbor is included in the list 	     */
/*---------------------------------------------------------------------------*/
static int
is_neighbor_in_list(const linkaddr_t *neighbor)
{
	struct neighbor_element *e;
	for(e = list_head(encounter_list); e != NULL; e = list_item_next(e)){
		if(linkaddr_cmp(neighbor, &e->neighbor)){
			return 1;
		}
	}
	return 0;
}

/*---------------------------------------------------------------------------*/
/*																		     */
/* 	update_neighbor:  update the neighbor time & phase entry       	         */
/*								             								 */
/*---------------------------------------------------------------------------*/


static int
update_neighbor (const linkaddr_t *neighbor, rtimer_clock_t time, rtimer_clock_t phase){
	if(is_neighbor_in_list(neighbor)){
		struct neighbor_element * e;
			for(e = list_head(encounter_list); e != NULL; e = list_item_next(e)){
				if(linkaddr_cmp(neighbor, &e->neighbor)){
					e->sample_channel = time;
					e->phase = phase;
					return 1;
				}
			}
	}
	return 0;
	
}

//mac_dsn = random_rand() % 256;


/*-------------------------------------------------------------------------------*/
/*	                                                                             */
/*                  send_packet(void): RDC send package protocol                 */
/*	                                                                             */
/*-------------------------------------------------------------------------------*/


static int
send_packet(void)
{
int len,hdr_len;	
char is_broadcast = 0;
const linkaddr_t * dest_address; //Destination Address.
const linkaddr_t * src_address;	//Source Address.
const linkaddr_t *update_neighbor_node;
struct neighbor_element * e;   // Used in case of unicast/syncronous-TX
struct ieee802154_ies r_ies;
struct queuebuf *packet;
frame802154_t frame;
static uint8_t wakeup_buf[TSCH_PACKET_MAX_LEN];
static int wakeup_len;
rtimer_clock_t t;
rtimer_clock_t _nextSamplePeriod;
rtimer_clock_t rztime;
rtimer_clock_t wakeup_tx_time;
uint8_t more_frame;
uint8_t async_dataframe_tx_wait=0;
uint8_t collisions;
uint8_t r_seqno;
uint8_t ack_requiered;
uint8_t got_ack=0;

if((someone_is_sending=Channel_Busy())==1){
		PRINTF("cslmac: channel is busy someone is transmiting \n");
		powercycle_turn_radio_off();
		return MAC_TX_COLLISION;

	}else{
		
#if !NETSTACK_CONF_BRIDGE_MODE
  /* If NETSTACK_CONF_BRIDGE_MODE is set, assume PACKETBUF_ADDR_SENDER is already set. */
  packetbuf_set_addr(PACKETBUF_ADDR_SENDER, &linkaddr_node_addr);
#endif		
		/*determine if it is a broadcast or unicast TX*/
		dest_address = packetbuf_addr(PACKETBUF_ADDR_RECEIVER);//geting the dest address form the buffer
		src_address = packetbuf_addr(PACKETBUF_ADDR_SENDER);//geting the source address form the buffer
		
		if(!Is_Broadcast(dest_address)&&!packetbuf_holds_broadcast() && is_neighbor_in_list(dest_address)){
			//it is a unicast TX
				/*synchonous Transmission*/
				/*unicast: sleep til the sample period of the neighbor then TX the dataframe */
			PRINTF("cslmac: Unicast & Synchronous transmission \n");	
				off();
				is_broadcast = 0;

#if NETSTACK_CONF_WITH_IPV6
		    PRINTDEBUG("cslmac: send unicast to %02x%02x:%02x%02x:%02x%02x:%02x%02x\n",
		           packetbuf_addr(PACKETBUF_ADDR_RECEIVER)->u8[0],
		           packetbuf_addr(PACKETBUF_ADDR_RECEIVER)->u8[1],
		           packetbuf_addr(PACKETBUF_ADDR_RECEIVER)->u8[2],
		           packetbuf_addr(PACKETBUF_ADDR_RECEIVER)->u8[3],
		           packetbuf_addr(PACKETBUF_ADDR_RECEIVER)->u8[4],
		           packetbuf_addr(PACKETBUF_ADDR_RECEIVER)->u8[5],
		           packetbuf_addr(PACKETBUF_ADDR_RECEIVER)->u8[6],
		           packetbuf_addr(PACKETBUF_ADDR_RECEIVER)->u8[7]);
#else
		    PRINTDEBUG("cslmac: send unicast to %u.%u\n",
		           packetbuf_addr(PACKETBUF_ADDR_RECEIVER)->u8[0],
		           packetbuf_addr(PACKETBUF_ADDR_RECEIVER)->u8[1]);
#endif /* NETSTACK_CONF_WITH_IPV6 */
				/*if it is a sync it is necesary wait til the next neighbor wakeup*/
				
				for(e = list_head(encounter_list); e != NULL; e = list_item_next(e)){
					const linkaddr_t *neighbor = packetbuf_addr(PACKETBUF_ADDR_RECEIVER);
					
					 if(linkaddr_cmp(neighbor, &e->neighbor)){
						 rtimer_clock_t wait, now, expected;
						/* We expect encounters to happen every DEFAULT_PERIOD sample_channel
						   units. The next expected encounter is at time e->sample_channel +
						   DEFAULT_PERIOD. To compute a relative offset, we subtract
						   with clock_time(). Because we are only interested in turning
						   on the radio within the DEFAULT_PERIOD period, we compute the
						   waiting time with modulo DEFAULT_PERIOD. */ 
						now = RTIMER_NOW();
						wait = ((rtimer_clock_t)(e->sample_channel - now)) % (DEFAULT_CLS_PERIOD);
						expected = now + wait - 2 * DEFAULT_ON_TIME;
						while(RTIMER_CLOCK_LT(RTIMER_NOW(), expected));
						we_are_sending = 1;
						LEDS_ON(LEDS_BLUE);
						on();
						collisions = 0;
						/*TX the dataframe*/
						/* restore the packet to send */
						queuebuf_to_packetbuf(packet);
						queuebuf_free(packet);
						NETSTACK_RADIO.send(packetbuf_hdrptr(), packetbuf_totlen());
						LEDS_OFF(LEDS_BLUE);
						/*wait ACK*/
						off();
						PRINTF("cslmac: wait ACK frame \n");
						t = RTIMER_NOW();
						watchdog_stop();
						on();
						got_ack = 0;
						waiting_for_packet = 1;
						while(RTIMER_CLOCK_LT(RTIMER_NOW(), t + cslmac_config.ack_wait_time) && got_ack == 0){//--
							/* See if we got an ACK */
							packetbuf_clear();
							len = NETSTACK_RADIO.read(packetbuf_dataptr(), PACKETBUF_SIZE);	
							if(len > 0){
								packetbuf_set_datalen(len);
								if(hdr_len = frame802154_parse(packetbuf_dataptr(), packetbuf_datalen(), &frame)>=0){
									if(frame.fcf.frame_type==FRAME802154_ACKFRAME && linkaddr_cmp
										(PACKETBUF_ADDR_RECEIVER, &linkaddr_node_addr)){
										/*the correct ACK_frame it is received*/
										/*update the timer and phase*/
										/*tsch_packet_parse_eack(const uint8_t *buf, int buf_size,
										  uint8_t seqno, frame802154_t *frame, struct ieee802154_ies *ies, uint8_t *hdr_len)*/
										  got_ack = 1;
										  if(tsch_packet_parse_eack(packetbuf_dataptr(),packetbuf_datalen(),&r_seqno,&frame,&r_ies,&hdr_len)>0){
											  update_neighbor_node=packetbuf_addr(PACKETBUF_ADDR_SENDER);
											  update_neighbor(update_neighbor_node,r_ies.CSL_period,r_ies.CSL_phase);
											  PRINTF("Correct ACK_frame Received \n");
										  }
										}else{
											/*FRAME received to another node or noise*/
											PRINTF("Unpredictable frame or noise Received \n");
											collisions++;
										}
								}else{
									PRINTF("cslmac: send failed to parse hdr_len: %u\n", hdr_len);
								}
							}else{
								got_ack = 0;
								PRINTF("cslmac: not ACK_frame was received: %u\n", len);
							}
						}
					 }
				}
		}else{
#if DEBUG				
				if(!Is_Broadcast(dest_address)){ 
					PRINTF("cslmac: Asynchronous Transmission\n");
				}else{
					PRINTF("cslmac: Broadcast Transmission\n");
				}
#endif	
			if(Is_Broadcast(dest_address)){
				off();
				is_broadcast = 1;	
				}
				/*the traitement for Broadcast/multicast is the same of asyncTX*/
				/*	1. use CSMA to access to the channel.
					2.during the WAKEUP period :
						2a create a wakeupframe with the short adress of the destination.
						2b TX the wakeup_frame.
					3.TX the payload data frame.
					4.wait for the ACK if it is requiered.
						4a. if not ACK is received start the Retransmission.
					5.update the time and phase_period.	
				*/
			//2.
				_nextSamplePeriod= RTIMER_NOW() + cslmac_config.maccslperiod;
				on();
				do{
						/*set the parameters*/
						uint8_t r_seqno = Generate_ack_seqno(); //generate a seq number.
						more_frame= packetbuf_attr(PACKETBUF_ATTR_PENDING); //gertting the package pending bit from the buffer.
						rztime = _nextSamplePeriod - RTIMER_NOW();
						//creation of the wakeup frame
						wakeup_len= tsch_packet_create_wakeup(wakeup_buf,dest_address,sizeof(wakeup_buf),seqno,more_frame,&rztime);
						if(wakeup_len>0){
							//prepare and transmission of the wakeup-frame
							NETSTACK_RADIO.prepare((const void *)wakeup_buf, wakeup_len);
							//transmission of the wakeup-frame
							t=RTIMER_NOW();
							NETSTACK_RADIO.send(wakeup_buf,sizeof(wakeup_buf));
							wakeup_tx_time=RTIMER_NOW()-t;
						
							if(wakeup_tx_time>(RTIMER_NOW()-_nextSamplePeriod)){
								async_dataframe_tx_wait = 1;
								PRINTF("cslmac: The wakeup_txtime is greater than the next sample period \n");
								powercycle_turn_radio_off();
							}
						}else{
							PRINTF("cslmac: Error creating a wakeup framer \n");
						}
					}while(RTIMER_CLOCK_LT(RTIMER_NOW(),_nextSamplePeriod)&&radio_is_on && !async_dataframe_tx_wait);
					//3. Tx the dataframe
					if(async_dataframe_tx_wait){
							powercycle_turn_radio_on();
							async_dataframe_tx_wait = 0;
					}
					queuebuf_to_packetbuf(packet);
#if NETSTACK_CONF_WITH_IPV6
		    PRINTDEBUG("cslmac: send broadcast to %02x%02x:%02x%02x:%02x%02x:%02x%02x\n",
		           packetbuf_addr(PACKETBUF_ADDR_RECEIVER)->u8[0],
		           packetbuf_addr(PACKETBUF_ADDR_RECEIVER)->u8[1],
		           packetbuf_addr(PACKETBUF_ADDR_RECEIVER)->u8[2],
		           packetbuf_addr(PACKETBUF_ADDR_RECEIVER)->u8[3],
		           packetbuf_addr(PACKETBUF_ADDR_RECEIVER)->u8[4],
		           packetbuf_addr(PACKETBUF_ADDR_RECEIVER)->u8[5],
		           packetbuf_addr(PACKETBUF_ADDR_RECEIVER)->u8[6],
		           packetbuf_addr(PACKETBUF_ADDR_RECEIVER)->u8[7]);
#else
		    PRINTDEBUG("cslmac: send broadcast to %u.%u\n",
		           packetbuf_addr(PACKETBUF_ADDR_RECEIVER)->u8[0],
		           packetbuf_addr(PACKETBUF_ADDR_RECEIVER)->u8[1]);
#endif /* NETSTACK_CONF_WITH_IPV6 */

					NETSTACK_RADIO.send(packetbuf_hdrptr(), packetbuf_totlen());
					ack_requiered = packetbuf_attr(PACKETBUF_ATTR_MAC_ACK);
					if(!is_broadcast && ack_requiered){	
					/*if it is not broadcast then we have to wait for ACK*/
						PRINTF("cslmac: wait for ACK frame \n");
						watchdog_stop();
						on();
						waiting_for_packet = 1;
						got_ack=0;
						while(RTIMER_CLOCK_LT(RTIMER_NOW(), t + cslmac_config.ack_wait_time) && got_ack == 0){
							packetbuf_clear();
							len = NETSTACK_RADIO.read(packetbuf_dataptr(), PACKETBUF_SIZE);
							if(len > 0){
								if(hdr_len = frame802154_parse(packetbuf_dataptr(), packetbuf_datalen(), &frame)>=0){
									if(frame.fcf.frame_type==FRAME802154_ACKFRAME && linkaddr_cmp(PACKETBUF_ADDR_RECEIVER, &linkaddr_node_addr)){
										/*the correct ACK_frame has been received*/
										/*update the timer and phase*/
										/*tsch_packet_parse_eack(const uint8_t *buf, int buf_size,
											uint8_t seqno, frame802154_t *frame, struct ieee802154_ies *ies, uint8_t *hdr_len)*/
										got_ack = 1;
											if(tsch_packet_parse_eack(packetbuf_dataptr(),packetbuf_datalen(),&r_seqno,&frame,&r_ies,&hdr_len)>0){
												/*looking for the neighbor & register it*/
												update_neighbor_node=packetbuf_addr(PACKETBUF_ADDR_SENDER);
												register_neighbor(update_neighbor_node,r_ies.CSL_period,r_ies.CSL_phase);
												PRINTF("cslmac: Correct ACK_frame Received \n");
											}
									}else{
										/*FRAME received to another node or noise*/
											PRINTF("Unpredictable frame or noise Received \n");
											collisions++;
									}
									
								}else{
									PRINTF("cslmac: send failed to parse %u\n", hdr_len);
								}
							}else{
								PRINTF("cslmac: not ack frame has been received %u\n", len);
							}
							
							
						}//to delete
					}//to delete
		}
		//end unicast and/or async transmission.
		watchdog_start();
		LEDS_OFF(LEDS_BLUE);
		if(got_ack && ack_requiered && !collisions){
			powercycle_turn_radio_off();
			PRINTF("cslmac: succesful transmission\n");
			
			return MAC_TX_OK;
		}else if(!ack_requiered && !collisions){
			PRINTF("cslmac: succesful transmission\n");
			powercycle_turn_radio_off();
					return MAC_TX_OK;
		}else{
			PRINTF("cslmac: error during transmission %u , %u\n",ack_requiered,got_ack);
			   powercycle_turn_radio_off();
				return MAC_TX_NOACK;
		}
		
		if(collisions != 0){
				someone_is_sending++;
				powercycle_turn_radio_off();
				return MAC_TX_COLLISION;
		}
		
	}
}

/*---------------------------------------------------------------------------*/


static void
cschedule_powercycle(clock_time_t time)
{

  if(cslmac_is_on) {
    if(time == 0) {
      time = 1; //set up the clock time to 1 if it is zero
    }
 	//Someone_Sending();
	ctimer_set(&cpowercycle_ctimer, time,
               (void (*)(void *))cpowercycle, NULL);//after ctime expired the function cpowercycle is called;
    //printf("cslmac: Accessing to powercycle after that %u ms\n",time);	
  }
    
  
}
/*---------------------------------------------------------------------------*/

static void
cschedule_sync_powercycle(clock_time_t time)
{

  if(cslmac_is_on) {
    if(time == 0) {
      time = 1; //set up the clock time to 1 if it is zero
    }
    if(radio_is_on != 0){
	//powercycle_turn_radio_off();	
    }
	Channel_Busy();
	ctimer_set(&cpowercycle_ctimer, time,
               (void (*)(void *))sync_powercycle, NULL);//after ctime expired the function cpowercycle is called;
    //printf("cslmac: Accessing to powercycle after that %u ms\n",time);
	//printf("sync_power\n");

  }
    
  
}
/*---------------------------------------------------------------------------*/

static void
sync_powercycle(void*ptr)
{
	PT_BEGIN(&pt2);
//	timer_set(&timer_on, CLOCK_SECOND / 2);	
	
	while(1){
		powercycle_turn_radio_on();
		CSCHEDULE_SYNCPOWERCYCLE(DEFAULT_ON_TIME);
		PT_YIELD(&pt2);
		
		if(next_sync_wakeup)
			{
				timer_set(&timer_off, CLOCK_SECOND / 2);	
				powercycle_turn_radio_off();
				while(!timer_expired(&timer_off)){}
				CSCHEDULE_SYNCPOWERCYCLE(DEFAULT_OFF_TIME);
				PT_YIELD(&pt2);
				//PRINTF("radio sync off \n");
				
				
		}else{
			powercycle_turn_radio_off();
			CSCHEDULE_SYNCPOWERCYCLE(DEFAULT_OFF_TIME);
			PT_YIELD(&pt2);
			//PRINTF("radio nosync off \n");
		}		
//		

	}
PT_END(&pt2);
}





static char
cpowercycle(void *ptr)
{

//  if(is_streaming) {//in case of streaming
//    if(!RTIMER_CLOCK_LT(RTIMER_NOW(), stream_until)) {
//      is_streaming = 0;
//      linkaddr_copy(&is_streaming_to, &linkaddr_null);
//      linkaddr_copy(&is_streaming_to_too, &linkaddr_null);
//    }
//  }

  PT_BEGIN(&pt);

  while(1) {
    /* Only wait for some cycles to pass for someone to start sending */
  //  if(someone_is_sending > 0) {
  //    someone_is_sending--;
  //  }
    /* If there were a strobe in the air, turn radio on */
    powercycle_turn_radio_on();
    CSCHEDULE_POWERCYCLE(DEFAULT_ON_TIME);
    PT_YIELD(&pt);

    if(cslmac_config.off_time > 0) {
      powercycle_turn_radio_off();
      if(waiting_for_packet != 0) {
	waiting_for_packet++;
	if(waiting_for_packet > 2) {
	  /* We should not be awake for more than two consecutive
	     power cycles without having heard a packet, so we turn off
	     the radio. */
	  waiting_for_packet = 0;
	  powercycle_turn_radio_off();
	}
      }
      CSCHEDULE_POWERCYCLE(DEFAULT_OFF_TIME);
      PT_YIELD(&pt);
    }
  }

  PT_END(&pt);
}

/*---------------------------------------------------------------------------*/
static void
set_global_address(void)
{
  //uip_ipaddr_t ipaddr;
  int i;
  uint8_t state;

  uip_ip6addr(&ipaddr, 0xfe80, 0, 0, 0, 0, 0, 0, 0);
  uip_ds6_set_addr_iid(&ipaddr, &uip_lladdr);
  uip_ds6_addr_add(&ipaddr, 0, 1);

  printf("IPv6 addresses: ");
  for(i = 0; i < UIP_DS6_ADDR_NB; i++) {
    state = uip_ds6_if.addr_list[i].state;
    if(uip_ds6_if.addr_list[i].isused &&
       (state == ADDR_TENTATIVE || state == ADDR_PREFERRED)) {
      uip_debug_ipaddr_print(&uip_ds6_if.addr_list[i].ipaddr);
      printf("\n");
    }
  }
}
/*---------------------------------------------------------------------------*/
PROCESS(hello_world_process, "Hello world process");
AUTOSTART_PROCESSES(&hello_world_process);

PROCESS_THREAD(hello_world_process, ev, data)
{ 

PROCESS_BEGIN();
	int e;
	 set_global_address();
	//clsmac_init();
	const linkaddr_t * src_address;	//Source Address.
	packetbuf_set_addr(PACKETBUF_ADDR_SENDER,(void*)&ipaddr);
	packetbuf_set_addr(PACKETBUF_ADDR_RECEIVER,(void*)&dest_addr);
	packetbuf_set_attr(PACKETBUF_ATTR_MAC_ACK,0);
	src_address = packetbuf_addr(PACKETBUF_ADDR_SENDER);//geting the source address form the buffer
	//clsmac_init();
	
/*tsch_packet_create_wakeup(uint8_t *buf,linkaddr_t *dest_addr,int buf_size, uint8_t seqno,uint8_t more_frame,uint16_t rztime)*/
//linkaddr_copy((linkaddr_t *)&p.src_addr, &linkaddr_node_addr);

static unsigned int message_number=1;
char buf[20];

sprintf(buf, "Message %d", message_number);
message_number++;
//strlen(buf) + 1
 memcpy(((char *)packetbuf_dataptr()) + 1, buf, strlen(buf) + 1);
packetbuf_set_datalen(strlen(buf) + 1);	
cslmac_is_on=1;
radio_is_on = 1;
waiting_for_packet = 0;
//off();
register_neighbor(&dest_addr,DEFAULT_CLS_PERIOD, 0);
send_packet();


//tsch_packet_create_wakeup(wakeupbuf,&dest_addr,sizeof(wakeupbuf),seqno,more_frame,rztime);
PROCESS_END();
	
}
/*---------------------------------------------------------------------------*/
