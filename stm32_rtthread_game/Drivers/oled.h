#ifndef __OLED__H__
#define __OLED__H__
#include "rtthread.h"
#include "game.h"

#define OLED_EVENT_REFRESH      (1<<0)
extern struct rt_event oled_event;
extern const unsigned char GUI_Font8x8ASCII_Data[8*128];
							
extern void OLED_DispString(int x0,int y0,char *str);	
extern void GameMapToLcdCache(unsigned char *gamemap);
void OLED_Clean(void);
extern int drv_oled_init(void);	
extern void GUI_ScrollSelect(struct gamestruct *pGameStr,unsigned char total,unsigned char index);
#endif
					
					