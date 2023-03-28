/*
 * adventures_with_ip.c
 *
 * Main source file. Contains main() and menu() functions.
 */
#include "xparameters.h"
#include "xgpio.h"
#include "xscugic.h"
#include "xil_exception.h"
#include "xil_printf.h"
#include "adventures_with_ip.h"

static int btn_value;
volatile int BUTTON_INTR_FLAG = 0;

/* ---------------------------------------------------------------------------- *
 * 									main()										*
 * ---------------------------------------------------------------------------- *
 * Runs all initial setup functions to initialise the audio codec and IP
 * peripherals, before calling the interactive menu system.
 * ---------------------------------------------------------------------------- */
int lab_test_main(void)
{
//	xil_printf("Entering Main\r\n");

	//Configure the IIC data structure
	IicConfig(XPAR_XIICPS_0_DEVICE_ID);

	//Configure the Audio Codec's PLL
	AudioPllConfig();

	//Configure the Line in and Line out ports.
	//Call LineInLineOutConfig() for a configuration that
	//enables the HP jack too.
	AudioConfigureJacks();

	xil_printf("ADAU1761 configured\n\r");

	int status;
	//----------------------------------------------------
	// INITIALIZE THE PERIPHERALS & SET DIRECTIONS OF GPIO
	//----------------------------------------------------

	// malloc()
	int sampling_rate = 48000;
	int seconds = 5;
	u32* left_buffer = malloc(sampling_rate*seconds*sizeof(u32));
//	memset(left_buffer, 0, sampling_rate*seconds*sizeof(u32));
	u32* right_buffer = malloc(sampling_rate*seconds*sizeof(u32));
//	memset(right_buffer, 0, sampling_rate*seconds*sizeof(u32));
	if (left_buffer == NULL || right_buffer == NULL)
		return (-1);

	while (1) {
		while(BUTTON_INTR_FLAG == 0){
		}

		BUTTON_INTR_FLAG = 0;

		if (btn_value == 4) {
//			record_audio(left_buffer, right_buffer, sampling_rate, seconds);

			memset(left_buffer, 0, sampling_rate*seconds*sizeof(u32));
			memset(right_buffer, 0, sampling_rate*seconds*sizeof(u32));
			for (int i = 0; i < sampling_rate*seconds; i++) {
				left_buffer[i] = Xil_In32(I2S_DATA_RX_L_REG);
				right_buffer[i] = Xil_In32(I2S_DATA_RX_R_REG);
				if (btn_value == 1)
					break;
				usleep(21);
			}
		}

		if (btn_value == 8) {
//			play_audio(left_buffer, right_buffer, sampling_rate, seconds);

			for (int i = 0; i < sampling_rate*seconds; i++) {
				Xil_Out32(I2S_DATA_TX_L_REG, left_buffer[i]);
				Xil_Out32(I2S_DATA_TX_R_REG, right_buffer[i]);
				usleep(21);
			}
		}

		if (btn_value == 16) {
//			play_audio(left_buffer, right_buffer, sampling_rate, seconds);

			for (int i = 0; i < sampling_rate*seconds; i++) {
				Xil_Out32(I2S_DATA_TX_L_REG, left_buffer[i]);
				Xil_Out32(I2S_DATA_TX_R_REG, right_buffer[i]);
				usleep(11);
			}
		}

		if (btn_value == 2) {
//			play_audio(left_buffer, right_buffer, sampling_rate, seconds);

			for (int i = 0; i < sampling_rate*seconds; i++) {
				Xil_Out32(I2S_DATA_TX_L_REG, left_buffer[i]);
				Xil_Out32(I2S_DATA_TX_R_REG, right_buffer[i]);
				usleep(42);
			}
		}

	}

	free(left_buffer);
	free(right_buffer);

    return 1;
}

