#include "soc/soc.h"
#include "soc/interrupt_reg.h"
#include "riscv/rvruntime-frames.h"
#include "soc/soc_caps.h"
#include "sdkconfig.h"
#include "riscv/rvruntime-frames.h"

///// GENERAL NOTE: Neither of these functions generally should get used.

//A high level callback (c compatible function)
.global asm_isr_handler
asm_isr_handler:
	lui	a5,0x60004	//GPIO
	li	a4,2		//Set output value to 2 (1<<1).
	sw	a4,4(a5)	//Set ODR GPIO1 to high.
	li	a4,-1
	sw	a4,76(a5)
	ret






.section .exception_vectors.text

	.balign 0x100
	.global _custom_vector_table
	.type _custom_vector_table, @function
_custom_vector_table:
	.option push
	.option norvc
	j _panic_handler			/* exception handler, entry 0 */
	j _interrupt_handler		/* 6 identical entries, all pointing to the interrupt handler */
	j _interrupt_handler		/* 6 identical entries, all pointing to the interrupt handler */
	j _interrupt_handler /* Something about bringup */		/* 6 identical entries, all pointing to the interrupt handler */
	j _interrupt_handler /* !!!! System timer? */		/* 6 identical entries, all pointing to the interrupt handler */
	j _interrupt_handler		/* 6 identical entries, all pointing to the interrupt handler */
	j _interrupt_handler		/* 6 identical entries, all pointing to the interrupt handler */
	j _interrupt_handler		/* 6 identical entries, all pointing to the interrupt handler */
	j _interrupt_handler		/* 6 identical entries, all pointing to the interrupt handler */
	j _interrupt_handler /* 9: THIS IS THE GPIO HANDLER */		/* 6 identical entries, all pointing to the interrupt handler */
	j _interrupt_handler		/* 6 identical entries, all pointing to the interrupt handler */
	j _interrupt_handler		/* 6 identical entries, all pointing to the interrupt handler */
	j _interrupt_handler		/* 6 identical entries, all pointing to the interrupt handler */
	j _interrupt_handler		/* 6 identical entries, all pointing to the interrupt handler */
	j _interrupt_handler		/* 6 identical entries, all pointing to the interrupt handler */
	j _interrupt_handler		/* 6 identical entries, all pointing to the interrupt handler */
	j _interrupt_handler		/* 6 identical entries, all pointing to the interrupt handler */
	j _interrupt_handler		/* 6 identical entries, all pointing to the interrupt handler */
	j _interrupt_handler		/* 6 identical entries, all pointing to the interrupt handler */
	j _interrupt_handler		/* 6 identical entries, all pointing to the interrupt handler */
	j _interrupt_handler		/* 6 identical entries, all pointing to the interrupt handler */

	j _interrupt_handler		/* 6 identical entries, all pointing to the interrupt handler */
	j _interrupt_handler		/* 6 identical entries, all pointing to the interrupt handler */
	j _interrupt_handler		/* 6 identical entries, all pointing to the interrupt handler */

	.rept (ETS_T1_WDT_INUM - 1 - 20 - 3)
	j _interrupt_handler		/* 24 identical entries, all pointing to the interrupt handler */
	.endr
	j _panic_handler			/* Call panic handler for ETS_T1_WDT_INUM interrupt (soc-level panic)*/
    j _panic_handler			/* Call panic handler for ETS_CACHEERR_INUM interrupt (soc-level panic)*/
    #ifdef CONFIG_ESP_SYSTEM_MEMPROT_FEATURE
    j _panic_handler			/* Call panic handler for ETS_MEMPROT_ERR_INUM interrupt (soc-level panic)*/
	.rept (ETS_MAX_INUM - ETS_MEMPROT_ERR_INUM)
	#else
	.rept (ETS_MAX_INUM - ETS_CACHEERR_INUM)
	#endif
	j _interrupt_handler		/* 6 identical entries, all pointing to the interrupt handler */

	j _interrupt_handler	//Reserved
	j _interrupt_handler	//Reserved
	j _interrupt_handler	//Reserved

	//After this is marked as ISR #31
	//You could jump to your own function here...
//	j _experimental_handler


	//OOORRRR WE COULD JUST BE CRAZY AND LET THE INTERRUPT FALL INTO CODE.
	sw   a4,  -4(sp)
	sw   a5,  -8(sp)

	lui	a5,0x60004	//GPIO
	li	a4,8		//Set output value to 2 (1<<1).
	sw	a4,4(a5)	//Set ODR GPIO1 to high.
	li	a4,-1
	sw	a4,76(a5)

	/* restore the rest of the registers */
	lw   a4,  -4(sp)
	lw   a5,  -8(sp)

	/* exit, this will also re-enable the interrupts */
	mret




//	j _asm_isr_handler_trampoline		/* 24 identical entries, all pointing to the interrupt handler */
//	j _asm_isr_handler_trampoline		/* 24 identical entries, all pointing to the interrupt handler */
//	j _asm_isr_handler_trampoline		/* 24 identical entries, all pointing to the interrupt handler */
//	j _asm_isr_handler_trampoline		/* 24 identical entries, all pointing to the interrupt handler */
//	j _asm_isr_handler_trampoline		/* 24 identical entries, all pointing to the interrupt handler */
//	j _asm_isr_handler_trampoline		/* 24 identical entries, all pointing to the interrupt handler */

//	j _interrupt_handler		/* 6 identical entries, all pointing to the interrupt handler */


	.endr

	.option pop
	.size _custom_vector_table, .-_custom_vector_table





#define CONTEXT_SIZE 128

	.global _experimental_handler
	.type _experimental_handler, @function

_experimental_handler:
	/* entry */
	addi sp, sp, -CONTEXT_SIZE
	sw   a4,  RV_STK_A4(sp)
	sw   a5,  RV_STK_A5(sp)

	lui	a5,0x60004	//GPIO
	li	a4,8		//Set output value to 2 (1<<1).
	sw	a4,4(a5)	//Set ODR GPIO1 to high.
	li	a4,-1
	sw	a4,76(a5)

	/* restore the rest of the registers */
	lw   a4,  RV_STK_A4(sp)
	lw   a5,  RV_STK_A5(sp)

	addi sp, sp, CONTEXT_SIZE

	/* exit, this will also re-enable the interrupts */
	mret
	.size  _experimental_handler, .-_experimental_handler






