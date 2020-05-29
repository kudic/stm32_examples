#ifndef __SNAKE__H__
#define __SNAKE__H__
#include "game.h"

#define SNAKE_DBG(x,...)       rt_kprintf("[Snake] ");rt_kprintf(x, ##__VA_ARGS__)

typedef struct SnakeNode{
	int x;
	int y;
	struct SnakeNode *next,*prev;
}SNAKELIST;
        
static void GameOver(void);
void Snake_Start(void);
void Snake_Stop(void);

#endif //__SNAKE__H__
