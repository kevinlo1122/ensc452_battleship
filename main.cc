#include "xparameters.h"
#include "xgpio.h"
#include "xscugic.h"
#include "xtmrctr.h"
#include "xuartps.h"
#include "xil_exception.h"
#include "xil_printf.h"
#include "xil_cache.h"
#include "xil_io.h"
#include "xil_types.h"
#include <stdlib.h>
#include <time.h>
#include <sleep.h>
#include <cstdlib>
#include <xgpiops.h>

// Definitions, constants, global variables
#define INTC_DEVICE_ID 			XPAR_PS7_SCUGIC_0_DEVICE_ID
#define BTNS_DEVICE_ID			XPAR_AXI_GPIO_0_DEVICE_ID
#define INTC_GPIO_INTERRUPT_ID 	XPAR_FABRIC_AXI_GPIO_0_IP2INTC_IRPT_INTR
#define BTN_INT 				XGPIO_IR_CH1_MASK

#define pbsw 51

#define NUM_SHIPS				5
#define CARRIER					5
#define BATTLESHIP				4
#define CRUISER					3
#define SUBMARINE				3
#define DESTROYER				2
#define EMPTY					0

#define NUM_BYTES_BUFFER		5242880
#define RED						0x003333FF
#define GREEN	 				0x0000FF00
#define BLUE 					0x00FFBB00
#define WHITE	 				0x00FFFFFF
#define BLACK	 				0xFF000000

#define BTN_DELAY				200000

XGpio BTN_INST;
XScuGic INTC_INST;
volatile bool BTN_INTR_FLAG;
static int BTN_VAL;
long unsigned int sw;
XGpioPs Gpio;
XGpioPs_Config *GPIOConfigPtr;

int * image_buffer_pointer 	= (int *)0x00900000;
int * main_menu 			= (int *)0x018D2008;
int * options 				= (int *)0x020BB00C;
int * board			 		= (int *)0x028A4010;
int * sprites		 		= (int *)0x0308D014;
int * confetti		 		= (int *)0x03876018;

int p1_score = 0;
int p2_score = 0;

// structs
struct coord{
	int x;
	int y;
};
struct ship{
	int type;
	int lives;
	bool is_destroyed;
	coord coords[5];
};


//----------------------------------------------------
// FUNCTION PROTOTYPES
//----------------------------------------------------
// setup
void initUART();
int initInterruptSystemSetup(XScuGic *XScuGicInstancePtr);
int initIntcFunction(u16 DeviceId, XGpio *GpioInstancePtr);
int initSecondaryBoard();
void initVGA();

// inputs
void buttonInterruptHandler(void *baseaddr_p);

// outputs
void updateCursor(int cursor);

// menu
int displayMainMenu();
void displayOptionsMenu();
int playGame();

// other
void setupP1ShipPlacement(ship* ships, char* board);
void setupP2ShipPlacement(ship* ships);
void attackPos(ship* ships, coord coord);
coord getP1AttackPos();
coord getP2AttackPos();
bool isDestroyed(ship* ships);
void updateCrosshair(coord coords);
void drawBox(int offset, int color, bool cross);

bool updateShip(coord coords, int color, int size, bool side, char* board);
void getP1ShipPos(coord* coords, int size, char* board);
void drawExplosion(coord coords);
void drawMiss(coord coords);
void drawHit(coord coords);
void drawSinkingShip(ship ship);
void drawConfetti();
void drawVictory();
void drawDefeat();



//----------------------------------------------------
// MAIN FUNCTION
//----------------------------------------------------
int main (void)
{
	// Begin initialization
	int status = 0;

	// Initialize UART
	initUART();

	// Initialize Push Buttons
	status = XGpio_Initialize(&BTN_INST, BTNS_DEVICE_ID);
	if(status != XST_SUCCESS) return XST_FAILURE;
	// Set all buttons direction to inputs
	XGpio_SetDataDirection(&BTN_INST, 1, 0xFF);

	GPIOConfigPtr = XGpioPs_LookupConfig(XPAR_PS7_GPIO_0_DEVICE_ID);
	status = XGpioPs_CfgInitialize(&Gpio, GPIOConfigPtr, GPIOConfigPtr ->BaseAddr);
	if (status != XST_SUCCESS) return XST_FAILURE;
    XGpioPs_SetDirectionPin(&Gpio, pbsw, 0x0);

	// Initialize connection to secondary board
	status = initSecondaryBoard();
	if (status != XST_SUCCESS) return XST_FAILURE;

	// Initialize interrupt controller
	status = initIntcFunction(INTC_DEVICE_ID, &BTN_INST);
	if(status != XST_SUCCESS) return XST_FAILURE;

	// Initialize video
	initVGA();

	xil_printf("\r\n\r\n");

	// Begin actual game
	int choice = 0;
	int winner = 0;

	while(true){
		choice = displayMainMenu();
		switch(choice){
			case 0:			// start
				winner = playGame();
				(winner == 1) ? p1_score++ : p2_score++;
				break;
			case 1:			// options
				displayOptionsMenu();
				break;
			case 2:			// quit
				return 0;
		}
	}
	return 0;
}

//----------------------------------------------------
// FUNCTION DEFINITIONS
//----------------------------------------------------

void initUART()
{
	u32 CntrlRegister;
	CntrlRegister = XUartPs_ReadReg(XPS_UART1_BASEADDR, XUARTPS_CR_OFFSET);
	XUartPs_WriteReg(XPS_UART1_BASEADDR, XUARTPS_CR_OFFSET,
				  ((CntrlRegister & ~XUARTPS_CR_EN_DIS_MASK) |
				   XUARTPS_CR_TX_EN | XUARTPS_CR_RX_EN));
}

int initInterruptSystemSetup(XScuGic *XScuGicInstancePtr)
{
	// Enable interrupt
	XGpio_InterruptEnable(&BTN_INST, BTN_INT);
	XGpio_InterruptGlobalEnable(&BTN_INST);

	Xil_ExceptionRegisterHandler(XIL_EXCEPTION_ID_INT,
			 	 	 	 	 	 (Xil_ExceptionHandler)XScuGic_InterruptHandler,
			 	 	 	 	 	 XScuGicInstancePtr);
	Xil_ExceptionEnable();


	return XST_SUCCESS;

}

int initIntcFunction(u16 DeviceId, XGpio *GpioInstancePtr)
{
	XScuGic_Config *IntcConfig;
	int status;

	// Interrupt controller initialization
	IntcConfig = XScuGic_LookupConfig(DeviceId);
	status = XScuGic_CfgInitialize(&INTC_INST, IntcConfig, IntcConfig->CpuBaseAddress);
	if(status != XST_SUCCESS) return XST_FAILURE;

	// Call to interrupt setup
	status = initInterruptSystemSetup(&INTC_INST);
	if(status != XST_SUCCESS) return XST_FAILURE;

	// Connect GPIO interrupt to handler
	status = XScuGic_Connect(&INTC_INST,
					  	  	 INTC_GPIO_INTERRUPT_ID,
					  	  	 (Xil_ExceptionHandler)buttonInterruptHandler,
					  	  	 (void *)GpioInstancePtr);
	if(status != XST_SUCCESS) return XST_FAILURE;

	// Enable GPIO interrupts interrupt
	XGpio_InterruptEnable(GpioInstancePtr, 1);
	XGpio_InterruptGlobalEnable(GpioInstancePtr);

	// Enable GPIO and timer interrupts in the controller
	XScuGic_Enable(&INTC_INST, INTC_GPIO_INTERRUPT_ID);

	return XST_SUCCESS;
}

void buttonInterruptHandler(void *InstancePtr)
{
	// Disable GPIO interrupts
	XGpio_InterruptDisable(&BTN_INST, BTN_INT);
	// Ignore additional button presses
	if ((XGpio_InterruptGetStatus(&BTN_INST) & BTN_INT) !=
			BTN_INT) {
			return;
		}
	BTN_VAL = XGpio_DiscreteRead(&BTN_INST, 1);
	BTN_INTR_FLAG = true;
	// Increment counter based on button value
    (void)XGpio_InterruptClear(&BTN_INST, BTN_INT);
    // Enable GPIO interrupts
    XGpio_InterruptEnable(&BTN_INST, BTN_INT);
}

int initSecondaryBoard()
{
	// Connect to secondary board and do all setup
	// Returns sucess or fail
	return XST_SUCCESS;
}

void initVGA()
{
	memset(image_buffer_pointer, 0, NUM_BYTES_BUFFER);
	// dow -data ../../../../Pictures/title.data 0x018D2008
	// dow -data ../../../../Pictures/options.data 0x020BB00C
	// dow -data ../../../../Pictures/board.data 0x028A4010
	// dow -data ../../../../Pictures/sprites.data 0x0308D014
	// dow -data ../../../../Pictures/victory.data 0x03876018
	Xil_DCacheFlush();
}

int displayMainMenu()
{
	memcpy(image_buffer_pointer, main_menu, NUM_BYTES_BUFFER);
	int cursor = 0;
	updateCursor(cursor);

	while(1) {
		while(BTN_INTR_FLAG == false);
		BTN_INTR_FLAG = false;

		if (BTN_VAL == 16){ 		// up
			(cursor == 0) ? cursor = 2 : cursor--;
			updateCursor(cursor);
		}
		else if (BTN_VAL == 2){ 	// down
			cursor = (cursor + 1) % 3;
			updateCursor(cursor);
		}
		else if (BTN_VAL == 1){		// center
			usleep(BTN_DELAY);
			return cursor;
		}
	}

	return 0;
}

void displayOptionsMenu()
{
	memcpy(image_buffer_pointer, options, NUM_BYTES_BUFFER);
	int cursor = 0;
	updateCursor(cursor);

	while(1) {
		while(BTN_INTR_FLAG == false);
		BTN_INTR_FLAG = false;

		if (BTN_VAL == 16){ 		// up
			(cursor == 0) ? cursor = 2 : cursor--;
			updateCursor(cursor);
		}
		else if (BTN_VAL == 2){ 	// down
			cursor = (cursor + 1) % 3;
			updateCursor(cursor);
		}
		else if (BTN_VAL == 1){		// center
			usleep(BTN_DELAY);
			if(cursor == 0){				// reset
				p1_score = 0;
				p2_score = 0;
			}
			else if (cursor == 1){			// sound
				continue;
			}
			else if(cursor == 2){			// back
				return;
			}
		}
	}
}

int playGame()
{
	// returns winner for score keeping later, 1 for player 1, 2 for player 2
	int winner = 0;

	// Initialize board for player 1
	char* p1_board = (char*) malloc(10*10*sizeof(char));
	memset(p1_board, 0, 10*10*sizeof(char));

	// Initialize board for player 2
	char* p2_board = (char*) malloc(10*10*sizeof(char));
	memset(p2_board, 0, 10*10*sizeof(char));
	memcpy(image_buffer_pointer, board, NUM_BYTES_BUFFER);

	// Player 1 ships
	ship* p1_ships = (ship*) malloc(5*sizeof(ship));
	setupP1ShipPlacement(p1_ships, p1_board);
	xil_printf("Player 1 places ships\r\n");


	// Player 2 ships
	ship* p2_ships = (ship*) malloc(5*sizeof(ship));
	setupP2ShipPlacement(p2_ships);
	xil_printf("Player 2 places ships\r\n");


	// Take turns shooting
	bool game_end = false;
	while (!game_end){
		// player 1 attack
		xil_printf("Player 1's turn\r\n");
		attackPos(p1_ships, getP1AttackPos());
		if (isDestroyed(p1_ships)){
			drawVictory();
			winner = 1;
			game_end = true;
		}


		// player 2 attack
		xil_printf("Player 2's turn\r\n");
		sleep(1);
//		attackPos(p1_ships, getP2AttackPos());
//		if (isDestroyed(p1_ships)){
//			winner = 2;
//			game_end = true;
//		}

//		// force end the game
//		u8 inp = 0x00;
//		if (XUartPs_IsReceiveData(XPS_UART1_BASEADDR)){
//			inp = XUartPs_ReadReg(XPS_UART1_BASEADDR, XUARTPS_FIFO_OFFSET);
//			switch(inp){
//				case '1':
//					for(int i = 0; i < NUM_SHIPS; i++){
//						p2_ships[i].is_destroyed = true;
//					}
//					break;
//				case '2':
//					for(int i = 0; i < NUM_SHIPS; i++){
//						p1_ships[i].is_destroyed = true;
//					}
//					break;
//				default:
//					sleep(1);
//					continue;
//			}
//		}
	}

	// Game ends, go back to main menu
	free(p1_board);
	free(p2_board);
	free(p1_ships);
	free(p2_ships);
	return winner;
}

void setupP1ShipPlacement(ship* ships, char* board)
{
	// use buttons to get placements (coordinates)
	// set into placement for easier format, then write to board for display
	int types[5] = {CARRIER, BATTLESHIP, CRUISER, SUBMARINE, DESTROYER};
	for(int i = 0; i < NUM_SHIPS; i++){
		ships[i].is_destroyed = false;
		ships[i].type = types[i];
		ships[i].lives = types[i];
		for(int j = 0; j < 5; j++){
			ships[i].coords[j].x = -1;
			ships[i].coords[j].y = -1;
		}
		getP1ShipPos(ships[i].coords, ships[i].type, board);
	}
	for(int i = 0; i < 10; i++){
		for(int j = 0; j < 10; j++){
			drawBox(147 + 13*1280 + i*100 + 1280*100*j, BLACK, true);
		}
	}
}

void setupP2ShipPlacement(ship* ships)
{
	// get placements from player 2 board (coordinates)
	// set into placement for easier format, then write to board for display
	int types[5] = {CARRIER, BATTLESHIP, CRUISER, SUBMARINE, DESTROYER};
	for(int i = 0; i < NUM_SHIPS; i++){
		ships[i].is_destroyed = false;
		ships[i].type = types[i];
		for(int j = 0; j < 5; j++){
			ships[i].coords[j].x = -1;
			ships[i].coords[j].y = -1;
		}
	}
}

void attackPos(ship* ships, coord coord)
{
	// check if attack coord is occupied
	// update values
	xil_printf("%d, %d is attacked\r\n", coord.x, coord.y);
	drawExplosion(coord);
	bool hit = false;
	for(int i = 0; i < NUM_SHIPS; i++){
		for(int j = 0; j < 5; j++){
			if (ships[i].coords[j].x == coord.x && ships[i].coords[j].y == coord.y){
				xil_printf("HIT\r\n");
				hit = true;
				ships[i].lives--;
				if (ships[i].lives <= 0)
				{
					ships[i].is_destroyed = true;
					xil_printf("A ship has been destroyed!\n\r");
					drawHit(coord);
					drawSinkingShip(ships[i]);
					return;

				}
			}
		}
	}
	if(hit){
		drawHit(coord);
	}
	else{
		drawMiss(coord);
	}
}

coord getP1AttackPos()
{
	// get input from player 1
	// return coords
	coord coords;
	coords.x = 0;
	coords.y = 0;
	updateCrosshair(coords);
	while(1) {
			while(BTN_INTR_FLAG == false);
			BTN_INTR_FLAG = false;

			if (BTN_VAL == 16){ 		// up
				(coords.y == 0) ? coords.y = 9 : coords.y--;
				updateCrosshair(coords);
			}
			else if (BTN_VAL == 2){ 	// down
				coords.y = (coords.y + 1) % 10;
				updateCrosshair(coords);
			}
			else if (BTN_VAL == 4){		// left
				(coords.x == 0) ? coords.x = 9 : coords.x--;
				updateCrosshair(coords);
			}
			else if (BTN_VAL == 8){		// right
				coords.x = (coords.x + 1) % 10;
				updateCrosshair(coords);
			}
			else if (BTN_VAL == 1){		// centre
				return coords;
			}
		}
	return coords;
}

coord getP2AttackPos()
{
	// get input from player 2
	// return coords
	coord coords;
	coords.x = 0;
	coords.y = 0;
	return coords;
}

bool isDestroyed(ship* ships)
{
	// return true if all ships are destroyed
	for(int i = 0; i < NUM_SHIPS; i++){
		if(!ships[i].is_destroyed){
			return false;
		}
	}
	return true;
}

void updateCursor(int cursor)
{
	// displays arrows for options menu, hardcoded for 3 choices for now
	for(int k = 0; k < 3; k++){
		for(int j = 0; j < 12; j++){
			for(int i = 0; i < 12-j; i++){
				image_buffer_pointer[(640+i+k*100)*1280 + 500+j] = BLACK;
			}
			for(int i = 0; i < 12-j; i++){
				image_buffer_pointer[(640-i+k*100)*1280 + 500+j] = BLACK;
			}
		}
	}
	for(int j = 0; j < 12; j++){
		for(int i = 0; i < 12-j; i++){
			image_buffer_pointer[(640+i+cursor*100)*1280 + 500+j] = RED;
		}
		for(int i = 0; i < 12-j; i++){
			image_buffer_pointer[(640-i+cursor*100)*1280 + 500+j] = RED;
		}
	}
	Xil_DCacheFlush();
	usleep(BTN_DELAY);
}

void updateCrosshair(coord coords)
{
	for(int i = 0; i < 10; i++){
		for(int j = 0; j < 10; j++){
			drawBox(147 + 13*1280 + i*100 + 1280*100*j, BLACK, false);
		}
	}
	drawBox(147 + 13*1280 + coords.x*100 + 1280*100*coords.y, RED, false);
	usleep(BTN_DELAY);
}

void drawBox(int offset, int color, bool cross)
{
	for(int i = 0; i < 94; i++){
		image_buffer_pointer[offset + i] = color;
		image_buffer_pointer[offset + 1280 + i] = color;
		image_buffer_pointer[offset + 93*1280 + i] = color;
		image_buffer_pointer[offset + 94*1280 + i] = color;
	}

	for(int i = 0; i < 94; i++){
		image_buffer_pointer[offset + 1280*i] = color;
		image_buffer_pointer[offset + 1 + 1280*i] = color;
		image_buffer_pointer[offset + 93 + 1280*i] = color;
		image_buffer_pointer[offset + 94 + 1280*i] = color;
	}

	if(cross){
		for(int i = 0; i < 94; i++){
			image_buffer_pointer[offset + i*1280 + i] = color;
			image_buffer_pointer[offset + 94*1280 - i*1280 + i] = color;
		}
	}
	Xil_DCacheFlush();
}

bool updateShip(coord* coords, int color, int size, bool side, char* board)
{
	bool ret = true;
	int valid_color = color;
	for(int i = 0; i < 10; i++){
		for(int j = 0; j < 10; j++){
			drawBox(147 + 13*1280 + i*100 + 1280*100*j, BLACK, true);
		}
	}

	for(int i = 0; i < 100; i++){
		if(board[i] != 0){
			drawBox(147 + 13*1280 + (i%10)*100 + 1280*100*(i/10), BLUE, true);
		}
	}
	int x = 0;
	int y = 0;
	int z = 0;

	if (side){
		for(int i = 0; i < size; i++){
			z = coords[0].x + i;
			if (z > 9  || (board[z+10*coords[0].y] != 0)){
				ret = false;
				valid_color = RED;
			}
		}

		for(int i = 1; i < size; i++){
			x = coords[0].x + i;
			x = x % 10;
			coords[i].x = x;
			coords[i].y = coords[0].y;
			drawBox(147 + 13*1280 + coords[0].x*100 + 1280*100*coords[0].y, valid_color, true);
			drawBox(147 + 13*1280 + (x)*100 + 1280*100*coords[0].y, valid_color, true);
		}
		usleep(BTN_DELAY);
	}
	else{
		for(int i = 0; i < size; i++){
			z = coords[0].y + i;
			if (z > 9 || (board[coords[0].x+10*z] != 0)){
				ret = false;
				valid_color = RED;
			}
		}
		for(int i = 1; i < size; i++){
			y = coords[0].y + i;
			y = y % 10;
			coords[i].y = y;
			coords[i].x = coords[x].x;
			drawBox(147 + 13*1280 + coords[0].x*100 + 1280*100*coords[0].y, valid_color, true);
			drawBox(147 + 13*1280 + (coords[0].x)*100 + 1280*100*y, valid_color, true);
		}
		usleep(BTN_DELAY);
	}
	return ret;
}

void getP1ShipPos(coord* coords, int size, char* board)
{
	// get input from player 1
	bool side = true;
	bool valid = true;
	coords[0].x = 0;
	coords[0].y = 0;
	valid = updateShip(coords, GREEN, size, side, board);
	while(1) {
			while(BTN_INTR_FLAG == false){
//				u8 inp = 0x00;
//				if (XUartPs_IsReceiveData(XPS_UART1_BASEADDR) && valid){
//					inp = XUartPs_ReadReg(XPS_UART1_BASEADDR, XUARTPS_FIFO_OFFSET);
//					if (inp == 'a'){
//						for(int i = 0; i < size; i++){
//							xil_printf("%d, %d \r\n" ,coords[i].x, coords[i].y);
//							board[10*coords[i].y + coords[i].x] = 1;
//						}
//						xil_printf("\r\n");
//						return;
//					}
//				}
		    	sw = XGpioPs_ReadPin(&Gpio, pbsw); //read pin
		    	if (sw == 1 && valid){
					for(int i = 0; i < size; i++){
						xil_printf("%d, %d \r\n" ,coords[i].x, coords[i].y);
						board[10*coords[i].y + coords[i].x] = 1;
					}
					xil_printf("\r\n");
					return;
		    	}
			}
			BTN_INTR_FLAG = false;

			if (BTN_VAL == 16){ 		// up
				(coords[0].y == 0) ? coords[0].y = 9 : coords[0].y--;
				valid = updateShip(coords, GREEN, size, side, board);
			}
			else if (BTN_VAL == 2){ 	// down
				coords[0].y = (coords[0].y + 1) % 10;
				valid = updateShip(coords, GREEN, size, side, board);
			}
			else if (BTN_VAL == 4){		// left
				(coords[0].x == 0) ? coords[0].x = 9 : coords[0].x--;
				valid = updateShip(coords, GREEN, size, side, board);
			}
			else if (BTN_VAL == 8){		// right
				coords[0].x = (coords[0].x + 1) % 10;
				valid = updateShip(coords, GREEN, size, side, board);
			}
			else if (BTN_VAL == 1){		// centre
				side = !side;
				valid = updateShip(coords, GREEN, size, side, board);
			}
		}
}

void drawExplosion(coord coords)
{
	int offset = 147 + 13*1280 + coords.x*100 + 1280*100*coords.y;
	for(int k = 0; k < 4; k++){
		for(int j = 0; j < 95; j++){
			for(int i = 0; i < 95; i++){
				image_buffer_pointer[offset+i+j*1280] = sprites[i+j*1280+95*k];
			}
		}
		Xil_DCacheFlush();
		usleep(85000);
	}
}

void drawMiss(coord coords)
{
	int offset = 147 + 13*1280 + coords.x*100 + 1280*100*coords.y;
	for(int j = 0; j < 95; j++){
		for(int i = 0; i < 95; i++){
			image_buffer_pointer[offset+i+j*1280] = sprites[i+j*1280+95*4];
		}
	}
	Xil_DCacheFlush();
}

void drawHit(coord coords)
{
	int rand_num = rand() % 2;
	int offset = 147 + 13*1280 + coords.x*100 + 1280*100*coords.y;
	for(int j = 0; j < 95; j++){
		for(int i = 0; i < 95; i++){
			image_buffer_pointer[offset+i+j*1280] = sprites[i+j*1280+95*(5+rand_num)];
		}
	}
	Xil_DCacheFlush();
}

void drawSinkingShip(ship ship)
{
	for(int i = 0; i < ship.type; i++){
		int offset = 147 + 13*1280 + ship.coords[i].x*100 + 1280*100*ship.coords[i].y;
		for(int k = 0; k < 5; k++){
			for(int j = 0; j < 95; j++){
				for(int i = 0; i < 95; i++){
					image_buffer_pointer[offset+i+j*1280] = sprites[i+j*1280+95*k+1280*95];
				}
			}
			Xil_DCacheFlush();
			usleep(85000);
		}
	}
}

void drawConfetti()
{
	for(int i = 0; i < NUM_BYTES_BUFFER / 4; ++i){
		if(confetti[i] != (int) BLACK){  // black?
			image_buffer_pointer[i] = confetti[i];
		}
	}
	Xil_DCacheFlush();
}

void drawVictory()
{
	drawConfetti();
	int offset = 10*1280 + 0*100 + 1280*100*1;
	int offset_sprite = 365 * 1280;
	for(int j = 0; j < 220; j++){
		for(int i = 0; i < 1235; i++){
			image_buffer_pointer[offset+i+j*1280] = sprites[offset_sprite+i+j*1280];
		}
	}
	Xil_DCacheFlush();
	sleep(1);
	while(1){
		while(BTN_INTR_FLAG == false);
		BTN_INTR_FLAG = false;
		if (BTN_VAL == 16 || BTN_VAL == 2 || BTN_VAL == 4 || BTN_VAL == 8 || BTN_VAL == 1){
			break;
		}
	}
}

void drawDefeat()
{
	int offset = 10*1280 + 0*100 + 1280*100*1;
	int offset_sprite = 185 * 1280;
	for(int j = 0; j < 170; j++){
		for(int i = 0; i < 1235; i++){
			image_buffer_pointer[offset+i+j*1280] = sprites[offset_sprite+i+j*1280];
		}
	}
	Xil_DCacheFlush();
	sleep(1);
	while(1){
		while(BTN_INTR_FLAG == false);
		BTN_INTR_FLAG = false;
		if (BTN_VAL == 16 || BTN_VAL == 2 || BTN_VAL == 4 || BTN_VAL == 8 || BTN_VAL == 1){
			break;
		}
	}
}



