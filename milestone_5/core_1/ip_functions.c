/*
 * ip_functions.c
 *
 * Contains all functions which pertain to setup and use of IP periperals.
 */

#include "adventures_with_ip.h"
#include <math.h>


/* ---------------------------------------------------------------------------- *
 * 								audio_stream()									*
 * ---------------------------------------------------------------------------- *
 * This function performs audio loopback streaming by sampling the input audio
 * from the codec and then immediately passing the sample to the output of the
 * codec.
 *
 * The main menu can be accessed by entering 'q' on the keyboard.
 * ---------------------------------------------------------------------------- */
//void audio_stream(){
//	u32  in_left, in_right;
//
//	while (!XUartPs_IsReceiveData(UART_BASEADDR)){
//		// Read audio input from codec
//		in_left = Xil_In32(I2S_DATA_RX_L_REG);
//		in_right = Xil_In32(I2S_DATA_RX_R_REG);
//		// Write audio output to codec
//		Xil_Out32(I2S_DATA_TX_L_REG, in_left);
//		Xil_Out32(I2S_DATA_TX_R_REG, in_left);
//	}
//
//	/* If input from the terminal is 'q', then return to menu.
//	 * Else, continue streaming. */
//	if(XUartPs_ReadReg(UART_BASEADDR, XUARTPS_FIFO_OFFSET) == 'q') menu();
//	else audio_stream();
//}

//void sin_out() {
//	int freq = 100;	// Hz
//	int num_samples = 48000 / freq;
//
//	short values[num_samples];
//	double amp = 60000;
//	for (int i = 0; i < num_samples; i++) {
//		values[i] = (short) (sin((double)i / (num_samples*2*M_PI)) * amp);
//	}
//
//	while (1) {
//		for (int i = 0; i < num_samples; i++) {
//			Xil_Out32(I2S_DATA_TX_L_REG, values[i]);
//			Xil_Out32(I2S_DATA_TX_R_REG, values[i]);
//		}
//	}
//}
