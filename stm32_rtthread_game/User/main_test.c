#include "rtthread.h"

#define THREAD_STACK_SIZE       512
#define THREAD_TIMESLICE        5

ALIGN(RT_ALIGN_SIZE)
static char thread1_stack[THREAD_STACK_SIZE];
static struct rt_thread thread1;

ALIGN(RT_ALIGN_SIZE)
static char thread2_stack[THREAD_STACK_SIZE];
static struct rt_thread thread2;

int count1=0,count2=0;

static void __delay(unsigned int t) //32us 后续可考虑使用 rt_rhread_delay
{
    char i = 100;
    while(t--)
    {
	while(i--);
    }
}

void thread1_entry(void* parameter)
{
    while(1){
        __delay(5000);
        rt_kprintf("thread 1 runing.\r\n");
        //rt_thread_mdelay(1);
    }
}

void thread2_entry(void* parameter)
{
    while(1){
        count2++;
        rt_kprintf("thread 2 runing.\r\n");
        rt_thread_mdelay(500);
    }
}

int main(void)
{
    rt_thread_init(&thread1,
                   "thread1",
                   thread1_entry,
                   RT_NULL,
                   &thread1_stack[0],
                   sizeof(thread1_stack),
                   4, THREAD_TIMESLICE);
    rt_thread_startup(&thread1);
    
    rt_thread_init(&thread2,
                   "thread2",
                   thread2_entry,
                   RT_NULL,
                   &thread2_stack[0],
                   sizeof(thread2_stack),
                   1, THREAD_TIMESLICE);
    rt_thread_startup(&thread2);
    
    while(1){
      //rt_kprintf("count1=%d,count2=%d.\r\n",count1,count2);
      rt_thread_mdelay(500);
    }
}
