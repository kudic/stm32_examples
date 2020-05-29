#include "game.h"
#include "rtthread.h"

unsigned char gamemap[HEIGHT][WIDTH]={0}; //游戏图像的像素缓存

void PrintfBlock(unsigned char *block_xx,int height,int width)//把游戏图像通过串口打印出来，便于调试分析
{
	int i,j,count=0;

	rt_kprintf("****************\r\n");
	for(i=0;i<height;i++){
		for(j=0;j<width;j++){
			rt_kprintf("%2d ",*(block_xx+count++));
		}
		rt_kprintf("\r\n");
	}
	rt_kprintf("\r\n");
}

