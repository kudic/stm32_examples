#include "oled.h"
#include "stm32f10x.h"
#include "string.h"
#include "rthw.h"

//              ------------------------以下为OLED显示所用到的引脚--------------------------------------
//              GND    		电源地
//              VCC   		接5V或3.3v电源
//              D0(CLK)         PA5（CLK）
//              D1(MOSI)        PA7（DIN）
//              RES   		接3.3v电源，当接地时会导致复位   
//              DC    		PB0
//              CS   		PB1        

/*****************************************************************
 ========================= private variable =================== 
******************************************************************/
#define THREAD_PRIORITY         3
#define THREAD_STACK_SIZE       512
#define THREAD_TIMESLICE        7

ALIGN(RT_ALIGN_SIZE)
static char oled_thread_stack[THREAD_STACK_SIZE];
static struct rt_thread oled_thread;

volatile static unsigned char LCD_cache[8][128]={0}; //64*128显存 每一bit代表一个像素

#define lcd_cs(a)	if (a)	\
					GPIOB->BSRR = GPIO_Pin_1;\
					else		\
					GPIOB->BRR = GPIO_Pin_1;

#define lcd_dc(a)	if (a)	\
					GPIOB->BSRR = GPIO_Pin_0;\
					else		\
					GPIOB->BRR = GPIO_Pin_0;
					
#define lcd_res(a)	if (a)	\
					GPIOA->BSRR = GPIO_Pin_12;\
					else		\
					GPIOA->BRR = GPIO_Pin_12;\
					
#define lcd_sid(a)	if (a)	\
	        GPIOA->BSRR = GPIO_Pin_7;\
					else		\
					GPIOA->BRR = GPIO_Pin_7;

#define lcd_sclk(a)	if (a)	\
	        GPIOA->BSRR = GPIO_Pin_5;\
					else		\
					GPIOA->BRR = GPIO_Pin_5;
#define  __nop() asm("nop")\
                                        
/*****************************************************************
 ========================= public variable ==================== 
******************************************************************/
struct rt_event oled_event;




/*****************************************************************
 ========================= private function =================== 
******************************************************************/
static void GPIO_Config(void)//初始化IO口
{	
  GPIO_InitTypeDef GPIO_InitStructure;

  RCC_APB2PeriphClockCmd( RCC_APB2Periph_GPIOA|RCC_APB2Periph_GPIOB, ENABLE); 													     
  RCC_APB2PeriphClockCmd( RCC_APB2Periph_GPIOA, ENABLE); 
									
  /*PB.0->DC引脚   PB.1->CS引脚*/	
  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1  ;	
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;    
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz; 
  GPIO_Init(GPIOB, &GPIO_InitStructure);		
 /*PA.5->CLK引脚  PA.7->MOSI引脚*/
  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_5 | GPIO_Pin_7 ;	
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;   
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz; 
  GPIO_Init(GPIOA, &GPIO_InitStructure);		
}

static void OLED_TX_Command(int data)  //写指令到LCD模块
{
	char i;
	lcd_dc(0);;;
	lcd_cs(0);
	for(i=0;i<8;i++)
   {lcd_sclk(0);;;
		
		if(data&0x80) {lcd_sid(1);;;}
		else {lcd_sid(0);;;}
		lcd_sclk(1);
		__nop();;;
	//	lcd_sclk(0);;;
	 	data<<=1;
   }
	 	lcd_dc(1);;;
	 lcd_cs(1);
}


static void OLED_TX_Data(int data)//写数据到LCD模块
{
	char i;
	lcd_dc(1);;;
	lcd_cs(0);
	for(i=0;i<8;i++)
   {
		lcd_sclk(0);;;
		if(data&0x80) {lcd_sid(1);;;}
		else {lcd_sid(0);;;}
		lcd_sclk(1);;;
		__nop();;;
		//lcd_sclk(0);;;
	 	data<<=1;
   }lcd_dc(1);;;
	 lcd_cs(1);
}

static void delay_ms(int n_ms) //大概延时        
{
 int j,k;
 for(j=0;j<n_ms;j++)
 for(k=0;k<5500;k++);
}

static void OLED_Init(void) 
{
	delay_ms(800);
        GPIO_Config();
	lcd_cs(0); //片选引脚激活
	
	OLED_TX_Command(0xAE);   //display off
	OLED_TX_Command(0x20);	//Set Memory Addressing Mode	
	OLED_TX_Command(0x10);	//00,Horizontal Addressing Mode;01,Vertical Addressing Mode;10,Page Addressing Mode (RESET);11,Invalid
	OLED_TX_Command(0xb0);	//Set Page Start Address for Page Addressing Mode,0-7
	OLED_TX_Command(0xc8);	//Set COM Output Scan Direction
	OLED_TX_Command(0x00);//---set low column address
	OLED_TX_Command(0x10);//---set high column address
	OLED_TX_Command(0x40);//--set start line address
	OLED_TX_Command(0x81);//--set contrast control register
	OLED_TX_Command(0xFF);
	OLED_TX_Command(0xa1);//--set segment re-map 0 to 127
	OLED_TX_Command(0xa6);//--set normal display
	OLED_TX_Command(0xa8);//--set multiplex ratio(1 to 64)
	OLED_TX_Command(0x3F);//
	OLED_TX_Command(0xa4);//0xa4,Output follows RAM content;0xa5,Output ignores RAM content
	OLED_TX_Command(0xd3);//-set display offset
	OLED_TX_Command(0x00);//-not offset
	OLED_TX_Command(0xd5);//--set display clock divide ratio/oscillator frequency
	OLED_TX_Command(0xf0);//--set divide ratio
	OLED_TX_Command(0xd9);//--set pre-charge period
	OLED_TX_Command(0x22);//
	OLED_TX_Command(0xda);//--set com pins hardware configuration
	OLED_TX_Command(0x12);
	OLED_TX_Command(0xdb);//--set vcomh
	OLED_TX_Command(0x20);//0x20,0.77xVcc
	OLED_TX_Command(0x8d);//--set DC-DC enable
	OLED_TX_Command(0x14);//
	OLED_TX_Command(0xaf);//--turn on oled panel 
	
}

static void LCD_DrawPoint(uint8_t x,uint8_t y,uint8_t color)  //描点
{
	uint8_t temp,indx,page;

	if(color>1) color=1;
	indx=y%8; //indx=0
	page=y/8;
	
	temp=LCD_cache[page][x];
        temp &= ~(0x01<<indx);  //clean bit
        temp |= color<<indx;    //write bit
	LCD_cache[page][x]=temp;
}

static void GUI_DrawChar(uint16_t x0,uint16_t y0,uint8_t ch,char Inverse) //在特定的位置显示一个字符
{
    uint8_t xcnt,ycnt,color,count=0;
    const uint8_t* pData;

    pData=&GUI_Font8x8ASCII_Data[8*(ch-0x20)];
	
    
	  for(xcnt=0;xcnt<8;xcnt++){
				count=0;
        for(ycnt=0;ycnt<8;ycnt++){
             if(Inverse){
                color=((*pData)&(0x80>>count))>0?0:1;
             }else{
                color=((*pData)&(0x80>>count))>0?1:0;
             }
            LCD_DrawPoint(y0+ycnt,63-(x0+xcnt),color);
            count++;
        }
        pData++;
    }
}

static void OLED_Updata(void) //将LCD_canche显存里的数据发送到olcd进行显示
{
	unsigned char i,j;
        rt_base_t level;
        //以防被调度中断发送
        level = rt_hw_interrupt_disable();
	lcd_cs(0);
	for(i=0;i<8;i++)
	{
		OLED_TX_Command(0xb0+i);
		OLED_TX_Command(0x00);
		OLED_TX_Command(0x10);
		for(j=0;j<128;j++)
		{
		  	OLED_TX_Data(LCD_cache[i][j]);
		}
	}
 	lcd_cs(1);
        rt_hw_interrupt_enable(level);
}

static void oled_thread_entry(void *parameter)
{
      while(1){
          if (rt_event_recv(&oled_event, OLED_EVENT_REFRESH,RT_EVENT_FLAG_AND|RT_EVENT_FLAG_CLEAR,
                      RT_WAITING_FOREVER, RT_NULL) == RT_EOK)
          {
                OLED_Updata();
          }
          rt_thread_mdelay(50);
      }
}
/*****************************************************************
 ========================= public function =================== 
*****************************************************************/
void OLED_DispString(int x0,int y0,char *str) //在指定的位置显示字符串
{
      while(*str){
          GUI_DrawChar(x0,y0,*str,0);
          str++;
          x0+=6;
          if(x0>=64){
                  x0=0;
                  y0+=10;
          }
      }
      
      if (rt_event_recv(&oled_event, OLED_EVENT_REFRESH,RT_EVENT_FLAG_AND,
                    RT_WAITING_NO, RT_NULL) != RT_EOK)
      {
          rt_event_send(&oled_event, OLED_EVENT_REFRESH);
      }
}

void GameMapToLcdCache(unsigned char *gamemap)  //对游戏图像（16*30）进行放大然后填入lcd（64*128）的显存，游戏界面图像的一个像素转变成lcd的4*4个像素
{
   int i,j,gm_x=15,gm_y=0;
   
   for(i=0;i<16;i++){
     gm_y=0;
     for(j=8;j<128;j++){ //行映射
       if((j%4==0)&&(j!=8)){
           gm_y++;
       }
     if(*(gamemap+gm_y*16+gm_x)){
           if(i%2==0){
             LCD_cache[i/2][j] |= 0x0F; //低四位
           }else{
             LCD_cache[i/2][j] |= 0xF0; //高四位
           }
     }else{
           if(i%2==0){
             LCD_cache[i/2][j] &= 0xF0; //低四位
           }else{
             LCD_cache[i/2][j] &= 0x0F; //高四位
                                   }
           }
     }
     gm_x--;
   }
   
    if (rt_event_recv(&oled_event, OLED_EVENT_REFRESH,RT_EVENT_FLAG_AND,
                    RT_WAITING_NO, RT_NULL) != RT_EOK)
    {
          rt_event_send(&oled_event, OLED_EVENT_REFRESH);
    }
}

void OLED_Clean(void)
{
  memset((unsigned char*)LCD_cache,0,sizeof(LCD_cache));
}

int drv_oled_init(void)
{
   rt_err_t result;
   //初始化oled硬件
   OLED_Init();
   
   // 初始化事件对象 
   result = rt_event_init(&oled_event, "oled_event", RT_IPC_FLAG_FIFO);
   if (result != RT_EOK)
   {
        rt_kprintf("drv_oled_init failed.\n");
        return -1;
   }
   //初始化线程
   rt_thread_init(&oled_thread,
                   "oled_thread",
                   oled_thread_entry,
                   RT_NULL,
                   &oled_thread_stack[0],
                   sizeof(oled_thread_stack),
                   THREAD_PRIORITY - 1, THREAD_TIMESLICE);
    rt_thread_startup(&oled_thread);
    
    rt_kprintf("drv_oled_init ok.\n");
    
    return RT_EOK;
}


void GUI_ScrollSelect(struct gamestruct *pGameStr,unsigned char total,unsigned char index)
{
  unsigned char x0,y0,x1,y1,i,j,k;
  char *str;
  
  x0 = 0;
  x1 = 64;
  y0 = 128/2 - (10*total)/2;
  y1 = y0+10;
  
  for(k = 0;k < total;k++){
    
      //描背景
      for(i = x0;i < x1;i++)
      {
        for(j = y0;j < y1;j++)
        {
           if(k == index) LCD_DrawPoint(j,i,1);
           else  LCD_DrawPoint(j,i,0);
        }
      }
    
      //写字符
      str = pGameStr[k].gamename;
      while(*str){
        if(k == index) GUI_DrawChar(x0+5,y0+1,*str,1);
        else GUI_DrawChar(x0+5,y0+1,*str,0);
        str++;
        x0+=6;
        if(x0>=64){
              x0=0;
              y0+=10;
        }
      }
      x0 = 0;
      y0+=10;
      y1+=10;
   }
  
   if (rt_event_recv(&oled_event, OLED_EVENT_REFRESH,RT_EVENT_FLAG_AND,
                    RT_WAITING_NO, RT_NULL) != RT_EOK)
   {
        rt_event_send(&oled_event, OLED_EVENT_REFRESH);
   }
}