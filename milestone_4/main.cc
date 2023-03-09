#include "xparameters.h"
#include "xgpio.h"
#include "xscugic.h"
//#include "xtmrctr.h"
#include "xuartps.h"
#include "xil_exception.h"
#include "xil_printf.h"
#include "xil_cache.h"
#include "xil_io.h"
#include "xil_types.h"
#include <sleep.h>
#include <cstdlib>
#include "platform.h"
#include "platform_config.h"
#include "ethernet.h"

// Definitions, constants, global variables
#define INTC_DEVICE_ID 			XPAR_PS7_SCUGIC_0_DEVICE_ID
#define BTNS_DEVICE_ID			XPAR_AXI_GPIO_0_DEVICE_ID
#define INTC_GPIO_INTERRUPT_ID 	XPAR_FABRIC_AXI_GPIO_0_IP2INTC_IRPT_INTR
#define BTN_INT 				XGPIO_IR_CH1_MASK

#define BOARD_SIZE				10
#define NUM_COORDS				5
#define NUM_SHIPS				5

#define CARRIER					5
#define BATTLESHIP				4
#define CRUISER					3
#define SUBMARINE				3
#define DESTROYER				2
#define EMPTY					0

#define NUM_BYTES_BUFFER		5242880
#define RED						0xFF
#define GREEN	 				0xFF00
#define BLUE 					0xFF0000
#define WHITE	 				0xFFFFFF
#define BLACK	 				0x000000

#define BTN_DELAY				200000


XGpio BTN_INST;
XScuGic INTC_INST;
volatile bool BTN_INTR_FLAG;
static int BTN_VAL;

int * image_buffer_pointer 	= (int *)0x00900000;
int * main_menu 			= (int *)0x018D2008;
int * options 				= (int *)0x020BB00C;
int * board			 		= (int *)0x028A4010;
int * image4_pointer 		= (int *)0x0308D014;
int * image5_pointer 		= (int *)0x03876018;

int p1_score = 0;
int p2_score = 0;

char player;
int msg_received = 0;
int recv_x;
int recv_y;
char recv_res;
int playing_game = 0;

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

coord curr;



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
void setupP1ShipPlacement(ship* ships);
void setupShips(ship* ships, char* board);
void setupP2ShipPlacement(ship* ships);
int attackPos(ship* ships, coord coord);
coord getP1AttackPos();
coord getP2AttackPos();
coord getAttackPos();
bool isDestroyed(ship* ships);
void drawEmptyBoard();
void updateCrosshair(coord coords);
void drawBox(int offset, int color);
void drawSmallerBox(int offset, int color);

bool updateShip(coord coords, int color, int size, bool side, char* board);
void getShipPos(coord* coords, int size, char* board);


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

	// Initialize connection to secondary board
	status = initSecondaryBoard();
	if (status != XST_SUCCESS) return XST_FAILURE;

	// Initialize interrupt controller
	status = initIntcFunction(INTC_DEVICE_ID, &BTN_INST);
	if(status != XST_SUCCESS) return XST_FAILURE;

	// Initialize video
	initVGA();

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

	// ask for user input to determine player 1 or 2
	xil_printf("PLAYER 1 will wait for connection as master\n\r");
	xil_printf("PLAYER 2 will connect to the master zedboard\n\r");
	xil_printf("Enter '1' or '2'\n\r");
	int quit = 0;
	while(!quit) {
		while (!XUartPs_IsReceiveData(XPS_UART1_BASEADDR));
		player = XUartPs_ReadReg(XPS_UART1_BASEADDR, XUARTPS_FIFO_OFFSET);
		switch(player){
		case '1':
			quit = 1;
			break;
		case '2':
			quit = 1;
			break;
		default:
			xil_printf("Invalid input, enter '1' or '2'\n\r");
			break;
		} // switch
	}
	eth_init(player);

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

	// dow -data ../../../../Users/arodillo/ensc452/battleship/images/title.data 0x018D2008
	// dow -data ../../../../Users/arodillo/ensc452/battleship/images/board.data 0x028A4010

	Xil_DCacheFlush();
}

int displayMainMenu()
{
	memcpy(image_buffer_pointer, main_menu, NUM_BYTES_BUFFER);
	int cursor = 0;
	updateCursor(cursor);

	while(1) {
		eth_loop();
//		while(BTN_INTR_FLAG == false);
//		BTN_INTR_FLAG = false;

		if (BTN_INTR_FLAG == true)
		{
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

	// // Initialize board for player 1
	// char* p1_board = (char*) malloc(10*10*sizeof(char));
	// memset(p1_board, 0, 10*10*sizeof(char));

	// // Initialize board for player 2
	// char* p2_board = (char*) malloc(10*10*sizeof(char));
	// memset(p2_board, 0, 10*10*sizeof(char));

	
	char* game_board = (char*) malloc(BOARD_SIZE*BOARD_SIZE*sizeof(char));
	memset(game_board, 0, BOARD_SIZE*BOARD_SIZE*sizeof(char));

	memcpy(image_buffer_pointer, board, NUM_BYTES_BUFFER);
	Xil_DCacheFlush();

	// // Player 1 ships
	// ship* p1_ships = (ship*) malloc(5*sizeof(ship));
	// setupShips(p1_ships);
	// xil_printf("Player 1 places ships\r\n");


	// // Player 2 ships
	// ship* p2_ships = (ship*) malloc(5*sizeof(ship));
	// setupShips(p2_ships);
	// xil_printf("Player 2 places ships\r\n");

	// Place ships on board
	xil_printf("Placing ships...\r\n");
	ship* my_ships = (ship*) malloc(NUM_SHIPS*sizeof(ship));
	setupShips(my_ships, game_board);


	// Take turns shooting
	bool game_end = false;
	playing_game = 1;
	curr.x = 0;
	curr.y = 0;
	drawEmptyBoard();
	while (!game_end) {

		// Player 1 moves first
		if (player == '1') {
			xil_printf("Player 1's turn\r\n\r\n");

			// select target
			coord target = getAttackPos();

			// send target to player 2
			send_coords(target.x, target.y);
			drawBox(147 + 13*1280 + curr.x*100 + 1280*100*curr.y, BLACK);

			// wait for result of move
			while (!msg_received)
				eth_loop();
			msg_received = 0;

			switch(recv_res) {
			case '1':
				// Player 1 wins
				winner = 1;
				game_end = true;
				continue;
			case '2':
				// Player 2 wins
				winner = 2;
				game_end = true;
				continue;
			case 'e':
				// Empty square
				drawSmallerBox(149 + 15*1280 + curr.x*100 + 1280*100*curr.y, GREEN);
				break;
			case 'h':
				// Ship was hit
				drawSmallerBox(149 + 15*1280 + curr.x*100 + 1280*100*curr.y, RED);
				break;
			} // switch

			// player 2's turn
			xil_printf("Player 2's turn\r\n\r\n");

			// wait for message containing player 2's target
			while (!msg_received)
				eth_loop();
			msg_received = 0;
			
			// process player 2's move
			target.x = recv_x;
			target.y = recv_y;
			int res = attackPos(my_ships, target);

			// send result to player 2
			if (isDestroyed(my_ships)) {
				winner = 2;
				send_result('2');
				break;
			} else {
				switch(res) {
				case 0:
					send_result('e');
					break;
				case 1:
					send_result('h');
					break;
				} // switch
			}

			// player 1's turn (loop back)

		} else if (player == '2') {
			xil_printf("Player 1's turn\r\n\r\n");

			// wait for message containing player 1's target
			while (!msg_received)
				eth_loop();
			msg_received = 0;
			
			// process player 1's move
			coord target;
			target.x = recv_x;
			target.y = recv_y;
			int res = attackPos(my_ships, target);

			// send result to player 1
			if (isDestroyed(my_ships)) {
				winner = 1;
				send_result('1');
				break;
			} else {
				switch(res) {
				case 0:
					send_result('e');
					break;
				case 1:
					send_result('h');
					break;
				} // switch
			}

			// player 2's turn
			xil_printf("Player 2's turn\r\n\r\n");

			// select target
			target = getAttackPos();

			// send target to player 1
			send_coords(target.x, target.y);
			drawBox(147 + 13*1280 + curr.x*100 + 1280*100*curr.y, BLACK);

			// wait for result of move
			while (!msg_received)
				eth_loop();
			msg_received = 0;

			switch(recv_res) {
			case '1':
				// Player 1 wins
				winner = 1;
				game_end = true;
				continue;
			case '2':
				// Player 2 wins
				winner = 2;
				game_end = true;
				continue;
			case 'e':
				// Empty square
				drawSmallerBox(149 + 15*1280 + curr.x*100 + 1280*100*curr.y, GREEN);
				break;
			case 'h':
				// Ship was hit
				drawSmallerBox(149 + 15*1280 + curr.x*100 + 1280*100*curr.y, RED);
				break;
			}

			// player 1's turn (loop back)

		}

	}
	playing_game = 0;

	// Game ends, go back to main menu
	// free(p1_board);
	// free(p2_board);
	// free(p1_ships);
	// free(p2_ships);
	free(game_board);
	free(my_ships);
	return winner;
}

void setupShips(ship* ships, char* board)
{
	// use buttons to get placements (coordinates)
	// set into placement for easier format, then write to board for display
	int types[5] = {CARRIER, BATTLESHIP, CRUISER, SUBMARINE, DESTROYER};
	for(int i = 0; i < NUM_SHIPS; i++){
		ships[i].is_destroyed = false;
		ships[i].type = types[i];
		ships[i].lives = types[i];
		for(int j = 0; j < NUM_COORDS; j++){
			ships[i].coords[j].x = -1;
			ships[i].coords[j].y = -1;
		}
		getShipPos(ships[i].coords, ships[i].type, board);
	}
}

int attackPos(ship* ships, coord coord)
{
	// check if attack coord is occupied
	// update values

	xil_printf("%d, %d is attacked\r\n", coord.x, coord.y);
	for (int i = 0; i < NUM_SHIPS; i++)
	{
		if (ships[i].is_destroyed)
			continue;

		int ship_type = ships[i].type;
		for (int j = 0; j < ship_type; j++)
		{
			if ((ships[i].coords[j].x == coord.x) && (ships[i].coords[j].y == coord.y))
			{
				ships[i].lives--;
				ships[i].coords[j].x = -1;
				ships[i].coords[j].y = -1;
				if (ships[i].lives == 0)
				{
					ships[i].is_destroyed = true;
					xil_printf("A ship has been destroyed!\n\r");
				}
				else
				{
					xil_printf("A ship has been hit\n\r");
				}
				
				return 1;
			}
		}
	}

	return 0;
}

coord getAttackPos()
{
	coord coords;
	coords.x = 0;
	coords.y = 0;
	updateCrosshair(coords);
	while(1) {
		eth_loop();

		if (BTN_INTR_FLAG)
		{
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
	}
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

void drawEmptyBoard()
{
	for(int i = 0; i < 10; i++){
		for(int j = 0; j < 10; j++){
			drawBox(147 + 13*1280 + i*100 + 1280*100*j, BLACK);
		}
	}
}

void updateCrosshair(coord coords)
{
//	for(int i = 0; i < 10; i++){
//		for(int j = 0; j < 10; j++){
//			drawBox(147 + 13*1280 + i*100 + 1280*100*j, BLACK);
//		}
//	}
	drawBox(147 + 13*1280 + curr.x*100 + 1280*100*curr.y, BLACK);
	drawBox(147 + 13*1280 + coords.x*100 + 1280*100*coords.y, RED);
	usleep(BTN_DELAY);
	curr.x = coords.x;
	curr.y = coords.y;
}

void drawBox(int offset, int color)
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
	Xil_DCacheFlush();
}

void drawSmallerBox(int offset, int color)
{
	for(int i = 0; i < 90; i++){
		image_buffer_pointer[offset + i] = color;
		image_buffer_pointer[offset + 1280 + i] = color;
		image_buffer_pointer[offset + 1280*2 + i] = color;
		image_buffer_pointer[offset + 88*1280 + i] = color;
		image_buffer_pointer[offset + 89*1280 + i] = color;
		image_buffer_pointer[offset + 90*1280 + i] = color;
	}

	for(int i = 0; i < 90; i++){
		image_buffer_pointer[offset + 1280*i] = color;
		image_buffer_pointer[offset + 1 + 1280*i] = color;
		image_buffer_pointer[offset + 2 + 1280*i] = color;
		image_buffer_pointer[offset + 88 + 1280*i] = color;
		image_buffer_pointer[offset + 89 + 1280*i] = color;
		image_buffer_pointer[offset + 90 + 1280*i] = color;
	}
	Xil_DCacheFlush();
}

bool updateShip(coord* coords, int color, int size, bool side, char* board)
{
	bool ret = true;
	int valid_color = color;
	for(int i = 0; i < 10; i++){
		for(int j = 0; j < 10; j++){
			drawBox(147 + 13*1280 + i*100 + 1280*100*j, BLACK);
		}
	}

	for(int i = 0; i < 100; i++){
		if(board[i] != 0){
			drawBox(147 + 13*1280 + (i%10)*100 + 1280*100*(i/10), BLUE);
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
			drawBox(147 + 13*1280 + coords[0].x*100 + 1280*100*coords[0].y, valid_color);
			drawBox(147 + 13*1280 + (x)*100 + 1280*100*coords[0].y, valid_color);
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
			drawBox(147 + 13*1280 + coords[0].x*100 + 1280*100*coords[0].y, valid_color);
			drawBox(147 + 13*1280 + (coords[0].x)*100 + 1280*100*y, valid_color);
		}
		usleep(BTN_DELAY);
	}
	return ret;
}

void getShipPos(coord* coords, int size, char* board)
{
	// get input from player 1
	bool side = true;
	bool valid = true;
	coords[0].x = 0;
	coords[0].y = 0;
	valid = updateShip(coords, GREEN, size, side, board);
	while(1) {
			while(BTN_INTR_FLAG == false){
				u8 inp = 0x00;
				if (XUartPs_IsReceiveData(XPS_UART1_BASEADDR) && valid){
					inp = XUartPs_ReadReg(XPS_UART1_BASEADDR, XUARTPS_FIFO_OFFSET);
					if (inp == 'a'){
						for(int i = 0; i < size; i++){
							xil_printf("%d, %d \r\n" ,coords[i].x, coords[i].y);
							board[10*coords[i].y + coords[i].x] = 1;
						}
						xil_printf("\r\n");
						return;
					}
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
