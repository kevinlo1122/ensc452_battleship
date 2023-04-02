#include <stdio.h>
#include "platform.h"
#include "xil_printf.h"
#include <sleep.h>
#include "xil_io.h"
//#include "adventures_with_ip.h"
#include "audio.h"

// FLAG bit usage for sound effects and background tracks:
// if all 0, no sound
// |	MSB(7)	|	6	|	5	|	4	|	3	|	2	|	1	|	0	|
// |  vol down	|vol up |defeat	|victory| sink	| miss	|hit fx	|gameplay
#define FLAG 					(*(volatile u8*)(0x20000))
#define SHARED_BASEADDR			0x0F000000

#define BACKGROUND_TRACK_ADDR	0x1FBC4464
#define BACKGROUND_SIZE			3390364		// in bytes
#define VICTORY_TRACK_ADDR		0x1FA1E4BF
#define VICTORY_SIZE			1728420
#define DEFEAT_TRACK_ADDR		0x1F971566
#define DEFEAT_SIZE				708440
#define HIT_FX_ADDR 			0x1F94C9D1
#define HIT_SIZE 				150420
#define MISS_FX_ADDR 			0x1F937084
#define MISS_SIZE 				88396
#define SINK_FX_ADDR 			0x1F8CAFEB
#define SINK_SIZE 				442520


// dow -data ../../../../Users/arodillo/ensc452/battleship/audio/background.raw 0x1FBC4464
// dow -data ../../../../Users/arodillo/ensc452/battleship/audio/victory.raw 0x1FA1E4BF
// dow -data ../../../../Users/arodillo/ensc452/battleship/audio/defeat.raw 0x1F971566
// dow -data ../../../../Users/arodillo/ensc452/battleship/audio/hit.raw 0x1F94C9D1
// dow -data ../../../../Users/arodillo/ensc452/battleship/audio/miss.raw 0x1F937084
// dow -data ../../../../Users/arodillo/ensc452/battleship/audio/sink.raw 0x1F8CAFEB

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

	// Main audio stuff
	int sampling_rate = 22050;
	int amp = 8;

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
		u8 curr_fx = 0;
		int temp, left, right;
		while (game_active) {
			for (int i = 0; i < BACKGROUND_SIZE/4; i++) {
				result = FLAG;

				// check if game over or in main menu
				if (result == 16 || result == 32 || result == 0)
					break;

				// if 16-bits per sample
				// every 32 bits has [left || right] i think
				// make sure sign is preserved, so i'm shifting the lower 16 all the way up first
				temp = bg_track[i];
				left = ((temp >> 16)) * amp;
				right = (((temp << 16) >> 16)) * amp;

				// check flag to determine if sound effects need to be played
				int fx_left, fx_right;
				if (curr_fx != result) {
					curr_fx = result;
					j = 0;
				}
				switch(result) {
				case 2:		// hit
					temp = hit_fx[j];
					fx_left = ((temp >> 16)) * amp;
					fx_right = (((temp << 16) >> 16)) * amp;
					left = left + fx_left;
					right = right + fx_right;

					if (++j == (HIT_SIZE/4)) {
						j = 0;
						FLAG = 1;
					}
					break;
				case 4:		// miss
					temp = miss_fx[j];
					fx_left = ((temp >> 16)) * amp;
					fx_right = (((temp << 16) >> 16)) * amp;
					left = left + fx_left;
					right = right + fx_right;

					if (++j == (MISS_SIZE/4)) {
						j = 0;
						FLAG = 1;
					}
					break;
				case 8:		// sink
					temp = sink_fx[j];
					fx_left = ((temp >> 16)) * amp;
					fx_right = (((temp << 16) >> 16)) * amp;
					left = left + fx_left;
					right = right + fx_right;

					if (++j == (SINK_SIZE/4)) {
						j = 0;
						FLAG = 1;
					}
				case 64:	// volume up
					if (amp < 64)
						amp = amp * 2;
				case 128:	// volume down
					if (amp > 1)
						amp = amp / 2;
				default:
					break;
				} // switch

				Xil_Out32(I2S_DATA_TX_L_REG, left);
				Xil_Out32(I2S_DATA_TX_R_REG, right);
				usleep(track_delay);
			}

			// check result if victory/defeat
			// play track
			j = 0;
			switch(result) {
			case 16:		// victory
				while (result) {
					temp = victory_track[j];
					left = ((temp >> 16)) * amp;
					right = (((temp << 16) >> 16)) * amp;

					Xil_Out32(I2S_DATA_TX_L_REG, left);
					Xil_Out32(I2S_DATA_TX_R_REG, right);
					usleep(track_delay);

					if (++j == (VICTORY_SIZE/4)) {
						j = 0;
					}

					result = FLAG;
				}
				break;
			case 32:
				while (result) {
					temp = defeat_track[j];
					left = ((temp >> 16)) * amp;
					right = (((temp << 16) >> 16)) * amp;

					Xil_Out32(I2S_DATA_TX_L_REG, left);
					Xil_Out32(I2S_DATA_TX_R_REG, right);
					usleep(track_delay);

					if (++j == (DEFEAT_SIZE/4)) {
						j = 0;
					}

					result = FLAG;
				}
				break;
			default:
				break;
			}

			// when returning to main menu
			// no audio should be playing
			game_active = FLAG;
		}
	}

    cleanup_platform();

    return 0;
}
