#include "libcflat.h"
#include "arm/processor.h"
#include "arm/sysinfo.h"
#include "arm/ptrace.h"
#include "heap.h"

static const char *processor_modes[] = {
	"USER_26", "FIQ_26" , "IRQ_26" , "SVC_26" ,
	"UK4_26" , "UK5_26" , "UK6_26" , "UK7_26" ,
	"UK8_26" , "UK9_26" , "UK10_26", "UK11_26",
	"UK12_26", "UK13_26", "UK14_26", "UK15_26",
	"USER_32", "FIQ_32" , "IRQ_32" , "SVC_32" ,
	"UK4_32" , "UK5_32" , "UK6_32" , "ABT_32" ,
	"UK8_32" , "UK9_32" , "UK10_32", "UND_32" ,
	"UK12_32", "UK13_32", "UK14_32", "SYS_32"
};

static char *vector_names[] = {
	"rst", "und", "svc", "pabt", "dabt", "addrexcptn", "irq", "fiq"
};

void show_regs(struct pt_regs *regs)
{
	unsigned long flags;
	char buf[64];

	printf("pc : [<%08lx>]    lr : [<%08lx>]    psr: %08lx\n"
	       "sp : %08lx  ip : %08lx  fp : %08lx\n",
		regs->ARM_pc, regs->ARM_lr, regs->ARM_cpsr,
		regs->ARM_sp, regs->ARM_ip, regs->ARM_fp);
	printf("r10: %08lx  r9 : %08lx  r8 : %08lx\n",
		regs->ARM_r10, regs->ARM_r9, regs->ARM_r8);
	printf("r7 : %08lx  r6 : %08lx  r5 : %08lx  r4 : %08lx\n",
		regs->ARM_r7, regs->ARM_r6, regs->ARM_r5, regs->ARM_r4);
	printf("r3 : %08lx  r2 : %08lx  r1 : %08lx  r0 : %08lx\n",
		regs->ARM_r3, regs->ARM_r2, regs->ARM_r1, regs->ARM_r0);

	flags = regs->ARM_cpsr;
	buf[0] = flags & PSR_N_BIT ? 'N' : 'n';
	buf[1] = flags & PSR_Z_BIT ? 'Z' : 'z';
	buf[2] = flags & PSR_C_BIT ? 'C' : 'c';
	buf[3] = flags & PSR_V_BIT ? 'V' : 'v';
	buf[4] = '\0';

	printf("Flags: %s  IRQs o%s  FIQs o%s  Mode %s\n",
		buf, interrupts_enabled(regs) ? "n" : "ff",
		fast_interrupts_enabled(regs) ? "n" : "ff",
		processor_modes[processor_mode(regs)]);

	if (!user_mode(regs)) {
		unsigned int ctrl, transbase, dac;
		asm volatile(
			"mrc p15, 0, %0, c1, c0\n"
			"mrc p15, 0, %1, c2, c0\n"
			"mrc p15, 0, %2, c3, c0\n"
		: "=r" (ctrl), "=r" (transbase), "=r" (dac));
		printf("Control: %08x  Table: %08x  DAC: %08x\n",
			ctrl, transbase, dac);
	}
}

void *get_sp(void)
{
	register unsigned long sp asm("sp");
	return (void *)sp;
}

static exception_fn exception_handlers[EXCPTN_MAX];

void install_exception_handler(enum vector v, exception_fn fn)
{
	if (v < EXCPTN_MAX)
		exception_handlers[v] = fn;
}

void do_handle_exception(enum vector v, struct pt_regs *regs)
{
	if (v < EXCPTN_MAX && exception_handlers[v]) {
		exception_handlers[v](regs);
		return;
	}

	if (v < EXCPTN_MAX)
		printf("Unhandled exception %d (%s)\n", v, vector_names[v]);
	else
		printf("%s called with vector=%d\n", __func__, v);
	printf("Exception frame registers:\n");
	show_regs(regs);
	exit(EINTR);
}

void start_usr(void (*func)(void))
{
	void *sp_usr = alloc_page() + PAGE_SIZE;
	asm volatile(
		"mrs	r0, cpsr\n"
		"bic	r0, #" __stringify(MODE_MASK) "\n"
		"orr	r0, #" __stringify(USR_MODE) "\n"
		"msr	cpsr_c, r0\n"
		"mov	sp, %0\n"
		"mov	pc, %1\n"
	:: "r" (sp_usr), "r" (func) : "r0");
}
