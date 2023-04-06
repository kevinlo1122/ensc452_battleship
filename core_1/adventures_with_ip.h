/*
 * adventures_with_ip.h
 *
 * Main header file.
 */

#ifndef ADVENTURES_WITH_IP_H_
#define ADVENTURES_WITH_IP_H_

/* ---------------------------------------------------------------------------- *
 * 								Header Files									*
 * ---------------------------------------------------------------------------- */
#include <stdio.h>
#include <xil_io.h>
#include <sleep.h>
#include "xiicps.h"
#include <xil_printf.h>
#include <xparameters.h>
#include "xgpio.h"
#include "xuartps.h"
#include "stdlib.h"
#include "xscugic.h"
#include "xil_exception.h"
#include "xil_types.h"

/* ---------------------------------------------------------------------------- *
 * 							Custom IP Header Files								*
 * ---------------------------------------------------------------------------- */
#include "audio.h"

/* ---------------------------------------------------------------------------- *
 * 							Prototype Functions									*
 * ---------------------------------------------------------------------------- */

/* ---------------------------------------------------------------------------- *
 * 						Redefinitions from xparameters.h 						*
 * ---------------------------------------------------------------------------- */
#define UART_BASEADDR XPAR_PS7_UART_1_BASEADDR

//#define BUTTON_SWITCH_BASE 	XPAR_GPIO_1_BASEADDR
//#define LED_BASE 			XPAR_LED_CONTROLLER_0_S00_AXI_BASEADDR
//#define BUTTON_SWITCH_ID 	XPAR_GPIO_1_DEVICE_ID

/* ---------------------------------------------------------------------------- *
 * 							Define GPIO Channels								*
 * ---------------------------------------------------------------------------- */
//#define BUTTON_CHANNEL 1
//#define SWITCH_CHANNEL 2

/* ---------------------------------------------------------------------------- *
 * 							Audio Scaling Factor								*
 * ---------------------------------------------------------------------------- */
#define SCALE 6

/* ---------------------------------------------------------------------------- *
 * 							Global Variables									*
 * ---------------------------------------------------------------------------- */
XIicPs Iic;
XGpio Gpio; // Gpio instance for buttons and switches


#endif /* ADVENTURES_WITH_IP_H_ */
