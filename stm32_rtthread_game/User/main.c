#include "stm32f10x.h"
#include <rtthread.h>
#include "joystick.h"
#include "oled.h"
#include "Snake.h"
#include "string.h"
#include "Tetris.h"

#define MAIN_DBG(x,...)       rt_kprintf("[main] ");rt_kprintf(x, ##__VA_ARGS__)

#define LD2_GPIO_PORT  GPIOC
#define LD2_PIN        GPIO_Pin_13
static struct rt_timer blink_timer;
static void MX_GPIO_Init(void);
static void blink_timeout(void *parameter);


struct gamestruct gamestr[]={
  {
    "Snake",
    Snake_Start,
    Snake_Stop,  
  },
  {
   "Tetris",
    Tetris_Start,
    Tetris_Stop,
  }
};

int main(void)
{
    char i,joy_value,game_total = 0;
    signed char index = 0;
  
    MX_GPIO_Init();
    drv_joystick_init();//定时器周期检测摇杆状态
    drv_oled_init();//独立的线程等待屏幕刷新事件
    
    //使用定时器，实现PC13引脚出led闪烁
    rt_timer_init(&blink_timer,"blink_timer",blink_timeout,RT_NULL,100,RT_TIMER_FLAG_PERIODIC);
    rt_timer_start(&blink_timer);
    
    //游戏数量
    game_total = sizeof(gamestr)/sizeof(struct gamestruct);
    //显示游戏选项
    GUI_ScrollSelect(gamestr,game_total,index);
    while (1)
    {
        while(1){
            //向上拨动摇杆
            if (rt_event_recv(&joystick_event, JOY_STATE_TRUN_UP,RT_EVENT_FLAG_OR,RT_WAITING_NO, RT_NULL) == RT_EOK)
            {
                MAIN_DBG("JOY_STATE_TRUN_UP\r\n");
                index--;
                if(index < 0){
                index = game_total-1;
                }
                GUI_ScrollSelect(gamestr,game_total,index);
                rt_thread_mdelay(500);
                //clean event
                rt_event_recv(&joystick_event, JOY_STATE_TRUN_UP,RT_EVENT_FLAG_OR|RT_EVENT_FLAG_CLEAR,RT_WAITING_NO, RT_NULL);
            }
            //向下拨动摇杆
            if (rt_event_recv(&joystick_event, JOY_STATE_TRUN_DOWN,RT_EVENT_FLAG_OR,RT_WAITING_NO, RT_NULL) == RT_EOK)
            {
                MAIN_DBG("JOY_STATE_TRUN_DOWN\r\n");
                index++;
                if(index == game_total){
                    index = 0;
                }
                GUI_ScrollSelect(gamestr,game_total,index);
                rt_thread_mdelay(500);
                //clean event
                rt_event_recv(&joystick_event, JOY_STATE_TRUN_DOWN,RT_EVENT_FLAG_OR|RT_EVENT_FLAG_CLEAR, RT_WAITING_NO, RT_NULL);
            }
            //短按中键，进入游戏
            if (rt_event_recv(&joystick_event, JOY_EVENT_KEY_RELEASE_SHORT,RT_EVENT_FLAG_OR|RT_EVENT_FLAG_CLEAR,
                       RT_WAITING_NO, RT_NULL) == RT_EOK)
            {
                MAIN_DBG("JOY_EVENT_KEY_RELEASE_SHORT\r\n");
                joy_value = get_joytick_sw();
                if(joy_value < 10){
                    gamestr[index].game_start();
                    break;
                }
            }
            rt_thread_mdelay(50);
       }
       //长按中键
       if (rt_event_recv(&joystick_event, JOY_EVENT_KEY_RELEASE_LONG,RT_EVENT_FLAG_OR|RT_EVENT_FLAG_CLEAR,
                      RT_WAITING_FOREVER, RT_NULL) == RT_EOK)
       {
            MAIN_DBG("Enter home page.\r\n");
            //返回首页,停止所有游戏线程
            for(i = 0;i < game_total;i++){
                gamestr[i].game_stop();
            }
            index = 0;
            OLED_Clean();
            GUI_ScrollSelect(gamestr,game_total,index);
        }
    }      
}

static void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC,ENABLE);
    GPIO_ResetBits(LD2_GPIO_PORT, LD2_PIN);

    GPIO_InitStruct.GPIO_Pin  = LD2_PIN;
    GPIO_InitStruct.GPIO_Mode  = GPIO_Mode_Out_PP;
    GPIO_InitStruct.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(LD2_GPIO_PORT, &GPIO_InitStruct);
}

static void blink_timeout(void *parameter)
{
  static char sw = 0;
  
  if(sw){
    GPIO_SetBits(LD2_GPIO_PORT, LD2_PIN);
  }else{
    GPIO_ResetBits(LD2_GPIO_PORT, LD2_PIN);
  }
  sw = !sw;
}

