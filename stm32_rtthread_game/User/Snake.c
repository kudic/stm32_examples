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
#define SPPED_MOVE        500   //蛇自动前进一步的周期500ms
static rt_tick_t speed_turn=0,speed_restart=0,speed_move=0,speed_max=200;
static int GameScore=0,GameStatus=GAMEPAUSE;

static SNAKELIST SnokeNodeBuff[HEIGHT*WIDTH];//先开辟一些空节点，因为不能用malloc
static int NodeBuffIndex=0,Snokedirection=TURN_UP_EV; //NodeBuffIndex记录所用的节点下标
static SNAKELIST SnokeHeadNode;//链表头节点

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
static void GAME_SnakeDestroyInGameMap(unsigned char (*gamemap)[WIDTH],SNAKELIST* SnakeList)//去掉蛇尾，移动前做得工作
{
	SNAKELIST *n=SnakeList->prev;
	
	gamemap[n->y][n->x]=0;
}

static void GAME_SnakeFillInGameMap(unsigned char (*gamemap)[WIDTH],SNAKELIST* SnakeList)//将蛇图像填入游戏图像缓存
{
	SNAKELIST *n=SnakeList;
	
	while(n->next!=NULL){
		gamemap[n->y][n->x]=1;
		n=n->next;
	}
}

static void GAME_SnakeListAddNode(SNAKELIST* SnakeList,int x,int y) //后部添加节点，吃食物时给链表添加节点
{
	SNAKELIST *h=SnakeList,*p=SnakeList->prev;
	
	SnokeNodeBuff[NodeBuffIndex].x=x;
	SnokeNodeBuff[NodeBuffIndex].y=y;
	SnokeNodeBuff[NodeBuffIndex].next=NULL;
	SnokeNodeBuff[NodeBuffIndex].prev=p;
	
	h->prev=&SnokeNodeBuff[NodeBuffIndex];//头节点的前继节点指向自身
	p->next=&SnokeNodeBuff[NodeBuffIndex];//原尾巴节点的后继节点指向自身
	
	NodeBuffIndex++;//指向SnokeNodeBuff下一个未用节点，为下一次做准备
}

static void GAME_BackgroundInit(unsigned char (*gamemap)[WIDTH]) //游戏背景初始化
{
	int i;
	
	memset(gamemap,0,HEIGHT*WIDTH);
	
	for(i=0;i<WIDTH;i++){ //上下边框
		gamemap[0][i]=1;
		gamemap[HEIGHT-1][i]=1;
	}
	for(i=0;i<HEIGHT;i++){ //左右边框
		gamemap[i][0]=1;
		gamemap[i][WIDTH-1]=1;
	}
}

static void GAME_NewSnake(SNAKELIST* SnakeList) //初始化链表，初始化蛇长度，4+1节
{
	int x=4,y=15,i;//蛇的初始位置
	
	SnakeList->x=x;
	SnakeList->y=y++;
	SnakeList->prev=SnakeList;
	SnakeList->next=SnakeList;
	
	for(i=0;i<4;i++){
		GAME_SnakeListAddNode(SnakeList,x,y+i);
	}
	GAME_SnakeFillInGameMap(gamemap,SnakeList);
}

static void GAME_NewFood(unsigned char (*gamemap)[WIDTH]) //在地图上随机产生新的食物
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

static unsigned char GAME_SnakeMove(unsigned char (*gamemap)[WIDTH],SNAKELIST* SnakeList,int dir,int *gamescore) // 蛇移动，dir=方向
{
	SNAKELIST *p,*h;
	unsigned char val,ret=1;

	GAME_SnakeDestroyInGameMap(gamemap,SnakeList); //去掉蛇尾，移动前做得工作
	
	h=SnakeList;
	p=SnakeList->prev;//指向前继节点，头节点的前继节点是尾巴
	
	//蛇移动
	while(p!=SnakeList){//从最后一个节点开始，后继结点的x、y等于前继节点的x、y，直到头节点为止,头节点的x、y先不变
		p->x=p->prev->x; //等于该节点的前继节点的x
		p->y=p->prev->y; //等于该节点的前继节点的y
		p=p->prev;
	}
	switch(dir){
		case TURN_UP_EV: //上
			h->y--;
			break;
		case TURN_DOWN_EV: //下
			h->y++;
			break;
		case TURN_LEFT_EV: //左
			h->x--;
			break;
		case TURN_RIGHT_EV: //右
			h->x++;
			break;
		default:
			break;
	}
	val=gamemap[h->y][h->x]; //蛇头将要经过的点
	
	switch(val){
		case 0: //可以移动
			break;
		case 1: //碰到障碍物或者蛇身
			GameStatus=GAMEPAUSE;
			GameOver();
			SNAKE_DBG("游戏结束！\r\n");
			ret=0;//游戏失败
			break;
		case 2: //吃到食物
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

static unsigned char Game_InputHandle(unsigned char event)//对输入按键事件的处理
{
	unsigned char ret=0;
	
	if(GameStatus==GAMEPAUSE&&event!=RESTART_EV){ //游戏状态未运行时，除非按下restart，否则不进入
		return 0;
	}
	
	if(event==5-Snokedirection){//按下蛇前进的相反方向时，忽略
		event=NON; 
	}
	
	switch(event){
	case NON:
                  speed_max=200;
                  if((rt_tick_get()-speed_move) > SPPED_MOVE){ //自动前进
                      speed_move = rt_tick_get();
                      event=Snokedirection;
                  }else{
                      break;
                  }
	case TURN_LEFT_EV: //蛇向左移动
                case TURN_RIGHT_EV://蛇向右移动
                case TURN_DOWN_EV://蛇向下移动	
	case TURN_UP_EV://蛇向上移动	
                   if((rt_tick_get() - speed_turn) > speed_max){
                           speed_turn = rt_tick_get();
                           speed_max=150;
                           Snokedirection=event;
                           ret=GAME_SnakeMove(gamemap,&SnokeHeadNode,event,&GameScore);	
                   }
                   break;
	case RESTART_EV://游戏复位
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

static void GameReady(void) //游戏初始界面，等待restart按键按下开始游戏
{
	OLED_Clean();
	OLED_DispString(0,30,"Please");
	OLED_DispString(0,40,"press the");
	OLED_DispString(0,50,"reset key");
	OLED_DispString(0,60,"to start.");
	OLED_DispString(0,70,"O.O");
}
static void GameOver(void) //游戏接收界面，按下restart按键继续游戏
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
            OLED_DispString(0,0,str); //屏幕上方显示分数
        }
        rt_thread_mdelay(30);
  }
}

void Snake_Start(void)
{
  
   /*进入游戏准备界面*/
   GameReady(); 
   /*初始化线程*/
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