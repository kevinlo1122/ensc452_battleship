/******************************************************************************
*
* Copyright (C) 2009 - 2014 Xilinx, Inc.  All rights reserved.
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in
* all copies or substantial portions of the Software.
*
* Use of the Software is limited solely to applications:
* (a) running on a Xilinx device, or
* (b) that interact with a Xilinx device through a bus or interconnect.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
* XILINX  BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
* WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF
* OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
*
* Except as contained in this notice, the name of the Xilinx shall not be used
* in advertising or otherwise to promote the sale, use or other dealings in
* this Software without prior written authorization from Xilinx.
*
******************************************************************************/

/*
 * helloworld.c: simple test application
 *
 * This application configures UART 16550 to baud rate 9600.
 * PS7 UART (Zynq) is not initialized by this application, since
 * bootrom/bsp configures it to baud rate 115200
 *
 * ------------------------------------------------
 * | UART TYPE   BAUD RATE                        |
 * ------------------------------------------------
 *   uartns550   9600
 *   uartlite    Configurable only in HW design
 *   ps7_uart    115200 (configured by bootrom/bsp)
 */

#include <stdio.h>
#include "platform.h"
#include "xil_printf.h"
#include <sleep.h>
#include "xil_io.h"
#include "adventures_with_ip.h"

#define FLAG 					(*(volatile u8*)(0x20000))
#define SHARED_BASEADDR			0x0F000000

#define BACKGROUND_TRACK_ADDR	0x1FBC4464
#define BACKGROUND_SIZE			3390364		// in bytes
#define VICTORY_TRACK_ADDR		0x1FA1E4BF
#define VICTORY_SIZE			1728420
#define DEFEAT_TRACK_ADDR		0x1F971566
#define DEFEAT_SIZE				708440

// effects still need to be added, these are not correct addresses/sizes
#define HIT_FX_ADDR 			0x1F94C9D1
#define HIT_SIZE 				150420
#define MISS_FX_ADDR 			0x1F937084
#define MISS_SIZE 				88396
#define SINK_FX_ADDR 			0x1F8CAFEB
#define SINK_SIZE 				442520


// dow -data ../../../../Users/arodillo/ensc452/battleship/audio/background.raw 0x1FBC4464
// dow -data ../../../../Users/arodillo/ensc452/battleship/audio/hit.raw 0x1F94C9D1
// dow -data ../../../../Users/arodillo/ensc452/battleship/audio/miss.raw
// dow -data ../../../../Users/arodillo/ensc452/battleship/audio/sink.raw
// dow -data ../../../../Users/arodillo/ensc452/battleship/audio/victory.raw 0x1FA1E4BF
// dow -data ../../../../Users/arodillo/ensc452/battleship/audio/defeat.raw 0x1F971566

int main()
{
    init_platform();

    //Configure the IIC data structure
	IicConfig(XPAR_XIICPS_0_DEVICE_ID);

	//Configure the Audio Codec's PLL
	AudioPllConfig();

	//Configure the Line in and Line out ports.
	//Call LineInLineOutConfig() for a configuration that
	//enables the HP jack too.
	AudioConfigureJacks();

//	xil_printf("ADAU1761 configured\n\r");

	// Main audio stuff
	int sampling_rate = 22050;

	int* bg_track = (int*) BACKGROUND_TRACK_ADDR;
	int* victory_track = (int*) VICTORY_TRACK_ADDR;
	int* defeat_track = (int*) DEFEAT_TRACK_ADDR;
	int* hit_fx = (int*) HIT_FX_ADDR;
	int* miss_fx = (int*) MISS_FX_ADDR;
	int* sink_fx = (int*) SINK_FX_ADDR;

	int track_delay = (int)(1e6 / sampling_rate);	// in microseconds
	int j = 0;

	while (1) {
		// if a game is started
		// play audio
		u8 game_active = FLAG;
		u8 result;
		while (game_active) {
			for (int i = 0; i < BACKGROUND_SIZE/4; i++) {
				result = FLAG;
				// check if game over
				// break;
				if (result == 8)
					break;

				// if 16-bits per sample
				// every 32 bits has [left || right] i think
				// make sure sign is preserved, so i'm shifting the lower 16 all the way up first
				int temp = bg_track[i];
				int left = ((temp >> 16)) * 8;
				int right = (((temp << 16) >> 16)) * 8;

				// check flag to determine if sound effects need to be played
				// example:
				// switch (result)
				// case HIT:
				// 	left = left + explosion_left
				// 	right = right + explosion_right
				int fx_left, fx_right;
				switch(result) {
				case 0:
					break;
				case 1:
					break;
				case 4:
					temp = hit_fx[j];
					fx_left = ((temp >> 16)) * 10;
					fx_right = (((temp << 16) >> 16)) * 10;
					left = left + fx_left;
					right = right + fx_right;

					if (++j == (HIT_SIZE/4)) {
						j = 0;
						FLAG = 1;
					}
					break;
				default:
					temp = 0;
				} // switch

				Xil_Out32(I2S_DATA_TX_L_REG, left);
				Xil_Out32(I2S_DATA_TX_R_REG, right);
				usleep(track_delay);
			}

			// check result if victory/defeat
			// play track
//			switch(result) {
//			case 256:	// victory
//				while (game_active) {
//					for (int i = 0; i < VICTORY_SIZE/4; i++) {
//						int temp = victory_track[i];
//						int left = ((temp >> 16)) * 4;
//						int right = (((temp << 16) >> 16)) * 4;
//
//						Xil_Out32(I2S_DATA_TX_L_REG, left);
//						Xil_Out32(I2S_DATA_TX_R_REG, right);
//						usleep(track_delay);
//					}
//				}
//			}

			// when returning to main menu
			// no audio should be playing
		}
	}


	// Shared memory read/write testing
    int* shared_ptr = (int*) SHARED_BASEADDR;
    int my_num = 15;
    int temp_num = 0;
    FLAG = 1;
    Xil_Out32(shared_ptr, my_num);

    int i = 0;
    while (1) {
    	temp_num = Xil_In32(shared_ptr);
    	xil_printf("ARM1 shared_num: %d \n\r", temp_num);
    	xil_printf("ARM1 my_num: %d\n\r", my_num);
    	Xil_Out32(shared_ptr, my_num);
    	usleep(5000000);

    	FLAG = 0;
    	while (FLAG == 0) {

    	}

    }

    cleanup_platform();

    return 0;
}
