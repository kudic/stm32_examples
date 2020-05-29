#ifndef __GAME_H__
#define __GAME_H__

#define NON                0xFF
#define TURN_LEFT_EV       0x01  //×ó
#define TURN_RIGHT_EV      0x04  //ÓÒ
#define TURN_DOWN_EV       0x02  //ÏÂ
#define TURN_UP_EV         0x03  //ÉÏ
#define RESTART_EV         0x05  //¸´Î»

#define  HEIGHT  30
#define  WIDTH   16
#define  GAMERUNING   0x00
#define  GAMEPAUSE    0x01

struct gamestruct{
  char gamename[20];
  void (*game_start)(void);
  void (*game_stop)(void);
};
extern unsigned char gamemap[HEIGHT][WIDTH];

void PrintfBlock(unsigned char *block_xx,int height,int width);

#endif //__GAME_H__