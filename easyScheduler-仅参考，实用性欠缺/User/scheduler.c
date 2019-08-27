#include "scheduler.h"

extern uint32_t  curr_task,next_task,PSP_array[4];

//设定PendSV优先级（初始化PendSV中断）
__asm void SetPendSVPro(void)
{
//		NVIC_SYSPRI14   EQU     0xE000ED22
//		NVIC_PENDSV_PRI EQU           0xFF
    
    LDR     R1, =NVIC_PENDSV_PRI    
    LDR     R0, =NVIC_SYSPRI14    
    STRB    R1, [R0]
    BX      LR
}

//触发PendSV中断
__asm void TriggerPendSV(void)
{
//		NVIC_INT_CTRL   EQU     0xE000ED04                              
//		NVIC_PENDSVSET  EQU     0x10000000                              

    LDR     R0, =NVIC_INT_CTRL                                 
    LDR     R1, =NVIC_PENDSVSET
    STR     R1, [R0]
    BX      LR
}

//PendSV中断服务函数
__asm void PendSV_Handler(void)
{ 
		/*
		***语法解析**
	{}：列表
	LDR 从存储器中加载字到一个寄存器中
	MRS <Rn>, <SReg> ;加载特殊功能寄存器的值到Rn
	MSR <Sreg>,<Rn> ;存储Rn 的值到特殊功能寄存器
	STMDB Rd!, {寄存器列表}   存储多个字到 Rd 处。每存一个字前Rd 自减一次，16位宽度（类似压栈效果）
	LDMIA Rd!, {寄存器列表}   从 Rd 处读取多个字。每读一个字后Rd 自增一次，16位宽度（类似出栈效果）
	LSL：左移指令
	#：立即数
	[Rd]:间接寻址Rd
	[Rd,xx]：Rd偏移xx
	STR：把一个寄存器按字存储到存储器中，左操作数赋值右操作数指令
	BX：返回指令
	LR：连接寄存器，保存子程序返回地址
		*/

    // 保存当前任务的寄存器内容
    MRS    R0, PSP     // 得到PSP  R0 = PSP 。
                       // xPSR, PC, LR, R12, R0-R3已自动保存
    STMDB  R0!,{R4-R11}// 起到了PUSH的效果，但不是PUSH命令，且PUSH只控制MSP
											
    // 加载下一个任务的内容
    LDR    R1,=__cpp(&curr_task)
    LDR    R3,=__cpp(&PSP_array)
    LDR    R4,=__cpp(&next_task)
    LDR    R4,[R4]     // 得到下一个任务的ID
    STR    R4,[R1]     // 设置 curr_task = next_task，R4赋值给R1
    LDR    R0,[R3, R4, LSL #2] // 从PSP_array中获取PSP的值，LSL左移指令，R4左移2bit，也就是扩大4倍（因为该该数组是uint32型，所以偏移n个数据实际应偏移4*n字节），扩大后的R4为R3的偏移量，也就是
    LDMIA  R0!,{R4-R11}// 将任务堆栈中的数值加载到R4-R11中，应该最先出R4

    MSR    PSP, R0     // 设置PSP指向此任务

    BX     LR          // 返回。return
                       // xPSR, PC, LR, R12, R0-R3会自动的恢复
}

//SysTick中断服务函数
void SysTick_Handler(void)
{
	static unsigned int cnt=0;
	if(++cnt%100==0)
	{
    if(curr_task==0)
        next_task=1;
    else
        next_task=0;
    TriggerPendSV();
	}
}


void TaskSwitch(void)
{
	
	
}
