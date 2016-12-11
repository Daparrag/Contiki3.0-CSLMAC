/**
 * \file
 *         A simple power saving MAC protocol based on CLS-MAC [SenSys 2016]
 * \author
 *         Adam Dunkels <adam@sics.se>
 *         Niclas Finne <nfi@sics.se>
 *         Joakim Eriksson <joakime@sics.se>
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

//#define DEFAULT_PERIOD_TIME (RTIMER_ARCH_SECOND / 160)
//#define MAC_CLS_PERIOD ((DEFAULT_PERIOD_TIME) + (RTIMER_ARCH_SECOND / NETSTACK_RDC_CHANNEL_CHECK_RATE - (DEFAULT_PERIOD_TIME)))

#define MAC_CLS_PERIOD 0

#ifdef MAC_CLS_PERIOD
#if MAC_CLS_PERIOD==0
#define DEFAULT_ON_TIME (RTIMER_ARCH_SECOND / 160)
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
//struct ringbufindex input_ringbuf;
//struct input_packet input_array[4];


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





static uint8_t is_streaming;
static uint8_t next_sync_wakeup;
static linkaddr_t is_streaming_to, is_streaming_to_too;
static rtimer_clock_t stream_until;



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
PRINTF("cslmac: channel it is free \n");
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
/*									     */
/* 		csl_mac_init: initialize the clsmac-driver 	             */
/*									     */
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
/*	       								     */
/* 	register_neighbor: introduce a new_neigbor_time & phase entry        */
/*									     */
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
/*									     */
/* 	update_neighbor:  update the neighbor time & phase entry       	     */
/*								             */
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
	ctimer_set(&cpowercycle_ctimer, time,
               (void (*)(void *))sync_powercycle, NULL);//after ctime expired the function cpowercycle is called;
    //printf("cslmac: Accessing to powercycle after that %u ms\n",time);
	//printf("sync_power\n");

  }
    
  
}

static void
input_packet(void){
	
	uint8_t *p;
	uint8_t len=0;
	uint8_t hdr_len=0;
	int16_t input_index;
	frame802154_t frame;	

	//printf("start \n");
	//timer_set(&timer_off, CLOCK_SECOND / 20);	
	//powercycle_turn_radio_off();
	//while(!timer_expired(&timer_off)){}
	
	//powercycle_turn_radio_on();
	//timer_set(&timer_on, CLOCK_SECOND / 20);
	if(NETSTACK_RADIO.pending_packet()&&NETSTACK_RADIO.receiving_packet())	{
		printf("pending package %u,receiving_packet %u\n",NETSTACK_RADIO.pending_packet(),NETSTACK_RADIO.receiving_packet());
			NETSTACK_RADIO.off();
			if(NETSTACK_RADIO.receiving_packet())
				{
		len = NETSTACK_RADIO.read(packetbuf_dataptr(), PACKETBUF_SIZE);
		printf("Package Received %u, leng : %u\n ",NETSTACK_RADIO.receiving_packet(),len);	 	
			}
		//while(!NETSTACK_RADIO.receiving_packet()){printf("not package Recieved \n");}
		//len = NETSTACK_RADIO.read(packetbuf_dataptr(), PACKETBUF_SIZE);
		//printf("Package Received %u, leng : %u\n ",NETSTACK_RADIO.receiving_packet(),len);
		
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
		printf("radio_is on");
		input_packet();
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

frame802154_fcf_t fcf;




static
 char
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
cslmac_is_on=1;
radio_is_on = 1;
waiting_for_packet = 0;

	int e;
	set_global_address();
	clsmac_init();
	const linkaddr_t * src_address;	//Source Address.
	packetbuf_set_addr(PACKETBUF_ADDR_SENDER,(void*)&ipaddr);
	src_address = packetbuf_addr(PACKETBUF_ADDR_SENDER);//geting the source address form the buffer
	
//	on();
	while(1){
	
	input_packet();
//	NETSTACK_RADIO.send(1,1);
}
//	clsmac_init();

PROCESS_END();
	
}
/*---------------------------------------------------------------------------*/
