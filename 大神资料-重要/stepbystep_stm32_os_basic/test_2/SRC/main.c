#include "stdio.h"
#include "stm32f4xx.h"

#define OS_EXCEPT_STK_SIZE 1024
#define TASK_1_STK_SIZE 128
#define TASK_2_STK_SIZE 128
#define TASK_3_STK_SIZE 128

#define TASK_IDLE_STK_SIZE 1024
#define OS_TASK_MAX_NUM 32
#define OS_TICKS_PER_SECOND 1000

#define  OS_USE_CRITICAL    OS_U32 cpu_sr;
#define  OS_ENTER_CRITICAL()  {cpu_sr = OS_CPU_SR_Save();}
#define  OS_EXIT_CRITICAL()   {OS_CPU_SR_Restore(cpu_sr);}
#define  OS_PendSV_Trigger() OSCtxSw()

typedef signed char OS_S8;
typedef signed short OS_S16;
typedef signed int OS_S32;
typedef unsigned char OS_U8;
typedef unsigned short OS_U16;
typedef unsigned int OS_U32;
typedef unsigned int OS_STK;

typedef void (*OS_TASK)(void);

typedef enum OS_TASK_STA
{
  TASK_READY,
  TASK_DELAY,
} OS_TASK_STA;

typedef struct OS_TCB
{
  OS_STK *StkAddr;
  OS_U32 TimeDly;
  OS_TASK_STA State;
}OS_TCB,*OS_TCBP;

OS_TCBP OS_TCB_TABLE[OS_TASK_MAX_NUM];
OS_TCBP g_OS_Tcb_CurP; 
OS_TCBP g_OS_Tcb_HighRdyP;
OS_U32 OS_TimeTick;
OS_U8 g_Prio_Cur; 
OS_U8 g_Prio_HighRdy;

static OS_STK OS_CPU_ExceptStk[OS_EXCEPT_STK_SIZE];
OS_STK *g_OS_CPU_ExceptStkBase;

static OS_TCB TCB_1;
static OS_TCB TCB_2;
static OS_TCB TCB_3;
static OS_TCB TCB_IDLE;
static OS_STK TASK_1_STK[TASK_1_STK_SIZE];
static OS_STK TASK_2_STK[TASK_2_STK_SIZE];
static OS_STK TASK_3_STK[TASK_3_STK_SIZE];
static OS_STK TASK_IDLE_STK[TASK_IDLE_STK_SIZE];

extern OS_U32 SystemCoreClock;

extern void OSStart_Asm(void);
extern void OSCtxSw(void);
extern OS_U32 OS_CPU_SR_Save(void);
extern void OS_CPU_SR_Restore(OS_U32);

void task_1(void);
void task_2(void);
void task_3(void);

void OS_Task_Idle(void);
void OS_TimeDly(OS_U32);
void OS_Task_Switch(void);
void OS_Task_Create(OS_TCB *,OS_TASK,OS_STK *,OS_U8);
void OS_Task_Delete(OS_U8);
void OS_Task_End(void);
void OS_Init(void);
void OS_Start(void);

void task_1(void)
{
    printf("[%d]Task 1 Runing!!!\n",OS_TimeTick);

    OS_Task_Create(&TCB_2,task_2,&TASK_2_STK[TASK_2_STK_SIZE-1],5);
    OS_Task_Create(&TCB_3,task_3,&TASK_3_STK[TASK_3_STK_SIZE-1],7);
}

void task_2(void)
{
  while(1)
  {
    printf("[%d]Task 2 Runing!!!\n",OS_TimeTick);
    OS_TimeDly(1000);
  }
}

void task_3(void)
{
  while(1)
  {
    printf("[%d]Task 3 Runing!!!\n",OS_TimeTick);
    OS_TimeDly(1500);
  }
}

void OS_Task_Idle(void)
{
  while(1)
  {
    asm("WFE"); 
    OS_Task_Switch();
    OS_PendSV_Trigger();
  }
}

void OS_TimeDly(OS_U32 ticks)
{
    OS_USE_CRITICAL
    
    OS_ENTER_CRITICAL();
    g_OS_Tcb_CurP->State=TASK_DELAY;
    g_OS_Tcb_CurP->TimeDly=ticks;
    OS_EXIT_CRITICAL();
    OS_Task_Switch();
    OS_PendSV_Trigger();
}

void OS_Task_Switch(void)
{
  OS_S32 i;
  OS_TCBP tcb_p;
  OS_USE_CRITICAL
  for(i=0;i<OS_TASK_MAX_NUM;i++)
  {
    tcb_p=OS_TCB_TABLE[i];
    if(tcb_p == NULL) continue;
    if(tcb_p->State==TASK_READY) break;
  }
  OS_ENTER_CRITICAL();
  g_OS_Tcb_HighRdyP=tcb_p;
  g_Prio_HighRdy=i;
  OS_EXIT_CRITICAL();
}

void OS_Task_Delete(OS_U8 prio)
{
  if(prio >= OS_TASK_MAX_NUM) return;
  OS_TCB_TABLE[prio]=0;
}

void OS_Task_End(void)
{
  printf("Task of Prio %d End\n",g_Prio_Cur);
  OS_Task_Delete(g_Prio_Cur);
  OS_Task_Switch();
  OS_PendSV_Trigger();
}

void OS_Task_Create(OS_TCB *tcb,OS_TASK task,OS_STK *stk,OS_U8 prio)
{
    OS_USE_CRITICAL
    OS_STK  *p_stk; 
    if(prio >= OS_TASK_MAX_NUM) return;
  
    OS_ENTER_CRITICAL();

    p_stk      = stk;
    p_stk      = (OS_STK *)((OS_STK)(p_stk) & 0xFFFFFFF8u);
    
    *(--p_stk) = (OS_STK)0x01000000uL;                          //xPSR
    *(--p_stk) = (OS_STK)task;                                  // Entry Point
    *(--p_stk) = (OS_STK)OS_Task_End;                                     // R14 (LR)
    *(--p_stk) = (OS_STK)0x12121212uL;                          // R12
    *(--p_stk) = (OS_STK)0x03030303uL;                          // R3
    *(--p_stk) = (OS_STK)0x02020202uL;                          // R2
    *(--p_stk) = (OS_STK)0x01010101uL;                          // R1
    *(--p_stk) = (OS_STK)0x00000000u;                           // R0
    
    *(--p_stk) = (OS_STK)0x11111111uL;                          // R11
    *(--p_stk) = (OS_STK)0x10101010uL;                          // R10
    *(--p_stk) = (OS_STK)0x09090909uL;                          // R9
    *(--p_stk) = (OS_STK)0x08080808uL;                          // R8
    *(--p_stk) = (OS_STK)0x07070707uL;                          // R7
    *(--p_stk) = (OS_STK)0x06060606uL;                          // R6
    *(--p_stk) = (OS_STK)0x05050505uL;                          // R5
    *(--p_stk) = (OS_STK)0x04040404uL;                          // R4
    
    tcb->StkAddr=p_stk;
    tcb->TimeDly=0;
    tcb->State=TASK_READY;
    OS_TCB_TABLE[prio]=tcb; 

    OS_EXIT_CRITICAL();
}

void SysTick_Handler(void)
{
  
  OS_TCBP tcb_p;
  OS_S32 i;
  OS_USE_CRITICAL
    
  OS_ENTER_CRITICAL();
  ++OS_TimeTick;
    for(i=0;i<OS_TASK_MAX_NUM;i++)
    {
      tcb_p=OS_TCB_TABLE[i];
      if(tcb_p == NULL) continue;
      if(tcb_p->State==TASK_DELAY) 
      {
        --tcb_p->TimeDly;
        if(tcb_p->TimeDly == 0) 
          tcb_p->State=TASK_READY;
      }
    }
  OS_EXIT_CRITICAL();
}


void OS_Init(void)
{
  int i;
  g_OS_CPU_ExceptStkBase = OS_CPU_ExceptStk + OS_EXCEPT_STK_SIZE - 1;
  asm("CPSID   I"); 
  for(i=0;i<OS_TASK_MAX_NUM;i++)
    OS_TCB_TABLE[i]=0;
  OS_TimeTick=0;
  OS_Task_Create(&TCB_IDLE,OS_Task_Idle,&TASK_IDLE_STK[TASK_IDLE_STK_SIZE-1],OS_TASK_MAX_NUM-1);
}

void OS_Start(void)
{
  OS_Task_Switch();
  SystemCoreClockUpdate();
  SysTick_Config(SystemCoreClock/OS_TICKS_PER_SECOND);
  OSStart_Asm();
}

int main()
{
  
  OS_Init();
  OS_Task_Create(&TCB_1,task_1,&TASK_1_STK[TASK_1_STK_SIZE-1],2);
  OS_Start();
 
  return 0;
}
