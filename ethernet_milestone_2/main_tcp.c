///*
// * Copyright (C) 2009 - 2019 Xilinx, Inc.
// * All rights reserved.
// *
// * Redistribution and use in source and binary forms, with or without modification,
// * are permitted provided that the following conditions are met:
// *
// * 1. Redistributions of source code must retain the above copyright notice,
// *    this list of conditions and the following disclaimer.
// * 2. Redistributions in binary form must reproduce the above copyright notice,
// *    this list of conditions and the following disclaimer in the documentation
// *    and/or other materials provided with the distribution.
// * 3. The name of the author may not be used to endorse or promote products
// *    derived from this software without specific prior written permission.
// *
// * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
// * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
// * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
// * SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
// * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
// * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
// * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
// * OF SUCH DAMAGE.
// *
// */
//
//#include <stdio.h>
//#include <sleep.h>
//
//#include "xil_types.h"
//#include "xil_io.h"
//#include "xil_exception.h"
//#include "xscugic.h"
//
//#include "xparameters.h"
//
//#include "netif/xadapter.h"
//
//#include "xgpio.h"
//
//#include "platform.h"
//#include "platform_config.h"
//#if defined (__arm__) || defined(__aarch64__)
//#include "xil_printf.h"
//#endif
//
//#include "lwip/tcp.h"
//#include "xil_cache.h"
//
//#if LWIP_IPV6==1
//#include "lwip/ip.h"
//#else
//#if LWIP_DHCP==1
//#include "lwip/dhcp.h"
//#endif
//#endif
//
///* defined by each RAW mode application */
//void print_app_header();
//int start_application();
//int transfer_data();
//void tcp_fasttmr(void);
//void tcp_slowtmr(void);
//
///* missing declaration in lwIP */
//void lwip_init();
//
//#if LWIP_IPV6==0
//#if LWIP_DHCP==1
//extern volatile int dhcp_timoutcntr;
//err_t dhcp_start(struct netif *netif);
//#endif
//#endif
//
//extern volatile int TcpFastTmrFlag;
//extern volatile int TcpSlowTmrFlag;
//static struct netif server_netif;
//struct netif *echo_netif;
//
//#if LWIP_IPV6==1
//void print_ip6(char *msg, ip_addr_t *ip)
//{
//	print(msg);
//	xil_printf(" %x:%x:%x:%x:%x:%x:%x:%x\n\r",
//			IP6_ADDR_BLOCK1(&ip->u_addr.ip6),
//			IP6_ADDR_BLOCK2(&ip->u_addr.ip6),
//			IP6_ADDR_BLOCK3(&ip->u_addr.ip6),
//			IP6_ADDR_BLOCK4(&ip->u_addr.ip6),
//			IP6_ADDR_BLOCK5(&ip->u_addr.ip6),
//			IP6_ADDR_BLOCK6(&ip->u_addr.ip6),
//			IP6_ADDR_BLOCK7(&ip->u_addr.ip6),
//			IP6_ADDR_BLOCK8(&ip->u_addr.ip6));
//
//}
//#else
//void
//print_ip(char *msg, ip_addr_t *ip)
//{
//	print(msg);
//	xil_printf("%d.%d.%d.%d\n\r", ip4_addr1(ip), ip4_addr2(ip),
//			ip4_addr3(ip), ip4_addr4(ip));
//}
//
//void
//print_ip_settings(ip_addr_t *ip, ip_addr_t *mask, ip_addr_t *gw)
//{
//
//	print_ip("Board IP: ", ip);
//	print_ip("Netmask : ", mask);
//	print_ip("Gateway : ", gw);
//}
//#endif
//
//#if defined (__arm__) && !defined (ARMR5)
//#if XPAR_GIGE_PCS_PMA_SGMII_CORE_PRESENT == 1 || XPAR_GIGE_PCS_PMA_1000BASEX_CORE_PRESENT == 1
//int ProgramSi5324(void);
//int ProgramSfpPhy(void);
//#endif
//#endif
//
//#ifdef XPS_BOARD_ZCU102
//#ifdef XPAR_XIICPS_0_DEVICE_ID
//int IicPhyReset(void);
//#endif
//#endif
//
//
//// stuff I added
//XGpio BTNInst;
//XScuGic INTCInst;
//static int btn_value;
//volatile int BUTTON_INTR_FLAG = 0;
//int sending = 0;
//
////static void BTN_Intr_Handler(void *baseaddr_p);
////static int InterruptSystemSetup(XScuGic *XScuGicInstancePtr);
////static int IntcInitFunction(u16 DeviceId, XGpio *GpioInstancePtr);
//
//// Parameter definitions
////#define INTC_DEVICE_ID_1 		XPAR_PS7_SCUGIC_0_DEVICE_ID
////#define BTNS_DEVICE_ID			XPAR_AXI_GPIO_1_DEVICE_ID
////#define INTC_GPIO_INTERRUPT_ID 	XPAR_FABRIC_AXI_GPIO_1_IP2INTC_IRPT_INTR
////
////#define BTN_INT 			XGPIO_IR_CH1_MASK
//
////
//
//
//
//
//int main()
//{
//#if LWIP_IPV6==0
//	ip_addr_t ipaddr, netmask, gw;
//
//#endif
//
//	/* the mac address of the board. this should be unique per board */
//	unsigned char mac_ethernet_address[] =
//	{ 0x00, 0x0a, 0x35, 0x00, 0x01, 0x02 };
//
//	echo_netif = &server_netif;
//
//#if defined (__arm__) && !defined (ARMR5)
//#if XPAR_GIGE_PCS_PMA_SGMII_CORE_PRESENT == 1 || XPAR_GIGE_PCS_PMA_1000BASEX_CORE_PRESENT == 1
//	ProgramSi5324();
//	ProgramSfpPhy();
//#endif
//#endif
//
///* Define this board specific macro in order perform PHY reset on ZCU102 */
//#ifdef XPS_BOARD_ZCU102
//	if(IicPhyReset()) {
//		xil_printf("Error performing PHY reset \n\r");
//		return -1;
//	}
//#endif
//
//	// FROM AUDIO LAB TEST
//	// Initialise Push Buttons
//	int status;
//	status = XGpio_Initialize(&BTNInst, BTNS_DEVICE_ID);
//	if(status != XST_SUCCESS) return XST_FAILURE;
//	// Set all buttons direction to inputs
//	XGpio_SetDataDirection(&BTNInst, 1, 0xFF);
//
//	// ABOVE FROM AUDIO LAB TEST
//
//	init_platform();
//
//#if LWIP_IPV6==0
//#if LWIP_DHCP==1
//    ipaddr.addr = 0;
//	gw.addr = 0;
//	netmask.addr = 0;
//#else
////	/* initialize IP addresses to be used */
////	IP4_ADDR(&ipaddr,  192, 168,   1, 10);
////	IP4_ADDR(&netmask, 255, 255, 255,  0);
////	IP4_ADDR(&gw,      192, 168,   1,  1);
//	/* initialize IP addresses to be used */
//	IP4_ADDR(&ipaddr,  169, 254,  50, 34);
//	IP4_ADDR(&netmask, 255, 255,   0,  0);
//	IP4_ADDR(&gw,      192, 168,   1,  1);
//#endif
//#endif
//
//	print_app_header();
//
//	lwip_init();
//
//#if (LWIP_IPV6 == 0)
//	/* Add network interface to the netif_list, and set it as default */
//	if (!xemac_add(echo_netif, &ipaddr, &netmask,
//						&gw, mac_ethernet_address,
//						PLATFORM_EMAC_BASEADDR)) {
//		xil_printf("Error adding N/W interface\n\r");
//		return -1;
//	}
//#else
//	/* Add network interface to the netif_list, and set it as default */
//	if (!xemac_add(echo_netif, NULL, NULL, NULL, mac_ethernet_address,
//						PLATFORM_EMAC_BASEADDR)) {
//		xil_printf("Error adding N/W interface\n\r");
//		return -1;
//	}
//	echo_netif->ip6_autoconfig_enabled = 1;
//
//	netif_create_ip6_linklocal_address(echo_netif, 1);
//	netif_ip6_addr_set_state(echo_netif, 0, IP6_ADDR_VALID);
//
//	print_ip6("\n\rBoard IPv6 address ", &echo_netif->ip6_addr[0].u_addr.ip6);
//
//#endif
//
//	netif_set_default(echo_netif);
//
//	/* now enable interrupts */
//	platform_enable_interrupts();
//
//	// FROM AUDIO LAB TEST
//	// Initialize interrupt controller
////	status = IntcInitFunction(INTC_DEVICE_ID, &BTNInst);
////	if(status != XST_SUCCESS) return XST_FAILURE;
//
//	// ABOVE FROM AUDIO LAB TEST
//
//
//
//	// BELOW FROM VIDEO TUTORIAL
////	XTmrCtr TimerInstancePtr;
////	int xStatus;
////	//-----------Setup Timer Interrupt---------------------------------------
////
////	xStatus = XTmrCtr_Initialize(&TimerInstancePtr,XPAR_AXI_TIMER_0_DEVICE_ID);
////
////	XTmrCtr_SetHandler(&TimerInstancePtr,
////	(XTmrCtr_Handler)Timer_InterruptHandler,
////	&TimerInstancePtr);
////
////	//Reset Values
////	XTmrCtr_SetResetValue(&TimerInstancePtr,
////	0, //Change with generic value
////	//0xFFF0BDC0);
////	//0x23C34600);
////	0xDC3CB9FF);
////	//Interrupt Mode and Auto reload
////	XTmrCtr_SetOptions(&TimerInstancePtr,
////	XPAR_AXI_TIMER_0_DEVICE_ID,
////	(XTC_INT_MODE_OPTION | XTC_AUTO_RELOAD_OPTION ));
////
////	xStatus=ScuGicInterrupt_Init(XPAR_PS7_SCUGIC_0_DEVICE_ID,&TimerInstancePtr);
//
//	// ABOVE FROM VIDEO TUTORIAL
//
//	/* specify that the network if is up */
//	netif_set_up(echo_netif);
//
//#if (LWIP_IPV6 == 0)
//#if (LWIP_DHCP==1)
//	/* Create a new DHCP client for this interface.
//	 * Note: you must call dhcp_fine_tmr() and dhcp_coarse_tmr() at
//	 * the predefined regular intervals after starting the client.
//	 */
//	dhcp_start(echo_netif);
//	dhcp_timoutcntr = 24;
//
//	while(((echo_netif->ip_addr.addr) == 0) && (dhcp_timoutcntr > 0))
//		xemacif_input(echo_netif);
//
//	if (dhcp_timoutcntr <= 0) {
//		if ((echo_netif->ip_addr.addr) == 0) {
//			xil_printf("DHCP Timeout\r\n");
//			xil_printf("Configuring default IP of 192.168.1.10\r\n");
//			IP4_ADDR(&(echo_netif->ip_addr),  192, 168,   1, 10);
//			IP4_ADDR(&(echo_netif->netmask), 255, 255, 255,  0);
//			IP4_ADDR(&(echo_netif->gw),      192, 168,   1,  1);
//		}
//	}
//
//	ipaddr.addr = echo_netif->ip_addr.addr;
//	gw.addr = echo_netif->gw.addr;
//	netmask.addr = echo_netif->netmask.addr;
//#endif
//
//	print_ip_settings(&ipaddr, &netmask, &gw);
//
//#endif
//
//	/* start the application (web server, rxtest, txtest, etc..) */
//	start_application();
//
//	// FROM VIDEO TUTORIAL
////	/*Enable the interrupt for the device and then cause (simulate) an interrupt so the handlers will be called*/
////	XScuGic_Enable(&InterruptController, 61);
////	XScuGic_SetPriorityTriggerType(&InterruptController, 61, 0xa0, 3);
//	// FROM VIDEO TUTORIAL
//
////	while(1) {
////		XTmrCtr_Start(&TimerInstancePtr,0);
////		while(TIMER_INTR_FLG == 0){
////		}
////
////		TIMER_INTR_FLG = 0;
////
////		xemacif_input(echo_netif);
////		transfer_data();
////	}
//
//	int count = 0;
//	int send_results = 0;
//	/* receive and process packets */
//	while (1) {
//		if (TcpFastTmrFlag) {
//			tcp_fasttmr();
//			TcpFastTmrFlag = 0;
//
//			if (count == 4) {
//				count = 0;
//				send_results = 1;
//			}
//			count++;
//		}
//		if (TcpSlowTmrFlag) {
//			tcp_slowtmr();
//			TcpSlowTmrFlag = 0;
//
////			if (count == 4) {
////				count = 0;
////				send_results = 1;
////			}
////			count++;
//		}
//		xemacif_input(echo_netif);
//
//		if (send_results == 1) {
//			send_results = 0;
//			transfer_data();
//		}
//
//		if (BUTTON_INTR_FLAG && send_results == 1) {
//			BUTTON_INTR_FLAG = 0;
//			send_results = 0;
//
//			if (btn_value != 0) {
//				transfer_data();
//			}
//		}
//	}
//
//	/* never reached */
//	cleanup_platform();
//
//	return 0;
//}
//
//void BTN_Intr_Handler(void *InstancePtr)
//{
//	// Disable GPIO interrupts
//	XGpio_InterruptDisable(&BTNInst, BTN_INT);
//	// Ignore additional button presses
//	if ((XGpio_InterruptGetStatus(&BTNInst) & BTN_INT) !=
//			BTN_INT) {
//			return;
//		}
//	btn_value = XGpio_DiscreteRead(&BTNInst, 1);
//	BUTTON_INTR_FLAG = 1;
//	// Increment counter based on button value
//    (void)XGpio_InterruptClear(&BTNInst, BTN_INT);
//    // Enable GPIO interrupts
//    XGpio_InterruptEnable(&BTNInst, BTN_INT);
//}
//
