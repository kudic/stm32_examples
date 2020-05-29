#include "Snake.h"
#include "stdio.h"
#include "string.h"
#include "oled.h"
#include "rtthread.h"
#include "joystick.h"

/*****************************************************************
 ========================= private variable =================== 
******************************************************************/
#define SPPED_RESTART_MAX 1000  //1
#define SPPED_MOVE        500   //���Զ�ǰ��һ��������500ms
static rt_tick_t speed_turn=0,speed_restart=0,speed_move=0,speed_max=200;
static int GameScore=0,GameStatus=GAMEPAUSE;

static SNAKELIST SnokeNodeBuff[HEIGHT*WIDTH];//�ȿ���һЩ�սڵ㣬��Ϊ������malloc
static int NodeBuffIndex=0,Snokedirection=TURN_UP_EV; //NodeBuffIndex��¼���õĽڵ��±�
static SNAKELIST SnokeHeadNode;//����ͷ�ڵ�

#define THREAD_PRIORITY         6
#define THREAD_STACK_SIZE       512
#define THREAD_TIMESLICE        5

ALIGN(RT_ALIGN_SIZE)
static char Snake_thread_stack[THREAD_STACK_SIZE];
static struct rt_thread Snake_thread;
/*****************************************************************
 ========================= public variable ==================== 
******************************************************************/
//...
//...
//...

/*****************************************************************
 ========================= private function =================== 
******************************************************************/
static void GAME_SnakeDestroyInGameMap(unsigned char (*gamemap)[WIDTH],SNAKELIST* SnakeList)//ȥ����β���ƶ�ǰ���ù���
{
	SNAKELIST *n=SnakeList->prev;
	
	gamemap[n->y][n->x]=0;
}

static void GAME_SnakeFillInGameMap(unsigned char (*gamemap)[WIDTH],SNAKELIST* SnakeList)//����ͼ��������Ϸͼ�񻺴�
{
	SNAKELIST *n=SnakeList;
	
	while(n->next!=NULL){
		gamemap[n->y][n->x]=1;
		n=n->next;
	}
}

static void GAME_SnakeListAddNode(SNAKELIST* SnakeList,int x,int y) //����ӽڵ㣬��ʳ��ʱ��������ӽڵ�
{
	SNAKELIST *h=SnakeList,*p=SnakeList->prev;
	
	SnokeNodeBuff[NodeBuffIndex].x=x;
	SnokeNodeBuff[NodeBuffIndex].y=y;
	SnokeNodeBuff[NodeBuffIndex].next=NULL;
	SnokeNodeBuff[NodeBuffIndex].prev=p;
	
	h->prev=&SnokeNodeBuff[NodeBuffIndex];//ͷ�ڵ��ǰ�̽ڵ�ָ������
	p->next=&SnokeNodeBuff[NodeBuffIndex];//ԭβ�ͽڵ�ĺ�̽ڵ�ָ������
	
	NodeBuffIndex++;//ָ��SnokeNodeBuff��һ��δ�ýڵ㣬Ϊ��һ����׼��
}

static void GAME_BackgroundInit(unsigned char (*gamemap)[WIDTH]) //��Ϸ������ʼ��
{
	int i;
	
	memset(gamemap,0,HEIGHT*WIDTH);
	
	for(i=0;i<WIDTH;i++){ //���±߿�
		gamemap[0][i]=1;
		gamemap[HEIGHT-1][i]=1;
	}
	for(i=0;i<HEIGHT;i++){ //���ұ߿�
		gamemap[i][0]=1;
		gamemap[i][WIDTH-1]=1;
	}
}

static void GAME_NewSnake(SNAKELIST* SnakeList) //��ʼ��������ʼ���߳��ȣ�4+1��
{
	int x=4,y=15,i;//�ߵĳ�ʼλ��
	
	SnakeList->x=x;
	SnakeList->y=y++;
	SnakeList->prev=SnakeList;
	SnakeList->next=SnakeList;
	
	for(i=0;i<4;i++){
		GAME_SnakeListAddNode(SnakeList,x,y+i);
	}
	GAME_SnakeFillInGameMap(gamemap,SnakeList);
}

static void GAME_NewFood(unsigned char (*gamemap)[WIDTH]) //�ڵ�ͼ����������µ�ʳ��
{
	rt_tick_t seed1,seed2;
	int x,y;
	
	while(1){
                seed1 = rt_tick_get();
		seed2 = rt_tick_get();
		x=seed1%WIDTH;
		y=seed2%HEIGHT;
		if(gamemap[y][x]==0){
			gamemap[y][x]=2;
			break;
		}
	}
}

static unsigned char GAME_SnakeMove(unsigned char (*gamemap)[WIDTH],SNAKELIST* SnakeList,int dir,int *gamescore) // ���ƶ���dir=����
{
	SNAKELIST *p,*h;
	unsigned char val,ret=1;

	GAME_SnakeDestroyInGameMap(gamemap,SnakeList); //ȥ����β���ƶ�ǰ���ù���
	
	h=SnakeList;
	p=SnakeList->prev;//ָ��ǰ�̽ڵ㣬ͷ�ڵ��ǰ�̽ڵ���β��
	
	//���ƶ�
	while(p!=SnakeList){//�����һ���ڵ㿪ʼ����̽���x��y����ǰ�̽ڵ��x��y��ֱ��ͷ�ڵ�Ϊֹ,ͷ�ڵ��x��y�Ȳ���
		p->x=p->prev->x; //���ڸýڵ��ǰ�̽ڵ��x
		p->y=p->prev->y; //���ڸýڵ��ǰ�̽ڵ��y
		p=p->prev;
	}
	switch(dir){
		case TURN_UP_EV: //��
			h->y--;
			break;
		case TURN_DOWN_EV: //��
			h->y++;
			break;
		case TURN_LEFT_EV: //��
			h->x--;
			break;
		case TURN_RIGHT_EV: //��
			h->x++;
			break;
		default:
			break;
	}
	val=gamemap[h->y][h->x]; //��ͷ��Ҫ�����ĵ�
	
	switch(val){
		case 0: //�����ƶ�
			break;
		case 1: //�����ϰ����������
			GameStatus=GAMEPAUSE;
			GameOver();
			SNAKE_DBG("��Ϸ������\r\n");
			ret=0;//��Ϸʧ��
			break;
		case 2: //�Ե�ʳ��
			GAME_SnakeListAddNode(SnakeList,h->x,h->y);
		  GAME_NewFood(gamemap);
		  (*gamescore)++;
			break;
		default:
			break;
	}

	GAME_SnakeFillInGameMap(gamemap,SnakeList);
	return ret;
}

static unsigned char Game_InputHandle(unsigned char event)//�����밴���¼��Ĵ���
{
	unsigned char ret=0;
	
	if(GameStatus==GAMEPAUSE&&event!=RESTART_EV){ //��Ϸ״̬δ����ʱ�����ǰ���restart�����򲻽���
		return 0;
	}
	
	if(event==5-Snokedirection){//������ǰ�����෴����ʱ������
		event=NON; 
	}
	
	switch(event){
	case NON:
                  speed_max=200;
                  if((rt_tick_get()-speed_move) > SPPED_MOVE){ //�Զ�ǰ��
                      speed_move = rt_tick_get();
                      event=Snokedirection;
                  }else{
                      break;
                  }
	case TURN_LEFT_EV: //�������ƶ�
                case TURN_RIGHT_EV://�������ƶ�
                case TURN_DOWN_EV://�������ƶ�	
	case TURN_UP_EV://�������ƶ�	
                   if((rt_tick_get() - speed_turn) > speed_max){
                           speed_turn = rt_tick_get();
                           speed_max=150;
                           Snokedirection=event;
                           ret=GAME_SnakeMove(gamemap,&SnokeHeadNode,event,&GameScore);	
                   }
                   break;
	case RESTART_EV://��Ϸ��λ
                    if((rt_tick_get() - speed_restart) > SPPED_RESTART_MAX){
                         speed_restart = rt_tick_get();
                         GameStatus=GAMERUNING;
                         GameScore=0;
                          Snokedirection=TURN_UP_EV;
                         GAME_BackgroundInit(gamemap);
                         GAME_NewSnake(&SnokeHeadNode);
                         GAME_NewFood(gamemap);
                         ret=1;
                    }
                    break;
	default:
                    //error
                    break;
	}
	return ret;
}

static void GameReady(void) //��Ϸ��ʼ���棬�ȴ�restart�������¿�ʼ��Ϸ
{
	OLED_Clean();
	OLED_DispString(0,30,"Please");
	OLED_DispString(0,40,"press the");
	OLED_DispString(0,50,"reset key");
	OLED_DispString(0,60,"to start.");
	OLED_DispString(0,70,"O.O");
}
static void GameOver(void) //��Ϸ���ս��棬����restart����������Ϸ
{
	OLED_Clean();
	OLED_DispString(0,50,"GAME OVER");
}

/*****************************************************************
 ========================= public function =================== 
*****************************************************************/
static void Snake_main(void *parameter)
{
  char event = NON;
  char ret;
  char str[20]={0};
  
  while(1){
        event = NON;
        if (rt_event_recv(&joystick_event, JOY_EVENT_TRUN_LEFT,RT_EVENT_FLAG_OR|RT_EVENT_FLAG_CLEAR,
                      RT_WAITING_NO, RT_NULL) == RT_EOK)
        {
            SNAKE_DBG("JOY_EVENT_TRUN_LEFT\r\n");
            event = TURN_LEFT_EV;
        }
	
        if (rt_event_recv(&joystick_event, JOY_STATE_TRUN_RIGHT,RT_EVENT_FLAG_OR|RT_EVENT_FLAG_CLEAR,
                      RT_WAITING_NO, RT_NULL) == RT_EOK)
        {
            SNAKE_DBG("JOY_STATE_TRUN_RIGHT\r\n");
            event = TURN_RIGHT_EV;
        }
	
        if (rt_event_recv(&joystick_event, JOY_STATE_TRUN_UP,RT_EVENT_FLAG_OR|RT_EVENT_FLAG_CLEAR,
                      RT_WAITING_NO, RT_NULL) == RT_EOK)
       {
            SNAKE_DBG("JOY_STATE_TRUN_UP\r\n");
            event = TURN_UP_EV;
        }
	
        if (rt_event_recv(&joystick_event, JOY_STATE_TRUN_DOWN,RT_EVENT_FLAG_OR|RT_EVENT_FLAG_CLEAR,
                      RT_WAITING_NO, RT_NULL) == RT_EOK)
       {
            SNAKE_DBG("JOY_STATE_TRUN_DOWN\r\n");
            event = TURN_DOWN_EV;
        }

        if (rt_event_recv(&joystick_event, JOY_EVENT_KEY_RELEASE_SHORT,RT_EVENT_FLAG_OR|RT_EVENT_FLAG_CLEAR,
                      RT_WAITING_NO, RT_NULL) == RT_EOK)
        {
            SNAKE_DBG("JOY_EVENT_KEY_RELEASE_SHORT\r\n");
            event = RESTART_EV;
        }
        
        ret = Game_InputHandle(event);
        if(ret){
            GameMapToLcdCache((unsigned char*)gamemap);
            //PrintfBlock((unsigned char*)gamemap,HEIGHT,WIDTH); 
            memset(str,0,sizeof(str));
            sprintf(str,"Score=%d",GameScore);
            OLED_DispString(0,0,str); //��Ļ�Ϸ���ʾ����
        }
        rt_thread_mdelay(30);
  }
}

void Snake_Start(void)
{
  
   /*������Ϸ׼������*/
   GameReady(); 
   /*��ʼ���߳�*/
   rt_thread_init(&Snake_thread,
                   "Snake_thread",
                   Snake_main,
                   RT_NULL,
                   &Snake_thread_stack[0],
                   sizeof(Snake_thread_stack),
                   THREAD_PRIORITY - 1, THREAD_TIMESLICE);
    rt_thread_startup(&Snake_thread);
    
    rt_kprintf("Snake_Start.\n");
    
}

void Snake_Stop(void)
{
  if(rt_thread_find("Snake_thread") != RT_NULL)
    rt_thread_detach(&Snake_thread);
}