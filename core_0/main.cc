#include <stdlib.h>
#include <cstdlib>
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
#include <xgpiops.h>
#include "platform.h"
#include "platform_config.h"
#include "ethernet.h"



//----------------------------------------------------
// DEFINITIONS AND GLOBALS
//----------------------------------------------------
#define FLAG 					(*(volatile u8*)(0x20000))
#define INTC_DEVICE_ID 			XPAR_PS7_SCUGIC_0_DEVICE_ID
#define BTNS_DEVICE_ID			XPAR_AXI_GPIO_0_DEVICE_ID
#define INTC_GPIO_INTERRUPT_ID 	XPAR_FABRIC_AXI_GPIO_0_IP2INTC_IRPT_INTR
#define BTN_INT 				XGPIO_IR_CH1_MASK
#define GPIO_PS_ID				XPAR_PS7_GPIO_0_DEVICE_ID
#define PBSW					51
#define BTN_DELAY				200000

#define NUM_BYTES_BUFFER		5242880
#define RED						0x003333FF
#define GREEN	 				0x0000FF00
#define BLUE 					0x00FFBB00
#define WHITE	 				0x00FFFFFF
#define BLACK	 				0xFF000000

#define NUM_SHIPS					5

#define SIZE_CARRIER				5
#define SIZE_BATTLESHIP				4
#define SIZE_CRUISER				3
#define SIZE_SUBMARINE				3
#define SIZE_DESTROYER  			2

#define ID_CARRIER					5
#define ID_BATTLESHIP				4
#define ID_CRUISER					3
#define ID_SUBMARINE			    2
#define ID_DESTROYER		    	1
#define ID_EMPTY			    	0

struct coord{
	int x;
	int y;
};
struct ship{
	int type;
	int size;
    int lives;
	bool is_destroyed;
	coord coords[5];
	coord hit_coord[5];
};

XGpioPs GPIO_PS;
XGpioPs_Config* GPIOConfigPtr;
XGpio BTN_INST;
XScuGic INTC_INST;
volatile bool BTN_INTR_FLAG;
static int BTN_VAL;
int PS_BTN_VAL;

int * image_buffer_pointer	= (int *)0x00900000;
int * main_menu				= (int *)0x018D2008;
int * options				= (int *)0x020BB00C;
int * board					= (int *)0x028A4010;
int * sprites				= (int *)0x0308D014;
int * confetti				= (int *)0x03876018;

char player = '0';
int msg_received = 0;
int playing_game = 0;

char recv_result;
int recv_x = 0;
int recv_y = 0;

coord curr;

char* my_placements;

//----------------------------------------------------
// FUNCTION PROTOTYPES
//----------------------------------------------------
// setup and hardware handlers
void initUART();
void initVGA();
void initEthernet();
void buttonInterruptHandler(void *baseaddr_p);
int initInterruptSystemSetup(XScuGic *XScuGicInstancePtr);
int initIntcFunction(u16 DeviceId, XGpio *GpioInstancePtr);

// game logic
void playSinglePlayerGame();
void playMultiplayerGame();
void setupShips(ship* ships, char* board);
void getShipPos(ship* ship, char* board);
bool updateShip(ship* ship, bool horizontal, char* board);
coord getAttackPos();
char attackPos(ship* ships, coord coord);
bool isDestroyed(ship* ships);
void send_attack(int* enemy_ships, coord* enemy_placement, bool* game_end);
void receive_attack(ship* my_ships);
void send_attack_offline(int* enemy_ships, coord* enemy_placement, bool* game_end, ship* my_ships);
void receive_attack_offline(ship* my_ships);

// display and animations
void updateCursorMainMenu(int cursor);
void updateCursorOptions(int cursor);
int displayMainMenu();
void displayOptionsMenu();
void displayPlayerSelection();
void drawBox(int offset, int color, bool cross);
void drawShipBox(ship ship, int color);
void updateCrosshair(coord coords);
void drawExplosion(coord coords);
void drawMiss(coord coords);
void drawHit(coord coords);
void drawSinkingShip(coord* coords, int type, int size);
void drawConfetti();
void drawVictory();
void drawDefeat();
void drawLives(int type);




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

void initEthernet()
{
	// Connect to other board
	eth_init(player);
	eth_loop();
	return;
}

void initVGA()
{
	memset(image_buffer_pointer, 0, NUM_BYTES_BUFFER);
	// dow -data ../../../../Pictures/title.data 0x018D2008
	// dow -data ../../../../Pictures/options.data 0x020BB00C
	// dow -data ../../../../Pictures/board.data 0x028A4010
	// dow -data ../../../../Pictures/sprites.data 0x0308D014
	// dow -data ../../../../Pictures/victory.data 0x03876018

	// dow -data ../../../../Users/arodillo/ensc452/battleship/images/title.data 0x018D2008
	// dow -data ../../../../Users/arodillo/ensc452/battleship/images/options.data 0x020BB00C
	// dow -data ../../../../Users/arodillo/ensc452/battleship/images/board.data 0x028A4010
	// dow -data ../../../../Users/arodillo/ensc452/battleship/images/sprites.data 0x0308D014
	// dow -data ../../../../Users/arodillo/ensc452/battleship/images/victory.data 0x03876018

	Xil_DCacheFlush();
}

int displayMainMenu()
{
	// display main menu options, singleplayer, multiplayer, options, quit
	memcpy(image_buffer_pointer, main_menu, NUM_BYTES_BUFFER);
	int cursor = 0;
	updateCursorMainMenu(cursor);

	while(true) {
		while(BTN_INTR_FLAG == false);
		BTN_INTR_FLAG = false;

		if (BTN_VAL == 16){ 		// up
			(cursor == 0) ? cursor = 3 : cursor--;
			updateCursorMainMenu(cursor);
		}
		else if (BTN_VAL == 2){ 	// down
			cursor = (cursor + 1) % 4;
			updateCursorMainMenu(cursor);
		}
		else if (BTN_VAL == 1){		// center
			for(int k = 0; k < 4; k++){
				for(int j = 0; j < 13; j++){
					for(int i = 0; i < 12-j; i++){
						image_buffer_pointer[(535+i+k*104)*1280 + 500+j] = BLACK;
					}
					for(int i = 0; i < 12-j; i++){
						image_buffer_pointer[(535-i+k*104)*1280 + 500+j] = BLACK;
					}
				}
			}
			Xil_DCacheFlush();
			usleep(BTN_DELAY);
			return cursor;
		}
	}

	return 0;
}

void displayOptionsMenu()
{
	// display options menu, just sound and back for now
	memcpy(image_buffer_pointer, options, NUM_BYTES_BUFFER);
	int cursor = 0;
	updateCursorOptions(cursor);

	while(true) {
		while(BTN_INTR_FLAG == false);
		BTN_INTR_FLAG = false;

		if (BTN_VAL == 16){ 		// up
			(cursor == 0) ? cursor = 1 : cursor--;
			updateCursorOptions(cursor);
		}
		else if (BTN_VAL == 2){ 	// down
			cursor = (cursor + 1) % 2;
			updateCursorOptions(cursor);
		}
		else if (BTN_VAL == 1){		// center
			usleep(BTN_DELAY);
			if(cursor == 0){				// sound
				continue;
			}
			else if (cursor == 1){			// back
				for(int k = 0; k < 2; k++){
					for(int j = 0; j < 12; j++){
						for(int i = 0; i < 12-j; i++){
							image_buffer_pointer[(740+i+k*104)*1280 + 500+j] = BLACK;
						}
						for(int i = 0; i < 12-j; i++){
							image_buffer_pointer[(740-i+k*104)*1280 + 500+j] = BLACK;
						}
					}
				}
				Xil_DCacheFlush();
				usleep(BTN_DELAY);
				return;
			}
		}
	}
}

void updateCursorMainMenu(int cursor)
{
	// draws cursor for main menu (4 choices)
	for(int k = 0; k < 4; k++){
		for(int j = 0; j < 12; j++){
			for(int i = 0; i < 12-j; i++){
				image_buffer_pointer[(535+i+k*104)*1280 + 500+j] = BLACK;
			}
			for(int i = 0; i < 12-j; i++){
				image_buffer_pointer[(535-i+k*104)*1280 + 500+j] = BLACK;
			}
		}
	}
	for(int j = 0; j < 12; j++){
		for(int i = 0; i < 12-j; i++){
			image_buffer_pointer[(535+i+cursor*104)*1280 + 500+j] = RED;
		}
		for(int i = 0; i < 12-j; i++){
			image_buffer_pointer[(535-i+cursor*104)*1280 + 500+j] = RED;
		}
	}
	Xil_DCacheFlush();
	usleep(BTN_DELAY);
}

void updateCursorOptions(int cursor)
{
	// draws cursor for options menu (2 choices)
	for(int k = 0; k < 2; k++){
		for(int j = 0; j < 12; j++){
			for(int i = 0; i < 12-j; i++){
				image_buffer_pointer[(740+i+k*104)*1280 + 500+j] = BLACK;
			}
			for(int i = 0; i < 12-j; i++){
				image_buffer_pointer[(740-i+k*104)*1280 + 500+j] = BLACK;
			}
		}
	}
	for(int j = 0; j < 12; j++){
		for(int i = 0; i < 12-j; i++){
			image_buffer_pointer[(740+i+cursor*104)*1280 + 500+j] = RED;
		}
		for(int i = 0; i < 12-j; i++){
			image_buffer_pointer[(740-i+cursor*104)*1280 + 500+j] = RED;
		}
	}
	Xil_DCacheFlush();
	usleep(BTN_DELAY);
}

void playSinglePlayerGame()
{
	// display board
	memcpy(image_buffer_pointer, board, NUM_BYTES_BUFFER);
	Xil_DCacheFlush();

	// toggle sound
	FLAG = 1;

	// memory for ship and board
	ship* my_ships = (ship*) malloc(NUM_SHIPS*sizeof(ship));
	my_placements = (char*) malloc(10*10*sizeof(char));
	memset(my_placements, 0, 10*10*sizeof(char));
	int enemy_ships[5] = {0,0,0,0,0};
	coord* enemy_placement = (coord*) malloc(5*5*sizeof(coord));
	memset(enemy_placement, 0, 5*5*sizeof(coord));

	// place ships
	setupShips(my_ships, my_placements);

	bool game_end = false;

	memset(my_placements, 0, 10*10*sizeof(char));
	while(true) {
//		if(player == '0'){
//			if(game_end) break;
//			send_attack_offline(enemy_ships, enemy_placement, &game_end, my_ships);
//		}
		if(game_end) break;
		send_attack_offline(enemy_ships, enemy_placement, &game_end, my_ships);
	}

	free(my_placements);
}

void playMultiplayerGame()
{
	// initalize connection after choosing player number
	initEthernet();

	// toggle sound
	FLAG = 1;

	// display board
	memcpy(image_buffer_pointer, board, NUM_BYTES_BUFFER);
	Xil_DCacheFlush();

	// memory for ship and board
	ship* my_ships = (ship*) malloc(NUM_SHIPS*sizeof(ship));
	my_placements = (char*) malloc(10*10*sizeof(char));
	memset(my_placements, 0, 10*10*sizeof(char));
	int enemy_ships[5] = {0,0,0,0,0};
	coord* enemy_placement = (coord*) malloc(5*5*sizeof(coord));
	memset(enemy_placement, 0, 5*5*sizeof(coord));


	// place ships
	setupShips(my_ships, my_placements);

	bool game_end = false;
	playing_game = 1;
	memset(my_placements, 0, 10*10*sizeof(char));

	// take turns shooting
	while(true){
		if(player == '1'){
			if(game_end) break;
			send_attack(enemy_ships, enemy_placement, &game_end);
			if(game_end) break;
			receive_attack(my_ships);
		}
		else if(player == '2'){
			if(game_end) break;
			receive_attack(my_ships);
			if(game_end) break;
			send_attack(enemy_ships, enemy_placement, &game_end);
		}
	}
	playing_game = 0;
	free(my_placements);
}

void send_attack(int* enemy_ships, coord* enemy_placement, bool* game_end)
{
	coord target = getAttackPos();
	drawExplosion(target);
	send_coords(target.x, target.y);
	while(!msg_received) eth_loop();
	msg_received = 0;
	FLAG = 4;
	switch(recv_result){
	case 0:
		drawMiss(target);
		break;
	case ID_CARRIER:
		drawHit(target);
		enemy_ships[0]++;
		enemy_placement[0+enemy_ships[0]-1].x = target.x;
		enemy_placement[0+enemy_ships[0]-1].y = target.y;
		if(enemy_ships[0] >= SIZE_CARRIER) drawSinkingShip(&enemy_placement[0], ID_CARRIER, SIZE_CARRIER);
		break;
	case ID_BATTLESHIP:
		drawHit(target);
		enemy_ships[1]++;
		enemy_placement[5+enemy_ships[1]-1].x = target.x;
		enemy_placement[5+enemy_ships[1]-1].y = target.y;
		if(enemy_ships[1] >= SIZE_BATTLESHIP) drawSinkingShip(&enemy_placement[5], ID_BATTLESHIP, SIZE_BATTLESHIP);
		break;
	case ID_CRUISER:
		drawHit(target);
		enemy_ships[2]++;
		enemy_placement[10+enemy_ships[2]-1].x = target.x;
		enemy_placement[10+enemy_ships[2]-1].y = target.y;
		if(enemy_ships[2] >= SIZE_CRUISER) drawSinkingShip(&enemy_placement[10], ID_CRUISER, SIZE_CRUISER);
		break;
	case ID_SUBMARINE:
		drawHit(target);
		enemy_ships[3]++;
		enemy_placement[15+enemy_ships[3]-1].x = target.x;
		enemy_placement[15+enemy_ships[3]-1].y = target.y;
		if(enemy_ships[3] >= SIZE_SUBMARINE) drawSinkingShip(&enemy_placement[15], ID_SUBMARINE, SIZE_SUBMARINE);
		break;
	case ID_DESTROYER:
		drawHit(target);
		enemy_ships[4]++;
		enemy_placement[20+enemy_ships[4]-1].x = target.x;
		enemy_placement[20+enemy_ships[4]-1].y = target.y;
		if(enemy_ships[4] >= SIZE_DESTROYER) drawSinkingShip(&enemy_placement[20], ID_DESTROYER, SIZE_DESTROYER);
		break;
	case 'W':
		*game_end = true;
		drawHit(target);
		drawVictory();
		break;
	default:
		drawMiss(target);
		break;
	}

}

void receive_attack(ship* my_ships)
{
	while(!msg_received) eth_loop();
	msg_received = 0;
	coord target;
	target.x = recv_x;
	target.y = recv_y;
	char result = attackPos(my_ships, target);

	if(isDestroyed(my_ships)){
		send_result('W');
		drawDefeat();
	}
	else{
		send_result(result);
	}
}

void send_attack_offline(int* enemy_ships, coord* enemy_placement, bool* game_end, ship* my_ships)
{
	coord target = getAttackPos();
	drawExplosion(target);
	recv_x = target.x;
	recv_y = target.y;
	receive_attack_offline(my_ships);
	switch(recv_result){
	case 0:
		drawMiss(target);
		break;
	case ID_CARRIER:
		drawHit(target);
		enemy_ships[0]++;
		enemy_placement[0+enemy_ships[0]-1].x = target.x;
		enemy_placement[0+enemy_ships[0]-1].y = target.y;
		if(enemy_ships[0] >= SIZE_CARRIER) drawSinkingShip(&enemy_placement[0], ID_CARRIER, SIZE_CARRIER);
		break;
	case ID_BATTLESHIP:
		drawHit(target);
		enemy_ships[1]++;
		enemy_placement[5+enemy_ships[1]-1].x = target.x;
		enemy_placement[5+enemy_ships[1]-1].y = target.y;
		if(enemy_ships[1] >= SIZE_BATTLESHIP) drawSinkingShip(&enemy_placement[5], ID_BATTLESHIP, SIZE_BATTLESHIP);
		break;
	case ID_CRUISER:
		drawHit(target);
		enemy_ships[2]++;
		enemy_placement[10+enemy_ships[2]-1].x = target.x;
		enemy_placement[10+enemy_ships[2]-1].y = target.y;
		if(enemy_ships[2] >= SIZE_CRUISER) drawSinkingShip(&enemy_placement[10], ID_CRUISER, SIZE_CRUISER);
		break;
	case ID_SUBMARINE:
		drawHit(target);
		enemy_ships[3]++;
		enemy_placement[15+enemy_ships[3]-1].x = target.x;
		enemy_placement[15+enemy_ships[3]-1].y = target.y;
		if(enemy_ships[3] >= SIZE_SUBMARINE) drawSinkingShip(&enemy_placement[15], ID_SUBMARINE, SIZE_SUBMARINE);
		break;
	case ID_DESTROYER:
		drawHit(target);
		enemy_ships[4]++;
		enemy_placement[20+enemy_ships[4]-1].x = target.x;
		enemy_placement[20+enemy_ships[4]-1].y = target.y;
		if(enemy_ships[4] >= SIZE_DESTROYER) drawSinkingShip(&enemy_placement[20], ID_DESTROYER, SIZE_DESTROYER);
		break;
	case 'W':
		*game_end = true;
		drawHit(target);
		drawVictory();
		break;
	case 255:
		drawHit(target);
		break;
	default:
		drawMiss(target);
		break;
	}

}

void receive_attack_offline(ship* my_ships)
{
	coord target;
	target.x = recv_x;
	target.y = recv_y;
	char result = attackPos(my_ships, target);

	if(isDestroyed(my_ships)){
		recv_result = 'W';
		drawDefeat();
	}
	else{
		recv_result = result;
	}
}

char attackPos(ship* ships, coord coord)
{
	// check if attack coord is occupied
	// update values
	// return 0 if miss, type if hit

	xil_printf("%d, %d is attacked\r\n", coord.x, coord.y);
	for (int i = 0; i < NUM_SHIPS; i++)
	{
		for(int j = 0; j < ships[i].size; j++){
			if(ships[i].coords[j].x == coord.x && ships[i].coords[j].y == coord.y){
				if(ships[i].hit_coord[j].x == coord.x && ships[i].hit_coord[j].y == coord.y){	// already hit
					xil_printf("Tile already hit!\n\r");
					return 255;
				}
				else{
					xil_printf("A ship has been hit\n\r");
					ships[i].lives--;
					ships[i].hit_coord[j].x = coord.x;
					ships[i].hit_coord[j].y = coord.y;

					if(ships[i].lives <= 0){
						ships[i].is_destroyed = true;
						xil_printf("A ship has been destroyed!\n\r");
						drawLives(ships[i].type);
					}

					return ships[i].type;
				}

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
	while(true) {
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
			if (my_placements[coords.y*10 + coords.x] == '1')
				continue;

			drawBox(147 + 13*1280 + coords.x*100 + 1280*100*coords.y, BLACK, false);
			my_placements[coords.y*10 + coords.x] = '1';
			return coords;
		}
	}

	return coords;
}

void updateCrosshair(coord coords)
{
	drawBox(147 + 13*1280 + curr.x*100 + 1280*100*curr.y, BLACK, false);
	drawBox(147 + 13*1280 + coords.x*100 + 1280*100*coords.y, RED, false);
	usleep(BTN_DELAY);
	curr.x = coords.x;
	curr.y = coords.y;
}

void displayPlayerSelection()
{
	// choose player 1 or 2 for connecting
	for(int i = 220; i < 950; i++){
		for(int j = 0; j < 1280; j++){
			image_buffer_pointer[i*1280 + j] = BLACK;
		}
	}
	Xil_DCacheFlush();
	for(int j = 0; j < 100; j++){
		for(int i = 0; i < 200; i++){
			image_buffer_pointer[220 + 1280*400 + i + j*1280] = sprites[587*1280 + i + j*1280];
			image_buffer_pointer[860 + 1280*400 + i + j*1280] = sprites[587*1280 + 200 + i + j*1280];

		}
	}
	Xil_DCacheFlush();
	usleep(BTN_DELAY);

	while(true) {
		while(BTN_INTR_FLAG == false);
		BTN_INTR_FLAG = false;
		if (BTN_VAL == 4){			// left (player 1)
			player = '1';
			return;
		}
		else if (BTN_VAL == 8){		// right (player 2)
			player = '2';
			return;
		}
	}
}

void setupShips(ship* ships, char* board)
{
	// use buttons to get placements
	// write to board for visuals
	int types[5] = {ID_CARRIER, ID_BATTLESHIP, ID_CRUISER, ID_SUBMARINE, ID_DESTROYER};
	int size[5] = {SIZE_CARRIER, SIZE_BATTLESHIP, SIZE_CRUISER, SIZE_SUBMARINE, SIZE_DESTROYER};
	int lives[5] = {SIZE_CARRIER, SIZE_BATTLESHIP, SIZE_CRUISER, SIZE_SUBMARINE, SIZE_DESTROYER};

	for(int i = 0; i < NUM_SHIPS; i++){
		ships[i].type = types[i];
		ships[i].size = size[i];
		ships[i].lives = lives[i];
		ships[i].is_destroyed = false;
		for(int j = 0; j < 5; j++){
			ships[i].coords[j].x = -1;
			ships[i].coords[j].y = -1;
			ships[i].hit_coord[j].x = -1;
			ships[i].hit_coord[j].y = -1;
		}
		drawShipBox(ships[i], GREEN);
		getShipPos(&ships[i], board);
		drawShipBox(ships[i], BLUE);
	}

	for(int i = 0; i < 10; i++){
		for(int j = 0; j < 10; j++){
			drawBox(147 + 13*1280 + i*100 + 1280*100*j, BLACK, true);
		}
		drawShipBox(ships[i], BLACK);
	}
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

void getShipPos(ship* ship, char* board)
{
	bool valid = true;
	bool horizontal = true;

	// starting at 0,0, set first coord to 0,0
	ship->coords[0].x = 0;
	ship->coords[0].y = 0;
	valid = updateShip(ship, horizontal, board);

	while(true) {
		while(BTN_INTR_FLAG == false){
			PS_BTN_VAL = XGpioPs_ReadPin(&GPIO_PS, PBSW);
			if (player != '0') eth_loop();
			if (PS_BTN_VAL == 1 && valid) {
				for(int i = 0; i < ship->size; i++){
					xil_printf("%d, %d \r\n" ,ship->coords[i].x, ship->coords[i].y);
					board[10*ship->coords[i].y + ship->coords[i].x] = 1;
				}
				xil_printf("\r\n");
				return;
			}
		}
		BTN_INTR_FLAG = false;

		if (BTN_VAL == 16){ 		// up
			(ship->coords[0].y == 0) ? ship->coords[0].y = 9 : ship->coords[0].y--;
			valid = updateShip(ship, horizontal, board);
		}
		else if (BTN_VAL == 2){ 	// down
			ship->coords[0].y = (ship->coords[0].y + 1) % 10;
			valid = updateShip(ship, horizontal, board);
		}
		else if (BTN_VAL == 4){		// left
			(ship->coords[0].x == 0) ? ship->coords[0].x = 9 : ship->coords[0].x--;
			valid = updateShip(ship, horizontal, board);
		}
		else if (BTN_VAL == 8){		// right
			ship->coords[0].x = (ship->coords[0].x + 1) % 10;
			valid = updateShip(ship, horizontal, board);
		}
		else if (BTN_VAL == 1){		// centre
			horizontal = !horizontal;
			valid = updateShip(ship, horizontal, board);
		}
	}


}

bool updateShip(ship* ship, bool horizontal, char* board)
{
	// clear all ship placements (visual)
	for(int i = 0; i < 10; i++){
		for(int j = 0; j < 10; j++){
			drawBox(147 + 13*1280 + i*100 + 1280*100*j, BLACK, true);
		}
	}

	// write the ship placements (visual)
	for(int i = 0; i < 100; i++){
		if(board[i] != 0){
			drawBox(147 + 13*1280 + (i%10)*100 + 1280*100*(i/10), BLUE, true);
		}
	}

	int x = 0;
	int y = 0;
	int z = 0;
	bool valid = true;
	int valid_color = GREEN;

	if (horizontal){
		for(int i = 0; i < ship->size; i++){
			z = ship->coords[0].x + i;
			if (z > 9  || (board[z+10*ship->coords[0].y] != 0)){
				valid = false;
				valid_color = RED;
			}
		}
		for(int i = 1; i < ship->size; i++){
			x = ship->coords[0].x + i;
			x = x % 10;
			ship->coords[i].x = x;
			ship->coords[i].y = ship->coords[0].y;
			drawBox(147 + 13*1280 + ship->coords[0].x*100 + 1280*100*ship->coords[0].y, valid_color, true);
			drawBox(147 + 13*1280 + (x)*100 + 1280*100*ship->coords[0].y, valid_color, true);
		}
		usleep(BTN_DELAY);
	}
	else{
		for(int i = 0; i < ship->size; i++){
			z = ship->coords[0].y + i;
			if (z > 9 || (board[ship->coords[0].x+10*z] != 0)){
				valid = false;
				valid_color = RED;
			}
		}
		for(int i = 1; i < ship->size; i++){
			y = ship->coords[0].y + i;
			y = y % 10;
			ship->coords[i].y = y;
			ship->coords[i].x = ship->coords[x].x;
			drawBox(147 + 13*1280 + ship->coords[0].x*100 + 1280*100*ship->coords[0].y, valid_color, true);
			drawBox(147 + 13*1280 + (ship->coords[0].x)*100 + 1280*100*y, valid_color, true);
		}
		usleep(BTN_DELAY);
	}
	return valid;
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

void drawShipBox(ship ship, int color)
{
	int offset = 0;
	switch(ship.type){
		case ID_CARRIER: // carrier
			offset = 525*1280 + 1155;
			for(int i = 0; i < 115; i++){
				image_buffer_pointer[offset + i] = color;
				image_buffer_pointer[offset + 1280 + i] = color;
				image_buffer_pointer[offset + 400*1280 + i] = color;
				image_buffer_pointer[offset + 401*1280 + i] = color;
			}

			for(int i = 0; i < 401; i++){
				image_buffer_pointer[offset + 1280*i] = color;
				image_buffer_pointer[offset + 1 + 1280*i] = color;
				image_buffer_pointer[offset + 115 + 1280*i] = color;
				image_buffer_pointer[offset + 116 + 1280*i] = color;
			}
			break;
		case ID_BATTLESHIP: // battleship
			offset = 102*1280 + 1177;
			for(int i = 0; i < 83; i++){
				image_buffer_pointer[offset + i] = color;
				image_buffer_pointer[offset + 1280 + i] = color;
				image_buffer_pointer[offset + 410*1280 + i] = color;
				image_buffer_pointer[offset + 411*1280 + i] = color;
			}

			for(int i = 0; i < 410; i++){
				image_buffer_pointer[offset + 1280*i] = color;
				image_buffer_pointer[offset + 1 + 1280*i] = color;
				image_buffer_pointer[offset + 83 + 1280*i] = color;
				image_buffer_pointer[offset + 84 + 1280*i] = color;
			}
			break;
		case ID_CRUISER: // cruiser
			offset = 700*1280 + 12;
			for(int i = 0; i < 84; i++){
				image_buffer_pointer[offset + i] = color;
				image_buffer_pointer[offset + 1280 + i] = color;
				image_buffer_pointer[offset + 317*1280 + i] = color;
				image_buffer_pointer[offset + 317*1280 + i] = color;
			}

			for(int i = 0; i < 317; i++){
				image_buffer_pointer[offset + 1280*i] = color;
				image_buffer_pointer[offset + 1 + 1280*i] = color;
				image_buffer_pointer[offset + 84 + 1280*i] = color;
				image_buffer_pointer[offset + 85 + 1280*i] = color;
			}
			break;
		case ID_SUBMARINE: // sub
			offset = 298*1280 + 15;
			for(int i = 0; i < 92; i++){
				image_buffer_pointer[offset + i] = color;
				image_buffer_pointer[offset + 1280 + i] = color;
				image_buffer_pointer[offset + 313*1280 + i] = color;
				image_buffer_pointer[offset + 314*1280 + i] = color;
			}

			for(int i = 0; i < 313; i++){
				image_buffer_pointer[offset + 1280*i] = color;
				image_buffer_pointer[offset + 1 + 1280*i] = color;
				image_buffer_pointer[offset + 92 + 1280*i] = color;
				image_buffer_pointer[offset + 93 + 1280*i] = color;
			}
			break;
		case ID_DESTROYER: // destroyer
			offset = 8*1280 + 33;
			for(int i = 0; i < 65; i++){
				image_buffer_pointer[offset + i] = color;
				image_buffer_pointer[offset + 1280 + i] = color;
				image_buffer_pointer[offset + 204*1280 + i] = color;
				image_buffer_pointer[offset + 204*1280 + i] = color;
			}

			for(int i = 0; i < 204; i++){
				image_buffer_pointer[offset + 1280*i] = color;
				image_buffer_pointer[offset + 1 + 1280*i] = color;
				image_buffer_pointer[offset + 65 + 1280*i] = color;
				image_buffer_pointer[offset + 66 + 1280*i] = color;
			}
			break;
	}
	Xil_DCacheFlush();

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
	FLAG = 4;
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
	FLAG = 2;
	int rand_num = rand() % 2;
	int offset = 147 + 13*1280 + coords.x*100 + 1280*100*coords.y;
	for(int j = 0; j < 95; j++){
		for(int i = 0; i < 95; i++){
			image_buffer_pointer[offset+i+j*1280] = sprites[i+j*1280+95*(5+rand_num)];
		}
	}
	Xil_DCacheFlush();
}

void drawSinkingShip(coord* coords, int type, int size)
{
	FLAG = 8;
	for(int i = 0; i < size; i++){
		int offset = 147 + 13*1280 + coords[i].x*100 + 1280*100*coords[i].y;
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
	FLAG = 16;
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
	FLAG = 32;
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

void drawLives(int type)
{
	int offset = 0;
	switch(type){
		case ID_DESTROYER:
			for(int i = 25; i < 95; i++){
				image_buffer_pointer[2*i*1280 + i] = RED;
				image_buffer_pointer[2*i*1280 + i + 1] = RED;
				image_buffer_pointer[2*i*1280 + i + 2] = RED;
				image_buffer_pointer[2*i*1280 + i + 3] = RED;
				image_buffer_pointer[2*i*1280+1280 + i + 1] = RED;
				image_buffer_pointer[2*i*1280+1280 + i + 2] = RED;
				image_buffer_pointer[2*i*1280+1280 + i + 3] = RED;
				image_buffer_pointer[2*i*1280+1280 + i + 4] = RED;
			}
			Xil_DCacheFlush();
			break;
		case ID_SUBMARINE: // or cruiser, both have 3
			offset = 335 * 1280 + 15;
			for(int i = 0; i < 75; i++){
				image_buffer_pointer[offset + 3*i*1280 + i] = RED;
				image_buffer_pointer[offset + 3*i*1280 + i + 1] = RED;
				image_buffer_pointer[offset + 3*i*1280 + i + 2] = RED;
				image_buffer_pointer[offset + 3*i*1280 + i + 3] = RED;
				image_buffer_pointer[offset + 3*i*1280+1280 + i + 1] = RED;
				image_buffer_pointer[offset + 3*i*1280+1280 + i + 2] = RED;
				image_buffer_pointer[offset + 3*i*1280+1280 + i + 3] = RED;
				image_buffer_pointer[offset + 3*i*1280+1280 + i + 4] = RED;
				image_buffer_pointer[offset + 3*i*1280+1280*2 + i + 1] = RED;
				image_buffer_pointer[offset + 3*i*1280+1280*2 + i + 2] = RED;
				image_buffer_pointer[offset + 3*i*1280+1280*2 + i + 3] = RED;
				image_buffer_pointer[offset + 3*i*1280+1280*2 + i + 4] = RED;
			}
			Xil_DCacheFlush();
			break;
		case ID_CRUISER:
			offset = 740 * 1280 + 13;
			for(int i = 0; i < 75; i++){
				image_buffer_pointer[offset + 3*i*1280 + i] = RED;
				image_buffer_pointer[offset + 3*i*1280 + i + 1] = RED;
				image_buffer_pointer[offset + 3*i*1280 + i + 2] = RED;
				image_buffer_pointer[offset + 3*i*1280 + i + 3] = RED;
				image_buffer_pointer[offset + 3*i*1280+1280 + i + 1] = RED;
				image_buffer_pointer[offset + 3*i*1280+1280 + i + 2] = RED;
				image_buffer_pointer[offset + 3*i*1280+1280 + i + 3] = RED;
				image_buffer_pointer[offset + 3*i*1280+1280 + i + 4] = RED;
				image_buffer_pointer[offset + 3*i*1280+1280*2 + i + 1] = RED;
				image_buffer_pointer[offset + 3*i*1280+1280*2 + i + 2] = RED;
				image_buffer_pointer[offset + 3*i*1280+1280*2 + i + 3] = RED;
				image_buffer_pointer[offset + 3*i*1280+1280*2 + i + 4] = RED;
			}
			Xil_DCacheFlush();
			break;
		case ID_BATTLESHIP:
			offset = 160 * 1280 + 1175;
			for(int i = 0; i < 80; i++){
				image_buffer_pointer[offset + 4*i*1280 + i] = RED;
				image_buffer_pointer[offset + 4*i*1280 + i + 1] = RED;
				image_buffer_pointer[offset + 4*i*1280 + i + 2] = RED;
				image_buffer_pointer[offset + 4*i*1280 + i + 3] = RED;
				image_buffer_pointer[offset + 4*i*1280+1280 + i + 1] = RED;
				image_buffer_pointer[offset + 4*i*1280+1280 + i + 2] = RED;
				image_buffer_pointer[offset + 4*i*1280+1280 + i + 3] = RED;
				image_buffer_pointer[offset + 4*i*1280+1280 + i + 4] = RED;
				image_buffer_pointer[offset + 4*i*1280+1280*2 + i + 1] = RED;
				image_buffer_pointer[offset + 4*i*1280+1280*2 + i + 2] = RED;
				image_buffer_pointer[offset + 4*i*1280+1280*2 + i + 3] = RED;
				image_buffer_pointer[offset + 4*i*1280+1280*2 + i + 4] = RED;
				image_buffer_pointer[offset + 4*i*1280+1280*3 + i + 1] = RED;
				image_buffer_pointer[offset + 4*i*1280+1280*3 + i + 2] = RED;
				image_buffer_pointer[offset + 4*i*1280+1280*3 + i + 3] = RED;
				image_buffer_pointer[offset + 4*i*1280+1280*3 + i + 4] = RED;
			}
			Xil_DCacheFlush();
			break;
		case ID_CARRIER:
			offset = 575 * 1280 + 1160;
			for(int i = 0; i < 110; i++){
				image_buffer_pointer[offset + 3*i*1280 + i] = RED;
				image_buffer_pointer[offset + 3*i*1280 + i + 1] = RED;
				image_buffer_pointer[offset + 3*i*1280 + i + 2] = RED;
				image_buffer_pointer[offset + 3*i*1280 + i + 3] = RED;
				image_buffer_pointer[offset + 3*i*1280+1280 + i + 1] = RED;
				image_buffer_pointer[offset + 3*i*1280+1280 + i + 2] = RED;
				image_buffer_pointer[offset + 3*i*1280+1280 + i + 3] = RED;
				image_buffer_pointer[offset + 3*i*1280+1280 + i + 4] = RED;
				image_buffer_pointer[offset + 3*i*1280+1280*2 + i + 1] = RED;
				image_buffer_pointer[offset + 3*i*1280+1280*2 + i + 2] = RED;
				image_buffer_pointer[offset + 3*i*1280+1280*2 + i + 3] = RED;
				image_buffer_pointer[offset + 3*i*1280+1280*2 + i + 4] = RED;
			}
			Xil_DCacheFlush();
			break;
	}

}


//----------------------------------------------------
// MAIN FUNCTION
//----------------------------------------------------
int main (void)
{
	int status = 0;

	// UART
	initUART();

	// Push buttons
	status = XGpio_Initialize(&BTN_INST, BTNS_DEVICE_ID);
	if(status != XST_SUCCESS) return XST_FAILURE;
	GPIOConfigPtr = XGpioPs_LookupConfig(GPIO_PS_ID);
	status = XGpioPs_CfgInitialize(&GPIO_PS, GPIOConfigPtr, GPIOConfigPtr->BaseAddr);
	if(status != XST_SUCCESS) return XST_FAILURE;
	XGpio_SetDataDirection(&BTN_INST, 1, 0xFF);
	XGpioPs_SetDirectionPin(&GPIO_PS, PBSW, 0x0);

	// Interrupt controller
	status = initIntcFunction(INTC_DEVICE_ID, &BTN_INST);
	if(status != XST_SUCCESS) return XST_FAILURE;

	// VGA
	initVGA();

	int choice = 0;
	while(true){
		choice = displayMainMenu();
		switch(choice){
		case 0:		// single player
			playSinglePlayerGame();
			break;
		case 1:		// multiplayer
			displayPlayerSelection();
			playMultiplayerGame();
			break;
		case 2:		// options
			displayOptionsMenu();
			break;
		case 3:		// quit
			return 0;
		}
	}

	return 0;
}
