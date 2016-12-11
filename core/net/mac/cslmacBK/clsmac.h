/*
 * clsmac.h
 *
 *  Created on: Jul 6, 2016
 *      Author: Homero
 */

#ifndef CLSMAC_H_
#define CLSMAC_H_

#include "sys/rtimer.h"
#include "net/mac/rdc.h"
#include "dev/radio.h"
#include "net/mac/tsch/tsch-conf.h"

#ifdef MAC_CLS_PERIOD /* CYCLE_TIME for channel checks, in rtimer ticks. */
#define CLS_PERIOD (MAC_CLS_PERIOD)
#else
#define CLS_PERIOD (RTIMER_ARCH_SECOND / NETSTACK_RDC_CHANNEL_CHECK_RATE)

#ifdef MAC_CSL_MAX_PERIOD /*CYCLE_TIME for unsicronized-TX sends the wakeup sequence*/
#define CLS_MAX_PERIOD MAC_CSL_MAX_PERIOD
#else
#define CLS_MAX_PERIOD CLS_PERIOD * 2


#ifdef MAC_CLS_EACK_WAIT_DURATION /* maximum time after to frame TX that the receiver
								 has to wait for EACK*/
#define  EACK_WAIT_DURATION MAC_CLS_EACK_WAIT_DURATION
#else
#define EACK_WAIT_DURATION RTIMER_ARCH_SECOND * 4256 //by default

#ifdef MAC_WAKEUP_PERIOD /*definition of te wakeup periods*/
#define WAKEUP_PERIOD MAC_WAKEUP_PERIOD
#else
#define WAKEUP_PERIOD  CLS_MAX_PERIOD

#ifdef MAC_SLEEP_PERIOD /*definition of sleep period*/
#define SLEEP_PERIOD MAC_SLEEP_PERIOD
#else
#define SLEEP_PERIOD CLS_PERIOD


#ifdef MAC_INTER_PACKET_DEADLINE
#define INTER_PACKET_DEADLINE               MAC_INTER_PACKET_DEADLINE
#else
#define INTER_PACKET_DEADLINE               RTIMER_ARCH_SECOND / 32
#endif

#ifdef MAC_LISTEN_TIME_AFTER_PACKET_DETECTED
#define TIME_AFTER_PACKET_DETECTED  MAC_LISTEN_TIME_AFTER_PACKET_DETECTED
#else   TIME_AFTER_PACKET_DETECTED (RTIMER_ARCH_SECOND / 80)
#endif

#ifdef MAC_CLS_FRAME_PENDING_TIME
#define FRAME_PENDING_TIME  MAC_CLS_FRAME_PENDING_TIME
#else   FRAME_PENDING_TIME TSCH_CONF_RX_WAIT
#endif


#define TSTX_OFFSET


/*Case: synchornous_Mode*/
#define CYCLE_TIME (RTIMER_ARCH_SECOND / NETSTACK_RDC_CHANNEL_CHECK_RATE)

#define SLEEP_POST_WAKEUP_DIFERENT_DEST (rz_time)= (MAX_PACKET_LEN * RTIMER_NOW()) + rz_time

#define SAMPLE_TIME RTIMER_NOW() +  CYCLE_TIME;

#define WAKEUP_FRAME_TIME RTIMER_NOW() +  CYCLE_TIME;

#define LISTEN_TIME_AFTER_PACKET_DETECTED RTIMER_NOW() + TIME_AFTER_PACKET_DETECTED

#undef LEDS_ON
#undef LEDS_OFF
#undef LEDS_TOGGLE

#define LEDS_ON(x) leds_on(x)
#define LEDS_OFF(x) leds_off(x)
#define LEDS_TOGGLE(x) leds_toggle(x)
#define DEBUG 0
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



enum cls_error_msg{
	MAC_PROCESS_OK,
	WRONG_CSL_CONFIG,
	WRONG_STATE_IN_START_CSL_SAMPLING,
	WRONG_STATE_IN_CSL_TIMERFIRES,
	WRONG_STATE_IN_CSL_SAMPLE,
	WRONG_STATE_IN_CSL_ENDOFFRAME,
	MAC_OPERATION_IN_PROGRESS
};


typedef enum {
	CSL_IDLE_MODE,
	CLS_SLEEP_MODE,
	CLS_RX_MODE,
	CLS_TX_MODE
}ieee802154e_cls_mod;


typedef enum{
	WAIT_WAKEUP, 		/*waiting for a wakeup frame*/
	RECIVED_WAKEUP, 	/*wakeup frame received*/
	WAKEUP_VALIDATE,	/*wakeup frame in validation*/
	WAIT_DATAFRAME,		/*wait for data frame*/
	RECIVED_DATAFRAME,  /*received_dataframe*/
	/*ACK_RX_PHASE*/
	PREPARE_EACK_FRAME,
	EACK_READY_TO_TX,
	TRASMITING_EACK,
	EACK_TX_SUCESSFUL,

}ieee802154e_cls_RX_Sub_States;


typedef enum{
	PREPARE_WAKEUP_FRAME, /*Preparing the wakeup frame*/
	READY_TO_TX_WAKEUP,   /*wakeup frame ready for Transmission*/
	TRASMITING_WAKEUP,    /*trasnmiting the wakeup frame*/
	SUCCESS_TX_WAKEUP,    /*successful TX of the wakeup_frame*/
/*DATA_TX_PHASE*/
	PREPARE_DATA_FRAME,   /*preparing dataframe*/
	DATA_READY_TO_TX,     /*dataframe ready to TX*/
	TRASNSMITING_DATA,	  /*Transmitting the dataframe*/
	DATA_TX_SUCESSFUL,    /*succesfull_TX_dataframe*/
/*ACK_TX_PHASE*/
	WAIT_EACKFRAME,		/*wait for EACKFRAME*/
	RECIVED_EACKFRAME,	 /*successful reception of ACK_FRAME*/
	ACK_FRAME_VALIDATED /*eackframe validated*/
}ieee802154e_cls_TX_Sub_States;







struct cslmac_config {
  ieee802154e_cls_mod cls_operation_mode;
  ieee802154e_cls_RX_Sub_States cls_RX_submode;
  ieee802154e_cls_TX_Sub_States cls_TX_submode;
  rtimer_clock_t macEnhAckWaitDuration;
  rtimer_clock_t macCSLPeriod;
  rtimer_clock_t macMaxCSLPeriod;
  rtimer_clock_t mac_cls_frame_pending_wait_t = 0;
};




#endif /* CLSMAC_H_ */
