/* See COPYRIGHT for copyright information. */

#include <inc/mmu.h>
#include <inc/memlayout.h>
#include <inc/trap.h>

#include <kern/picirq.h>


###################################################################
# exceptions/interrupts
###################################################################

/* TRAPHANDLER defines a globally-visible function for handling a trap.
 * It pushes a trap number onto the stack, then jumps to _alltraps.
 * Use TRAPHANDLER for traps where the CPU automatically pushes an error code.
 *
 * You shouldn't call a TRAPHANDLER function from C, but you may
 * need to _declare_ one in C (for instance, to get a function pointer
 * during IDT setup).  You can declare the function with
 *   void NAME();
 * where NAME is the argument passed to TRAPHANDLER.
 */
#define TRAPHANDLER(name, num)						\
.data;\
	.long name;\
.text;\
	.globl name;		/* define global symbol for 'name' */	\
	.type name, @function;	/* symbol type is function */		\
	.align 2;		/* align function definition */		\
	name:			/* function starts here */		\
	pushl $(num);							\
	jmp _alltraps

/* Use TRAPHANDLER_NOEC for traps where the CPU doesn't push an error code.
 * It pushes a 0 in place of the error code, so the trap frame has the same
 * format in either case.
 */
#define TRAPHANDLER_NOEC(name, num)					\
.data;\
	.long name;\
.text;\
	.globl name;							\
	.type name, @function;						\
	.align 2;							\
	name:								\
	pushl $0;							\
	pushl $(num);							\
	jmp _alltraps

#define dummy()\
.data;\
	.long 0

.data
	.globl handler
	.align 4
handler:
.text
/*
 * Lab 3: Your code here for generating entry points for the different traps.
 */
	TRAPHANDLER_NOEC(handler0, T_DIVIDE);
	TRAPHANDLER_NOEC(handler1, T_DEBUG);
	TRAPHANDLER_NOEC(handler2, T_NMI);
	TRAPHANDLER_NOEC(handler3, T_BRKPT);
	TRAPHANDLER_NOEC(handler4, T_OFLOW);
	TRAPHANDLER_NOEC(handler5, T_BOUND);
	TRAPHANDLER_NOEC(handler6, T_ILLOP);
	TRAPHANDLER_NOEC(handler7, T_DEVICE);
	TRAPHANDLER(handler8, T_DBLFLT);
	# TRAPHANDLER(handler9, 9);
	dummy();
	TRAPHANDLER(handler10, T_TSS);
	TRAPHANDLER(handler11, T_SEGNP);
	TRAPHANDLER(handler12, T_STACK);
	TRAPHANDLER(handler13, T_GPFLT);
	TRAPHANDLER(handler14, T_PGFLT);
	# TRAPHANDLER(handler15, 15);
	dummy();
	TRAPHANDLER_NOEC(handler16, T_FPERR);
	TRAPHANDLER_NOEC(handler17, T_ALIGN);
	TRAPHANDLER_NOEC(handler18, T_MCHK);
	TRAPHANDLER_NOEC(handler19, T_SIMDERR);
.data
	.space 48# (31-19)*4
.text
	TRAPHANDLER_NOEC(handler32, IRQ_OFFSET);
	TRAPHANDLER_NOEC(handler33, IRQ_OFFSET+1);
	TRAPHANDLER_NOEC(handler34, IRQ_OFFSET+2);
	TRAPHANDLER_NOEC(handler35, IRQ_OFFSET+3);
	TRAPHANDLER_NOEC(handler36, IRQ_OFFSET+4);
	TRAPHANDLER_NOEC(handler37, IRQ_OFFSET+5);
	TRAPHANDLER_NOEC(handler38, IRQ_OFFSET+6);
	TRAPHANDLER_NOEC(handler39, IRQ_OFFSET+7);
	TRAPHANDLER_NOEC(handler40, IRQ_OFFSET+8);
	TRAPHANDLER_NOEC(handler41, IRQ_OFFSET+9);
	TRAPHANDLER_NOEC(handler42, IRQ_OFFSET+10);
	TRAPHANDLER_NOEC(handler43, IRQ_OFFSET+11);
	TRAPHANDLER_NOEC(handler44, IRQ_OFFSET+12);
	TRAPHANDLER_NOEC(handler45, IRQ_OFFSET+13);
	TRAPHANDLER_NOEC(handler46, IRQ_OFFSET+14);
	TRAPHANDLER_NOEC(handler47, IRQ_OFFSET+15);
	TRAPHANDLER_NOEC(handler48, T_SYSCALL);


/*
 * Lab 3: Your code here for _alltraps
 */
 	_alltraps:
 	pushl %ds
 	pushl %es
 	pushal
 	movw $GD_KD, %ax
 	movw %ax, %ds
 	movw $GD_KD, %ax
 	movw %ax, %es
 	pushl %esp
 	call trap



