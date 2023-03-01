/*
 * Copyright (C) 2009 - 2019 Xilinx, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
 * SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
 * OF SUCH DAMAGE.
 *
 */

#include <stdio.h>
#include <sleep.h>

#include "xil_types.h"
#include "xil_io.h"
#include "xil_exception.h"
#include "xscugic.h"

#include "xparameters.h"

#include "netif/xadapter.h"

#include "xgpio.h"
#include "xuartps.h"

#include "platform.h"
#include "platform_config.h"
#if defined (__arm__) || defined(__aarch64__)
#include "xil_printf.h"
#endif

#include "lwip/tcp.h"
#include "xil_cache.h"

#define UART_BASEADDR XPAR_PS7_UART_1_BASEADDR

/* defined by each RAW mode application */
void print_app_header();
int start_application();
int transfer_data();
void tcp_fasttmr(void);
void tcp_slowtmr(void);

/* missing declaration in lwIP */
void lwip_init();

extern volatile int TcpFastTmrFlag;
extern volatile int TcpSlowTmrFlag;
static struct netif server_netif;
struct netif *echo_netif;

void
print_ip(char *msg, ip_addr_t *ip)
{
	print(msg);
	xil_printf("%d.%d.%d.%d\n\r", ip4_addr1(ip), ip4_addr2(ip),
			ip4_addr3(ip), ip4_addr4(ip));
}

void
print_ip_settings(ip_addr_t *ip, ip_addr_t *mask, ip_addr_t *gw)
{

	print_ip("Board IP: ", ip);
	print_ip("Netmask : ", mask);
	print_ip("Gateway : ", gw);
}


// stuff I added
XGpio BTNInst;
XScuGic INTCInst;
static int btn_value;
volatile int BUTTON_INTR_FLAG = 0;
int sending = 0;


int main()
{
#if LWIP_IPV6==0
	ip_addr_t ipaddr, netmask, gw;
	ip_addr_t player1_ipaddr;
	ip_addr_t player2_ipaddr;

#endif

	/* the mac address of the board. this should be unique per board */
	unsigned char mac_ethernet_address[] =
	{ 0x00, 0x0a, 0x35, 0x00, 0x01, 0x02 };

	unsigned char player1_mac_address[] =
	{ 0x00, 0x0a, 0x35, 0x00, 0x01, 0x02 };

	unsigned char player2_mac_address[] =
	{ 0x00, 0x0a, 0x35, 0x00, 0x01, 0x04 };

	echo_netif = &server_netif;

	// FROM AUDIO LAB TEST
	// Initialise Push Buttons
	int status;
	status = XGpio_Initialize(&BTNInst, BTNS_DEVICE_ID);
	if(status != XST_SUCCESS) return XST_FAILURE;
	// Set all buttons direction to inputs
	XGpio_SetDataDirection(&BTNInst, 1, 0xFF);

	// ABOVE FROM AUDIO LAB TEST

	// START
	u8 input = 0x00;
	xil_printf("Press 's' to begin\n\r");
	while(input != 's') {
			while (!XUartPs_IsReceiveData(UART_BASEADDR));
			input = XUartPs_ReadReg(UART_BASEADDR, XUARTPS_FIFO_OFFSET);
	}
	input = 0x00;

	init_platform();

//	/* initialize IP addresses to be used */
//	IP4_ADDR(&ipaddr,  192, 168,   1, 10);
//	IP4_ADDR(&netmask, 255, 255, 255,  0);
//	IP4_ADDR(&gw,      192, 168,   1,  1);

	/* initialize IP addresses to be used */
//	IP4_ADDR(&ipaddr,  169, 254,  50, 34);
//	IP4_ADDR(&netmask, 255, 255,   0,  0);
//	IP4_ADDR(&gw,      192, 168,   1,  1);

	IP4_ADDR(&player1_ipaddr,  192, 168,   1, 10);
	IP4_ADDR(&player2_ipaddr,  192, 168,   1, 11);

//	IP4_ADDR(&player1_ipaddr,  169, 254,  37, 58);
//	IP4_ADDR(&player1_ipaddr,  169, 254,  58, 43);
//	IP4_ADDR(&player2_ipaddr,  169, 254,  58, 34);
//	IP4_ADDR(&netmask, 255, 255,   0,  0);

	IP4_ADDR(&netmask, 255, 255, 255,  0);
	IP4_ADDR(&gw,      192, 168,   1,  1);

	lwip_init();


	u8 player = 0x00;
	u8 quit = 0x00;
	u32 CntrlRegister;

	CntrlRegister = XUartPs_ReadReg(UART_BASEADDR, XUARTPS_CR_OFFSET);

	XUartPs_WriteReg(UART_BASEADDR, XUARTPS_CR_OFFSET,
				  ((CntrlRegister & ~XUARTPS_CR_EN_DIS_MASK) |
				   XUARTPS_CR_TX_EN | XUARTPS_CR_RX_EN));

	// ask for user input to determine player 1 or 2
	xil_printf("PLAYER 1 will wait for connection as master\n\r");
	xil_printf("PLAYER 2 will connect to the master zedboard\n\r");
	xil_printf("Enter '1' or '2'\n\r");
	while(!quit) {
		while (!XUartPs_IsReceiveData(UART_BASEADDR));
		player = XUartPs_ReadReg(UART_BASEADDR, XUARTPS_FIFO_OFFSET);
		// Select function based on UART input
		switch(player){
		case '1':
			/* Add network interface to the netif_list, and set it as default */
			if (!xemac_add(echo_netif, &player1_ipaddr, &netmask,
								&gw, player1_mac_address,
								PLATFORM_EMAC_BASEADDR)) {
				xil_printf("Error adding N/W interface\n\r");
				return -1;
			}
			print_ip_settings(&player1_ipaddr, &netmask, &gw);
			quit = 1;
			break;
		case '2':
			/* Add network interface to the netif_list, and set it as default */
			if (!xemac_add(echo_netif, &player2_ipaddr, &netmask,
								&gw, player2_mac_address,
								PLATFORM_EMAC_BASEADDR)) {
				xil_printf("Error adding N/W interface\n\r");
				return -1;
			}
			print_ip_settings(&player2_ipaddr, &netmask, &gw);
			quit = 1;
			break;
		default:
			xil_printf("Invalid input, enter '1' or '2'\n\r");
			break;
		} // switch
	}

	netif_set_default(echo_netif);

	/* now enable interrupts */
	platform_enable_interrupts();

	/* specify that the network if is up */
	netif_set_up(echo_netif);


	xil_printf("Configured as player %c\n\r", player);
	xil_printf("Press 's' to start application...\n\r");

	while (input != 's') {
		while (!XUartPs_IsReceiveData(UART_BASEADDR));
		input = XUartPs_ReadReg(UART_BASEADDR, XUARTPS_FIFO_OFFSET);
	}

	/* start the application */
	if (player == '1') {
		start_application(player, NULL);
	} else {
		start_application(player, &player1_ipaddr);
	}

	int count = 0;
	int send_results = 0;
	/* receive and process packets */
	while (1) {
		if (TcpFastTmrFlag) {	// 250ms
			tcp_fasttmr();
			TcpFastTmrFlag = 0;

			if (count == 4) {
				count = 0;
				send_results = 1;
			}
			count++;
		}
		if (TcpSlowTmrFlag) {	// 500ms
			tcp_slowtmr();
			TcpSlowTmrFlag = 0;

		}
		xemacif_input(echo_netif);

//		if (send_results == 1) {
//			send_results = 0;
//			transfer_data();
//		}

		if (BUTTON_INTR_FLAG && send_results == 1) {
			BUTTON_INTR_FLAG = 0;

			if (btn_value == 1) {
				send_results = 0;
				transfer_data();
			}
		}
	}

	/* never reached */
	cleanup_platform();

	return 0;
}

void BTN_Intr_Handler(void *InstancePtr)
{
	// Disable GPIO interrupts
	XGpio_InterruptDisable(&BTNInst, BTN_INT);
	// Ignore additional button presses
	if ((XGpio_InterruptGetStatus(&BTNInst) & BTN_INT) !=
			BTN_INT) {
			return;
		}
	btn_value = XGpio_DiscreteRead(&BTNInst, 1);
	BUTTON_INTR_FLAG = 1;
	// Increment counter based on button value
    (void)XGpio_InterruptClear(&BTNInst, BTN_INT);
    // Enable GPIO interrupts
    XGpio_InterruptEnable(&BTNInst, BTN_INT);
}

