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
#include <string.h>
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
#include "lwip/err.h"
#include "xil_cache.h"
#include "ethernet.h"


#define UART_BASEADDR XPAR_PS7_UART_1_BASEADDR

/* defined by each RAW mode application */
//void print_app_header();
//int start_application(char player, ip_addr_t* ipaddr);
//int transfer_data();

extern "C" {
	void tcp_fasttmr(void);
	void tcp_slowtmr(void);

	/* missing declaration in lwIP */
	void lwip_init(void);
}

extern volatile int TcpFastTmrFlag;
extern volatile int TcpSlowTmrFlag;
static struct netif server_netif;
struct netif *echo_netif;

int this_player;
int sending = 0;

struct tcp_pcb send_pcb;
struct tcp_pcb* test_pcb;
int send_pcb_initialized = 0;

int send_count = 0;
uint8_t thing = 0x00;
extern int msg_received;
extern int recv_x;
extern int recv_y;
extern char recv_res;

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

int transfer_data(int x, int y) {
	if (send_pcb_initialized) {
//		char msg[] = "test1\n\r";
		char x_char = '0' + x;
		char y_char = '0' + y;
		char msg[] = {x_char, ',', y_char, '\n', '\r', '\0'};

		if (strlen(msg) > tcp_sndbuf(test_pcb))
		{
			xil_printf("NOT ENOUGH SPACE IN BUFFER\n\r");
			return 1;
		}
		else
		{
			xil_printf("Tx: %s\n\r", msg);
			err_t err = tcp_write(test_pcb, msg, strlen(msg), 1);

			if (err != ERR_OK) {
				xil_printf("ERROR on tcp_write()\n\r");
				return 1;
			}

//			err = tcp_output(test_pcb);
//			if (err != ERR_OK) {
//				xil_printf("ERROR on tcp_output()\n\r");
//				return 1;
//			}


		}

	}
	return 0;
}

int send_result(int res) {
	if (send_pcb_initialized) {
//		char msg[] = "test1\n\r";
		char res_char = '0';
		if (res > 0)
			res_char = '1';
		else
			res_char = '0';

		char msg[] = {res_char, '\n', '\r', '\0'};

		if (strlen(msg) > tcp_sndbuf(test_pcb))
		{
			xil_printf("NOT ENOUGH SPACE IN BUFFER\n\r");
			return 1;
		}
		else
		{
//			xil_printf("Tx: %s\n\r", msg);
			err_t err = tcp_write(test_pcb, msg, strlen(msg), 1);

			if (err != ERR_OK) {
				xil_printf("ERROR on tcp_write()\n\r");
				return 1;
			}

//			err = tcp_output(test_pcb);
//			if (err != ERR_OK) {
//				xil_printf("ERROR on tcp_output()\n\r");
//				return 1;
//			}


		}

	}
	return 0;
}

void print_app_header()
{
#if (LWIP_IPV6==0)
	xil_printf("\n\r\n\r-----lwIP TCP echo server ------\n\r");
#else
	xil_printf("\n\r\n\r-----lwIPv6 TCP echo server ------\n\r");
#endif
	xil_printf("TCP packets sent to port 6001 will be echoed back\n\r");
}

err_t sent_callback(void *arg, struct tcp_pcb *tpcb, u16_t len)
{
	send_pcb = *tpcb;

	char msg[] = "test\n\r";

	err_t err = tcp_write(&send_pcb, msg, strlen(msg), 1);
	if (err != ERR_OK) {
		xil_printf("ERROR \n\r");
		return err;
	}

	err = tcp_output(&send_pcb);
	if (err != ERR_OK) {
		xil_printf("ERROR on tcp_output()\n\r");
		return err;
	}

	return ERR_OK;
}

err_t recv_callback(void *arg, struct tcp_pcb *tpcb,
                               struct pbuf *p, err_t err)
{
//	xil_printf("recv_callback\n\r");

	/* do not read the packet if we are not in ESTABLISHED state */
	if (!p) {
		xil_printf("Closing connection...\n\r");
		tcp_close(tpcb);
		tcp_recv(tpcb, NULL);
		send_pcb_initialized = 0;
		return ERR_OK;
	}

	/* indicate that the packet has been received */
	tcp_recved(tpcb, p->len);

	if (send_pcb_initialized == 0) {
		test_pcb = tpcb;
		send_pcb_initialized = 1;
		err = tcp_write(tpcb, "init", strlen("init"), 1);
		xil_printf("Initializing pcb for replies\n\r");
	} else {
		xil_printf("Rx : %s\n\r", p->payload);
//		if (player == '1') {
//			char x_char = p->payload[0];
//			char y_char = p->payload[2];
//			recv_x = x_char - '0';
//			recv_y = y_char - '0';
//		} else if (player == '2') {
////			char res = p->payload[0];
//		}
	}

	/* may need to check if there is space in buffer, should perform a check like below */
//	if (tcp_sndbuf(tpcb) > p->len) {
//		err = tcp_write(tpcb, p->payload, p->len, 1);
//	} else
//		xil_printf("no space in tcp_sndbuf\n\r");

	/* free the received pbuf */
	pbuf_free(p);

	msg_received = 1;
	return ERR_OK;
}

err_t accept_callback(void *arg, struct tcp_pcb *newpcb, err_t err)
{
	static int connection = 1;

	/* set the receive callback for this connection */
	tcp_recv(newpcb, recv_callback);

	/* just use an integer number indicating the connection id as the
	   callback argument */
	tcp_arg(newpcb, (void*)(UINTPTR)connection);

	xil_printf("Accepting connection\n\r");

	/* increment for subsequent accepted connections */
	connection++;

	return ERR_OK;
}

err_t connected_callback(void* arg, struct tcp_pcb* newpcb, err_t err)
{
	static int connection = 1;

	/* set the receive callback for this connection */
	tcp_recv(newpcb, recv_callback);

	/* just use an integer number indicating the connection id as the
	   callback argument */
	tcp_arg(newpcb, (void*)(UINTPTR)connection);
	xil_printf("Establishing connection\n\r");

	test_pcb = newpcb;
	send_pcb_initialized = 1;
	err = tcp_write(newpcb, "init", strlen("init"), 1);
	xil_printf("Initializing pcb for replies\n\r");

	/* increment for subsequent accepted connections */
	connection++;

	return ERR_OK;
}


int start_application(char player, ip_addr_t* ipaddr)
{
	struct tcp_pcb *pcb;
	err_t err;
	unsigned port = 7;

	/* create new TCP PCB structure */
	pcb = tcp_new_ip_type(IPADDR_TYPE_ANY);
	if (!pcb) {
		xil_printf("Error creating PCB. Out of Memory\n\r");
		return -1;
	}

	/* bind to specified @port */
	err = tcp_bind(pcb, IP_ANY_TYPE, port);
	if (err != ERR_OK) {
		xil_printf("Unable to bind to port %d: err = %d\n\r", port, err);
		return -2;
	}

	/* we do not need any arguments to callback functions */
	tcp_arg(pcb, NULL);

	// if player 1: wait and listen for connection requests
	// if player 2: try to establish connection with player 1
	if (player == '1') {
		this_player = 1;

		/* listen for connections */
		pcb = tcp_listen(pcb);
		if (!pcb) {
			xil_printf("Out of memory while tcp_listen\n\r");
			return -3;
		}

		/* specify callback to use for incoming connections */
		tcp_accept(pcb, accept_callback);

		xil_printf("Waiting for connection @ port %d\n\r", port);

	} else {
		this_player = 2;

		// connect to other Zedboard
		err = tcp_connect(pcb, ipaddr, port, connected_callback);
		if (err != ERR_OK) {
			xil_printf("Unable to establish connection (tcp_connect)\n\r");
			return -4;
		}

		xil_printf("Attempting to connect to other board\n\r");

	}

	return 0;
}


int eth_init(char player)
{
#if LWIP_IPV6==0
//	ip_addr_t ipaddr, netmask, gw;
//	ip_addr_t player1_ipaddr;
//	ip_addr_t player2_ipaddr;
	ip_addr_t player1_ipaddr, player2_ipaddr, netmask, gw;

#endif

	/* the mac address of the board. this should be unique per board */
	unsigned char player1_mac_address[] =
	{ 0x00, 0x0a, 0x35, 0x00, 0x01, 0x02 };

	unsigned char player2_mac_address[] =
	{ 0x00, 0x0a, 0x35, 0x00, 0x01, 0x04 };

	echo_netif = &server_netif;

	// FROM AUDIO LAB TEST
	// Initialise Push Buttons
//	int status;
//	status = XGpio_Initialize(&BTNInst, BTNS_DEVICE_ID);
//	if(status != XST_SUCCESS) return XST_FAILURE;
//	// Set all buttons direction to inputs
//	XGpio_SetDataDirection(&BTNInst, 1, 0xFF);

	// ABOVE FROM AUDIO LAB TEST



	// START
//	u8 input = 0x00;
//	xil_printf("Press 's' to begin\n\r");
//	while(input != 's') {
//			while (!XUartPs_IsReceiveData(UART_BASEADDR));
//			input = XUartPs_ReadReg(UART_BASEADDR, XUARTPS_FIFO_OFFSET);
//	}
//	input = 0x00;

	init_platform();

//	/* initialize IP addresses to be used */
	IP4_ADDR(&player1_ipaddr,  192, 168,   1, 10);
	IP4_ADDR(&player2_ipaddr,  192, 168,   1, 11);

	IP4_ADDR(&player1_ipaddr,  169, 254,  58, 38);
//	IP4_ADDR(&player1_ipaddr,  169, 254,  58, 93);
	IP4_ADDR(&player2_ipaddr,  169, 254,  58, 34);
	IP4_ADDR(&netmask, 255, 255,   0,  0);

//	IP4_ADDR(&netmask, 255, 255, 255,  0);
	IP4_ADDR(&gw,      192, 168,   1,  1);

	lwip_init();

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
		break;
	default:
		if (!xemac_add(echo_netif, &player1_ipaddr, &netmask,
							&gw, player1_mac_address,
							PLATFORM_EMAC_BASEADDR)) {
			xil_printf("Error adding N/W interface\n\r");
			return -1;
		}
		print_ip_settings(&player1_ipaddr, &netmask, &gw);
		break;
	} // switch


	netif_set_default(echo_netif);

	/* now enable interrupts */
	platform_enable_interrupts();

	/* specify that the network if is up */
	netif_set_up(echo_netif);


	xil_printf("Configured as player %c\n\r", player);
	xil_printf("Press 's' to start application...\n\r");

	u8 input = 0x00;
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

	return 0;
}

void eth_loop()
{
	if (TcpFastTmrFlag) {	// 250ms
		tcp_fasttmr();
		TcpFastTmrFlag = 0;

	}
	if (TcpSlowTmrFlag) {	// 500ms
		tcp_slowtmr();
		TcpSlowTmrFlag = 0;

	}
	xemacif_input(echo_netif);

}
