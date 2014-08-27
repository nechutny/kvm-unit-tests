/*
 * Bit offsets for Cortex-A15 VFPv4 Status registers
 * 
 * Defined at
 * http://infocenter.arm.com/help/index.jsp?topic=/com.arm.doc.ddi0438i/CDEFCBDC.html
 *
 * Copyright (C) 2014, Stanislav Nechutny <stanislav@nechutny.net>
 *
 * This work is licensed under the terms of the GNU LGPL, version 2.
 */
#ifndef _ARM_VFP_H_
#define _ARM_VFP_H_

// Create double union with given name and value
#define DOUBLE_UNION(name, value)			\
	union {						\
		unsigned long long input;		\
		double d;				\
	} name = { value };

#define FLOAT_UNION(name, value)			\
	union {						\
		unsigned long input;			\
		float f;				\
	} name = { value };

/*
 * Test vfp instruction and check exceptions.
 * 
 * Macro as first step set to _pass variable 0, reset FPSCR exceptions, then
 * execute instruction(s), compare exceptions with expected and increase _pass
 * by 1 if success. 
 *
 * _ins		string with assembler instruction and operands. Can carry more
 * 		instructions separated by new line.
 * 		e.g. "faddd %P[result], %P[num1], %P[num2]"
 *
 * _pass	Variable with exception check status.
 * 		0 fail
 * 		1 success
 *
 * _result	Double/Float variable for write.
 * 		Accessable via %[result]
 *
 * _num1	Double/Float variable for passing value to vfp register.
 * 		Accessable via %[num1]
 *
 * _num2	Double/Float variable for passing value to vfp register.
 * 		Accessable via %[num2]
 *
 * _exceptions	Expected exceptions.
 * 		For easy value assembling are defined FPSCR_* constants
 *
 * NOTE:	When using double-precision operands and GCC's -O2 argument is
 * 		suggested to use %P[name]
 */
#define TEST_VFP_EXCEPTION(_ins, _pass, _result, _num1, _num2, _exceptions) \
asm volatile(								\
	"mov %[pass], #0"			"\n"			\
	"fmrx r0, fpscr"			"\n"			\
	"bic r0, r0, %[mask]"			"\n"			\
	"fmxr fpscr, r0"			"\n"			\
	_ins					"\n"			\
	"fmrx r0, fpscr"			"\n"			\
	"and r0, r0, %[mask]"			"\n"			\
	"cmp r0, %[exceptions]"			"\n"			\
	"addeq %[pass], %[pass], #1"					\
	: [result]"=?w?t" (_result),					\
	  [pass]"=r" (_pass)						\
	: [num1]"?w?t" (_num1),						\
	  [num2]"?w?t" (_num2),						\
	  [exceptions]"r" (_exceptions),				\
	  [mask]"r" (FPSCR_CUMULATIVE)					\
	: "r0"								\
	);

/*
 * Test vfp instruction and check status flags.
 * 
 * Macro as first step set to _pass variable 0, then execute instruction(s),
 * compare status flags with expected and increase _pass by 1 if success. 
 *
 * _ins		string with assembler instruction and operands. Can carry more
 * 		instructions separated by new line.
 * 		e.g. "fcmpd %P[num1], %P[num2]"
 *
 * _pass	Variable with flags check status.
 * 		0 fail
 * 		1 success
 *
 * _result	Double/Float variable for write.
 * 		Accessable via %[result]
 *
 * _num1	Double/Float variable for passing value to vfp register.
 * 		Accessable via %[num1]
 *
 * _num2	Double/Float variable for passing value to vfp register.
 * 		Accessable via %[num2]
 *
 * _flags	Expected status flags.
 * 		For easy value assembling are defined FPSCR_* constants
 *
 * NOTE:	When using double-precision operands and GCC's -O2 argument is
 * 		suggested to use %P[name]
 */
#define TEST_VFP_STATUS_FLAGS(_ins, _pass, _result, _num1, _num2, _flags) \
asm volatile(								\
	"mov %[pass], #0"			"\n"			\
	_ins					"\n"			\
	"fmrx r0, fpscr"			"\n"			\
	"and r0, r0, %[mask]"			"\n"			\
	"cmp r0, %[flags]"			"\n"			\
	"addeq %[pass], %[pass], #1"					\
	: [result]"=?w?t" (_result),					\
	  [pass]"=r" (_pass)						\
	: [num1]"?w?t" (_num1),						\
	  [num2]"?w?t" (_num2),						\
	  [flags]"r" (_flags),						\
	  [mask]"r" (FPSCR_STATUS_FLAGS)				\
	: "r0"								\
	);

// Constants for DOUBLE_UNION
#define DOUBLE_PLUS_INF		0x7ff0000000000000
#define DOUBLE_MINUS_INF	0xfff0000000000000
#define DOUBLE_PLUS_NULL	0x0000000000000000
#define DOUBLE_MINUS_NULL	0x8000000000000000
#define DOUBLE_PLUS_NAN		0x7ff0000000000020
#define DOUBLE_MINUS_NAN	0xfff0000000000020

// Constants for FLOAT_UNION
#define FLOAT_PLUS_INF		0x7f800000
#define FLOAT_MINUS_INF		0xff800000
#define FLOAT_PLUS_NULL		0x00000000
#define FLOAT_MINUS_NULL	0x80000000
#define FLOAT_PLUS_NAN		0x7f800002
#define FLOAT_MINUS_NAN		0xff800002

/*********************
 * FPSID bits        *
 *********************/
/*
 * Indicates the implementer
 * 0x41 ARM Limited.
 */
#define FPSID_IMP	(255UL << 24)

/*
 * Software bit. This bit indicates that a system provides only software
 * emulation of the VFP floating-point instructions:
 * 0x0 The system includes hardware support for VFP floating-point operations.
 */
#define FPSID_SW	(1UL << 23)

/*
 * Subarchitecture version number:
 * 0x04 VFP architecture v4 with Common VFP subarchitecture v3. The VFP
 * 	architecture version is indicated by the MVFR0 and MVFR1 registers.
 */
#define FPSID_SUB	(127UL << 16)

/*
 * Part number
 * Indicates the part number for the floating-point implementation:
 * 0x30 VFP.
 */
#define FPSID_PART	(255UL << 8)

/*
 * Indicates the variant number:
 * 0xF Cortex-A15.
 */
#define FPSID_VAR	(15UL << 4)

/*
 * Indicates the revision number for the floating-point implementation:
 * 0x0 Revision 0.
 */
#define FPSID_REV	(15UL << 0)

/*********************
 * FPSCR bits        *
 *********************/
/* Set if comparison produces a less than result			 */
#define FPSCR_N		(1UL << 31)

/* Set if comparison produces an equal result				 */
#define FPSCR_Z		(1UL << 30)

/* Set if comparison produces an equal, greater than, or unordered result*/
#define FPSCR_C		(1UL << 29)

/* Set if comparison produces an unordered result			 */
#define FPSCR_V		(1UL << 28)

/*
 * Saturation cumulative flag.
 * If Advanced SIMD is not implemented, this bit is UNK/SBZP.
 */
#define FPSCR_QC	(1UL << 27)

/*
 * Alternative Half-Precision control bit:
 * 0 IEEE half-precision format selected.
 * 1 Alternative half-precision format selected.
 */
#define FPSCR_AHP	(1UL << 26)

/*
 * Default NaN mode enable bit:
 * 0 = default NaN mode disabled
 * 1 = default NaN mode enabled.
 */
#define FPSCR_DN	(1UL << 25)

/*
 * Flush-to-zero mode enable bit:
 * 0 = flush-to-zero mode disabled
 * 1 = flush-to-zero mode enabled.
 */
#define FPSCR_FZ	(1UL << 24)

/*
 * Rounding mode control field:
 * b00 = round to nearest (RN) mod
 * b01 = round towards plus infinity (RP) mode
 * b10 = round towards minus infinity (RM) mode
 * b11 = round towards zero (RZ) mode.
 */
#define FPSCR_RMODE	(3UL << 22)

/*
 * See Vector length and stride control
 * http://infocenter.arm.com/help/index.jsp?topic=/com.arm.doc.ddi0344b/Chdfafia.html
 *
 * __DEPRECATED IN ARMv7__
 */
#define FPSCR_STRIDE	(3UL << 20)
#define FPSCR_LEN	(7UL << 16)

/* Reserved								 */
#define FPSCR_RESERVED	(1UL << 19 | 255UL << 8 | 3UL << 5)

/* Input Denormal cumulative exception bit.				 */
#define FPSCR_IDC	(1UL << 7)

/* Inexact cumulative exception bit.					 */
#define FPSCR_IXC	(1UL << 4)

/* Underflow cumulative exception bit.					 */
#define FPSCR_UFC	(1UL << 3)

/* Overflow cumulative exception bit.					 */
#define FPSCR_OFC	(1UL << 2)

/* Division by Zero cumulative exception bit.				 */
#define FPSCR_DZC	(1UL << 1)

/* Invalid Operation cumulative exception bit.				 */
#define FPSCR_IOC	(1UL << 0)

/* Comulative exceptions bits						 */
#define FPSCR_CUMULATIVE	(FPSCR_QC | FPSCR_IDC | FPSCR_IXC | \
FPSCR_UFC | FPSCR_OFC | FPSCR_DZC | FPSCR_IOC)

/* Status flags bits							 */
#define FPSCR_STATUS_FLAGS	(FPSCR_Z | FPSCR_C | FPSCR_N | FPSCR_V)

#define FPSCR_NO_EXCEPTION	0ULL
#define FPSCR_EMPTY_STATUS	0ULL

/*********************
 * MVFR1 bits        *
 *********************/
/*
 * Indicates whether the Advanced SIMD or VFP supports fused multiply
 * accumulate operations:
 * 0x1 Supported.
 */
#define MVFR1_ASIMD_FMAC	(15UL << 28)

/*
 * Indicates whether the VFP supports half-precision floating-point conversion
 * operations:
 * 0x1 Supported.
 */
#define MVFR1_VFP_HPFP		(15UL << 24)

/*
 * Indicates whether the Advanced SIMD extension supports half-precision
 * floating-point conversion operations:
 * 0x1 Supported.
 * If Advanced SIMD is implemented, the reset value is 0x1.
 * If Advanced SIMD is not implemented, the reset value is 0x0.
 */
#define MVFR1_ASIMD_HPFP	(15UL << 20)

/*
 * Indicates whether the Advanced SIMD extension supports single-precision
 * floating-point operations:
 * 0x1 Supported.
 * If Advanced SIMD is implemented, the reset value is 0x1.
 * If Advanced SIMD is not implemented, the reset value is 0x0.
 */
#define MVFR1_ASIMD_SPFP	(15UL << 16)

/*
 * Indicates whether the Advanced SIMD extension supports integer operations:
 * 0x1 Supported.
 * If Advanced SIMD is implemented, the reset value is 0x1.
 * If Advanced SIMD is not implemented, the reset value is 0x0.
 */
#define MVFR1_ASIMD_INT		(15UL << 12)

/*
 * Indicates whether the Advanced SIMD extension supports load/store
 * instructions:
 * 0x1 Supported.
 * If Advanced SIMD is implemented, the reset value is 0x1.
 * If Advanced SIMD is not implemented, the reset value is 0x0.
 */
#define MVFR1_ASIMD_LDST	(15UL << 8)

/*
 * Indicates whether the VFP hardware implementation supports only the Default
 * NaN mode:
 * 0x1 Hardware supports propagation of NaN values.
 */
#define MVFR1_D_NAN		(15UL << 4)

/*
 * Indicates whether the VFP hardware implementation supports only the
 * Flush-to-Zero mode of operation:
 * 0x1 Hardware supports full denormalized number arithmetic.
 */
#define MVFR1_VFP_FTZ		(15UL << 0)

/*********************
 * MVFR0 bits        *
 *********************/
/*
 * Indicates the rounding modes supported by the VFP floating-point hardware:
 * 0x1 Supported.
 */
#define MVFR0_VFP_RND		(15UL << 28)

/*
 * Indicates the hardware support for VFP short vectors:
 * 0x0 Not supported.
 */
#define MVFR0_VFP_SHV		(15UL << 24)

/*
 * Indicates the hardware support for VFP square root operations:
 * 0x1 Supported.
 */
#define MVFR0_VFP_SQRT		(15UL << 20)

/*
 * Indicates the hardware support for VFP divide operations:
 * 0x1 Supported.
 */
#define MVFR0_VFP_DIV		(15UL << 16)

/*
 * Indicates whether the VFP hardware implementation supports exception
 * trapping:
 * 0x0 Not supported.
 */
#define MVFR0_VFP_EXC		(15UL << 12)

/*
 * Indicates the hardware support for VFP double-precision operations:
 * 0x2 VFPv4 double-precision supported.
 */
#define MVFR0_VFP_DP		(15UL << 8)

/*
 * Indicates the hardware support for VFP single-precision operations:
 * 0x2 VFPv4 single-precision supported.
 */
#define MVFR0_VFP_SP		(15UL << 4)

/*
 * Indicates support for the Advanced SIMD register bank:
 * 0x2 32 x 64-bit registers supported.
 */
#define MVFR0_ASIMD_REGS	(15UL << 0)

/*********************
 * FPEXC bits        *
 *********************/
/*
 * Exception bit. The Cortex-A15 implementation does not generate asynchronous
 * VFP exceptions, therefore this bit is RAZ/WI.
 */
#define FPEXC_EX	(1UL << 31)

/*
 * Enable bit. A global enable for the Advanced SIMD and VFP extensions:
 * 0 The Advanced SIMD and VFP extensions are disabled.
 * 1 The Advanced SIMD and VFP extensions are enabled and operate normally.
 * The EN bit is cleared at reset.
 */
#define FPEXC_EN	(1UL << 30)

#define FPEXC_RESERVED	(~(FPEXC_EX|FPEXC_EN))

#endif /* _ARM_VFP_H_ */
