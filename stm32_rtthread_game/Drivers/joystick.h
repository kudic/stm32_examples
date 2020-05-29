#ifndef __JOYSTICK_H__
#define __JOYSTICK_H__


#include "rtthread.h"
#define JOY_EVENT_X                     (1<<0)
#define JOY_EVENT_Y                     (1<<1)
#define JOY_EVENT_KEY                   (1<<2)
#define JOY_EVENT_KEY_RELEASE_SHORT     (1<<3)       
#define JOY_EVENT_KEY_RELEASE_LONG      (1<<4)
#define JOY_EVENT_TRUN_LEFT             (1<<5)
#define JOY_STATE_TRUN_RIGHT            (1<<6)
#define JOY_STATE_TRUN_UP               (1<<7)
#define JOY_STATE_TRUN_DOWN             (1<<8)

typedef enum{
	KEY_STATE_IDLE,
	KEY_STATE_ACTION,
	KEY_STATE_PRESS,
	KEY_STATE_RELEASE,
}eKEY_STATE;

extern struct rt_event joystick_event; 

extern int drv_joystick_init(void);
extern char get_joytick_x(void); //0~100   左：小于50  右：大于50  
extern char get_joytick_y(void);//0~100   上：小于50  下：大于50  
extern char get_joytick_sw(void);//0 ：push ，1：nor

#endif //__JOYSTICK_H__
