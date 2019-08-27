/**
  ******************************************************************************
  * @file    main.c
  * @author  fire
  * @version V1.0
  * @date    2013-xx-xx
  * @brief   测试led
  ******************************************************************************
  * @attention
  *
  * 实验平台:野火 iSO-MINI STM32 开发板 
  * 论坛    :http://www.chuxue123.com
  * 淘宝    :http://firestm32.taobao.com
  *
  ******************************************************************************
  */ 
	
#include "stm32f10x.h"
#include "bsp_led.h"
#include "scheduler.h"

//定义最小栈
//17的含义：一共有16个寄存器需要保存（其中8个会自动保存），由于ARM是满栈向下生长型，因此需要多设一个长度
//17是最小栈长度? 任务内创建数组将会溢出，但是为什么创建多个变量不会?
#define MIN_STACK_SIZE	27

#define HW32_REG(ADDRESS)  (*((volatile unsigned long  *)(ADDRESS)))

uint32_t  curr_task=0;      //当前执行任务
uint32_t  next_task=1;      //下一个任务
uint32_t  task0_stack[MIN_STACK_SIZE];	//任务0的栈。

uint32_t  task1_stack[MIN_STACK_SIZE];	//任务1的栈
uint32_t  PSP_array[4];			//PSP数组，存储栈地址

u8 task0_handle=1;					//与调度无关
u8 task1_handle=1;					//与调度无关

int add(int x)
{
	int y=x;
	return y+x;
}

//任务0
void task0(void) 
{ 
    while(1)
    {
        if(task0_handle==1)
        {
            LED2_TOGGLE;
            task0_handle=0;
            task1_handle=1;
        }
    }
}

//任务1
uint32_t temp1,temp2,temp3,temp4;
void task1(void)
{
//		int a[3];		
		int b=4;	
		int d=b-4;	
		int e=d+2;
    while(1)
    {
        if(task1_handle==1)
        {
						d=add(b);
//						b=d;
            LED1_TOGGLE;
            task1_handle=0;
            task0_handle=1;
        }
    }
}


//Systick节拍定时器初始化
void SysTick_Init(void)
{
	/* SystemFrequency / 1000    1ms中断一次
	 * SystemFrequency / 100000	 10us中断一次
	 * SystemFrequency / 1000000 1us中断一次
	 */
//	if (SysTick_Config(SystemFrequency / 100000))	// ST3.0.0库版本
	if (SysTick_Config(SystemCoreClock / 1000))	// ST3.5.0库版本
	{ 
		/* Capture error */ 
		while (1);
	}
		// 关闭滴答定时器  
//	SysTick->CTRL &= ~ SysTick_CTRL_ENABLE_Msk;
	SysTick->CTRL |=  SysTick_CTRL_ENABLE_Msk;
}

int temp=0;
#define DEBUG 0
int main(void)
{
		//初始化PendSV和LED
    SetPendSVPro();
    LED_GPIO_Config();
    
#if !DEBUG
//		/*初始化任务堆栈*/
//		//PSP_array中存储的为task0_stack数组的首地址，即task0_stack[0]地址
		PSP_array[0] = ((unsigned int) task0_stack);
	
    //初始化栈内容，只需要初始化入口地址也就是PC指针的指向值和xPSR寄存器即可，其他通用寄存器复位后无需初始化。
	  //14和15含义：进入PendSV中断后会自动保存8个重要寄存器，最先保存的是xPSR和PC寄存器，排除第16个空栈，应该是14和15。
		//为什么14是PC,15是xPSR，这和权威指南不一致，但是和μC/OS系统一致，难道是权威指南出错了？
		//对于未访问（求址和求值）的变量，编译器可能不会将其安排进栈里面，而是直接放在通用寄存器计算。
		//因此存在一个问题，如果任务在进while前建立了需要访问的变量，将会引起栈变化，由于PSP指针在任务运行前就已经被指向了栈底
		//任务开始运行时，由于新建变量，会导致栈变化，结果使得栈数组的14和15位置则会被变量覆盖，那么新的栈指针位置如何确定从而避免该问题？
    HW32_REG((PSP_array[0] + 14*sizeof(uint32_t) )) = (unsigned long) task0; /* PC */
    HW32_REG((PSP_array[0] + 15*sizeof(uint32_t) )) = 0x01000000;            /* xPSR */
    
    PSP_array[1] = ((unsigned int) task1_stack);
    HW32_REG((PSP_array[1] + 14*sizeof(uint32_t))) = (unsigned long) task1; /* PC */
    HW32_REG((PSP_array[1] + 15*sizeof(uint32_t))) = 0x01000000;            /* xPSR */    
#else
		//PSP_array中存储的为task0_stack数组的尾地址-16*4，即task0_stack[1023-16]地址
    PSP_array[0] = ((unsigned int) task0_stack)+sizeof(task0_stack) ;
    //初始化栈内容，只需要初始化入口地址也就是PC指针的指向值和xPSR寄存器即可，其他通用寄存器复位后无需初始化。
	  //14和15含义：进入PendSV中断后会自动保存8个重要寄存器，最先保存的是xPSR和PC寄存器，排除第16个空栈，应该是14和15。
    HW32_REG((PSP_array[0] - 2 )) = (unsigned long) task0; /* PC */
    HW32_REG((PSP_array[0] - 1 )) = 0x01000000;            /* xPSR */
    
    PSP_array[1] = ((unsigned int) task1_stack)+sizeof(task1_stack) ;
    HW32_REG((PSP_array[1] - 2 )) = (unsigned long) task1; /* PC */
    HW32_REG((PSP_array[1] - 1 )) = 0x01000000;            /* xPSR */   
#endif		
     
    /* 设置PSP指向任务0堆栈的栈顶（应该是底吧，虽然是高地址） */
#if !DEBUG
    __set_PSP((PSP_array[curr_task] + (MIN_STACK_SIZE-1)*sizeof(uint32_t))); 
#else
		__set_PSP((PSP_array[curr_task = 0,curr_task])); 
#endif

		//初始化节拍定时器
    SysTick_Init();   
		
    /* 使用堆栈指针，非特权级状态 */
    __set_CONTROL(0x3);//使用用户级线程模式+使用PSP堆栈指针，为什么不能放在SysTick初始化函数前面
    
    /* 改变CONTROL后执行ISB (architectural recommendation) */
//    __ISB();

    /* 启动任务0 */
    task0();  
		
    while(1)
		{
			;
		}
}


/*********************************************END OF FILE**********************/
