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
//#include <string.h>
//#include <sleep.h>
//
//#include "lwip/err.h"
//#include "lwip/tcp.h"
//#if defined (__arm__) || defined (__aarch64__)
//#include "xil_printf.h"
//#endif
//
//struct tcp_pcb send_pcb;
//struct tcp_pcb* test_pcb;
//int send_pcb_initialized = 0;
//
//int send_count = 0;
//extern int sending;
//uint8_t thing = 0x00;
//
//int transfer_data() {
//	if (send_pcb_initialized) {
//		char msg[] = "test1\n\r";
//
//		if (strlen(msg) > tcp_sndbuf(test_pcb))
//		{
//			xil_printf("NOT ENOUGH SPACE IN BUFFER\n\r");
//			return 1;
//		}
//		else
//		{
//			xil_printf("Tx: %s\n\r", msg);
//			err_t err = tcp_write(test_pcb, msg, strlen(msg), 1);
//
//			if (err != ERR_OK) {
//				xil_printf("ERROR \n\r");
//				return 1;
//			}
////			err = tcp_output(test_pcb);
//
//			if (err != ERR_OK) {
//				xil_printf("ERROR on tcp_output()\n\r");
//				return 1;
//			}
//
////			err = tcp_write(test_pcb, "test2\n\r", strlen("test2\n\r"), 1);
//			if (err != ERR_OK) {
//				xil_printf("ERROR on empty string()\n\r");
//				return 1;
//			}
//		}
//
//	}
//	return 0;
//}
//
//void print_app_header()
//{
//#if (LWIP_IPV6==0)
//	xil_printf("\n\r\n\r-----lwIP TCP echo server ------\n\r");
//#else
//	xil_printf("\n\r\n\r-----lwIPv6 TCP echo server ------\n\r");
//#endif
//	xil_printf("TCP packets sent to port 6001 will be echoed back\n\r");
//}
//
//err_t sent_callback(void *arg, struct tcp_pcb *tpcb, u16_t len)
//{
//	send_pcb = *tpcb;
//
//	char msg[] = "test\n\r";
//
//	err_t err = tcp_write(&send_pcb, msg, strlen(msg), 1);
//	if (err != ERR_OK) {
//		xil_printf("ERROR \n\r");
//		return err;
//	}
//
//	err = tcp_output(&send_pcb);
//	if (err != ERR_OK) {
//		xil_printf("ERROR on tcp_output()\n\r");
//		return err;
//	}
//
//	return ERR_OK;
//}
//
//err_t recv_callback(void *arg, struct tcp_pcb *tpcb,
//                               struct pbuf *p, err_t err)
//{
////	xil_printf("recv_callback\n\r");
//
//	/* do not read the packet if we are not in ESTABLISHED state */
//	if (!p) {
//		xil_printf("Closing connection...\n\r");
//		tcp_close(tpcb);
//		tcp_recv(tpcb, NULL);
//		send_pcb_initialized = 0;
//		return ERR_OK;
//	}
//
//	/* indicate that the packet has been received */
//	tcp_recved(tpcb, p->len);
//
//	if (send_pcb_initialized == 0) {
//		test_pcb = tpcb;
//		send_pcb_initialized = 1;
//		err = tcp_write(tpcb, "init", strlen("init"), 1);
//		xil_printf("Initializing pcb for replies\n\r");
//	} else {
//		xil_printf("Rx : %s\n\r", p->payload);
//	}
//
//	/* may need to check if there is space in buffer, should perform a check like below */
////	if (tcp_sndbuf(tpcb) > p->len) {
////		err = tcp_write(tpcb, p->payload, p->len, 1);
////	} else
////		xil_printf("no space in tcp_sndbuf\n\r");
//
//	/* free the received pbuf */
//	pbuf_free(p);
//
//	return ERR_OK;
//}
//
//err_t accept_callback(void *arg, struct tcp_pcb *newpcb, err_t err)
//{
//	static int connection = 1;
//
//	/* set the receive callback for this connection */
//	tcp_recv(newpcb, recv_callback);
//
//	/* just use an integer number indicating the connection id as the
//	   callback argument */
//	tcp_arg(newpcb, (void*)(UINTPTR)connection);
//
//	xil_printf("Accepting connection\n\r");
//
//	/* increment for subsequent accepted connections */
//	connection++;
//
//	return ERR_OK;
//}
//
//err_t connected_callback(void* arg, struct tcp_pcb* newpcb, err_t err)
//{
//	static int connection = 1;
//
//	/* set the receive callback for this connection */
//	tcp_recv(newpcb, recv_callback);
//
//	/* just use an integer number indicating the connection id as the
//	   callback argument */
//	tcp_arg(newpcb, (void*)(UINTPTR)connection);
//	xil_printf("Establishing connection\n\r");
//
//	test_pcb = newpcb;
//	send_pcb_initialized = 1;
//	err = tcp_write(newpcb, "init", strlen("init"), 1);
//	xil_printf("Initializing pcb for replies\n\r");
//
//	/* increment for subsequent accepted connections */
//	connection++;
//
//	return ERR_OK;
//}
//
//
//int start_application(char player, ip_addr_t* ipaddr)
//{
//	struct tcp_pcb *pcb;
//	err_t err;
//	unsigned port = 7;
//
//	/* create new TCP PCB structure */
//	pcb = tcp_new_ip_type(IPADDR_TYPE_ANY);
//	if (!pcb) {
//		xil_printf("Error creating PCB. Out of Memory\n\r");
//		return -1;
//	}
//
//	/* bind to specified @port */
//	err = tcp_bind(pcb, IP_ANY_TYPE, port);
//	if (err != ERR_OK) {
//		xil_printf("Unable to bind to port %d: err = %d\n\r", port, err);
//		return -2;
//	}
//
//	/* we do not need any arguments to callback functions */
//	tcp_arg(pcb, NULL);
//
//	// if player 1: wait and listen for connection requests
//	// if player 2: try to establish connection with player 1
//	if (player == '1') {
//
//		/* listen for connections */
//		pcb = tcp_listen(pcb);
//		if (!pcb) {
//			xil_printf("Out of memory while tcp_listen\n\r");
//			return -3;
//		}
//
//		/* specify callback to use for incoming connections */
//		tcp_accept(pcb, accept_callback);
//
//		xil_printf("Waiting for connection @ port %d\n\r", port);
//
//	} else {
//
//		// connect to other Zedboard
//		err = tcp_connect(pcb, ipaddr, port, connected_callback);
//		if (err != ERR_OK) {
//			xil_printf("Unable to establish connection (tcp_connect)\n\r");
//			return -4;
//		}
//
//		xil_printf("Attempting to connect to other board\n\r");
//
//	}
//
//	return 0;
//}
