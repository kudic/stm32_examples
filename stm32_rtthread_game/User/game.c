#include "game.h"
#include "rtthread.h"

unsigned char gamemap[HEIGHT][WIDTH]={0}; //��Ϸͼ������ػ���

void PrintfBlock(unsigned char *block_xx,int height,int width)//����Ϸͼ��ͨ�����ڴ�ӡ���������ڵ��Է���
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

