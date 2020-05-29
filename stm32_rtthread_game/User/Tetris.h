#ifndef __TETRIS__H__
#define __TETRIS__H__
#include "game.h"

#define LEFT   0x00
#define RIGHT  0x01

#define TETRIS_DBG(x,...)       rt_kprintf("[Tetris] ");rt_kprintf(x, ##__VA_ARGS__)

typedef struct{
	int x_pre;
	int y_pre;
	int x_cur;
	int y_cur;
	unsigned char block_cur[4][4];
	unsigned char block_pre[4][4];
}ActiveBlockStruct;

static void GameOver(void); 
void Tetris_Start(void);
void Tetris_Stop(void);

#endif //__TETRIS__H__
