/**
 * \file
 *         A simple power saving MAC protocol based on CLS-MAC [SenSys 2016]
 * \author
 *         Adam Dunkels <adam@sics.se>
 *         Niclas Finne <nfi@sics.se>
 *         Joakim Eriksson <joakime@sics.se>
 */
/**/
#include "contiki-conf.h" 
#include "dev/leds.h"
#include "dev/radio.h"
#include "dev/watchdog.h"
#include "net/netstack.h"
#include "lib/random.h"
#include "net/rime/rime.h"
#include "sys/compower.h"
#include "sys/pt.h"
#include "sys/rtimer.h" 
#include "lib/list.h"
#include "lib/memb.h"
#include "contiki-conf.h"
#include "net/mac/tsch/tsch.h"
#include  "net/mac/tsch/tsch-packet.h"
#include  "net/mac/tsch/tsch-slot-operation.h"
#include "net/mac/cslmac/cslmac.h"

#ifdef EXPERIMENT_SETUP
#include "experiment-setup.h"
#endif

#include <string.h>


 /* CHANNEL_CHECK_RATE is enforced to be a power of two.
 * If RTIMER_ARCH_SECOND is not also a power of two, there will be an inexact
 * number of channel checks per second due to the truncation of CYCLE_TIME.
 * This will degrade the effectiveness of phase optimization with neighbors that
 * do not have the same truncation error.
 * Define SYNC_CYCLE_STARTS to ensure an integral number of checks per second.
 */
#if RTIMER_ARCH_SECOND & (RTIMER_ARCH_SECOND - 1)
#define SYNC_CYCLE_STARTS                    1
#endif

//#define DEFAULT_PERIOD_TIME (RTIMER_ARCH_SECOND / 160)
//#define MAC_CLS_PERIOD ((DEFAULT_PERIOD_TIME) + (RTIMER_ARCH_SECOND / NETSTACK_RDC_CHANNEL_CHECK_RATE - (DEFAULT_PERIOD_TIME)))

#ifdef MAC_CLS_PERIOD
#if MAC_CLS_PERIOD==0
#define DEFAULT_ON_TIME (MAC_CLS_PERIOD)
#define DEFAULT_OFF_TIME 0
#else
#define DEFAULT_ON_TIME (MAC_CLS_PERIOD / 320)
#define DEFAULT_OFF_TIME(9 * MAC_CLS_PERIOD / 320)
#endif
#else
#define DEFAULT_ON_TIME (RTIMER_ARCH_SECOND / 320)
#define DEFAULT_OFF_TIME (RTIMER_ARCH_SECOND / NETSTACK_RDC_CHANNEL_CHECK_RATE - DEFAULT_ON_TIME)
#endif

#ifdef CSL_CONF_INTER_PACKET_DEADLINE
#define INTER_PACKET_DEADLINE               CSL_CONF_INTER_PACKET_DEADLINE
#else
#define INTER_PACKET_DEADLINE               CLOCK_SECOND / 32
#endif

#define DEFAULT_CLS_PERIOD (DEFAULT_OFF_TIME + DEFAULT_ON_TIME)
#define WAIT_TIME_BEFORE_STROBE_ACK RTIMER_ARCH_SECOND / 1000
#define DEFAULT_STROBE_WAIT_TIME (7 * DEFAULT_ON_TIME / 8)
#define DEFAULT_IFS_SPACE (5 * DEFAULT_ON_TIME / 10)


#ifdef CSL_CONF_LISTEN_TIME_AFTER_PACKET_DETECTED
#define LISTEN_TIME_AFTER_PACKET_DETECTED  CSL_CONF_LISTEN_TIME_AFTER_PACKET_DETECTED
#else
#define LISTEN_TIME_AFTER_PACKET_DETECTED  RTIMER_ARCH_SECOND / 80
#endif




#include <stdio.h>

#define SQNO_LIST_LENGTH 8


struct neighbor_element {
  struct neighbor_element *next;
  linkaddr_t neighbor;
  rtimer_clock_t sample_channel; //frecuency for sample the channel i.e Time_on + Time_sleep.
  rtimer_clock_t phase; //case o multiplechannels.
  uint8_t is_synchnode;
  uint8_t previous_sqno[SQNO_LIST_LENGTH];
  uint8_t index_sqno;
};

/*struct frame_sqno{
	struct frame_sqno *next_sqno;
	uint8_t sqno;
};*/

#define MAX_NEIGHBORS 8

LIST(encounter_list);
MEMB(encounter_memb, struct neighbor_element, MAX_NEIGHBORS);

//LIST(sqno_list);
//MEMB(sqno_memb,struct frame_sqno, SQNO_LIST_LENGTH);





static struct rtimer rt;
static struct pt pt2;

static volatile uint8_t cslmac_is_on = 0;

static volatile unsigned char waiting_for_packet = 0;
static volatile unsigned char someone_is_sending = 0;
static volatile unsigned char we_are_sending = 0;
static volatile unsigned char radio_is_on = 0;
static volatile unsigned char is_listening=0;
static volatile unsigned char ack_requested=0;
static volatile unsigned char more_framers=0;

//struct frame_sqno * tail;
struct neighbor_element * e;
static uint8_t on_phase=0;

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
#define PRINTADDRS(dest) printf("tsch-package: address %02x%02x:%02x%02x:%02x%02x:%02x%02x \n", ((uint8_t *)dest)[0], ((uint8_t *)dest)[1], ((uint8_t *)dest)[2], ((uint8_t *)dest)[3], ((uint8_t *)dest)[4], ((uint8_t *)dest)[5], ((uint8_t *)dest)[6], ((uint8_t *)dest)[7])
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
/*int len;
uint8_t wakeupbuf [TSCH_PACKET_MAX_LEN];
linkaddr_t dest_addr= {0xff,0xff, 0, 0, 0, 0, 0, 0};
uint8_t seqno=0x1d;
uint8_t more_frame=1;
uint16_t rztime =0xaaaa;*/
/*------------------------*/



static uint8_t is_streaming;
static uint8_t next_sync_wakeup;
static int we_are_receiving_burst = 0;
rtimer_clock_t next_time_cycle;
//static linkaddr_t is_streaming_to, is_streaming_to_too;
//static rtimer_clock_t stream_until;

static uint8_t wakeack_seqno;


struct cslmac_config cslmac_config = {
  DEFAULT_ON_TIME,
  DEFAULT_OFF_TIME,
  4 * DEFAULT_ON_TIME + DEFAULT_OFF_TIME,
   5 * DEFAULT_ON_TIME,  //DEFAULT_STROBE_WAIT_TIME,
 1 * DEFAULT_ON_TIME + DEFAULT_OFF_TIME,
 DEFAULT_IFS_SPACE
};


/*---------------------------------------------------------------------------*/
/* 				on: turn on the radio 		             */
/*---------------------------------------------------------------------------*/
static void
on(void)
{
  if(cslmac_is_on && radio_is_on == 0 && we_are_sending ==0) {
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
  if(cslmac_is_on && radio_is_on != 0) {
    radio_is_on = 0;
	//serial_line_clear();
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
uint8_t len;
/*simple test for recognize if some one is sending or it is interference*/
	if(NETSTACK_RADIO.pending_packet()) {

		if ((len= NETSTACK_RADIO.read(packetbuf_dataptr(), PACKETBUF_SIZE))>0){
			PRINTF("cslmac: someone sending %u\n",len);
			return 1;

		}
	}else if (!(NETSTACK_RADIO.receiving_packet() || NETSTACK_RADIO.channel_clear())){
		PRINTF("cslmac: interference detected\n");
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
	if(((address->u8[0]==255)&&(address->u8[1]==255) )|| (packetbuf_holds_broadcast()))return 1;
	return 0;
}

/*----------------------------------------------------------------------------*/
/* 			Generate_ack_seqno:used for generate a sequncial number fo wakeup */
/*			or unicast										 	 			  */
/*----------------------------------------------------------------------------*/
static char
Generate_ack_seqno(void){
	 wakeack_seqno=random_rand() % 256 + 1;
	 return wakeack_seqno;
}

/*---------------------------------------------------------------------------*/
/* 			Get_ack_seqno:used for generate a sequncial number fo wakeup     */
/*			or unicast										 	 			 */
/*---------------------------------------------------------------------------*/
/*static char
Get_ack_seqno(void){
	return wakeack_seqno;
}*/
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
static struct ctimer cpowercycle_ctimer;
clock_time_t next_timer_on;
static volatile rtimer_clock_t cycle_start,now;

static char sync_powercycle(void*ptr);
static void cschedule_sync_powercycle(clock_time_t time);
#define CSCHEDULE_SYNCPOWERCYCLE(rtime) cschedule_sync_powercycle((1ul * CLOCK_SECOND * (rtime)) / RTIMER_ARCH_SECOND)


/*updatedcycle*/
//static void cslmac_schedule_powercycle(struct rtime * t, void *ptr);
static char cslmac_powercycle(struct rtimer *t, void *ptr);
static void cslmac_sync_powercycle(struct rtimer *t, rtimer_clock_t fixed_time);
/*---------------------------------------------------------------------------*/
/*									     									 */
/* 		csl_schedule_powercycle: setup the powercycle_cycle      			 */
/*									     									 */
/*---------------------------------------------------------------------------*/
static void
cslmac_schedule_powercycle(struct rtimer * t, void *ptr){
	cslmac_powercycle(t, ptr);
};


/*---------------------------------------------------------------------------*/
/*									     									 */
/* 		cslmac_powercycle: define the powercycle_cycle 		     			 */
/*									     									 */
/*---------------------------------------------------------------------------*/

rtimer_clock_t start_cycle;

static char cslmac_powercycle(struct rtimer * t, void *ptr){
static rtimer_clock_t start;	
#if SYNC_CYCLE_STARTS
  static volatile rtimer_clock_t sync_cycle_start;
  static volatile uint8_t sync_cycle_phase;
#endif
   PT_BEGIN(&pt2);

#if SYNC_CYCLE_STARTS
  sync_cycle_start = RTIMER_NOW();
#else
  cycle_start = RTIMER_NOW();
#endif   

    while(1) {
    	static uint8_t packet_seen;

#if SYNC_CYCLE_STARTS
    	if(sync_cycle_phase++ == NETSTACK_RDC_CHANNEL_CHECK_RATE) {
     	sync_cycle_phase = 0;
      	sync_cycle_start += RTIMER_ARCH_SECOND;
      	cycle_start = sync_cycle_start;
    	} else {
#if (RTIMER_ARCH_SECOND * NETSTACK_RDC_CHANNEL_CHECK_RATE) > 65535
      	cycle_start = sync_cycle_start + ((unsigned long)(sync_cycle_phase*RTIMER_ARCH_SECOND))/NETSTACK_RDC_CHANNEL_CHECK_RATE;
#else
      	cycle_start = sync_cycle_start + (sync_cycle_phase*RTIMER_ARCH_SECOND)/NETSTACK_RDC_CHANNEL_CHECK_RATE;
#endif
		}
#else
    cycle_start += DEFAULT_CLS_PERIOD;//CYCLE_TIME;
#endif	
    now=RTIMER_NOW();
    next_timer_on=0;
    start_cycle=RTIMER_NOW();
    powercycle_turn_radio_on(); //start the listening phase
    on_phase=1;
    //PRINTF("startcycle at:  (%u)\n", start_cycle);
    	while(RTIMER_CLOCK_LT(RTIMER_NOW(),now + DEFAULT_ON_TIME)){
    		if(NETSTACK_RADIO.channel_clear() == 0) {
          packet_seen = 1;
          break;
        	}

    	}

      if(packet_seen) {
      	start = RTIMER_NOW();
      	 while(we_are_sending == 0 && radio_is_on &&
            RTIMER_CLOCK_LT(RTIMER_NOW(),
                            (start + LISTEN_TIME_AFTER_PACKET_DETECTED))) {

      	 	if(NETSTACK_RADIO.pending_packet()) {
          		break;
        	}

      	 }

      }
      if(radio_is_on) {
        if(!(NETSTACK_RADIO.receiving_packet() ||
             NETSTACK_RADIO.pending_packet()) ||
             !RTIMER_CLOCK_LT(RTIMER_NOW(),
                 (start + LISTEN_TIME_AFTER_PACKET_DETECTED))) {	
          powercycle_turn_radio_off();
      	  on_phase=0;
        }
        packet_seen=0;
      }

      if(RTIMER_CLOCK_LT(RTIMER_NOW() - cycle_start, DEFAULT_CLS_PERIOD - DEFAULT_ON_TIME * 4)) {
      	/* Schedule the next powercycle interrupt*/
     	if(!on_phase) next_timer_on=RTIMER_NOW()+DEFAULT_OFF_TIME + cycle_start;
      	cslmac_sync_powercycle(t, DEFAULT_OFF_TIME + cycle_start);
      	PT_YIELD(&pt2);
      }
    }
 PT_END(&pt2);
}

/*---------------------------------------------------------------------------*/
/*									     									 */
/* 		cslmac_sync_powercycle: setup a synchronous powercycle 		     		 */
/*									     									 */
/*---------------------------------------------------------------------------*/
static void cslmac_sync_powercycle(struct rtimer *t, rtimer_clock_t fixed_time){
	int r;
	rtimer_clock_t now;

  if(cslmac_is_on) {
  	now = RTIMER_NOW();
  	if(RTIMER_CLOCK_LT(fixed_time, now + RTIMER_GUARD_TIME)) {
      fixed_time = now + RTIMER_GUARD_TIME;
    }
     r = rtimer_set(t, fixed_time, 1, cslmac_schedule_powercycle, NULL);
    if(r != RTIMER_OK) {
      PRINTF("schedule_powercycle: could not set rtimer\n");
    }
    next_time_cycle=fixed_time;
  }
}

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
  next_sync_wakeup=0;
  PT_INIT(&pt2);

  cslmac_is_on = 1;

  //start the neighbor list
  list_init(encounter_list);
  memb_init(&encounter_memb);
  //start the sqno list
   //list_init(sqno_list);
  //memb_init(&sqno_memb);
  //Scheduler the wakeup and listen Phase phase
  off();
  //CSCHEDULE_SYNCPOWERCYCLE(RTIMER_NOW()+DEFAULT_OFF_TIME);
  rtimer_set(&rt, RTIMER_NOW() + DEFAULT_ON_TIME, 1, cslmac_schedule_powercycle, NULL);
  //cslmac_schedule_powercycle(RTIMER_NOW()+DEFAULT_OFF_TIME);
}

/*---------------------------------------------------------------------------*/
/*	       								     								 */
/* 	register_neighbor: introduce a new_neigbor_time & phase entry        	 */
/*									     									 */
/*---------------------------------------------------------------------------*/
static void
register_neighbor(const linkaddr_t *neighbor, rtimer_clock_t time, rtimer_clock_t phase)
{
  //struct neighbor_element *e;

  /* If we have an entry for this neighbor already, we renew it. */
  for(e = list_head(encounter_list); e != NULL; e = list_item_next(e)) {
    if(linkaddr_cmp(neighbor, &e->neighbor)) {
      e->sample_channel = time;
	  e->phase = phase;
	  e->is_synchnode=1;
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
	e->is_synchnode=1;
    list_add(encounter_list, e);
  }
}

static void register_secuence(const uint8_t sqno, const linkaddr_t *neighbor)
{
	 for(e = list_head(encounter_list); e != NULL; e = list_item_next(e)) {
		if(linkaddr_cmp(neighbor, &e->neighbor)) {
			e->previous_sqno[e->index_sqno]=sqno;
			if(e->index_sqno <= SQNO_LIST_LENGTH){
				e->index_sqno++;
			}else{
				e->index_sqno = 0;
			}
			break;
		}
	}
	if(e == NULL) {
		 e = memb_alloc(&encounter_memb);
		  if(e == NULL) {
			  /* We could not allocate memory for this encounter, so we just drop it. */
			  return;
		  }
		 linkaddr_copy(&e->neighbor, neighbor);
		e->is_synchnode=0;
		list_add(encounter_list, e);	
	}
}

static int duplicate_secuence(const uint8_t sqno,const linkaddr_t *neighbor){
	
	uint8_t i;
	 for(e = list_head(encounter_list); e != NULL; e = list_item_next(e)) {
			if(linkaddr_cmp(neighbor, &e->neighbor)) {
				for(i = 0; i<= SQNO_LIST_LENGTH; i++){
					if(sqno==e->previous_sqno[i]){
						return 1;
					}
				}
				return 0;
			}
		}
	return 0;	
}

/*---------------------------------------------------------------------------*/
/* is_neighbor_in_list: returns if a neighbor is included in the list 	     */
/*---------------------------------------------------------------------------*/
static int
is_neighbor_in_list(const linkaddr_t *neighbor)
{
	
	//struct neighbor_element *e;
	for(e = list_head(encounter_list); e != NULL; e = list_item_next(e)){
		if(linkaddr_cmp(neighbor, &e->neighbor) && e->is_synchnode){
			//memb_free(&encounter_memb,e);
			return 1;
		}
	}
	//memb_free(&encounter_memb,e);
	return 0;
}


static rtimer_clock_t retrieve_neighbor_period(const linkaddr_t *neighbor){
	//struct neighbor_element * e;
	rtimer_clock_t csl_period;
	for(e = list_head(encounter_list); e != NULL; e = list_item_next(e)){
		 if(linkaddr_cmp(neighbor, &e->neighbor) && e->is_synchnode) {
			 csl_period=e->sample_channel;
			 //memb_free(&encounter_memb,e);
				return csl_period;
		}
	}
	//memb_free(&encounter_memb,e);
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
		//struct neighbor_element * e;
			for(e = list_head(encounter_list); e != NULL; e = list_item_next(e)){
				if(linkaddr_cmp(neighbor, &e->neighbor)&&e->is_synchnode){
					e->sample_channel = time;
					e->phase = phase;
					//memb_free(&encounter_memb,e);
					return 1;
				}
			}
		//memb_free(&encounter_memb,e);	
	}
	return 0;

}


static int
remove_neighbor (const linkaddr_t *neighbor){
	//struct neighbor_element * e;
	for(e = list_head(encounter_list); e != NULL; e = list_item_next(e)){
		if(linkaddr_cmp(neighbor, &e->neighbor)){
			list_remove(encounter_list, e);
			memb_free(&encounter_memb,e);
			return 1;
		}
	}
	//memb_free(&encounter_memb,e);
	return 0;	
}



//mac_dsn = random_rand() % 256;
static uint8_t Got_frame_type(uint8_t * rx_hdr_pointer){
	if(rx_hdr_pointer==NULL) return -1;
	uint8_t frame_type = (rx_hdr_pointer[0] & 7);
	PRINTF("frame_type receive: (%u)\n", frame_type);
	return frame_type;
}

struct queuebuf * packet;
/*-------------------------------------------------------------------------------*/
/*	                                                                             */
/*                  send_packet(void): RDC send package protocol                 */
/*	                                                                             */
/*-------------------------------------------------------------------------------*/
static int
send_packet(void)
{
int len=0;
int hdr_len=0;
const linkaddr_t  * dest_address; //Destination Address.
const linkaddr_t *update_neighbor_node;
struct ieee802154_ies r_ies;
frame802154_t ack_frame;
static uint8_t ack_buf[PACKETBUF_SIZE];
static uint8_t wakeup_buf[PACKETBUF_SIZE];
static int wakeup_len;
rtimer_clock_t t;
rtimer_clock_t _nextSamplePeriod;
rtimer_clock_t rztime;
rtimer_clock_t wakeup_tx_time;
rtimer_clock_t wakeup_tx_wait_time;
uint8_t more_frame;
uint8_t async_dataframe_tx_wait=0;
uint8_t collisions=0;
uint8_t r_seqno=0;
uint8_t got_ack=0;
uint8_t requiere_wakeup=0;
uint8_t requiere_ack=0;
uint8_t is_synchronous=0;
uint8_t * p_ack_buf=ack_buf;
uint8_t * tx_frame_type;

//powercycle_turn_radio_on();
//t = RTIMER_NOW();
//if((Channel_Busy())==1){
//		PRINTF("cslmac: channel is busy someone is transmiting \n");
//		powercycle_turn_radio_off();
//		return MAC_TX_COLLISION;

	//}
	
#if !NETSTACK_CONF_BRIDGE_MODE
  /* If NETSTACK_CONF_BRIDGE_MODE is set, assume PACKETBUF_ADDR_SENDER is already set. */
  packetbuf_set_addr(PACKETBUF_ADDR_SENDER, &linkaddr_node_addr);
#endif	
	
//dest_address = packetbuf_addr(PACKETBUF_ADDR_RECEIVER);//geting the dest address form the buffer

/*-----------------------determine if it is a broadcast or unicast TX sync or async-----------------------*/
	if(packetbuf_holds_broadcast()){
		//is_broadcast = 1;
		is_synchronous=0;
		requiere_wakeup=1;
		requiere_ack=0;
		PRINTF("cslmac: send broadcast\n");
	}else if(is_neighbor_in_list(packetbuf_addr(PACKETBUF_ADDR_RECEIVER))){
		//is_broadcast = 0;
		is_synchronous=1;
		requiere_wakeup=1;
		requiere_ack=1;
#if NETSTACK_CONF_WITH_IPV6
		    PRINTDEBUG("cslmac: send synchronous unicast to %02x%02x:%02x%02x:%02x%02x:%02x%02x\n",
		           packetbuf_addr(PACKETBUF_ADDR_RECEIVER)->u8[0],
		           packetbuf_addr(PACKETBUF_ADDR_RECEIVER)->u8[1],
		           packetbuf_addr(PACKETBUF_ADDR_RECEIVER)->u8[2],
		           packetbuf_addr(PACKETBUF_ADDR_RECEIVER)->u8[3],
		           packetbuf_addr(PACKETBUF_ADDR_RECEIVER)->u8[4],
		           packetbuf_addr(PACKETBUF_ADDR_RECEIVER)->u8[5],
		           packetbuf_addr(PACKETBUF_ADDR_RECEIVER)->u8[6],
		           packetbuf_addr(PACKETBUF_ADDR_RECEIVER)->u8[7]);
#else
		    PRINTDEBUG("cslmac: send synchronous unicast to %u.%u\n",
		           packetbuf_addr(PACKETBUF_ADDR_RECEIVER)->u8[0],
		           packetbuf_addr(PACKETBUF_ADDR_RECEIVER)->u8[1]);
#endif /* NETSTACK_CONF_WITH_IPV6 */		
		
	}else{
		//is_broadcast = 0;
		is_synchronous=0;
		requiere_wakeup=1;
		requiere_ack=1;
		PRINTF("cslmac: send asynchronous unicast\n");
#if NETSTACK_CONF_WITH_IPV6
		    PRINTDEBUG("cslmac: send asynchronous unicast to %02x%02x:%02x%02x:%02x%02x:%02x%02x\n",
		           packetbuf_addr(PACKETBUF_ADDR_RECEIVER)->u8[0],
		           packetbuf_addr(PACKETBUF_ADDR_RECEIVER)->u8[1],
		           packetbuf_addr(PACKETBUF_ADDR_RECEIVER)->u8[2],
		           packetbuf_addr(PACKETBUF_ADDR_RECEIVER)->u8[3],
		           packetbuf_addr(PACKETBUF_ADDR_RECEIVER)->u8[4],
		           packetbuf_addr(PACKETBUF_ADDR_RECEIVER)->u8[5],
		           packetbuf_addr(PACKETBUF_ADDR_RECEIVER)->u8[6],
		           packetbuf_addr(PACKETBUF_ADDR_RECEIVER)->u8[7]);
#else
		    PRINTDEBUG("cslmac: send asynchronous unicast to %u.%u\n",
		           packetbuf_addr(PACKETBUF_ADDR_RECEIVER)->u8[0],
		           packetbuf_addr(PACKETBUF_ADDR_RECEIVER)->u8[1]);
#endif /* NETSTACK_CONF_WITH_IPV6 */		
}
/*-----------------------------------create the dataframe----------------------------------------------*/
len = NETSTACK_FRAMER.create();
	if(len<0){
		PRINTF("cslmac: error Creating the dataframe (%u)\n",len);
		return MAC_TX_ERR_FATAL;
	}
	
 more_frame= packetbuf_attr(PACKETBUF_ATTR_PENDING); //gertting the package pending bit from the buffer.	
 
 packetbuf_compact();
if(packet==NULL){ 
 packet = queuebuf_new_from_packetbuf();//copy of the packagebuf.
	if(packet == NULL) {
		/* No buffer available */
		PRINTF("cslmac: send failed, no queue buffer available (of %u)\n",QUEUEBUF_CONF_NUM);
		return MAC_TX_ERR;
	}
}else{
	queuebuf_update_from_packetbuf(packet);
}	
/*-----------------------------------Transmission_phase--------------------------------------------------*/	
			if(requiere_wakeup){
				/*	1. use CSMA to access to the channel.
					2.during the WAKEUP period :
						2a create a wakeupframe with the short adress of the destination.
						2b TX the wakeup_frame.
					3.TX the payload data frame.
					4.wait for the ACK if it is requiered.
						4a. if not ACK is received start the Retransmission.
					5.update the time and phase_period.
				*/
				/*set the parameters of wakeup*/
				if(is_synchronous){
          rtimer_clock_t wait;
						dest_address=queuebuf_addr(packet, PACKETBUF_ADDR_RECEIVER);
						rtimer_clock_t neighbor_time = retrieve_neighbor_period(dest_address);
						powercycle_turn_radio_off();
            t=RTIMER_NOW();
            we_are_sending=1;
            wait=next_time_cycle;
          while(RTIMER_CLOCK_LT(RTIMER_NOW(),wait + neighbor_time - 5 * DEFAULT_ON_TIME));
						_nextSamplePeriod= RTIMER_NOW()+ 5 * DEFAULT_ON_TIME /*+ 5 * DEFAULT_ON_TIME / 2*/;
						//PRINTF("wakeup for neighbor_time (%i)\n",_nextSamplePeriod - RTIMER_NOW());
          //PRINTF("cslmac: synchronous parameters neighbor_time(%u), _nextSamplePeriod(%u), _ctimewait(%u), _now(%u),_start_cycle(%u),_next_time_cycle(%u)\n",neighbor_time,_nextSamplePeriod,ctimewait,t,start_cycle,next_time_cycle);
						we_are_sending=0;					
				}else{

							_nextSamplePeriod= RTIMER_NOW() +  1 * cslmac_config.maccslperiod- 8 * cslmac_config.on_time;
				}

				r_seqno = Generate_ack_seqno(); //generate a seq number.
				powercycle_turn_radio_on();

        if(NETSTACK_RADIO.channel_clear() == 0) {
        someone_is_sending++;
        powercycle_turn_radio_off();
       return MAC_TX_COLLISION;
      } 

        we_are_sending=1;
				PRINTF("sending_wakeup frame with seqno: (%u)\n",r_seqno);
				do{
						rztime = _nextSamplePeriod - RTIMER_NOW();
						//creation of the wakeup frame
						wakeup_len= tsch_packet_create_wakeup(wakeup_buf,dest_address,sizeof(wakeup_buf),r_seqno,more_frame,rztime);
						if(wakeup_len>0){
							//prepare and transmission of the wakeup-frame
							//transmission of the wakeup-frame
							t=RTIMER_NOW();
							NETSTACK_RADIO.send(wakeup_buf,wakeup_len);
							wakeup_tx_time=RTIMER_NOW()-t;
							if(wakeup_tx_time>(RTIMER_NOW()-_nextSamplePeriod)){
								async_dataframe_tx_wait = 1;
								we_are_sending = 0;
								PRINTF("cslmac: The wakeup_txtime is greater than the next sample period \n");
								powercycle_turn_radio_off();
							}
							wakeup_tx_wait_time = RTIMER_NOW() +  cslmac_config.ifs;
							while(RTIMER_CLOCK_LT(RTIMER_NOW(),wakeup_tx_wait_time));	
						}else{
							PRINTF("cslmac: Error creating a wakeup framer \n");
							return MAC_TX_ERR;
						}
					}while(RTIMER_CLOCK_LT(RTIMER_NOW(),_nextSamplePeriod)&&radio_is_on && !async_dataframe_tx_wait);
			}/*end Wakeup_tx*/
			
			if(1){				
				async_dataframe_tx_wait = 0;
				queuebuf_to_packetbuf(packet);
				//queuebuf_free(packet);
				tx_frame_type=packetbuf_hdrptr();
				PRINTF("cslmac: sending frame_type %u\n",tx_frame_type[0]&7);
				powercycle_turn_radio_on();
				we_are_sending=1;
				NETSTACK_RADIO.send(packetbuf_hdrptr(), packetbuf_totlen());
				r_seqno=packetbuf_attr(PACKETBUF_ATTR_MAC_SEQNO);
				we_are_sending=0;
			}/*end of the Transmission*/
			powercycle_turn_radio_off();
			if(requiere_ack){
				//got_ack=1;
				t=RTIMER_NOW();
				/*if the node are waiting for ACK frame the node can sleep for a small period of time since the recever must to */
				while(RTIMER_CLOCK_LT(RTIMER_NOW(),t + 13 * cslmac_config.ack_wait_time /20));
				PRINTF("cslmac: wakeup for receive the ack framer\n");
				t=RTIMER_NOW();
				powercycle_turn_radio_on();
				waiting_for_packet=1;
				watchdog_stop();
				while(RTIMER_CLOCK_LT(RTIMER_NOW(),t + 4 * cslmac_config.ack_wait_time / 2) & !got_ack){
					
					/*during this period any frame could be arrived we are interesting just in the first ACK_frame*/
					//packetbuf_clear();
					if(!NETSTACK_RADIO.receiving_packet()&&NETSTACK_RADIO.pending_packet()){
						
						len=NETSTACK_RADIO.read(ack_buf,sizeof(ack_buf));
						
						if(len!=0){
							if(Got_frame_type(p_ack_buf)==0x2){
								if(tsch_packet_parse_eack(ack_buf,sizeof(ack_buf),r_seqno,&ack_frame,&r_ies,(uint8_t *)hdr_len)>0){
									got_ack = 1;
									waiting_for_packet=0;
									powercycle_turn_radio_off();
									update_neighbor_node = ack_frame.src_addr;
									rtimer_clock_t next_wakeup = abs(RTIMER_NOW()-start_cycle) + r_ies.CSL_period;
									r_ies.CSL_period=next_wakeup;
									if(is_neighbor_in_list(update_neighbor_node)){
										update_neighbor (update_neighbor_node, r_ies.CSL_period, r_ies.CSL_phase);
									}else{
										register_neighbor(update_neighbor_node,r_ies.CSL_period,r_ies.CSL_phase);
										}
											//remove_neighbor (update_neighbor_node);
											PRINTF("cslmac: Correct ACK_frame Received \n");
								}else{
											PRINTF("cslmac: fail passing the eack frame \n");			
									}
								
							}
						}
					}
				}
				
				if(!got_ack){
					if(is_synchronous){
              PRINTF("NOT ack has been received \n");     
						if(!remove_neighbor(dest_address)){
              PRINTF("cslmac: fail emoving the neighbor \n");     
            }
					}
				waiting_for_packet=0;
				powercycle_turn_radio_off();		
				}
				
				watchdog_start();
			}/*end of the reception of the ACK*/
	if(got_ack && requiere_ack){
			PRINTF("cslmac: succesful transmission\n");
			return MAC_TX_OK;
	}else if(!got_ack && requiere_ack){
			someone_is_sending++;
			return MAC_TX_COLLISION;
	}
PRINTF("cslmac: succesful transmission\n");	
return MAC_TX_OK;	
			
}/*fuction_end*/

/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/

static void
cschedule_sync_powercycle(clock_time_t time)
{
	
  if(cslmac_is_on) {
    if(time == 0) {
      time = 1; //set up the clock time to 1 if it is zero
    }
	ctimer_set(&cpowercycle_ctimer, time,
               (void (*)(void *))sync_powercycle, NULL);//after ctime expired the function cpowercycle is called;
    //printf("cslmac: Accessing to powercycle after that %u ms\n",time);
	//printf("sync_power\n");

  }


}
/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/


static char
sync_powercycle(void*ptr)
{
	PT_BEGIN(&pt2);
//	timer_set(&timer_on, CLOCK_SECOND / 2);


	while(1){
	if(someone_is_sending > 0){
		someone_is_sending --;
	}
		
		on_phase=1;
		next_timer_on=0;
		powercycle_turn_radio_on();
		CSCHEDULE_SYNCPOWERCYCLE(DEFAULT_ON_TIME);
		PT_YIELD_UNTIL(&pt2,ctimer_expired(&cpowercycle_ctimer));
		//PT_YIELD(&pt2,);

		if(next_sync_wakeup)
			{
				
				powercycle_turn_radio_off();
				CSCHEDULE_SYNCPOWERCYCLE(DEFAULT_OFF_TIME);
				PT_YIELD(&pt2);
				


		}else{
			on_phase=0;
			powercycle_turn_radio_off();
			next_timer_on=RTIMER_NOW()+DEFAULT_OFF_TIME;
			CSCHEDULE_SYNCPOWERCYCLE(DEFAULT_OFF_TIME);
			PT_YIELD_UNTIL(&pt2,ctimer_expired(&cpowercycle_ctimer));
		}
//

	}
PT_END(&pt2);
}
/*---------------------------------------------------------------------------*/
void Clear_Buf(void){
	uint8_t task_buf[PACKETBUF_SIZE];
	while(NETSTACK_RADIO.pending_packet()|| NETSTACK_RADIO.receiving_packet()){
		PRINTF("cslmac: clear the rx_buffer");
		NETSTACK_RADIO.read(task_buf,sizeof(task_buf));
		packetbuf_clear();
	}
}
static struct ctimer ct2;
static struct ctimer bt;
static void off_for_packet(void *ptr);
static void on_for_packet(void *ptr);

//uint8_t wait_framer=0;

/*---------------------------------------------------------------------------*/
/* Timer callback triggered when receiving a burst, after having
   waited for a next packet for a too long time. Turns the radio off
   and leaves burst reception mode */
static void
recv_burst_off(void *ptr)
{
  off();
  we_are_receiving_burst = 0;
}


static void on_for_packet(void *ptr){
	on();
	waiting_for_packet=1;
  clock_time_t next_time = (CLOCK_SECOND * ((10*DEFAULT_ON_TIME) - RTIMER_GUARD_TIME)) / RTIMER_ARCH_SECOND;
	ctimer_set(&ct2, next_time,off_for_packet, NULL);
	PRINTF("cslmac: Radio on for recieve a dataframe \n");
	
}


static void off_for_packet(void *ptr){
	if(waiting_for_packet){
		PRINTF("cslmac: Radio off since dataframe hasn't been recieved \n");
		waiting_for_packet=0;
		off();
		return;
	}
	off();
	PRINTF("cslmac: Radio off dataframe was correct received\n");
	ctimer_stop(&ct2);
  //return 1;
};

/*---------------------------------------------------------------------------*/
/* 				input_packet: Processing the input packages 	 			 */
/*-------------------------------------------------------	--------------------*/

static void
input_packet(void)
{
int rx_frame_type;
uint8_t rx_wakeup_len;
uint8_t rx_seqno;
frame802154_t rx_frame;
struct ieee802154_ies rx_ies;
uint8_t tx_result;
uint8_t rx_hdr_length=1;	
linkaddr_t ack_dest_addr;
uint8_t ack_mode;
uint8_t ack_seqno;
uint16_t node_period;	
static uint8_t eack_buf[PACKETBUF_SIZE];
static int eack_len;


//struct ieee802154_ies eack_ies;	

	uint8_t * hdr_packet = packetbuf_dataptr();
	rx_frame_type=Got_frame_type(hdr_packet);
	
	if(!we_are_receiving_burst) {
	powercycle_turn_radio_off();
	}
		
	if(rx_frame_type==0x05){
		/*check if it is a frame for this node*/
		if(linkaddr_cmp(packetbuf_addr(PACKETBUF_ADDR_RECEIVER),
												&linkaddr_node_addr) ||
							linkaddr_cmp(packetbuf_addr(PACKETBUF_ADDR_RECEIVER),
								&linkaddr_null)){
									
			/*parse the wakeup-frame*/
						if((rx_wakeup_len = tsch_packet_parse_wake_up(hdr_packet,packetbuf_datalen(),&rx_seqno,&rx_frame,&rx_ies,&rx_hdr_length))<=0){
								PRINTF("cslmac: probable error parsing the wakeup_frame or isn't a wakeup frame \n");
								return;
						}
			/*check if duplicates */			
			if(duplicate_secuence(rx_seqno,packetbuf_addr(PACKETBUF_ADDR_RECEIVER))){
					PRINTF("Droping a duplicate wakeup_frame with sqno(%u) \n",rx_seqno);
					packetbuf_clear();
					powercycle_turn_radio_off();
					return;
					
				}else{
					register_secuence(rx_seqno,packetbuf_addr(PACKETBUF_ADDR_RECEIVER));
				}
/*-------------------------------------------SLEEP PERIOD-------------------------------------------------------------------------*/
        //clock_time_t timer_on_pack= (CLOCK_SECOND * 2 * rx_ies.ie_rz_time)/RTIMER_ARCH_SECOND;
				//ctimer_set(&ct2,timer_on_pack, on_for_packet, NULL);
        ctimer_set(&ct2,rx_ies.ie_rz_time/240 , on_for_packet, NULL);
				//PRINTF("cslmac: Wakeup frame received correctlly (%i), (%i)\n",(rx_ies.ie_rz_time, now + rx_ies.ie_rz_time - 3 * DEFAULT_ON_TIME / 2));
		}else{

        if((rx_wakeup_len = tsch_packet_parse_wake_up(hdr_packet,packetbuf_datalen(),&rx_seqno,&rx_frame,&rx_ies,&rx_hdr_length))<=0){
                PRINTF("cslmac: probable error parsing the wakeup_frame or isn't a wakeup frame \n");
                return;
            }

        if(duplicate_secuence(rx_seqno,packetbuf_addr(PACKETBUF_ADDR_RECEIVER))){
          PRINTF("Droping a duplicate wakeup_frame with sqno(%u) \n",rx_seqno);
          packetbuf_clear();
          powercycle_turn_radio_off();
          return;

         }else{
          register_secuence(rx_seqno,packetbuf_addr(PACKETBUF_ADDR_RECEIVER));
        } 

       waiting_for_packet=0; 
       ctimer_set(&ct2,(rx_ies.ie_rz_time/240 + (10*DEFAULT_ON_TIME) - RTIMER_GUARD_TIME) , off_for_packet, NULL); 

    }				
				 
	}else if (rx_frame_type==0x01){

		int duplicate = 0;
		
		if(linkaddr_cmp(packetbuf_addr(PACKETBUF_ADDR_RECEIVER),
												&linkaddr_node_addr) ||
							linkaddr_cmp(packetbuf_addr(PACKETBUF_ADDR_RECEIVER),
								&linkaddr_null)){
										
										waiting_for_packet=0;
                      powercycle_turn_radio_off();
												/*check if it is a frame for this node*/
											we_are_receiving_burst = packetbuf_attr(PACKETBUF_ATTR_PENDING);
											if(we_are_receiving_burst) {
												PRINTF("recieving burst traffic\n");
												on();
												/* Set a timer to turn the radio off in case we do not receive
													a next packet */
												ctimer_set(&bt, INTER_PACKET_DEADLINE, recv_burst_off, NULL);
											} else {
												off();
												ctimer_stop(&bt);
											}


										if(NETSTACK_FRAMER.parse() >= 0){
                      /*check for duplicates*/
                            /* Check for duplicate packet. */
                      duplicate = mac_sequence_is_duplicate();
                          if(duplicate) {
                              /* Drop the packet. */
                            PRINTF("cslmac: Drop duplicate\n");
                            powercycle_turn_radio_off();
                            return;      

                          } else {
                            mac_sequence_register_seqno();
                          }
                        if(!duplicate) {
												NETSTACK_MAC.input();
												PRINTF("cslmac: dataframe has been receive data(%u) \n",packetbuf_datalen());
                        }
										}else{
											PRINTF("cslmac: error parsing the dataframe(%u) \n",packetbuf_datalen());
											return;
										}


																				
										if(linkaddr_cmp(packetbuf_addr(PACKETBUF_ADDR_RECEIVER),
											&linkaddr_node_addr)){
											PRINTF("cslmac: reception Unicast\n");
										/*1.build the eack*/
											linkaddr_copy (&ack_dest_addr,packetbuf_addr(PACKETBUF_ADDR_SENDER));//linkaddr to send the ACK.
											ack_seqno = packetbuf_attr(PACKETBUF_ATTR_MAC_SEQNO);//setup the ack_seqno as the same of the dataframe received.
											//node_period=cslmac_config.maccslperiod;//the csl_period 
											rtimer_clock_t next_wakeup= abs(next_time_cycle - RTIMER_NOW()); //remain_time to wakeup.  //next_time_cycle;//RTIMER_NOW() + etimer_expiration_time(&cpowercycle_ctimer.etimer);
										PRINTF("the remaintime for receiver is (%u) in the on_phase (%u)\n",next_wakeup,on_phase);
										node_period = next_wakeup;
										//	if(!on_phase){
										//		node_period = next_wakeup;
										//	}else{
										//		node_period = next_wakeup /*+ cslmac_config.off_time*/;
										//	}
											//PRINTF("cslmac: the node Period is equal to (%u)/(%u) \n",node_period,cslmac_config.maccslperiod);
											ack_mode=WAKE_UP_EACK;// it is the type of the eACK frame
											
											if((eack_len=tsch_packet_create_eack(eack_buf, sizeof(eack_buf),
													&ack_dest_addr, ack_seqno, 0, 0,ack_mode,node_period,0))<= 0){
														
														PRINTF("cslmac: Error creating the Eack_frame length (%u)\n",eack_len);
														powercycle_turn_radio_off();
														return;
													}
										/*2.send the eack*/
											/*send the eack frame*/
											/*Transmiting the frame*/		
											
											if((tx_result=NETSTACK_RADIO.send(eack_buf,eack_len))>0){
												PRINTF("cslmac: error sending a frame possible collision (%u)\n",tx_result);
														we_are_sending=0;
														powercycle_turn_radio_off();
														return;
											}
											//powercycle_turn_radio_on();
											//we_are_sending=1;
											PRINTF("The eack succesful transmited\n");
											we_are_sending=0;
											powercycle_turn_radio_off();	
										}
									
								}
		
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
  //PT_YIELD_UNTIL(pt2,etimer_expired(&nexton_timer_struct));
  
  if(someone_is_sending>0) {
   // PRINTF("cxmac: should queue packet, now just dropping %d %d %d %d.\n",
   // 		wait_framer, someone_is_sending, sending_frame, radio_is_on);

    /*if some one is sending we have to wakeup again in the next wakeup interval*/
    someone_is_sending--;
    RIMESTATS_ADD(sendingdrop);
    ret = MAC_TX_COLLISION;
  } else {
    PRINTF("cslmac: send immediately.\n");
    ret = send_packet();
  }

  mac_call_sent_callback(sent, ptr, ret, 1);
}
/*---------------------------------------------------------------------------*/




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
static int
turn_off(int keep_radio_on)
{
	cslmac_is_on = 0;
  if(keep_radio_on) {
    return NETSTACK_RADIO.on();
  } else {
    return NETSTACK_RADIO.off();
  }
}
/*---------------------------------------------------------------------------*/


static int
turn_on(void)
{
	cslmac_is_on = 1;
  /*  rtimer_set(&rt, RTIMER_NOW() + cxmac_config.off_time, 1,
      (void (*)(struct rtimer *, void *))powercycle, NULL);*/
	CSCHEDULE_SYNCPOWERCYCLE(DEFAULT_OFF_TIME);
  return 1;
}


/*---------------------------------------------------------------------------*/
static unsigned short
channel_check_interval(void)
{
  return (1ul * CLOCK_SECOND * DEFAULT_CLS_PERIOD) / RTIMER_ARCH_SECOND;
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

