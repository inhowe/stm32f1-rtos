#include "stdio.h"
#define OS_EXCEPT_STK_SIZE 1024
#define TASK_1_STK_SIZE 1024
#define TASK_2_STK_SIZE 1024

typedef unsigned int OS_STK;
typedef void (*OS_TASK)(void);

typedef struct OS_TCB
{
  OS_STK *StkAddr;
}OS_TCB,*OS_TCBP;


OS_TCBP g_OS_Tcb_CurP; 
OS_TCBP g_OS_Tcb_HighRdyP;

static OS_STK OS_CPU_ExceptStk[OS_EXCEPT_STK_SIZE];
OS_STK *g_OS_CPU_ExceptStkBase;

static OS_TCB TCB_1;
static OS_TCB TCB_2;
static OS_STK TASK_1_STK[TASK_1_STK_SIZE];
static OS_STK TASK_2_STK[TASK_2_STK_SIZE];

extern void OSStart_Asm(void);
extern void OSCtxSw(void);

void Task_Switch()
{
  if(g_OS_Tcb_CurP == &TCB_1)
    g_OS_Tcb_HighRdyP=&TCB_2;
  else
    g_OS_Tcb_HighRdyP=&TCB_1;
 
  OSCtxSw();
}


void task_1()
{
  while(1)
  {
    printf("Task 1 Runing!!!\n");
    Task_Switch();
    printf("Task 1 Runing!!!\n");
    Task_Switch();
  }
}

void task_2()
{
  while(1)
  {
    printf("Task 2 Runing!!!\n");
    Task_Switch();
    printf("Task 2 Runing!!!\n");
    Task_Switch();
  }
}

void Task_End(void)
{
  printf("Task End\n");
  while(1)
  {}
}

void Task_Create(OS_TCB *tcb,OS_TASK task,OS_STK *stk)
{
    OS_STK  *p_stk;
    p_stk      = stk;
    p_stk      = (OS_STK *)((OS_STK)(p_stk) & 0xFFFFFFF8u);
    
    *(--p_stk) = (OS_STK)0x01000000uL;                          //xPSR
    *(--p_stk) = (OS_STK)task;                                  // Entry Point
    *(--p_stk) = (OS_STK)Task_End;                                     // R14 (LR)
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
}


int main()
{
  
  g_OS_CPU_ExceptStkBase = OS_CPU_ExceptStk + OS_EXCEPT_STK_SIZE - 1;
  
  Task_Create(&TCB_1,task_1,&TASK_1_STK[TASK_1_STK_SIZE-1]);
  Task_Create(&TCB_2,task_2,&TASK_2_STK[TASK_1_STK_SIZE-1]);
    
  g_OS_Tcb_HighRdyP=&TCB_1;
  
  OSStart_Asm();
  
  return 0;
}
