static int
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
/*	                                                                         */
/*                  send_packet(void): RDC send package protocol             */
/*	                                                                         */
/*---------------------------------------------------------------------------*/

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
struct queuebuf * packet;
frame802154_t frame;
static uint8_t wakeup_buf[TSCH_PACKET_MAX_LEN];
static int wakeup_len;
rtimer_clock_t t;
rtimer_clock_t _nextSamplePeriod;
rtimer_clock_t rztime;
rtimer_clock_t wakeup_tx_time;
uint8_t more_frame;
uint8_t async_dataframe_tx_wait=0;
uint8_t collisions=0;
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
								if((hdr_len = frame802154_parse(packetbuf_dataptr(), packetbuf_datalen(), &frame))>=0){
									if(frame.fcf.frame_type==FRAME802154_ACKFRAME && linkaddr_cmp
										(PACKETBUF_ADDR_RECEIVER, &linkaddr_node_addr)){
										/*the correct ACK_frame it is received*/
										/*update the timer and phase*/
										/*tsch_packet_parse_eack(const uint8_t *buf, int buf_size,
										  uint8_t seqno, frame802154_t *frame, struct ieee802154_ies *ies, uint8_t *hdr_len)*/
										  got_ack = 1;
										  if(tsch_packet_parse_eack(packetbuf_dataptr(),packetbuf_datalen(),r_seqno,&frame,&r_ies,(uint8_t *)hdr_len)>0){
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
						wakeup_len= tsch_packet_create_wakeup(wakeup_buf,dest_address,sizeof(wakeup_buf),r_seqno,more_frame,rztime);
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
								if((hdr_len = frame802154_parse(packetbuf_dataptr(), packetbuf_datalen(), &frame))>=0){
									if(frame.fcf.frame_type==FRAME802154_ACKFRAME && linkaddr_cmp(PACKETBUF_ADDR_RECEIVER, &linkaddr_node_addr)){
										/*the correct ACK_frame has been received*/
										/*update the timer and phase*/
										/*tsch_packet_parse_eack(const uint8_t *buf, int buf_size,
											uint8_t seqno, frame802154_t *frame, struct ieee802154_ies *ies, uint8_t *hdr_len)*/
										got_ack = 1;
											if(tsch_packet_parse_eack(packetbuf_dataptr(),packetbuf_datalen(),r_seqno,&frame,&r_ies,(uint8_t *)hdr_len)>0){
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
		}else if(ack_requiered && !got_ack){
			PRINTF("cslmac: error during transmission %u , %u\n",ack_requiered,got_ack);
			   powercycle_turn_radio_off();
				return MAC_TX_NOACK;
		}
		
		if(collisions != 0){
			PRINTF("cslmac: someone is sending %u\n",collisions);
				someone_is_sending++;
				powercycle_turn_radio_off();
				return MAC_TX_COLLISION;
		}
		
	}
}
