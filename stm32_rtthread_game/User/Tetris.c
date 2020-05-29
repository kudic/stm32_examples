#include "Tetris.h"
#include "stdio.h"
#include "string.h"
#include "oled.h"
#include "rtthread.h"
#include "joystick.h"
#include "game.h"

/*****************************************************************
 ========================= private variable =================== 
******************************************************************/
static unsigned char block_01[4][4]={
	{0,0,0,0},
	{0,1,1,0},
	{0,1,1,0},
	{0,0,0,0},
};

static unsigned char block_02[4][4]={
	{0,1,0,0},
	{0,1,0,0},
	{0,1,0,0},
	{0,1,0,0},
};


static unsigned char block_03[4][4]={
	{0,0,0,0},
	{0,1,0,0},
	{1,1,1,0},
	{0,0,0,0},
};

static unsigned char block_04[4][4]={
	{0,0,0,0},
	{1,0,0,0},
	{1,1,1,0},
	{0,0,0,0},
};

static unsigned char block_05[4][4]={
	{0,0,0,0},
	{0,0,1,0},
	{1,1,1,0},
	{0,0,0,0},
};

static unsigned char block_06[4][4]={
	{0,0,0,0},
	{0,0,1,0},
	{0,1,1,0},
	{0,1,0,0},
};

static unsigned char block_07[4][4]={
	{0,0,0,0},
	{0,1,0,0},
	{0,1,1,0},
	{0,0,1,0},
};

static unsigned char *block_arry[7]={  //所有不同方块的集合
	(unsigned char*)block_01,
	(unsigned char*)block_02,
	(unsigned char*)block_03,
	(unsigned char*)block_04,
	(unsigned char*)block_05,
	(unsigned char*)block_06,
	(unsigned char*)block_07,
};
#define SPEED_X_MAX       200   //500ms 连续按下的最大移动速度
#define SPEED_Y_MAX       50    //50ms
#define SPPED_REC_MAX     500   //500ms
#define SPPED_RESTART_MAX 1000  //1s
static rt_tick_t speed_x=0,speed_y=0,speed_rev=0,speed_restart=0;

ActiveBlockStruct abs={0};
static int GameScore=0,GameStatus=GAMEPAUSE;

#define THREAD_PRIORITY         6
#define THREAD_STACK_SIZE       512
#define THREAD_TIMESLICE        5
ALIGN(RT_ALIGN_SIZE)
static char Tetris_thread_stack[THREAD_STACK_SIZE];
static struct rt_thread Tetris_thread;

/*****************************************************************
 ========================= public variable ==================== 
******************************************************************/

/*****************************************************************
 ========================= private function =================== 
******************************************************************/
static void BlockCopy(unsigned char (*block_des)[4],unsigned char (*block_src)[4]) //方块复制
{
	int i,j;
	
	for(i=0;i<4;i++){
		for(j=0;j<4;j++){
			block_des[i][j]=block_src[i][j];
		}
	}
}

static void RevolveBlock(unsigned char (*block_xx)[4],unsigned char num)  //"num"顺时钟旋转90度的次数
{
	int i,j,n;
	unsigned char block_temp[4][4];
	
	while(num){
   	memcpy(block_temp,block_xx,16);
		for(i=0,n = 4-1;i<4;i++,n--){
    		for(j=0;j<4;j++){
            block_xx[j][n] = block_temp[i][j];
    		}
   	}
   	num--;
  }
}

static unsigned char __Check_Equal_Arry_(unsigned char *arry,int len,int val)//检测数组每个元素是否都等于val
{
	while(len--){
		if(arry[len]!=val){
			return 0;
		}
	}
	return 1;
}

static void GAME_EliminateBlocks(unsigned char (*gamemap)[WIDTH],int row) //row要消除的行
{
	int i,j;
	
	for(i=row;i>=0;i--){ //逐行下移
		for(j=0;j<WIDTH;j++){
			if(i==0){
				gamemap[i][j]=0;
			}else{
				gamemap[i][j]=gamemap[i-1][j];
			}
		  if(__Check_Equal_Arry_(gamemap[i],WIDTH,0)){ //遇到空行提前结束
		  		break;
		  }
 	  }
  }	
}

static void GAME_GetScore(unsigned char (*gamemap)[WIDTH],int* gamescore)//检查有没有完整的一行 有则消除 并加一分 
{
  int i,ret=0;
	
	for(i=HEIGHT-1;i>=0;i--){ //从下往上扫描
		if(__Check_Equal_Arry_(gamemap[i],WIDTH,0)){ //遇到空行提前结束
				break;
		}
    ret=__Check_Equal_Arry_(gamemap[i],WIDTH,1);//判断i行是否都等于1 return 1表示都等于1
		if(ret){//完整一行，进行消除，下移
				GAME_EliminateBlocks(gamemap,i);
			  (*gamescore)++;
			  i++;//下移后再从该行进行扫描
		}
	}
}

static unsigned char GAME_CheckFillInAllow(unsigned char (*gamemap)[WIDTH],ActiveBlockStruct* AbsStr) //return 0 表示不能填入，return 1 表示能填入
{
	int i,j,x,y,ret = 1;
	unsigned char (*block_cur)[4];
	
	x=AbsStr->x_cur;
	y=AbsStr->y_cur;
	block_cur=AbsStr->block_cur;
	
	for(i=0;i<4;i++){
		for(j=0;j<4;j++){
			if(y+i<0||x+j<0||y+i>=HEIGHT||x+j>=WIDTH){ //出界
				if(block_cur[i][j]){
					ret=0;
					break;
				}
			}else{  //没出界
				if((gamemap[y+i][x+j]+block_cur[i][j])>1){
					ret=0;
					break;
				}
			}
		}
	}
	return ret;
}


static void GAME_BlockDestroyInGameMap(unsigned char (*gamemap)[WIDTH],ActiveBlockStruct* AbsStr)//清楚块，移动前做得工作
{
	int i,j,x,y;
	unsigned char (*block_cur)[4];
	
	x=AbsStr->x_cur;
	y=AbsStr->y_cur;
	block_cur=AbsStr->block_cur;
	
	for(i=0;i<4;i++){
		for(j=0;j<4;j++){
			if(block_cur[i][j]!=0)
			 gamemap[y+i][x+j]=0;
		}
	}
}

static void GAME_BlockFillInGameMap(unsigned char (*gamemap)[WIDTH],ActiveBlockStruct* AbsStr)//将方块填入游戏图像缓存
{
	int i,j,x,y;
	unsigned char (*block_cur)[4];
	
	x=AbsStr->x_cur;
	y=AbsStr->y_cur;
	AbsStr->x_pre=AbsStr->x_cur;
	AbsStr->y_pre=AbsStr->y_cur;
	block_cur=AbsStr->block_cur;
	
	for(i=0;i<4;i++){
		for(j=0;j<4;j++){
			if(block_cur[i][j]!=0)
			gamemap[y+i][x+j]=block_cur[i][j];
		}
	}
}

static unsigned char GAME_NewBlock(unsigned char (*gamemap)[WIDTH],ActiveBlockStruct* AbsStr) //return 0表示不能创建，游戏失败。
{
	unsigned char randomnum,ret=1;
	unsigned int seed;
	
	AbsStr->x_cur=WIDTH/2-2;
	AbsStr->y_cur=0;
	AbsStr->x_pre=AbsStr->x_cur;
	AbsStr->y_pre=AbsStr->y_cur;
        seed = rt_tick_get();
	randomnum=seed%7;//产生0~6的随机数
	//randomnum=0; //for test
	BlockCopy(AbsStr->block_cur,(unsigned char (*)[4])block_arry[randomnum]);
	BlockCopy(AbsStr->block_pre,AbsStr->block_cur);
	if(GAME_CheckFillInAllow(gamemap,AbsStr)==0){
		GameStatus=GAMEPAUSE;
		GameOver();
		rt_kprintf("游戏结束！\r\n");
		ret=0;
	}else{
		GAME_BlockFillInGameMap(gamemap,AbsStr);
	}
	return ret;
}

static unsigned char GAME_BlockTurnDown(unsigned char (*gamemap)[WIDTH],ActiveBlockStruct* AbsStr,int* gamescore)//方块向下移动
{
	unsigned char ret=1;
	
	GAME_BlockDestroyInGameMap(gamemap,AbsStr);
	AbsStr->y_cur++;
	
	if(!GAME_CheckFillInAllow(gamemap,AbsStr)){//不能再往下移动，则判断是否得分并创建新的块
		AbsStr->y_cur--;
		GAME_BlockFillInGameMap(gamemap,AbsStr);//还原
		GAME_GetScore(gamemap,gamescore);
		ret=GAME_NewBlock(gamemap,AbsStr);
	}else{
		GAME_BlockFillInGameMap(gamemap,AbsStr);
	}
	
	return ret;
}

static void GAME_BlockTurnLeftOrRight(unsigned char (*gamemap)[WIDTH],ActiveBlockStruct* AbsStr,int dir)//方块左右移动
{
	GAME_BlockDestroyInGameMap(gamemap,AbsStr);
	if(dir==LEFT){
		AbsStr->x_cur--;
	}else if(dir==RIGHT){
		AbsStr->x_cur++;
	}
	
	if(!GAME_CheckFillInAllow(gamemap,AbsStr)){//不能移动
		AbsStr->x_cur=AbsStr->x_pre;
		AbsStr->y_cur=AbsStr->y_pre;
		GAME_BlockFillInGameMap(gamemap,AbsStr);//还原
	}else{
		GAME_BlockFillInGameMap(gamemap,AbsStr);
	}
}

static void GAME_BlockRevolve(unsigned char (*gamemap)[WIDTH],ActiveBlockStruct* AbsStr)//顺时钟旋转90度
{
	GAME_BlockDestroyInGameMap(gamemap,AbsStr);
	RevolveBlock(AbsStr->block_cur,1);
	
	if(!GAME_CheckFillInAllow(gamemap,AbsStr)){//不能移动
    BlockCopy(AbsStr->block_cur,AbsStr->block_pre);
		GAME_BlockFillInGameMap(gamemap,AbsStr);//还原
	}else{
		GAME_BlockFillInGameMap(gamemap,AbsStr);
	}
}

static unsigned char Game_InputHandle(unsigned char event)//对输入按键事件的处理
{
	unsigned char ret=0;
	
	if(GameStatus==GAMEPAUSE&&event!=RESTART_EV){
		return 0;
	}
	
	switch(event){
	case NON:
		break;
	case TURN_LEFT_EV: //方块向左移动
		if(rt_tick_get()-speed_x > SPEED_X_MAX){
			speed_x = rt_tick_get();
			GAME_BlockTurnLeftOrRight(gamemap,&abs,LEFT);
			ret=1;
		}
		break;
	case TURN_RIGHT_EV://方块向右移动
		if(rt_tick_get()-speed_x > SPEED_X_MAX){
			speed_x = rt_tick_get();
			GAME_BlockTurnLeftOrRight(gamemap,&abs,RIGHT);
			ret=1;
		}
		break;
	case TURN_DOWN_EV://方块向下移动
		if(rt_tick_get()-speed_y > SPEED_Y_MAX){
			speed_y = rt_tick_get();
			ret=GAME_BlockTurnDown(gamemap,&abs,&GameScore);
		}
		break;
	case TURN_UP_EV://方块顺时钟旋转90度
		if(rt_tick_get() - speed_rev > SPPED_REC_MAX){
			speed_rev = rt_tick_get();
			GAME_BlockRevolve(gamemap,&abs);
			ret=1;
		}
		break;
	case RESTART_EV://游戏复位
                if(rt_tick_get() - speed_restart > SPPED_RESTART_MAX){
			speed_restart = rt_tick_get();
			GameStatus=GAMERUNING;
			memset(gamemap,0,HEIGHT*WIDTH);
			GAME_NewBlock(gamemap,&abs);
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
static void Tetris_main(void *parameter)
{
  char event = NON,ret,str[20]={0};
  rt_tick_t autodowm;
  
  while(1){
        event = NON;
        
        if(rt_tick_get()-autodowm > 500){
            autodowm = rt_tick_get();
            event = TURN_DOWN_EV;
        }
        
        if (rt_event_recv(&joystick_event, JOY_EVENT_TRUN_LEFT,RT_EVENT_FLAG_OR|RT_EVENT_FLAG_CLEAR,
                      RT_WAITING_NO, RT_NULL) == RT_EOK)
        {
		TETRIS_DBG("JOY_EVENT_TRUN_LEFT\r\n");
                event = TURN_LEFT_EV;
	}
	
	if (rt_event_recv(&joystick_event, JOY_STATE_TRUN_RIGHT,RT_EVENT_FLAG_OR|RT_EVENT_FLAG_CLEAR,
                      RT_WAITING_NO, RT_NULL) == RT_EOK)
        {
		TETRIS_DBG("JOY_STATE_TRUN_RIGHT\r\n");
                event = TURN_RIGHT_EV;
	}
	
	if (rt_event_recv(&joystick_event, JOY_STATE_TRUN_UP,RT_EVENT_FLAG_OR|RT_EVENT_FLAG_CLEAR,
                      RT_WAITING_NO, RT_NULL) == RT_EOK)
       {
		TETRIS_DBG("JOY_STATE_TRUN_UP\r\n");
                event = TURN_UP_EV;
	}
	
	if (rt_event_recv(&joystick_event, JOY_STATE_TRUN_DOWN,RT_EVENT_FLAG_OR|RT_EVENT_FLAG_CLEAR,
                      RT_WAITING_NO, RT_NULL) == RT_EOK)
       {
		TETRIS_DBG("JOY_STATE_TRUN_DOWN\r\n");
                event = TURN_DOWN_EV;
	}

        if (rt_event_recv(&joystick_event, JOY_EVENT_KEY_RELEASE_SHORT,RT_EVENT_FLAG_OR|RT_EVENT_FLAG_CLEAR,
                      RT_WAITING_NO, RT_NULL) == RT_EOK)
        {
		TETRIS_DBG("JOY_EVENT_KEY_RELEASE_SHORT\r\n");
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

void Tetris_Start(void)
{
  
   GameReady(); //进入游戏准备界面
     //初始化线程
   rt_thread_init(&Tetris_thread,
                   "Tetris_thread",
                   Tetris_main,
                   RT_NULL,
                   &Tetris_thread_stack[0],
                   sizeof(Tetris_thread_stack),
                   THREAD_PRIORITY - 1, THREAD_TIMESLICE);
    rt_thread_startup(&Tetris_thread);
    
    rt_kprintf("Tetris_Start.\n");
    
}

void Tetris_Stop(void)
{
  if(rt_thread_find("Tetris_thread") != RT_NULL)
    rt_thread_detach(&Tetris_thread);
}