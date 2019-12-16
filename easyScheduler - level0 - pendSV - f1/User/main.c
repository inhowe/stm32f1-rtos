/**
  ******************************************************************************
  */ 
	
#include "stm32f10x.h"
#include "peripheral.h"

//栈大小
//思考1：最小栈空间8*4Byte，为什么OS_EXCEPT_STK_SIZE是17？
/*
	TASK_1_STK的数据类型是UINT32型，一个元素就可以保存一个寄存器，如果是UCHAR型则当然需要32+1Byte。
	多出来一个元素是因为Task_Create()中用的--ptr的操作符特性就是先减1再赋值。因此数组尾部必定多出一个0元素
	这个多出来元素其实也可以用来作为栈溢出报警。
*/
//思考2：为什么TASK_1_STK_SIZE和TASK_2_STK_SIZE是19，而不是上面说的17？
/*
	TASK_2_STK_SIZE多了一个延时函数，函数内部还有一个临时变量，
	因此如果恰好在延时函数中切换任务，延时函数地址和变量都会占据栈空间
*/
//思考3：为什么TASK_1_STK_SIZE改为17依然能正确工作，而改TASK_2_STK_SIZE为17却不行？
/*
  存粹是运气好，编译器在编译时，task2栈地址跟随在task1后面，task2栈空间不够的话多出来的入栈必然会压入task1栈范围，
  这种越界访问导致工作异常，而如果task1栈空间不够的话也会越界，但不会影响到task2的栈，
	如果task1的栈地址前面的内存空间恰好没人使用，依然是正常工作的
*/
//思考4：屏蔽task1的延时后，为什么task2灯是长短闪而不是单闪？（请把printf函数屏蔽掉）
/*
	这原因就是临界区的问题，翻转IO不是原子操作，在while循环下大概率会在翻转过程进行一半的时候发生切换。
	在反转IO前后分别关闭总中断和开启总中断可以解决问题，试试解除进出临界区的语句
*/
#define OS_EXCEPT_STK_SIZE 17
#define TASK_1_STK_SIZE 19
#define TASK_2_STK_SIZE 19

//类型定义和重映射
typedef unsigned int OS_STK;
typedef unsigned int OS_U32;
typedef void (*OS_TASK)(void *);//函数指针
typedef struct OS_TCB
{
  OS_STK *StkAddr;//存栈顶地址的指针
}OS_TCB,*OS_TCBP;

OS_TCBP g_OS_Tcb_CurP;//当前任务控制块指针 
OS_TCBP g_OS_Tcb_HighRdyP;//就绪任务控制块指针

//异常栈与其栈基址
static OS_STK OS_CPU_ExceptStk[OS_EXCEPT_STK_SIZE];
OS_STK *g_OS_CPU_ExceptStkBase;

//任务栈与其控制块
static OS_STK TASK_1_STK[TASK_1_STK_SIZE];
static OS_STK TASK_2_STK[TASK_2_STK_SIZE];
static OS_TCB TCB_1;
static OS_TCB TCB_2;

//汇编函数声明
extern void OSStart_Asm(void);//OS启动
extern void OSCtxSw(void);//上下文切换
extern OS_U32 OS_CPU_SR_Save(void);    //保存CPU寄存器
extern void OS_CPU_SR_Restore(OS_U32); //恢复CPU寄存器

//临界区控制
#define OS_USE_CRITICAL OS_U32 cpu_sr; //该变量用于保存PRIMASK值
#define OS_ENTER_CRITICAL()    \
  {                            \
    cpu_sr = OS_CPU_SR_Save(); \
  } 																	//进入临界区（注意：该代码不可嵌套使用）
#define OS_EXIT_CRITICAL()     \
  {                            \
    OS_CPU_SR_Restore(cpu_sr); \
  }                                   //退出临界区
	
#define OS_PendSV_Trigger() OSCtxSw() //触发PendSV中断（触发上下文切换）
	
//延时函数
extern vu32 sysCnt;
void delay(u32 cnt)
{
	vu32 old;
	old=sysCnt;//这条语句需要3条汇编语句执行，不是原子操作，可能会被打断，因此不安全（思考：1条汇编语句算原子操作吗？）
	while(1)
	{
		if(sysCnt-old<cnt)
			continue;
		else
			break;
	}
}

//任务1
void task1(void* para) 
{ 
//	OS_USE_CRITICAL;
	para=para;
	while(1)
	{
//		OS_ENTER_CRITICAL();
		LED1_TOGGLE;
//    printf("123"); 
//		OS_EXIT_CRITICAL();
//		delay(200);
	}
}

//任务2
void task2(void* para)
{
//	OS_USE_CRITICAL;
	para=para;
	while(1)
	{
//		OS_ENTER_CRITICAL();
		LED2_TOGGLE;
//    printf("abc");
//		OS_EXIT_CRITICAL();
		delay(250);
	}
}

//任务调度
void Task_Switch(void)
{
	//更改就绪的任务控制块
  if(g_OS_Tcb_CurP == &TCB_1)
    g_OS_Tcb_HighRdyP=&TCB_2;
  else
    g_OS_Tcb_HighRdyP=&TCB_1;
	//上下文切换
  OSCtxSw();
}

//创建任务（初始化栈+初始化任务控制块）
void Task_Create(OS_TCB *tcb,OS_TASK task,OS_STK *stk)
{
    OS_STK  *p_stk;
    p_stk      = stk;
		//R13为SP指针，不需要保存
    *(--p_stk) = (OS_STK)0x01000000uL;								//xPSR，初始值
    *(--p_stk) = (OS_STK)task;												// Entry Point，入口地址，即任务地址
    *(--p_stk) = (OS_STK)0xFFFFFFFFuL;								// R14 (LR) (init value will cause fault if ever used)，存储返回地址，任务不应该返回。
    *(--p_stk) = (OS_STK)0x12121212uL;								// R12，下面的通用寄存器均无需初始化，以编号进行初始化只是方便识别。
    *(--p_stk) = (OS_STK)0x03030303uL;								// R3
    *(--p_stk) = (OS_STK)0x02020202uL;								// R2
    *(--p_stk) = (OS_STK)0x01010101uL;								// R1
    *(--p_stk) = (OS_STK)0x00000000uL;								// R0
    
    *(--p_stk) = (OS_STK)0x11111111uL;								// R11
    *(--p_stk) = (OS_STK)0x10101010uL;								// R10
    *(--p_stk) = (OS_STK)0x09090909uL;								// R9
    *(--p_stk) = (OS_STK)0x08080808uL;								// R8
    *(--p_stk) = (OS_STK)0x07070707uL;								// R7
    *(--p_stk) = (OS_STK)0x06060606uL;								// R6
    *(--p_stk) = (OS_STK)0x05050505uL;								// R5
    *(--p_stk) = (OS_STK)0x04040404uL;								// R4
    
    tcb->StkAddr=p_stk;//初始化的栈顶地址赋值给任务控制块
}

//Systick节拍定时器初始化
void SysTick_Init(void)
{
	/* SystemFrequency / 1000    1ms中断一次
	 * SystemFrequency / 100000	 10us中断一次
	 */
	if (SysTick_Config(SystemCoreClock / 1000))	// ST3.5.0库版本
	{ 
		/* Capture error */ 
		while (1);
	}
	//开启SysTick
	SysTick->CTRL |=  SysTick_CTRL_ENABLE_Msk;
}


int main(void)
{
	//初始化LED, USART1和SysTick
	LED_GPIO_Config();
	USART1_Config();
	SysTick_Init();   

	//栈基址指向栈底（ARM是向下生长型栈，所以栈底在高地址）
	g_OS_CPU_ExceptStkBase = OS_CPU_ExceptStk + OS_EXCEPT_STK_SIZE;

	//创建2个任务
	Task_Create(&TCB_1,task1,&TASK_1_STK[TASK_1_STK_SIZE - 1]);
	Task_Create(&TCB_2,task2,&TASK_2_STK[TASK_2_STK_SIZE - 1]);
	
	//将任务1设为就绪态
	g_OS_Tcb_HighRdyP=&TCB_1;

	//OS启动
	OSStart_Asm(); 
		
	while(1)
	{}
}

/*********************************************END OF FILE**********************/
