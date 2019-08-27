#ifndef __SCHEDULER_H
#define __SCHEDULER_H

#include "stm32f10x.h"

#define NVIC_INT_CTRL        0xE000ED04	//	; Interrupt control state register.
#define NVIC_SYSPRI14        0xE000ED22	//	; System priority register (priority 14).
#define NVIC_PENDSV_PRI            0xFF	//	; PendSV priority value (lowest).
#define NVIC_PENDSVSET       0x10000000	//	; Value to trigger PendSV exception.

__asm void SetPendSVPro(void);
__asm void TriggerPendSV(void);
__asm void PendSV_Handler(void);
void SysTick_Handler(void);
	
#endif

