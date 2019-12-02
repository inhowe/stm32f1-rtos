/**
  ******************************************************************************
  */

#include "stm32f10x.h"
#include "peripheral.h"

//栈大小
#define TASK_IDLE_STK_SIZE 32
#define OS_EXCEPT_STK_SIZE 32
#define TASK_1_STK_SIZE 32
#define TASK_2_STK_SIZE 32

//类型定义和重映射
typedef signed char OS_S8;
typedef signed short OS_S16;
typedef signed int OS_S32;
typedef unsigned char OS_U8;
typedef unsigned short OS_U16;
typedef unsigned int OS_U32;
typedef unsigned int OS_STK;
typedef void (*OS_TASK)(void *); //函数指针

//任务状态枚举
typedef enum OS_TASK_STA
{
  TASK_READY,
  TASK_DELAY,
} OS_TASK_STA;

//任务控制块
typedef struct OS_TCB
{
  OS_STK *StkAddr;   //存栈指针（栈顶地址）的变量（不是栈指针变量）
  OS_U32 TimeDly;    //延时节拍数
  OS_TASK_STA State; //任务状态
} OS_TCB, *OS_TCBP;

#define OS_TASK_MAX_NUM 16       //最大任务数
#define OS_TICKS_PER_SECOND 1000 //系统节拍频率

OS_U8 g_Prio_Cur;                      //当前被选中任务的优先级（正在运行的任务的优先级）
OS_U8 g_Prio_HighRdy;                  //所有就绪任务中最高的优先级
OS_TCBP g_OS_Tcb_CurP;                 //当前被选中任务控制块指针（正在运行的任务的任务控制块指针）
OS_TCBP g_OS_Tcb_HighRdyP;             //所有就绪任务中最高优先级的任务控制块指针
OS_U32 OS_TimeTick;                    //节拍计数
OS_TCBP OS_TCB_TABLE[OS_TASK_MAX_NUM]; //任务控制块表

//异常栈与其栈基址
static OS_STK OS_CPU_ExceptStk[OS_EXCEPT_STK_SIZE];
OS_STK *g_OS_CPU_ExceptStkBase;

//任务栈与其控制块
static OS_STK TASK_IDLE_STK[TASK_IDLE_STK_SIZE];
static OS_STK TASK_1_STK[TASK_1_STK_SIZE];
static OS_STK TASK_2_STK[TASK_2_STK_SIZE];
static OS_TCB TCB_IDLE;
static OS_TCB TCB_1;
static OS_TCB TCB_2;

//汇编函数声明
extern void OSStart_Asm(void);         //OS启动
extern void OSCtxSw(void);             //上下文切换
extern OS_U32 OS_CPU_SR_Save(void);    //保存CPU寄存器
extern void OS_CPU_SR_Restore(OS_U32); //恢复CPU寄存器

#define OS_USE_CRITICAL OS_U32 cpu_sr; //该变量用于保存PRIMASK值
#define OS_ENTER_CRITICAL()    \
  {                            \
    cpu_sr = OS_CPU_SR_Save(); \
  }                                   //进入临界区（注意：该代码不可嵌套使用）
#define OS_EXIT_CRITICAL()     \
  {                            \
    OS_CPU_SR_Restore(cpu_sr); \
  }                                   //退出临界区
	
#define OS_PendSV_Trigger() OSCtxSw() //触发PendSV中断（触发上下文切换）

//任务切换（按优先级顺序寻找第一个就绪的任务）
//注意：只是切换到就绪任务中的最高优先级任务，还没进行上下文切换
//问题：在中断和中断嵌套中，是否需要统计嵌套层数
void OS_Task_Switch(void)
{
  OS_S32 i;
  OS_TCBP tcb_p;
  OS_USE_CRITICAL
  //（此方法不合理，只是便于理解，主要是寻找时间不定，影响实时性，可参考uCOS的查表法，可将寻找时间变为常数）
  for (i = 0; i < OS_TASK_MAX_NUM; i++)
  {
    tcb_p = OS_TCB_TABLE[i];
    if (tcb_p == NULL)
      continue;
    if (tcb_p->State == TASK_READY)
      break;
  }
  OS_ENTER_CRITICAL();
  g_OS_Tcb_HighRdyP = tcb_p;
  g_Prio_HighRdy = i;
  OS_EXIT_CRITICAL();
}

//延时函数（给任务设定延时节拍、调度任务、）
void OS_TimeDly(OS_U32 ticks)
{
  OS_USE_CRITICAL;

  OS_ENTER_CRITICAL();
  g_OS_Tcb_CurP->State = TASK_DELAY;
  g_OS_Tcb_CurP->TimeDly = ticks;
  OS_EXIT_CRITICAL();
  OS_Task_Switch();
  OS_PendSV_Trigger();
}

//任务1
void task1(void *para)
{
  OS_USE_CRITICAL;
  para = para;
  while (1)
  {
    OS_ENTER_CRITICAL();
    LED1_TOGGLE;
    printf("123");
    OS_EXIT_CRITICAL();
    OS_TimeDly(200);
  }
}

//任务2
void task2(void *para)
{
  OS_USE_CRITICAL;
  para = para;
  while (1)
  {
	  OS_ENTER_CRITICAL();
    LED2_TOGGLE;
    printf("abc");
	  OS_EXIT_CRITICAL();
    OS_TimeDly(400);
  }
}

//空闲任务
void OS_Task_Idle(void *para)
{
  while (1)
  {
    __ASM("WFE");//进入睡眠态，降低功耗，也减少任务无意义的切换（试下一下如果在CPU利用率很低的情况下，while疯狂切换重复空闲任务是否有必要？）
								 //进出睡眠态会导致实时性降低，如果看重实时性，可参考uCOS，在任何中断退出时调用OSIntExit()，这样实时性会更加高（不仅仅systick中断可能引起任务切换，其他任何中断都会引起切换），
								 //但正如该函数源码所示，这样操作要记录中断嵌套层数，同时OSIntExit()中的上下文切换函数OSIntCtxSw()和普通的OSCtxSw()理论上还有所区别，会使程序进一步复杂。
    OS_Task_Switch();
    OS_PendSV_Trigger();
  }
}

//创建任务
void OS_Task_Create(OS_TCB *tcb, OS_TASK task, OS_STK *stk, OS_U8 prio)
{
  OS_USE_CRITICAL
  OS_STK *p_stk;

  if (prio >= OS_TASK_MAX_NUM)
    return;

  OS_ENTER_CRITICAL();

  //初始化栈
  p_stk = stk;
//  p_stk = (OS_STK *)((OS_STK)(p_stk) & 0xFFFFFFF8u);

  *(--p_stk) = (OS_STK)0x01000000uL; //xPSR
  *(--p_stk) = (OS_STK)task;         // Entry Point
  *(--p_stk) = (OS_STK)0xFFFFFFFFuL; // R14 (LR)。初始状态下没地方返回了，因此填ff，也可以用一个End任务地址代替
  *(--p_stk) = (OS_STK)0x12121212uL; // R12
  *(--p_stk) = (OS_STK)0x03030303uL; // R3
  *(--p_stk) = (OS_STK)0x02020202uL; // R2
  *(--p_stk) = (OS_STK)0x01010101uL; // R1
  *(--p_stk) = (OS_STK)0x00000000uL; // R0

  *(--p_stk) = (OS_STK)0x11111111uL; // R11
  *(--p_stk) = (OS_STK)0x10101010uL; // R10
  *(--p_stk) = (OS_STK)0x09090909uL; // R9
  *(--p_stk) = (OS_STK)0x08080808uL; // R8
  *(--p_stk) = (OS_STK)0x07070707uL; // R7
  *(--p_stk) = (OS_STK)0x06060606uL; // R6
  *(--p_stk) = (OS_STK)0x05050505uL; // R5
  *(--p_stk) = (OS_STK)0x04040404uL; // R4

  //初始化任务控制块
  tcb->StkAddr = p_stk;
  tcb->TimeDly = 0;
  tcb->State = TASK_READY;
  OS_TCB_TABLE[prio] = tcb;

  OS_EXIT_CRITICAL();
}

//OS初始化
void OS_Init(void)
{
  int i;
  //设定异常栈基址。ARM是满递减栈
  g_OS_CPU_ExceptStkBase = OS_CPU_ExceptStk + OS_EXCEPT_STK_SIZE;
  //关总中断
  __ASM("CPSID   I");
  //初始化任务控制块链表
  for (i = 0; i < OS_TASK_MAX_NUM; i++)
    OS_TCB_TABLE[i] = 0;
  OS_TimeTick = 0;
  //建立一个空闲任务
  OS_Task_Create(&TCB_IDLE, OS_Task_Idle, &TASK_IDLE_STK[TASK_IDLE_STK_SIZE - 1], OS_TASK_MAX_NUM - 1);
}

//OS启动
void OS_Start(void)
{
  //任务切换
  OS_Task_Switch();
  //初始化systick定时器
  if (SysTick_Config(SystemCoreClock / OS_TICKS_PER_SECOND))
    while (1)
      ;
  SysTick->CTRL |= SysTick_CTRL_ENABLE_Msk;
  //OS启动（初始化PendSV；初始化PSP、MSP；设定PendSV触发标志）
  OSStart_Asm();
}

int main(void)
{
  //初始化LED, USART1
	LED_GPIO_Config();
	USART1_Config();

  OS_Init();

  //创建2个任务
  OS_Task_Create(&TCB_1, task1, &TASK_1_STK[TASK_1_STK_SIZE - 1], 10);
  OS_Task_Create(&TCB_2, task2, &TASK_2_STK[TASK_2_STK_SIZE - 1], 11);

  //OS启动
  OS_Start();

  while (1)
  {
  }
}

//systick中断服务函数（给所有任务的延时节拍减1）
//任务的调度函数在空闲任务里触发，uCOS也是
void SysTick_Handler(void)
{
  OS_TCBP tcb_p;
  OS_S32 i;
  OS_USE_CRITICAL

  OS_ENTER_CRITICAL();
  ++OS_TimeTick;
  //给所有任务的节拍数减1
  //该方法还不够快，参考uCOS，利用一个链表，直接跳过所有NULL的任务控制块
  for (i = 0; i < OS_TASK_MAX_NUM; i++)
  {
    tcb_p = OS_TCB_TABLE[i];
    if (tcb_p == NULL)
      continue;
    if (tcb_p->State == TASK_DELAY)
    {
      --tcb_p->TimeDly;	//思考：为什么不用后自减？（前自减效率更高，无需保存副本）
      if (tcb_p->TimeDly == 0)
        tcb_p->State = TASK_READY;
    }
  }
  OS_EXIT_CRITICAL();
}

/*********************************************END OF FILE**********************/
