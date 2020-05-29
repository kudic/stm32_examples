#include "joystick.h"
#include "stm32f10x.h"

//              ------------------------����Ϊҡ�����õ�������--------------------------------------
//              GND    		��Դ��
//              VCC   		��5V��3.3v��Դ
//              VRX             PA.2
//              VRY             PA.1
//              SW    		PA.0 

/*****************************************************************
 ========================= private variable =================== 
******************************************************************/
volatile static u16 ADC_ConvertedValue[10][3];//����DMA�ɼ�����
volatile static u16 ConvertedValue[3];//����ƽ��ֵ
static struct rt_timer adc_timer;//��ʱ��
static struct {
  char x,y,sw;
  rt_tick_t sw_tick;
}joy_stat;

/*****************************************************************
 ========================= public variable ==================== 
******************************************************************/
struct rt_event joystick_event; 

/*****************************************************************
 ========================= private function =================== 
******************************************************************/
static void Adc_GPIO_Config(void)
{
        GPIO_InitTypeDef GPIO_InitStructure;
	/*ʹ��GPIO��ADC1ͨ��ʱ��*/
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA |RCC_APB2Periph_ADC1	, ENABLE );	  

	/*��PA0����Ϊģ������*/                         
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_2;
	/*��GPIO����Ϊģ������*/
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN;		
	GPIO_Init(GPIOA, &GPIO_InitStructure);	
	
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0;
	/*��GPIO����Ϊģ������*/
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN;		
	GPIO_Init(GPIOA, &GPIO_InitStructure);	
	
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_1;
	/*��GPIO����Ϊģ������*/
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN;		
	GPIO_Init(GPIOA, &GPIO_InitStructure);	
}

static void ADC_Configuration(void)
{
	ADC_InitTypeDef ADC_InitStructure; 
	DMA_InitTypeDef DMA_InitStructure; 
	
	Adc_GPIO_Config();	
        /*72M/6=12,ADC���ʱ�䲻�ܳ���14M*/
	RCC_ADCCLKConfig(RCC_PCLK2_Div6);  
	
	/*������ ADC1 ��ȫ���Ĵ�������ΪĬ��ֵ*/
	ADC_DeInit(ADC1); 
        /*ADC����ģʽ:ADC1��ADC2�����ڶ���ģʽ*/
	ADC_InitStructure.ADC_Mode = ADC_Mode_Independent;	
	/*ģ��ת�������ڵ�ͨ��ģʽ*/
	ADC_InitStructure.ADC_ScanConvMode = ENABLE;	
	/*ģ��ת�������ڵ���ת��ģʽ*/
	ADC_InitStructure.ADC_ContinuousConvMode = ENABLE;
        /*ADCת��������������ⲿ��������*/	
	ADC_InitStructure.ADC_ExternalTrigConv = ADC_ExternalTrigConv_None;	
	/*ADC�����Ҷ���*/
	ADC_InitStructure.ADC_DataAlign = ADC_DataAlign_Right;	
	/*˳����й���ת����ADCͨ������Ŀ*/
	ADC_InitStructure.ADC_NbrOfChannel = 3;	
	/*����ADC_InitStruct��ָ���Ĳ�����ʼ������ADCx�ļĴ���*/   
	ADC_Init(ADC1, &ADC_InitStructure);	
 
	ADC_RegularChannelConfig(ADC1, ADC_Channel_0, 1, ADC_SampleTime_239Cycles5 );
	ADC_RegularChannelConfig(ADC1, ADC_Channel_1, 2, ADC_SampleTime_239Cycles5 );
	ADC_RegularChannelConfig(ADC1, ADC_Channel_2, 3, ADC_SampleTime_239Cycles5 );
        /*ʹ��ָ����ADC1*/
	ADC_Cmd(ADC1, ENABLE);	
	ADC_DMACmd(ADC1, ENABLE);

	/*����ָ����ADC1��У׼�Ĵ���*/
	ADC_ResetCalibration(ADC1);	
	/*��ȡADC1����У׼�Ĵ�����״̬,����״̬��ȴ�*/
	while(ADC_GetResetCalibrationStatus(ADC1));
	/*��ʼָ��ADC1��У׼*/
	ADC_StartCalibration(ADC1);		
        /*��ȡָ��ADC1��У׼����,����״̬��ȴ�*/
	while(ADC_GetCalibrationStatus(ADC1));		
	
	RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1, ENABLE);

	DMA_InitStructure.DMA_PeripheralBaseAddr = (u32)&(ADC1->DR); 
	DMA_InitStructure.DMA_MemoryBaseAddr =(u32)&ADC_ConvertedValue; 
	DMA_InitStructure.DMA_DIR = DMA_DIR_PeripheralSRC; 
	DMA_InitStructure.DMA_BufferSize = 30; 
	DMA_InitStructure.DMA_PeripheralInc = DMA_PeripheralInc_Disable; 
	DMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Enable; 
	DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_HalfWord; 
	DMA_InitStructure.DMA_MemoryDataSize = DMA_MemoryDataSize_HalfWord; 
	DMA_InitStructure.DMA_Mode = DMA_Mode_Circular; 
	DMA_InitStructure.DMA_Priority = DMA_Priority_High; 
	DMA_InitStructure.DMA_M2M = DMA_M2M_Disable; 
		
	DMA_Init(DMA1_Channel1, &DMA_InitStructure); 
	DMA_Cmd(DMA1_Channel1,ENABLE);
	ADC_SoftwareStartConvCmd(ADC1, ENABLE);
}

static void get_Average(void)//ȡƽ��ֵ
{
    u8 i,j;
    int sum;
	
    for(i=0;i<3;i++){
        sum=0;
        for(j=0;j<10;j++){	
            sum+=ADC_ConvertedValue[j][i];
        }
        ConvertedValue[i]=sum/10;
    }
}

static void adc_timeout(void *parameter)
{
  char x,y,sw;
  static eKEY_STATE x_state,y_state,sw_action,sw_state=KEY_STATE_IDLE;
  static unsigned int x_hold_tick,y_hold_tick,sw_hold_tick,sw_tick;
  
  get_Average();
  
  x = ConvertedValue[2]*100/(1<<12);
  y = ConvertedValue[1]*100/(1<<12);
  sw = ConvertedValue[0] < 0x100?1:0;
  
  //ˮƽ����
  if(x != joy_stat.x){
       joy_stat.x = x;
        if (rt_event_recv(&joystick_event, JOY_EVENT_X,RT_EVENT_FLAG_OR,
                      RT_WAITING_NO, RT_NULL) != RT_EOK)
        {
          rt_event_send(&joystick_event, JOY_EVENT_X);//���͡�ҡ��ˮƽ�仯���¼�
        }
  
   }
  
   if(joy_stat.x <45 ||joy_stat.x > 55){
     switch(x_state){
     case KEY_STATE_IDLE:
       x_hold_tick = rt_tick_get();
       x_state = KEY_STATE_ACTION;
       break;
     case KEY_STATE_ACTION:
       if(rt_tick_get()-x_hold_tick > 50){//����
         if(joy_stat.x <45) {
             if (rt_event_recv(&joystick_event, JOY_EVENT_TRUN_LEFT,RT_EVENT_FLAG_OR,
                 RT_WAITING_NO, RT_NULL) != RT_EOK)
             {
                   rt_event_send(&joystick_event, JOY_EVENT_TRUN_LEFT);//���͡�ҡ������ڡ��¼�
             }
         }
         else if(joy_stat.x >55){
             if (rt_event_recv(&joystick_event, JOY_STATE_TRUN_RIGHT,RT_EVENT_FLAG_OR,
                 RT_WAITING_NO, RT_NULL) != RT_EOK)
             {
                   rt_event_send(&joystick_event, JOY_STATE_TRUN_RIGHT);//���͡�ҡ�����Ұڡ��¼�
             }
         }
         else x_state = KEY_STATE_IDLE;
       }
       break;
     default:
       x_state = KEY_STATE_IDLE;
       break;
     }
   }else{
      x_state = KEY_STATE_IDLE;
   }
        
  //��ֱ����
  if( y != joy_stat.y){
        joy_stat.y = y;
        if (rt_event_recv(&joystick_event, JOY_EVENT_Y,RT_EVENT_FLAG_OR,
                      RT_WAITING_NO, RT_NULL) != RT_EOK)
        {
            rt_event_send(&joystick_event, JOY_EVENT_Y);//���͡�ҡ�˴�ֱ�仯���¼�
        }
  }
  if(joy_stat.y <45 ||joy_stat.y > 55){
    switch(y_state){
    case KEY_STATE_IDLE:
      y_hold_tick = rt_tick_get();
      y_state = KEY_STATE_ACTION;
      break;
    case KEY_STATE_ACTION:
      if(rt_tick_get()-y_hold_tick > 50){//����
        if(joy_stat.y <45) {
            if (rt_event_recv(&joystick_event, JOY_STATE_TRUN_UP,RT_EVENT_FLAG_OR,
                RT_WAITING_NO, RT_NULL) != RT_EOK)
            {
                  rt_event_send(&joystick_event, JOY_STATE_TRUN_UP);//���͡�ҡ�����ϰڡ��¼�
            }
        }
        else if(joy_stat.y >55){
            if (rt_event_recv(&joystick_event, JOY_STATE_TRUN_DOWN,RT_EVENT_FLAG_OR,
                RT_WAITING_NO, RT_NULL) != RT_EOK)
            {
                  rt_event_send(&joystick_event, JOY_STATE_TRUN_DOWN);//���͡�ҡ�����°ڡ��¼�
            }
        }
        else y_state = KEY_STATE_IDLE;
      }
      break;
    default:
      y_state = KEY_STATE_IDLE;
      break;
    }
  }else{
     y_state = KEY_STATE_IDLE;
  }
  
  //sw����
  if(sw != joy_stat.sw){
       joy_stat.sw = sw;
       if (rt_event_recv(&joystick_event, JOY_EVENT_KEY,RT_EVENT_FLAG_OR,
                      RT_WAITING_NO, RT_NULL) != RT_EOK)
       {
            rt_event_send(&joystick_event, JOY_EVENT_KEY);
       }
      if(joy_stat.sw == 1){
        sw_action = KEY_STATE_ACTION;
        sw_tick = rt_tick_get();
      }else{
        sw_action = KEY_STATE_IDLE;
      }
  }
  
  switch(sw_state){
  case KEY_STATE_IDLE:
      if(rt_tick_get()- sw_tick > 50){//������
        if(sw_action == KEY_STATE_ACTION){
          sw_hold_tick = rt_tick_get();
          sw_state = KEY_STATE_PRESS;
        }
      }
     break;
  case KEY_STATE_PRESS:
    if(joy_stat.sw == 0){
      if(rt_tick_get() - sw_hold_tick < 2000){
         if (rt_event_recv(&joystick_event, JOY_EVENT_KEY_RELEASE_SHORT,RT_EVENT_FLAG_OR,
                 RT_WAITING_NO, RT_NULL) != RT_EOK)
        {
           rt_event_send(&joystick_event, JOY_EVENT_KEY_RELEASE_SHORT);//���͡�sw�̰��ͷš��¼�
     
        }
      }else{
        if (rt_event_recv(&joystick_event, JOY_EVENT_KEY_RELEASE_LONG,RT_EVENT_FLAG_OR,
                 RT_WAITING_NO, RT_NULL) != RT_EOK)
       {
           rt_event_send(&joystick_event, JOY_EVENT_KEY_RELEASE_LONG);//���͡�sw�����ͷš��¼�
     
       }
      }
      sw_state = KEY_STATE_IDLE;
    }
    break;
  default:
    sw_state = KEY_STATE_IDLE;
    break;
  }
  
}

/*****************************************************************
 ========================= public function =================== 
*****************************************************************/
char get_joytick_x(void) //0~100   ��С��50  �ң�����50  
{
  return joy_stat.x;
}

char get_joytick_y(void)//0~100   �ϣ�С��50  �£�����50  
{
  return joy_stat.y;
}

char get_joytick_sw(void)//0 ��push ��1��nor
{
  return joy_stat.sw;
}

int drv_joystick_init(void)
{
    rt_err_t result;

    ADC_Configuration();
    
        /* ��ʼ���¼����� */
    result = rt_event_init(&joystick_event, "joystick_event", RT_IPC_FLAG_FIFO);
    if (result != RT_EOK)
    {
        rt_kprintf("drv_joystick_init failed.\n");
        return -1;
    }
    
    rt_timer_init(&adc_timer,"adc_timer",adc_timeout,RT_NULL,50,RT_TIMER_FLAG_PERIODIC);
    rt_timer_start(&adc_timer);
    
    rt_kprintf("drv_joystick_init ok.\n");
    return 0;
}


