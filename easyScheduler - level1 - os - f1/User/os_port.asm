;用到的汇编语法
;EQU 替换语句，类似#define
;EXTERN 声明语句
;EXPORT 导出语句；
;LDR    load register 装载语句，类似mov
;CBZ    比较，如果结果为0 就转移
;STR    把一个寄存器按字存储到存储器中
;MSR    特殊寄存器的写入，比如PSP
;CPSIE  使能PRIMASK
;CPSID  失能PRIMASK
;SUB    减
;ADD    加
;STM    存储若干寄存器中的字到一片连续的地址空间中，自增型
;LDM    从一片连续的地址空间中加载多个字到若干寄存器
;B      无条件转移
;BL     跳转并链接LR寄存器（用于函数返回）（）
;LR     链接寄存器，在PC指针跳转到函数前，存储PC指针的值
;END    汇编语言结束
;MRS/MSR    用于读/写特殊功能寄存器（PRIMASK、FAULTMASK、SP）到通用寄存器中
;=      伪指令，定义标号所在地址或一个常数
;更多指令查阅Cortex权威指南，注意：指令集支持后缀，因此如果找不到完全一致的指令，可以注意有没有近似匹配的指令。

NVIC_INT_CTRL   EQU     0xE000ED04				; 中断控制及状态寄存器Interrupt control state register.（第28bit置1设立PendSV中断）
NVIC_SYSPRI14   EQU     0xE000ED22				; PendSV的优先级寄存器 (priority 14).
NVIC_PENDSV_PRI EQU           0xFF				; PendSV优先级值PendSV priority value (lowest).
NVIC_PENDSVSET  EQU     0x10000000				; 写NVIC_PENDSVSET到NVIC_INT_CTRL即可触发PendSV中断Value to trigger PendSV exception.

	;不知道干嘛的
    AREA |.text|, CODE, READONLY, ALIGN=2
    THUMB	;THUMB指令集？
    REQUIRE8
    PRESERVE8
 
  ;外部变量声明
  EXTERN  g_OS_CPU_ExceptStkBase
  EXTERN  g_OS_Tcb_CurP
  EXTERN  g_OS_Tcb_HighRdyP
	  
  ;函数接口导出
  EXPORT OSCtxSw
  EXPORT OSStart_Asm
  EXPORT PendSV_Handler
  EXPORT OS_CPU_SR_Save
  EXPORT OS_CPU_SR_Restore
	  
OS_CPU_SR_Save
    MRS     R0, PRIMASK ; PRIMASK是1bit中断掩码寄存器，置1时关闭除FAULT和NMI外所有中断，相当于保存中断开关状态
    CPSID   I           ; 关闭所有中断
    BX      LR          ; 函数返回。

OS_CPU_SR_Restore
    MSR     PRIMASK, R0
    BX      LR
	
;上下文切换（触发PendSV中断）
;思考：uCOS中OSCtxSw和OSIntCtxSw有什么用？他们为什么名字上有区别而内容上却无区别呢？什么情况下内容上会有区别呢？
OSCtxSw
    LDR     R0, =NVIC_INT_CTRL                  ;NVIC_INT_CTRL传R0
    LDR     R1, =NVIC_PENDSVSET                 ;NVIC_PENDSVSET传R1
    STR     R1, [R0]                            ;R1的值存储到R0所存地址指向的位置中去（间接寻址）
    BX      LR                                  ;函数返回，PendSV中断被悬挂（使能），等待触发。Enable interrupts at processor level

;OS启动（初始化PendSV；初始化PSP、MSP；设定PendSV触发标志）
OSStart_Asm
    LDR     R0, =NVIC_SYSPRI14                  ; 配置PendSV异常的优先级（初始化）Set the PendSV exception priority
    LDR     R1, =NVIC_PENDSV_PRI
    STRB    R1, [R0]

    MOVS    R0, #0                              ; PSP指针初始化为0。Set the PSP to 0 for initial context switch call
    MSR     PSP, R0

    LDR     R0, =g_OS_CPU_ExceptStkBase         ; MSP指针初始化为g_OS_CPU_ExceptStkBase处。Initialize the MSP to the OS_CPU_ExceptStkBase
    LDR     R1, [R0]
    MSR     MSP, R1    

    LDR     R0, =NVIC_INT_CTRL                  ; 见OSCtxSw函数解释。Trigger the PendSV exception (causes context switch)
    LDR     R1, =NVIC_PENDSVSET
    STR     R1, [R0]

    CPSIE   I                                   ; 使能中断。Enable interrupts at processor level

OSStartHang
    B       OSStartHang                         ; 一个死循环，不应该一直在这执行。Should never get here
    
    

PendSV_Handler
    CPSID   I                                   ; 关闭全局中断。Prevent interruption during context switch
    MRS     R0, PSP                             ; 读取PSP到R0。PSP is process stack pointer
    CBZ     R0, OS_CPU_PendSVHandler_nosave     ; 判断R0是不是0，是0的话跳转到nosave标签去，因为是0则是首次执行
                                                ;，无需保存上文，直接切换下文即可。Skip register save the first time
   ;保存上文
    SUBS    R0, R0, #0x20                       ; R0减0x20=32往低地址偏移8个寄存器*4字节长度，以保存剩余的8个寄存器。R0 = R0 - 0x20; Save remaining regs r4-11 on process stack
    STM     R0, {R4-R11}                        ; 将R4-R11的数据存储到R0所标记开始的地址中去，每次存储会导致地址+4，但R0本身不变。说明：在进中断时PSP指针会自动随着入栈偏移，但是我们的任务控制块里的栈指针变量不会自动变化，因此要手动把它调整一下

	  LDR     R1, =g_OS_Tcb_CurP                  ; 把g_OS_Tcb_CurP地址传给R1，g_OS_Tcb_CurP指向的是一个TCB结构
    LDR     R1, [R1]                            ; 间接寻址，获取TCB结构，该结构体的第一个变量就是存栈指针（栈顶地址）的变量）因此其实是传的存栈指针的变量OSTCBCur->OSTCBStkPtr = SP;
    STR     R0, [R1]                            ; 获取存栈指针的变量，并将当前PSP指针存进去，上文保存完成。R0 is SP of process being switched out

    ;切换下文                                   ; At this point, entire context of process has been saved
OS_CPU_PendSVHandler_nosave
	  LDR     R0, =g_OS_Tcb_CurP                  ; 获取存储当前任务控制块地址的变量，OSTCBCur  = OSTCBHighRdy;  （注意“地址”两个字的位置）
    LDR     R1, =g_OS_Tcb_HighRdyP              ; 获取存储就绪任务控制块地址的变量
    LDR     R2, [R1]                            ; 获取就绪任务控制块地址，存入R2
    STR     R2, [R0]                            ; 将就绪任务控制块地址赋值给当前任务控制块（刷新当前任务）

	  LDR     R0, [R2]                            ; 获取栈顶地址并加载进R0。R0 is new process SP; SP = OSTCBHighRdy->OSTCBStkPtr;
  
    LDM     R0, {R4-R11}                        ; 将R0所标地址开始的数据依次读入8个寄存器R4-R11。每读一次，地址会+4（但R0不变）。Restore r4-11 from new process stack
    ADDS    R0, R0, #0x20                       ; R0增大0x20=32（因为前面恢复了8个寄存器，1个寄存器占4Byte）
            
	  MSR     PSP, R0                             ; 新的R0（新的栈顶地址）传给PSP指针。Load PSP with new process SP		
    ORR     LR, LR, #0x04                       ; 重要：确保异常返回后使用PSP指针，而不是MSP指针。LR和0x04或运算后存储在LR中。当CM3响应异常后LR将被重新解释，不再是普通的子程序返回值，详细解释参见《权威指南》第九章约前10页内容。Ensure exception return uses process stack
    
    CPSIE   I                                   ; 启动中断
    BX      LR                                  ; 返回子程序Exception return will restore remaining context
  
    END