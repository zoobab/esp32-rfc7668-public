/*
 * Copyright (C) 2014 BlueKitchen GmbH
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holders nor the names of
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 * 4. Any redistribution, use, or modification is done solely for
 *    personal benefit and not for any commercial purpose or for
 *    monetary gain.
 *
 * THIS SOFTWARE IS PROVIDED BY BLUEKITCHEN GMBH AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL MATTHIAS
 * RINGWALD OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * Please inquire about commercial licensing options at 
 * contact@bluekitchen-gmbh.com
 *
 */

// *****************************************************************************
/* EXAMPLE_START(spp_and_le_counter): Dual mode example
 * 
 * @text The SPP and LE Counter example combines the Bluetooth Classic SPP Counter
 * and the Bluetooth LE Counter into a single application.
 *
 * In this Section, we only point out the differences to the individual examples
 * and how how the stack is configured.
 */
// *****************************************************************************

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
 
#include "btstack.h"
#include "l2cap.h"
#include "sixxlowpan_test.h"
#include "netif/rfc7668.h"
#include "netif/rfc7668_opts.h"
#include "lwip/ip6_addr.h"
#include "lwip/tcpip.h"

int btstack_main(int argc, const char * argv[]);

struct netif rfc7668_netif;
ip6_addr_t ip6addr_local;

#define RFCOMM_SERVER_CHANNEL 1
#define HEARTBEAT_PERIOD_MS 1000

//WTF is this?!?
#define TEST_COD 0x1234
#define NUM_ROWS 25
#define NUM_COLS 40
#define DATA_VOLUME (10 * 1000 * 1000)

/*
 * @section Advertisements 
 *
 * @text The Flags attribute in the Advertisement Data indicates if a device is in dual-mode or not.
 * Flag 0x06 indicates LE General Discoverable, BR/EDR not supported although we're actually using BR/EDR.
 * In the past, there have been problems with Anrdoid devices when the flag was not set.
 * Setting it should prevent the remote implementation to try to use GATT over LE/EDR, which is not 
 * implemented by BTstack. So, setting the flag seems like the safer choice (while it's technically incorrect).
 */
/* LISTING_START(advertisements): Advertisement data: Flag 0x06 indicates LE-only device */
const uint8_t adv_data[] = {
    // Flags general discoverable, BR/EDR not supported
    0x02, 0x01, 0x05, 
    // IPSP profile
    0x03, 0x03, 0x20, 0x18,
    // Name
    0x08, 0x09, 'T', 'o', 'R', 'a', 'D', 'e', 'S', 
};

static btstack_packet_callback_registration_t hci_event_callback_registration;
uint8_t adv_data_len = sizeof(adv_data);
static bd_addr_t peer_addr;

//static uint8_t  test_data[NUM_ROWS * NUM_COLS];
//static state_t state = W4_SDP_RESULT;

//IPSP - buffer
static uint8_t   ipsp_service_buffer[1300];
static uint8_t   ipsp_addr_type;
static uint16_t  ipsp_cid = 0;
static hci_con_handle_t le_connection_handle;
ip6_addr_t src;

/** 
 * Find remote peer by COD
 */
#define INQUIRY_INTERVAL 5/*
static void start_scan(void){
    printf("Starting inquiry scan..\n");
    hci_send_cmd(&hci_inquiry, HCI_INQUIRY_LAP, INQUIRY_INTERVAL, 0);
    state = W4_PEER_COD;
}
static void stop_scan(void){
    printf("Stopping inquiry scan..\n");
    hci_send_cmd(&hci_inquiry_cancel);
    state = W4_SCAN_COMPLETE;
}*/


// returns 1 if name is found in advertisement
#if 0
static int advertisement_report_contains_name(const char * name, uint8_t * advertisement_report){
    // get advertisement from report event
    const uint8_t * adv_data = gap_event_advertising_report_get_data(advertisement_report);
    uint16_t        adv_len  = gap_event_advertising_report_get_data_length(advertisement_report);
    int             name_len = strlen(name);

    // iterate over advertisement data
    ad_context_t context;
    for (ad_iterator_init(&context, adv_len, adv_data) ; ad_iterator_has_more(&context) ; ad_iterator_next(&context)){
        uint8_t data_type    = ad_iterator_get_data_type(&context);
        uint8_t data_size    = ad_iterator_get_data_len(&context);
        const uint8_t * data = ad_iterator_get_data(&context);
        int i;
        switch (data_type){
            case BLUETOOTH_DATA_TYPE_SHORTENED_LOCAL_NAME:
            case BLUETOOTH_DATA_TYPE_COMPLETE_LOCAL_NAME:
                // compare common prefix
                for (i=0; i<data_size && i<name_len;i++){
                    if (data[i] != name[i]) break;
                }
                // prefix match
                return 1;
            default:
                break;
        }
    }
    return 0;
}
#endif

/* 
 * @section Packet Handler
 * 
 * @text The packet handler of the combined example is just the combination of the individual packet handlers.
 */

static void packet_handler (uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);

    bd_addr_t event_addr;
    //int i;
    //int num_responses;

	switch (packet_type) {
		case HCI_EVENT_PACKET:
			switch (hci_event_packet_get_type(packet)) {

				case HCI_EVENT_PIN_CODE_REQUEST:
					// inform about pin code request
                    printf("Pin code request - using '0000'\n");
                    hci_event_pin_code_request_get_bd_addr(packet, event_addr);
					hci_send_cmd(&hci_pin_code_request_reply, &event_addr, 4, "0000");
					break;

                case HCI_EVENT_USER_CONFIRMATION_REQUEST:
                    // inform about user confirmation request
                    printf("SSP User Confirmation Request with numeric value '%06"PRIu32"'\n", little_endian_read_32(packet, 8));
                    printf("SSP User Confirmation Auto accept\n");
                    break;

                case HCI_EVENT_DISCONNECTION_COMPLETE:
                    break;

                case HCI_EVENT_LE_META:
                    switch (hci_event_le_meta_get_subevent_code(packet)) {
                        case HCI_SUBEVENT_LE_CONNECTION_COMPLETE:
                            break;
                    }
                    break;  

                case GAP_EVENT_ADVERTISING_REPORT:
                    break;

                case BTSTACK_EVENT_STATE:
                    if (btstack_event_state_get_state(packet) != HCI_STATE_WORKING) return;
                    break;

                case HCI_EVENT_EXTENDED_INQUIRY_RESPONSE:
                    break;
                    
                case HCI_EVENT_INQUIRY_COMPLETE:
                    printf("Inquiry complete\n");
                    break;
                    
                case HCI_EVENT_COMMAND_COMPLETE:
                case HCI_EVENT_NUMBER_OF_COMPLETED_PACKETS:
                //completely unknown...
                case 0x61:
                case 0x66:
                case 0x6e:
					//Command complete, do nothing...
					break;

                default:
					printf("Unknown HCI EVT: %d / 0x%x\n",hci_event_packet_get_type(packet),hci_event_packet_get_type(packet));
                    break;
			}
            break;

        default:
			printf("Unknown packet_type: %d / 0x%x\n",packet_type,packet_type);
            break;
	}
}


void l2cap_ipsp_cb(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size)
{
	uint8_t ret = 0;
	struct pbuf *p;
	
	switch(packet_type)
	{
		case HCI_EVENT_PACKET:
			switch (hci_event_packet_get_type(packet)) {
				                    
                case L2CAP_EVENT_LE_INCOMING_CONNECTION:
					/**
					 * @format 1BH2222
					 * @param address_type
					 * @param address
					 * @param handle
					 * @param psm
					 * @param local_cid
					 * @param remote_cid
					 * @param remote_mtu
					 */
					ipsp_cid = l2cap_event_le_incoming_connection_get_local_cid(packet);
					le_connection_handle = l2cap_event_le_incoming_connection_get_handle(packet);
					l2cap_event_le_incoming_connection_get_address(packet, peer_addr);
					ipsp_addr_type = l2cap_event_le_incoming_connection_get_address_type(packet);
					ret = l2cap_le_accept_connection(ipsp_cid,ipsp_service_buffer,sizeof(ipsp_service_buffer),L2CAP_LE_AUTOMATIC_CREDITS);
					if(ret != 0) 
					{
						printf("IPSP - error accepting conn: %d\n",ret);
					} else {
						printf("IPSP - incoming connection accepted, local CID: %d, handle: %d\n",ipsp_cid,le_connection_handle);
						printf("IPSP - peer: ");
						for(uint8_t i=0;i<sizeof(peer_addr);i++) printf("%2X:",*(peer_addr+i));
						printf("\n");
						netif_set_link_up(&rfc7668_netif);
						netif_set_up(&rfc7668_netif);
						xEventGroupSetBits(lowpan_ble_flags,LOWPAN_BLE_FLAG_CONNECTED);
						
					}
					break;
                case L2CAP_EVENT_LE_CAN_SEND_NOW:
					#if LOWPAN_BLE_DEBUG_L2CAP_CB
						printf("IPSP - LE can send now...\n");
					#endif
					break;
                case L2CAP_EVENT_LE_PACKET_SENT:
					#if LOWPAN_BLE_DEBUG_L2CAP_CB
						printf("IPSP - LE packet sent...\n");
					#endif
					break;
                case L2CAP_EVENT_LE_CHANNEL_CLOSED:
					printf("IPSP - LE channel closed...\n");
					l2cap_le_disconnect(ipsp_cid);
					xEventGroupClearBits(lowpan_ble_flags,LOWPAN_BLE_FLAG_CONNECTED);
					break;
                case L2CAP_EVENT_LE_CHANNEL_OPENED:
					#if LOWPAN_BLE_DEBUG_L2CAP_CB
						printf("IPSP - LE channel opened...\n");
					#endif
					break;
					
				case L2CAP_EVENT_CHANNEL_CLOSED:
					#if LOWPAN_BLE_DEBUG_L2CAP_CB
						printf("IPSP - channel closed...\n");
					#endif
					l2cap_disconnect(ipsp_cid, 0);
					break;
				                    
                case L2CAP_EVENT_CHANNEL_OPENED:
                case L2CAP_EVENT_INCOMING_CONNECTION:
				case L2CAP_EVENT_CAN_SEND_NOW:
				
				case L2CAP_EVENT_CONNECTION_PARAMETER_UPDATE_REQUEST:
				case L2CAP_EVENT_CONNECTION_PARAMETER_UPDATE_RESPONSE:
				case L2CAP_EVENT_TIMEOUT_CHECK:
					printf("IPSP - event without LE? Problem... (EVT: %d / 0x%2X)\n", hci_event_packet_get_type(packet),hci_event_packet_get_type(packet));
					break;
                    
				default:
					printf("IPSP - HCI EVT: %d, 0x%2X\n",hci_event_packet_get_type(packet),hci_event_packet_get_type(packet));
					break;
			}
		break;
		
		
		case L2CAP_DATA_PACKET:	
			if(ipsp_addr_type == BD_ADDR_TYPE_LE_PUBLIC)
			{
				ble_addr_to_eui64((uint8_t *)src.addr, peer_addr, 1);
			} else {
				ble_addr_to_eui64((uint8_t *)src.addr, peer_addr, 0);
			}
			
			p = pbuf_alloc(PBUF_RAW, 127, PBUF_REF);
			p->payload = packet;
			p->len = size;
			#if LOWPAN_BLE_DEBUG_L2CAP_CB
				printf("L2CAP: len: %d, tot_len: %d\n",p->len,p->tot_len);
			#endif
			//TODO: über netif aufrufen
			rfc7668_input(p,&rfc7668_netif,&src);
		break;
				
		default:
			printf("IPSP, unknown packet type: %d / 0x%2X\n",packet_type,packet_type);
		break;
	}
}


err_t rfc7668_send_L2CAP(struct netif *netif, struct pbuf *p)
{
	return l2cap_le_send_data(ipsp_cid, p->payload, p->len);
}


/* LISTING_START(MainConfiguration): Init L2CAP RFCOMM SDO SM ATT Server and start heartbeat timer */
int btstack_main(int argc, const char * argv[])
{
    UNUSED(argc);
    (void)argv;

	//create flag group
	lowpan_ble_flags = xEventGroupCreate();

    // register for HCI events
    hci_event_callback_registration.callback = &packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);

    l2cap_init();

	//TODO: hier die security/pairing sachen richtig einstellen...
    gap_ssp_set_io_capability(SSP_IO_CAPABILITY_DISPLAY_YES_NO);

    // short-cut to find other ODIN module?!? -> welche COD (class of device)?
    gap_set_class_of_device(TEST_COD);

    gap_discoverable_control(1);

    // setup le device db
    le_device_db_init();

    // enabled EIR
    hci_set_inquiry_mode(INQUIRY_MODE_STANDARD);
    //hci_set_inquiry_mode(INQUIRY_MODE_RSSI);

    // setup SM: Display only
    sm_init();


    // setup advertisements
    uint16_t adv_int_min = 0x0030;
    uint16_t adv_int_max = 0x0030;
    uint8_t adv_type = 0;
    bd_addr_t null_addr;
    memset(null_addr, 0, 6);
    gap_advertisements_set_params(adv_int_min, adv_int_max, adv_type, 0, null_addr, 0x07, 0x00);
    gap_advertisements_set_data(adv_data_len, (uint8_t*) adv_data);
    gap_advertisements_enable(1);

	//TODO: notwendig??
    //gatt_client_init();
    //spp_create_test_data();
    
    
    //setup l2cap for IPSP support
    //BT_PSM_IPSP => 0x0023 /* Internet Protocol Support Profile  */
    //WARNING: change security level 0!!!
    l2cap_le_register_service(l2cap_ipsp_cb,PSM_IPSP,LEVEL_0);

    // turn on!
	hci_power_control(HCI_POWER_ON);
	
	//activate LWIP stack & add the RFC7668 network IF
	tcpip_init(NULL, NULL); 
	
	netif_add(&rfc7668_netif, 
	//fields are only available if compiled with IPv4
	#if LWIP_IPV4
		NULL,NULL,NULL,
	#endif /* LWIP_IPV4 */
	 NULL, rfc7668_if_init, rfc7668_input);
	 netif_add_ip6_address(&rfc7668_netif,&ip6addr_local,NULL);
	 rfc7668_netif.name[0] = 'B';
	 rfc7668_netif.name[1] = 'T';
	 
	 //net_if add docs:
	 	/*callback function that is called to pass ingress packets up in the protocol layer stack.
It is recommended to use a function that passes the input directly to the stack (netif_input(), NO_SYS=1 mode) or via sending a message to TCPIP thread (tcpip_input(), NO_SYS=0 mode).
These functions use netif flags NETIF_FLAG_ETHARP and NETIF_FLAG_ETHERNET to decide whether to forward to ethernet_input() or ip_input(). In other words, the functions only work when the netif driver is implemented correctly!
Most members of struct netif should be be initialized by the netif init function = netif driver (init parameter of this function).
IPv6: Don't forget to call netif_create_ip6_linklocal_address() after setting the MAC address in struct netif.hwaddr (IPv6 requires a link-local address).*/
	 
	rfc7668_netif.linkoutput = rfc7668_send_L2CAP;

    return 0;
}
/* LISTING_END */
/* EXAMPLE_END */
