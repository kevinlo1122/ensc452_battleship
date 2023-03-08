#ifndef __ETHERNET_H_
#define __ETHERNET_H_

int send_coords(int x, int y);
int send_result(char res);
int eth_init(char player);
void eth_loop();

#endif
