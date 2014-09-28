/*
 * Test ARMv7 (Cortex-a15) VFPv4
 *
 * Copyright (C) 2014, Stanislav Nechutny <stanislav@nechutny.net>
 *
 * This work is licensed under the terms of the GNU LGPL, version 2.
 */
#include "asm/asm-offsets.h"
#include "asm/processor.h"
#include "util.h"
#include "arm/vfp.h"

#define TESTGRP "vfc"

#define FABSD_NUM	1.56473475206407319770818276083446107804775238037109375
#define FABSS_NUM	124520603648.0
#define FNEGD_NUM	1.98995110431943678097610472832457162439823150634765625
#define FNEGS_NUM	124371093714814364473622528.0

static struct pt_regs expected_regs;

/*
 * Capture the current register state and execute an instruction
 * that causes an exception. The test handler will check that its
 * capture of the current register state matches the capture done
 * here.
 *
 * NOTE: update clobber list if passed insns needs more than r0,r1
 */
#define test_exception(pre_insns, excptn_insn, post_insns)	\
	asm volatile(						\
		pre_insns "\n"					\
		"mov	r0, %0\n"				\
		"stmia	r0, { r0-lr }\n"			\
		"mrs	r1, cpsr\n"				\
		"str	r1, [r0, #" xstr(S_PSR) "]\n"		\
		"mov	r1, #-1\n"				\
		"str	r1, [r0, #" xstr(S_OLD_R0) "]\n"	\
		"add	r1, pc, #8\n"				\
		"str	r1, [r0, #" xstr(S_R1) "]\n"		\
		"str	r1, [r0, #" xstr(S_PC) "]\n"		\
		excptn_insn "\n"				\
		post_insns "\n"					\
		:						\
		: "r" (&expected_regs) : "r0", "r1")

/*
 * Unfortunetaly we can't use FPSCR_TEST_EXCEPTION, because gcc
 * can't combine + and ? in contraint. So we must modify it for
 * double and single precision.
 */
#define TEST_VFP_DOUBLE(_ins)						\
	asm volatile(	"fmrx r0, fpscr                         \n"	\
			"bic r0, r0, %[mask]                    \n"	\
			"fmxr fpscr, r0                         \n"	\
			_ins						\
			"fmrx %[pass], fpscr                    \n"	\
			"and %[pass], %[pass], %[mask]          \n"	\
			: [result]"+w" (result.d),			\
			  [pass]"+r" (pass)				\
			: [num1]"w" (num1.d),				\
			  [num2]"w" (num2.d),				\
			  [mask]"r" (FPSCR_CUMULATIVE)			\
			: "r0");

#define TEST_VFP_FLOAT(_ins)						\
	asm volatile(	"fmrx r0, fpscr                         \n"	\
			"bic r0, r0, %[mask]                    \n"	\
			"fmxr fpscr, r0                         \n"	\
			_ins						\
			"fmrx %[pass], fpscr                    \n"	\
			"and %[pass], %[pass], %[mask]          \n"	\
			: [result]"+t" (result.f),			\
			  [pass]"+r" (pass)				\
			: [num1]"t" (num1.f),				\
			  [num2]"t" (num2.f),				\
			  [mask]"r" (FPSCR_CUMULATIVE)			\
			: "r0");

int handled;
static void und_handler()
{
	handled = 1;
}

/**
 *	Enable VFP 
 *	http://infocenter.arm.com/help/index.jsp?topic=/com.arm.doc.ddi0438i/CDEDBHDD.html
 */
static inline void enable_vfp()
{
	asm volatile(
		"mov r0, #0x00F00000"		"\n"
		"mcr p15, 0, r0, c1, c0, 2"	"\n"
		"isb"				"\n"

		"vmsr fpexc, %[enable_bit]"
		:
		: [enable_bit]"r" (FPEXC_EN)
		: "r0"
	);
}

/**
 *	Disable VFP 
 *	http://infocenter.arm.com/help/index.jsp?topic=/com.arm.doc.ddi0438i/CDEDBHDD.html
 */
static inline void disable_vfp()
{
	asm volatile(
		"mov r0, #0"			"\n"
		"vmsr fpexc, r0"		"\n"
		:
		:
		: "r0"
	);
}

static int mvfr0_is_supported(unsigned long bits)
{
	unsigned long pass = 0;
	asm volatile(
		"fmrx r0, mvfr0"		"\n"
		"and r0, r0, %[mask]"		"\n"
		"cmp r0, #1"			"\n"
		"addcs %[pass], %[pass], #1"
		: [pass]"+r" (pass)
		: [mask]"r" (bits)
		: "r0"
	);
	return pass;
}

static void test_available()
{
	/* Check if is supported SQRT */
	report("%s[%s]", mvfr0_is_supported(MVFR0_VFP_SQRT), testname, "SQRT");

	/* Check if is supported DIV */
	report("%s[%s]", mvfr0_is_supported(MVFR0_VFP_DIV), testname, "DIV");

	/* Check if are supported Double-precision op. */
	report("%s[%s]", mvfr0_is_supported(MVFR0_VFP_DP), testname, "Double");

	/* Check if are supported Single-precision op. */
	report("%s[%s]", mvfr0_is_supported(MVFR0_VFP_SP), testname, "Single");
}

static void test_cumulative()
{
	/*
	 * FPSCR exception bits are cumulative, so must be se to 0 manualy
	 */
	DOUBLE_UNION(result, 0ULL);
	DOUBLE_UNION(num1, 0ULL);
	DOUBLE_UNION(num2, 0ULL);
	unsigned long pass = 0;
	
	/*
	 * First instruction set Invalid operation, second don't set any
	 * exception, so result should be FPSCR_IOC
	 */
	num1.d = 123;
	num2.d = -852;
	TEST_VFP_EXCEPTION(
		"fsqrtd %P[result], %P[num2]"	"\n"
		"fabsd %P[result], %P[num1]",
		pass, result.d, num1.d, num2.d, FPSCR_IOC);
	report("%s[%s]", (pass), testname, "IOC");

	/*
	 * First instruction set Inexact bit and second Invalid operation
	 * So expected is OR of this two values
	 */
	num1.d = 123.98765;
	num2.d = -152;
	TEST_VFP_EXCEPTION(
		"fsqrtd %P[result], %P[num1]"	"\n"
		"fsqrtd %P[result], %P[num2]",
		pass, result.d, num1.d, num2.d, (FPSCR_IOC|FPSCR_IXC) );
	report("%s[%s]", (pass), testname, "IOC|IXC");
}

static void test_fabsd()
{
	DOUBLE_UNION(result, 0ULL);
	DOUBLE_UNION(num, 0ULL);
	unsigned long pass = 0;
	
	/*
	 * Test maximal precision
	 * IEEE 754 Double is:
	 * 	1 bit for sign
	 * 	11 bits exponent
	 * 	52 bits fract.
	 *
	 * -1.56473475206407319770818276083446107804775238037109375
	 * is
	 * 101111111111100100001001001001110100111010110000011110100101001
	 * so expected is
	 * 001111111111100100001001001001110100111010110000011110100101001
	 * resp. 1.56473475206407319770818276083446107804775238037109375
	 */
	num.d = -FABSD_NUM;
	TEST_VFP_EXCEPTION("fabsd %P[result], %P[num1]",
		pass, result.d, num.d, NULL, FPSCR_NO_EXCEPTION);
	report("%s[%s]", (result.d == FABSD_NUM && pass), testname, "-num");
	
	/*
	 * Test -inf
	 * expected is +inf
	 * Inf is stored:
	 * 1 sign bit (+inf/-inf)
	 * 11 exponent bits all set to 1
	 * 52 fract bits all set to 0
	 */
	num.input = DOUBLE_MINUS_INF;
	TEST_VFP_EXCEPTION("fabsd %P[result], %P[num1]",
		pass, result.d, num.d, NULL, FPSCR_NO_EXCEPTION);
	report("%s[%s]", (result.input == DOUBLE_PLUS_INF && pass), testname, "-inf");
}

static void test_fabss()
{
	FLOAT_UNION(result, 0ULL);
	FLOAT_UNION(num, 0ULL);
	unsigned long pass = 0;
	
	/*
	 * Test maximal precision
	 */
	num.f = -FABSS_NUM;
	TEST_VFP_EXCEPTION("fabss %[result], %[num1]",
		pass, result.f, num.f, NULL, FPSCR_NO_EXCEPTION);
	report("%s[%s]", (result.f == FABSS_NUM && pass), testname, "-num");
	
	/*
	 * Test -inf
	 * expected is +inf
	 */
	num.input = FLOAT_MINUS_INF;
	TEST_VFP_EXCEPTION("fabss %[result], %[num1]",
		pass, result.f, num.f, NULL, FPSCR_NO_EXCEPTION);
	report("%s[%s]", (result.input == FLOAT_PLUS_INF && pass), testname, "-inf");
}

static void test_faddd()
{
	DOUBLE_UNION(num1, 0ULL);
	DOUBLE_UNION(num2, 0ULL);
	DOUBLE_UNION(result, 0ULL);
	unsigned long pass = 0;
	
	/* Test 2 nums */
	num1.d = 1.328125;
	num2.d = -0.0625;
	TEST_VFP_EXCEPTION("faddd %P[result], %P[num1], %P[num2]",
		pass, result.d, num1.d, num2.d, FPSCR_NO_EXCEPTION);
	report("%s[%s]", (result.d == 1.265625 && pass), testname,"num");
	
	/*
	 * Testing faddd for maximal precision
	 *  1.11000101010011110100011101010110010101110100011000111
	 * +1.11111010001110001010011000011000110001000001010011101
	 * 
	 * = 010000000000110111111100001111110110101101111000110110101101100
	 */
	num1.d = 1.77074094636852741313504111531074158847332000732421875;
	num2.d = 1.97742689232480339800446245135390199720859527587890625;
	TEST_VFP_EXCEPTION("faddd %P[result], %P[num1], %P[num2]",
		pass, result.d, num1.d, num2.d, FPSCR_NO_EXCEPTION);
	report("%s[%s]", ( pass &&
		result.d == 3.748167838693330811139503566664643585681915283203125),
		testname, "max precision");

	/*
	 * Test inf+inf
	 * Expected result is +inf as defined in IEEE 754
	 */
	num1.input = DOUBLE_PLUS_INF;
	num2.input = DOUBLE_PLUS_INF;
	TEST_VFP_EXCEPTION("faddd %P[result], %P[num1], %P[num2]",
		pass, result.d, num1.d, num2.d, FPSCR_NO_EXCEPTION);
	report("%s[%s]", (result.input == num1.input), testname, "(inf)+(inf)");

	/*
	 * Test inf+num
	 * Expected result is +inf
	 */
	num1.input = DOUBLE_PLUS_INF;
	num2.input = 0x3000000000000500;
	TEST_VFP_EXCEPTION("faddd %P[result], %P[num1], %P[num2]",
		pass, result.d, num1.d, num2.d, FPSCR_NO_EXCEPTION);
	report("%s[%s]", (result.input == DOUBLE_PLUS_INF && pass), testname, "inf+num");

	/*
	 * Test (-inf)+(inf)
	 * Canceling two infinities should set IOC
	 */
	num1.input = DOUBLE_PLUS_INF;
	num2.input = DOUBLE_MINUS_INF;
	TEST_VFP_EXCEPTION(
		"faddd %P[result], %P[num1], %P[num2]",
		pass, result.d, num1.d, num2.d, FPSCR_IOC);
	report("%s[%s]", (pass), testname, "(-inf)+(inf)");
}

static void test_fadds()
{
	FLOAT_UNION(num1, 0ULL);
	FLOAT_UNION(num2, 0ULL);
	FLOAT_UNION(result, 0ULL);
	unsigned long pass = 0;
	
	/*
	 * Test inf+inf
	 * Expected result is +inf as defined in IEEE 754
	 */
	num1.input = FLOAT_PLUS_INF;
	num2.input = FLOAT_PLUS_INF;
	TEST_VFP_EXCEPTION("fadds %[result], %[num1], %[num2]",
		pass, result.f, num1.f, num2.f, FPSCR_NO_EXCEPTION);
	report("%s[%s]", (result.input == num1.input), testname, "(inf)+(inf)");

	/*
	 * Test inf+num
	 * Expected result is +inf
	 */
	num1.input = FLOAT_PLUS_INF;
	num2.input = 0x30000500;
	TEST_VFP_EXCEPTION("fadds %[result], %[num1], %[num2]",
		pass, result.f, num1.f, num2.f, FPSCR_NO_EXCEPTION);
	report("%s[%s]", (result.input == FLOAT_PLUS_INF && pass), testname, "inf+num");

	/*
	 * Test (-inf)+(inf)
	 * Canceling two infinities should set IOC
	 */
	num1.input = FLOAT_PLUS_INF;
	num2.input = FLOAT_MINUS_INF;
	TEST_VFP_EXCEPTION(
		"fadds %[result], %[num1], %[num2]",
		pass, result.f, num1.f, num2.f, FPSCR_IOC);
	report("%s[%s]", (pass), testname, "(-inf)+(inf)");
}

static void test_fcmpd()
{
	int pass;
	DOUBLE_UNION(num1,0);
	DOUBLE_UNION(num2,0);
	DOUBLE_UNION(result, 0);
	
	/*
	 * Test for nulled status flags
	 */
	TEST_VFP_STATUS_FLAGS("fcmpd %P[num1], %P[num2]",
		pass, result.d, 1.5, -1.2, FPSCR_EMPTY_STATUS);
	report("%s[%s]", pass, testname, "empty");

	/*
	 * Test equality
	 * Expected is ZF=1 and CF=1
	 */
	TEST_VFP_STATUS_FLAGS("fcmpd %P[num1], %P[num2]",
		pass, result.d, 2.0, 2.0, FPSCR_Z|FPSCR_C);
	report("%s[%s]", pass, testname, "Equal");

	/*
	 * Test +null and -null
	 * 
	 * The IEEE 754 standard specifies equality if one is +0 and the
	 * other is -0.
	 */
	num1.input = DOUBLE_PLUS_NULL;
	num2.input = DOUBLE_MINUS_NULL;
	TEST_VFP_STATUS_FLAGS("fcmpd %P[num1], %P[num2]",
		pass, result.d, num1.d, num2.d, FPSCR_Z|FPSCR_C);
	report("%s[%s]", pass, testname, "+0.0,-0.0");

	/*
	 * Test (nan), (nan)
	 * Comparing two Not a number values should set IOC bit in FPSCR
	 */
	num1.input = DOUBLE_PLUS_NAN;
	num2.input = DOUBLE_MINUS_NAN;
	TEST_VFP_EXCEPTION("fcmpd %P[num1], %P[num2]",
		pass, result.d, num1.d, num2.d, FPSCR_IOC);
	report("%s[%s]", pass, testname, "NaN");
}

static void test_fcmps()
{
	int pass;
	FLOAT_UNION(num1,0);
	FLOAT_UNION(num2,0);
	FLOAT_UNION(result, 0);
	
	/*
	 * Test for nulled status flags
	 */
	TEST_VFP_STATUS_FLAGS("fcmps %[num1], %[num2]",
		pass, result.f, (float)1.5, (float)-1.2, FPSCR_EMPTY_STATUS);
	report("%s[%s]", pass, testname, "empty");

	/*
	 * Test ZF
	 * Expected is ZF=1
	 */
	TEST_VFP_STATUS_FLAGS("fcmps %[num1], %[num2]",
		pass, result.f, (float)2.0, (float)2.0, FPSCR_Z|FPSCR_C);
	report("%s[%s]", pass, testname, "ZF");

	/*
	 * Test +null and -null
	 * 
	 * The IEEE 754 standard specifies equality if one is +0 and the
	 * other is -0.
	 */
	num1.input = FLOAT_PLUS_NULL;
	num2.input = FLOAT_MINUS_NULL;
	TEST_VFP_STATUS_FLAGS("fcmps %[num1], %[num2]",
		pass, result.f, num1.f, num2.f, FPSCR_Z|FPSCR_C);
	report("%s[%s]", pass, testname, "+0.0,-0.0");

	/*
	 * Test (nan), (nan)
	 * Comparing two Not a number values should set IOC bit in FPSCR
	 */
	num1.input = FLOAT_PLUS_NAN;
	num2.input = FLOAT_MINUS_NAN;
	TEST_VFP_EXCEPTION("fcmps %[num1], %[num2]",
		pass, result.f, num1.f, num2.f, FPSCR_IOC);
	report("%s[%s]", pass, testname, "NaN");
}

static void test_fcpyd()
{
	DOUBLE_UNION(num, 0ULL);
	DOUBLE_UNION(result, 0ULL);
	unsigned long pass = 0;
	
	/*
	 * Test num to num
	 * Copy double value from one register to second.
	 * Expected result is same value in num2 as num1
	 */
	num.d = 1.75;
	TEST_VFP_EXCEPTION("fcpyd %P[result], %P[num1]",
		pass, result.d, num.d, NULL, FPSCR_NO_EXCEPTION);
	report("%s[%s]", (num.input == result.input && pass), testname, "num");

	/*
	 * Test num to same register
	 * Copy value from register to same register.
	 * Expected result is -1.5
	 */
	num.d = result.d = -1.5;
	asm volatile(
		"fcpyd %P[num1], %P[num1]"
		: [num1]"+w" (num.d)
	);
	report("%s[%s]", (result.input == num.input), testname, "same reg");

	/*
	 * Test copy NaN
	 * Try copy +NaN from ome register to second.
	 * Expected result is same NaN in second register as in first.
	 */
	num.input = DOUBLE_PLUS_NAN;
	TEST_VFP_EXCEPTION("fcpyd %P[result], %P[num1]",
		pass, result.d, num.d, NULL, FPSCR_NO_EXCEPTION);
	report("%s[%s]", (result.input == DOUBLE_PLUS_NAN && pass), testname, "NaN");
}

static void test_fcpys()
{
	FLOAT_UNION(num, 0ULL);
	FLOAT_UNION(result, 0ULL);
	unsigned long pass = 0;
	
	/*
	 * Test num to num
	 * Copy float value from one register to second.
	 * Expected result is same value in num2 as num1
	 */
	num.f = 428170093236191673322385178624.0;
	TEST_VFP_EXCEPTION("fcpys %[result], %[num1]",
		pass, result.f, num.f, NULL, FPSCR_NO_EXCEPTION);
	report("%s[%s]", (num.input == result.input && pass), testname, "num");

	/*
	 * Test num to same register
	 * Copy value from register to same register.
	 * Expected result is -1.5
	 */
	num.f = result.f = -398459532293342546724806197248.0;
	asm volatile(
		"fcpys %[num1], %[num1]"
		: [num1]"+w" (num.f)
	);
	report("%s[%s]", (result.input == num.input), testname, "same reg");

	/*
	 * Test copy NaN
	 * Try copy +NaN from ome register to second.
	 * Expected result is same NaN in second register as in first.
	 */
	num.input = FLOAT_PLUS_NAN;
	TEST_VFP_EXCEPTION("fcpys %[result], %[num1]",
		pass, result.f, num.f, NULL, FPSCR_NO_EXCEPTION);
	report("%s[%s]", (result.input == FLOAT_PLUS_NAN && pass), testname, "NaN");
}

static void test_fcvtds()
{
	FLOAT_UNION(num, 0ULL);
	DOUBLE_UNION(result, 0ULL);
	unsigned long pass = 0;

	/*
	 * Convert single-precision number to double
	 */
	num.f = 7642.75;
	TEST_VFP_EXCEPTION("fcvtds %P[result], %[num1]",
		pass, result.d, num.f, NULL, FPSCR_NO_EXCEPTION);
	report("%s[%s]", (result.d == 7642.75 && pass), testname, "Num");

	/*
	 * Convert signed single-precision number to double
	 */
	num.f = -642.75;
	TEST_VFP_EXCEPTION("fcvtds %P[result], %[num1]",
		pass, result.d, num.f, NULL, FPSCR_NO_EXCEPTION);
	report("%s[%s]", (result.d == -642.75 && pass), testname, "-Num");

	/*
	 * Convert  null
	 */
	num.input = FLOAT_PLUS_NULL;
	TEST_VFP_EXCEPTION("fcvtds %P[result], %[num1]",
		pass, result.d, num.f, NULL, FPSCR_NO_EXCEPTION);
	report("%s[%s]", (result.input == DOUBLE_PLUS_NULL && pass),
		testname, "0");

	/*
	 * Convert signed null
	 */
	num.input = FLOAT_MINUS_NULL;
	TEST_VFP_EXCEPTION("fcvtds %P[result], %[num1]",
		pass, result.d, num.f, NULL, FPSCR_NO_EXCEPTION);
	report("%s[%s]", (result.input == DOUBLE_MINUS_NULL && pass),
		testname, "-0");

	/*
	 * Convert Inf
	 */
	num.input = FLOAT_PLUS_INF;
	TEST_VFP_EXCEPTION("fcvtds %P[result], %[num1]",
		pass, result.d, num.f, NULL, FPSCR_NO_EXCEPTION);
	report("%s[%s]", (result.input == DOUBLE_PLUS_INF && pass),
		testname, "Inf");

	/*
	 * Convert -NaN
	 */
	num.input = FLOAT_MINUS_NAN;
	TEST_VFP_EXCEPTION("fcvtds %P[result], %[num1]",
		pass, result.d, num.f, NULL, FPSCR_IOC);
	report("%s[%s]", (pass), testname, "-NaN");
}

static void test_fcvtsd()
{
	DOUBLE_UNION(num, 0ULL);
	FLOAT_UNION(result, 0ULL);
	unsigned long pass = 0;

	/*
	 * Convert double-precision number to single
	 */
	num.d = 7642.75;
	TEST_VFP_EXCEPTION("fcvtsd %[result], %P[num1]",
		pass, result.f, num.d, NULL, FPSCR_NO_EXCEPTION);
	report("%s[%s]", (result.f == 7642.75 && pass), testname, "Num");

	/*
	 * Convert signed single-precision number to double
	 */
	num.d = -642.75;
	TEST_VFP_EXCEPTION("fcvtsd %[result], %P[num1]",
		pass, result.f, num.d, NULL, FPSCR_NO_EXCEPTION);
	report("%s[%s]", (result.f == -642.75 && pass), testname, "-Num");

	/*
	 * Convert null
	 */
	num.input = DOUBLE_PLUS_NULL;
	TEST_VFP_EXCEPTION("fcvtsd %[result], %P[num1]",
		pass, result.f, num.d, NULL, FPSCR_NO_EXCEPTION);
	report("%s[%s]", (result.input == FLOAT_PLUS_NULL && pass),
		testname, "0");

	/*
	 * Convert signed null
	 */
	num.input = DOUBLE_MINUS_NULL;
	TEST_VFP_EXCEPTION("fcvtsd %[result], %P[num1]",
		pass, result.f, num.d, NULL, FPSCR_NO_EXCEPTION);
	report("%s[%s]", (result.input == FLOAT_MINUS_NULL && pass),
		testname, "-0");

	/*
	 * Convert Inf
	 */
	num.input = DOUBLE_PLUS_INF;
	TEST_VFP_EXCEPTION("fcvtsd %[result], %P[num1]",
		pass, result.f, num.d, NULL, FPSCR_NO_EXCEPTION);
	report("%s[%s]", (result.input == FLOAT_PLUS_INF && pass),
		testname, "Inf");

	/*
	 * Convert -NaN
	 */
	num.input = DOUBLE_MINUS_NAN;
	TEST_VFP_EXCEPTION("fcvtsd %[result], %P[num1]",
		pass, result.f, num.d, NULL, FPSCR_IOC);
	report("%s[%s]", (pass), testname, "-NaN");

	/*
	 * Convert Double-preciosin number to single-precision which is bigger
	 * than float maximal value
	 * 
	 */
	num.d = -86468416584861351.1328461651165;
	TEST_VFP_EXCEPTION("fcvtsd %[result], %P[num1]",
		pass, result.f, num.d, NULL, FPSCR_IXC);
	report("%s[%s]", (pass), testname, "Inexact");
}

static void test_fdivd()
{
	DOUBLE_UNION(num1, 0ULL);
	DOUBLE_UNION(num2, 0ULL);
	DOUBLE_UNION(result, 0ULL);
	unsigned long pass = 0;
	
	/*
	 * Test (num1)/(-num2)
	 * Divide positive number by negative.
	 * Expected result is coresponding negative number.
	 */
	num1.d = 0.75;
	num2.d = -0.9375;
	TEST_VFP_EXCEPTION("fdivd %P[result], %P[num1], %P[num2]",
		pass, result.d, num1.d, num2.d, FPSCR_NO_EXCEPTION);
	report("%s[%s]", (result.d == -0.8 && pass), testname, "(num)/(-num)");

	/*
	 * Test (num1)/(num2)
	 * Divivde two positive numbers.
	 * Expected result is coresponding positive number.
	 */
	num1.d = 0.875;
	num2.d = 0.125;
	TEST_VFP_EXCEPTION("fdivd %P[result], %P[num1], %P[num2]",
		pass, result.d, num1.d, num2.d, FPSCR_NO_EXCEPTION);
	report("%s[%s]", (result.d == 7.0 && pass), testname, "(num)/(num)");

	/*
	 * Test 0/0
	 * Divide by zero.
	 * Should set IOC bit to 1, others to 0
	 */
	num1.d = 0.0;
	num2.d = 0.0;
	TEST_VFP_EXCEPTION("fdivd %P[result], %P[num1], %P[num2]",
		pass, result.d, num1.d, num2.d, FPSCR_IOC);
	report("%s[%s]", (pass), testname, "0/0");

	/*
	 * Test 0/-0
	 * Divide positive zero by negative zero
	 * Should set IOC bit to 1, others to 0
	 */
	num1.input = DOUBLE_PLUS_NULL;
	num2.input = DOUBLE_MINUS_NULL;
	TEST_VFP_EXCEPTION("fdivd %P[result], %P[num1], %P[num2]",
		pass, result.d, num1.d, num2.d, FPSCR_IOC);
	report("%s[%s]", (pass), testname, "0/-0");

	/*
	 * Test 1.25/-0
	 * Divide number by negative zero.
	 * Should set DZC bit to 1, others to 0
	 */
	num1.d = 1.25;
	num2.input = DOUBLE_MINUS_NULL;
	TEST_VFP_EXCEPTION("fdivd %P[result], %P[num1], %P[num2]",
		pass, result.d, num1.d, num2.d, FPSCR_DZC);
	report("%s[%s]", (pass), testname, "num/-0");

	/*
	 * Test -inf/inf
	 * Divide -infinity by +infinity
	 * Should set IOC bit to 1, others to 0
	 */
	num1.input = DOUBLE_MINUS_INF;
	num2.input = DOUBLE_PLUS_INF;
	TEST_VFP_EXCEPTION("fdivd %P[result], %P[num1], %P[num2]",
		pass, result.d, num1.d, num2.d, FPSCR_IOC);
	report("%s[%s]", (pass), testname, "(-inf)/(inf)");

	/*
	 * Test 1.0/1.0
	 * Divide 1 by 1, fract is 0, so test correct detecting 1.0 vs. 0.0
	 * Should set FPSCR bits to 0 and expected result is 1.0
	 */
	num2.d = num1.d = 1.0;
	TEST_VFP_EXCEPTION("fdivd %P[result], %P[num1], %P[num2]",
		pass, result.d, num1.d, num2.d, FPSCR_NO_EXCEPTION);
	report("%s[%s]", (result.d == num2.d && pass), testname, "(1.0)/(1.0)");
}

static void test_fdivs()
{
	FLOAT_UNION(num1, 0ULL);
	FLOAT_UNION(num2, 0ULL);
	FLOAT_UNION(result, 0ULL);
	unsigned long pass = 0;
	
	/*
	 * Test (num1)/(-num2)
	 * Divide positive number by negative.
	 * Expected result is coresponding negative number.
	 */
	num1.f = 0.75;
	num2.f = -0.1;
	TEST_VFP_EXCEPTION("fdivs %[result], %[num1], %[num2]",
		pass, result.f, num1.f, num2.f, FPSCR_NO_EXCEPTION);
	report("%s[%s]", (result.f == -7.5 && pass), testname, "(num)/(-num)");

	/*
	 * Test (num1)/(num2)
	 * Divivde two positive numbers.
	 * Expected result is coresponding positive number.
	 */
	num1.f = 0.875;
	num2.f = 0.125;
	TEST_VFP_EXCEPTION("fdivs %[result], %[num1], %[num2]",
		pass, result.f, num1.f, num2.f, FPSCR_NO_EXCEPTION);
	report("%s[%s]", (result.f == 7.0 && pass), testname, "(num)/(num)");

	/*
	 * Test 0/0
	 * Divide by zero.
	 * Should set IOC bit to 1, others to 0
	 */
	num1.f = 0.0;
	num2.f = 0.0;
	TEST_VFP_EXCEPTION("fdivs %[result], %[num1], %[num2]",
		pass, result.f, num1.f, num2.f, FPSCR_IOC);
	report("%s[%s]", (pass), testname, "0/0");

	/*
	 * Test 0/-0
	 * Divide positive zero by negative zero
	 * Should set IOC bit to 1, others to 0
	 */
	num1.input = FLOAT_PLUS_NULL;
	num2.input = FLOAT_MINUS_NULL;
	TEST_VFP_EXCEPTION("fdivs %[result], %[num1], %[num2]",
		pass, result.f, num1.f, num2.f, FPSCR_IOC);
	report("%s[%s]", (pass), testname, "0/-0");

	/*
	 * Test 1.25/-0
	 * Divide number by negative zero.
	 * Should set DZC bit to 1, others to 0
	 */
	num1.f = 1.25;
	num2.input = FLOAT_MINUS_NULL;
	TEST_VFP_EXCEPTION("fdivs %[result], %[num1], %[num2]",
		pass, result.f, num1.f, num2.f, FPSCR_DZC);
	report("%s[%s]", (pass), testname, "num/-0");

	/*
	 * Test -inf/inf
	 * Divide -infinity by +infinity
	 * Should set IOC bit to 1, others to 0
	 */
	num1.input = FLOAT_MINUS_INF;
	num2.input = FLOAT_PLUS_INF;
	TEST_VFP_EXCEPTION("fdivs %[result], %[num1], %[num2]",
		pass, result.f, num1.f, num2.f, FPSCR_IOC);
	report("%s[%s]", (pass), testname, "(-inf)/(inf)");

	/*
	 * Test 1.0/1.0
	 * Divide 1 by 1, fract is 0, so test correct detecting 1.0 vs. 0.0
	 * Should set FPSCR bits to 0 and expected result is 1.0
	 */
	num2.f = num1.f = 1.0;
	TEST_VFP_EXCEPTION("fdivs %[result], %[num1], %[num2]",
		pass, result.f, num1.f, num2.f, FPSCR_NO_EXCEPTION);
	report("%s[%s]", (result.f == num2.f && pass), testname, "(1.0)/(1.0)");
}

static void test_fmacd()
{
	DOUBLE_UNION(num1, 0ULL);
	DOUBLE_UNION(num2, 0ULL);
	DOUBLE_UNION(result, 0ULL);
	unsigned long pass = 0;

	/*
	 * Test 0+0*0
	 */
	TEST_VFP_DOUBLE("fmacd %P[result], %P[num1], %P[num2] \n");
	report("%s[%s]", (result.d == 0 && pass == FPSCR_NO_EXCEPTION),
		testname, "0");

	/*
	 * Test 0+inf*1
	 */
	result.d = 0;
	num1.input = DOUBLE_PLUS_INF;
	num2.d = 1;
	TEST_VFP_DOUBLE("fmacd %P[result], %P[num1], %P[num2] \n");
	report("%s[%s]", (result.input == DOUBLE_PLUS_INF && pass == FPSCR_NO_EXCEPTION),
		testname, "INF");

	/*
	 * Test cancel INF
	 * -Inf+inf*inf
	 */
	result.input = DOUBLE_MINUS_INF;
	num1.input = num2.input = DOUBLE_PLUS_INF;
	TEST_VFP_DOUBLE("fmacd %P[result], %P[num1], %P[num2] \n");
	report("%s[%s]", (pass == FPSCR_IOC), testname, "Canceling inf");

	/*
	 * Test calculating with NaN
	 */
	result.d = 9455659;
	num1.d = 12348.5;
	num2.input = DOUBLE_PLUS_NAN;
	TEST_VFP_DOUBLE("fmacd %P[result], %P[num1], %P[num2] \n");
	report("%s[%s]", (pass == FPSCR_IOC), testname, "NaN");
}

static void test_fmacs()
{
	FLOAT_UNION(num1, 0UL);
	FLOAT_UNION(num2, 0UL);
	FLOAT_UNION(result, 0UL);
	unsigned long pass = 0UL;

	/*
	 * Test 0+0*0
	 */
	TEST_VFP_FLOAT("fmacs %[result], %[num1], %[num2] \n");
	report("%s[%s]", (result.f == 0 && pass == FPSCR_NO_EXCEPTION),
		testname, "0");

	/*
	 * Test 0+inf*1
	 */
	result.f = 0;
	num1.input = FLOAT_PLUS_INF;
	num2.f = 1;
	TEST_VFP_FLOAT("fmacs %[result], %[num1], %[num2] \n");
	report("%s[%s]", (result.input == FLOAT_PLUS_INF && pass == FPSCR_NO_EXCEPTION),
		testname, "INF");

	/*
	 * Test cancel INF
	 * -Inf+inf*inf
	 */
	result.input = FLOAT_MINUS_INF;
	num1.input = num2.input = FLOAT_PLUS_INF;
	TEST_VFP_FLOAT("fmacs %[result], %[num1], %[num2] \n");
	report("%s[%s]", (pass == FPSCR_IOC), testname, "Canceling inf");

	/*
	 * Test calculating with NaN
	 */
	result.f = 9455659;
	num1.f = 12348.5;
	num2.input = FLOAT_PLUS_NAN;
	TEST_VFP_FLOAT("fmacs %[result], %[num1], %[num2] \n");
	report("%s[%s]", (pass == FPSCR_IOC), testname, "NaN");
}

static void test_fmdhr()
{
	unsigned long num1 = DOUBLE_TOP(DOUBLE_PLUS_NULL);
	DOUBLE_UNION(num2, DOUBLE_MINUS_NULL);

	/* Copy top of +0 to double containing -0 */
	asm volatile("fmdhr %P[num2], %[num1]"
	: [num2]"+w" (num2.d)
	: [num1]"r" (num1)
	);
	report("%s[%s]", (num2.input == DOUBLE_PLUS_NULL), testname, "Null");
	
	/* Copy top of -Inf to double containing +Inf */
	num2.input = DOUBLE_PLUS_INF;
	num1 = DOUBLE_TOP(DOUBLE_MINUS_INF);
	asm volatile("fmdhr %P[num2], %[num1]"
	: [num2]"+w" (num2.d)
	: [num1]"r" (num1)
	);
	report("%s[%s]", (num2.input == DOUBLE_MINUS_INF), testname, "+Inf");

	/* Copy top of +NaN to double containing -NaN */
	num2.input = DOUBLE_MINUS_NAN;
	num1 = DOUBLE_TOP(DOUBLE_PLUS_NAN);
	asm volatile("fmdhr %P[num2], %[num1]"
	: [num2]"+w" (num2.d)
	: [num1]"r" (num1)
	);
	report("%s[%s]", (num2.input == DOUBLE_PLUS_NAN), testname, "+NaN");
}

static void test_fmdlr()
{
	unsigned long num1 = DOUBLE_BOTTOM(DOUBLE_PLUS_NULL);
	DOUBLE_UNION(num2, DOUBLE_MINUS_NULL);

	/* Copy bottom of +0 to double containing -0 */
	asm volatile("fmdlr %P[num2], %[num1]"
	: [num2]"+w" (num2.d)
	: [num1]"r" (num1)
	);
	report("%s[%s]", (num2.input == DOUBLE_MINUS_NULL), testname, "Null");
	
	/* Copy bottom of -Inf to double containing +Inf */
	num2.input = DOUBLE_PLUS_INF;
	num1 = DOUBLE_BOTTOM(DOUBLE_MINUS_INF);
	asm volatile("fmdlr %P[num2], %[num1]"
	: [num2]"+w" (num2.d)
	: [num1]"r" (num1)
	);
	report("%s[%s]", (num2.input == DOUBLE_PLUS_INF), testname, "+Inf");

	/* Copy bottom of +NaN to double containing -NaN */
	num2.input = DOUBLE_MINUS_NAN;
	num1 = DOUBLE_BOTTOM(DOUBLE_PLUS_NAN);
	asm volatile("fmdlr %P[num2], %[num1]"
	: [num2]"+w" (num2.d)
	: [num1]"r" (num1)
	);
	report("%s[%s]", (num2.input == DOUBLE_MINUS_NAN), testname, "+NaN");
}

static void test_fmrdh()
{
	unsigned long num1;
	DOUBLE_UNION(num2, DOUBLE_MINUS_NULL);

	/* Copy top of +0 to rX */
	asm volatile("fmrdh %[num1], %P[num2]"
	: [num1]"=r" (num1)
	: [num2]"w" (num2.d)
	);
	report("%s[%s]", (num1 == DOUBLE_TOP(DOUBLE_MINUS_NULL)),
		testname, "-Null");
	
	/* Copy top of -Inf to rX */
	num2.input = DOUBLE_PLUS_INF;
	asm volatile("fmrdh %[num1], %P[num2]"
	: [num1]"=r" (num1)
	: [num2]"w" (num2.d)
	);
	report("%s[%s]", (num1 == DOUBLE_TOP(DOUBLE_PLUS_INF)),
		testname, "+Inf");

	/* Copy top of +NaN to rX */
	num2.input = DOUBLE_MINUS_NAN;
	asm volatile("fmrdh %[num1], %P[num2]"
	: [num1]"=r" (num1)
	: [num2]"w" (num2.d)
	);
	report("%s[%s]", (num1 == DOUBLE_TOP(DOUBLE_MINUS_NAN)),
		testname, "-NaN");
}

static void test_fmrdl()
{
	unsigned long num1;
	DOUBLE_UNION(num2, DOUBLE_MINUS_NULL);

	/* Copy bottom of +0 to rX */
	asm volatile("fmrdl %[num1], %P[num2]"
	: [num1]"=r" (num1)
	: [num2]"w" (num2.d)
	);
	report("%s[%s]", (num1 == DOUBLE_BOTTOM(DOUBLE_MINUS_NULL)),
		testname, "-Null");
	
	/* Copy bottom of -Inf to rX */
	num2.input = DOUBLE_PLUS_INF;
	asm volatile("fmrdl %[num1], %P[num2]"
	: [num1]"=r" (num1)
	: [num2]"w" (num2.d)
	);
	report("%s[%s]", (num1 == DOUBLE_BOTTOM(DOUBLE_PLUS_INF)),
		testname, "+Inf");

	/* Copy bottom of +NaN to rX */
	num2.input = DOUBLE_MINUS_NAN;
	asm volatile("fmrdl %[num1], %P[num2]"
	: [num1]"=r" (num1)
	: [num2]"w" (num2.d)
	);
	report("%s[%s]", (num1 == DOUBLE_BOTTOM(DOUBLE_MINUS_NAN)),
		testname, "-NaN");
}

static void test_fmdrr()
{
	unsigned long num1 = DOUBLE_BOTTOM(DOUBLE_PLUS_NULL), num2 = DOUBLE_TOP(DOUBLE_PLUS_NULL);
	DOUBLE_UNION(num3, 0xFFFFFFFFFFFFFFFF);

	/* Copy all 0 to double containing all bits set to 1 */
	asm volatile("fmdrr %P[num3], %[num1], %[num2]"
	: [num3]"=w" (num3.d)
	: [num1]"r" (num1),
	  [num2]"r" (num2)
	);
	report("%s[%s]", (num3.input == 0ULL), testname, "Null");

	/* Convert splitted -Inf double to double register */
	num1 = DOUBLE_BOTTOM(DOUBLE_MINUS_INF);
	num2 = DOUBLE_TOP(DOUBLE_MINUS_INF);
	asm volatile("fmdrr %P[num3], %[num1], %[num2]"
	: [num3]"=w" (num3.d)
	: [num1]"r" (num1),
	  [num2]"r" (num2)
	);
	report("%s[%s]", (num3.input == DOUBLE_MINUS_INF), testname, "-Inf");

	/* Convert splitted +NaN double to double register */
	num1 = DOUBLE_BOTTOM(DOUBLE_PLUS_NAN);
	num2 = DOUBLE_TOP(DOUBLE_PLUS_NAN);
	asm volatile("fmdrr %P[num3], %[num1], %[num2]"
	: [num3]"=w" (num3.d)
	: [num1]"r" (num1),
	  [num2]"r" (num2)
	);
	report("%s[%s]", (num3.input == DOUBLE_PLUS_NAN), testname, "+NaN");
}

static void test_fmrrd()
{
	unsigned long num1 = 0xFFFFFFF, num2 = 0xFFFFFFFF;
	DOUBLE_UNION(num3, DOUBLE_PLUS_NULL);

	/* Copy 0 double to two rX registers */
	asm volatile("fmrrd %[num1], %[num2], %P[num3]"
	: [num1]"=r" (num1),
	  [num2]"=r" (num2)
	: [num3]"w" (num3.d)
	);
	report("%s[%s]", (num1 == DOUBLE_BOTTOM(DOUBLE_PLUS_NULL) &&
			  num2 == DOUBLE_TOP(DOUBLE_PLUS_NULL) ),
		testname, "Null");

	/* Split -Inf double to two rX registers */
	num3.input = DOUBLE_MINUS_INF;
	asm volatile("fmrrd %[num1], %[num2], %P[num3]"
	: [num1]"=r" (num1),
	  [num2]"=r" (num2)
	: [num3]"w" (num3.d)
	);
	report("%s[%s]", (num1 == DOUBLE_BOTTOM(DOUBLE_MINUS_INF) &&
			  num2 == DOUBLE_TOP(DOUBLE_MINUS_INF) ),
		testname, "-Inf");

	/* Split +NaN double to two rX registers */
	num3.input = DOUBLE_PLUS_NAN;
	asm volatile("fmrrd %[num1], %[num2], %P[num3]"
	: [num1]"=r" (num1),
	  [num2]"=r" (num2)
	: [num3]"w" (num3.d)
	);
	report("%s[%s]", (num1 == DOUBLE_BOTTOM(DOUBLE_PLUS_NAN) &&
			  num2 == DOUBLE_TOP(DOUBLE_PLUS_NAN) ),
		testname, "+NaN");
}

static void test_fmrrs()
{
	unsigned long num1 = 0UL, num2 = 0UL;
	FLOAT_UNION(num3, 0xFFFFFFFF);
	FLOAT_UNION(num4, 0xFFFFFFFF);

	/* Copy single precision registers to rX */
	asm volatile(
		"fcpys s0, %[num3]"			"\n"
		"fcpys s1, %[num4]"			"\n"
		"fmrrs %[num1], %[num2], {s0, s1}"
	: [num1]"=r" (num1),
	  [num2]"=r" (num2)
	: [num3]"t" (num3.f),
	  [num4]"t" (num4.f)
	);
	report("%s[%s]", (num1 == 0xFFFFFFFF && num2 == 0xFFFFFFFF ),
		testname, "1s");

	num3.input = FLOAT_MINUS_NAN;
	num4.input = FLOAT_PLUS_INF;
	/* Copy -NaN and +Inf to rX registers */
	asm volatile(
		"fcpys s0, %[num3]"			"\n"
		"fcpys s1, %[num4]"			"\n"
		"fmrrs %[num1], %[num2], {s0, s1}"
	: [num1]"=r" (num1),
	  [num2]"=r" (num2)
	: [num3]"t" (num3.f),
	  [num4]"t" (num4.f)
	);
	report("%s[%s]", (num1 == FLOAT_MINUS_NAN &&
			  num2 == FLOAT_PLUS_INF ),
		testname, "Inf and -NaN");
}

static void test_fmrs()
{
	unsigned long num1 = 0xFFFFFFFF;
	FLOAT_UNION(num2, FLOAT_PLUS_NULL);

	/* Copy nulls from single to rX */
	asm volatile("fmrs %[num1], %[num2]"
	: [num1]"=r" (num1)
	: [num2]"t" (num2.f)
	);
	report("%s[%s]", (num1 == FLOAT_PLUS_NULL), testname, "Null");

	/* Copy -INF from single to rX */
	num2.input = FLOAT_MINUS_INF;
	asm volatile("fmrs %[num1], %[num2]"
	: [num1]"=r" (num1)
	: [num2]"t" (num2.f)
	);
	report("%s[%s]", (num1 == FLOAT_MINUS_INF), testname, "-Inf");

	/* Copy +NaN from single to rX */
	num2.input = FLOAT_PLUS_NAN;
	asm volatile("fmrs %[num1], %[num2]"
	: [num1]"=r" (num1)
	: [num2]"t" (num2.f)
	);
	report("%s[%s]", (num1 == FLOAT_PLUS_NAN), testname, "+NaN");
}

static void test_fmscd()
{
	DOUBLE_UNION(num1, 0ULL);
	DOUBLE_UNION(num2, 0ULL);
	DOUBLE_UNION(result, 0ULL);
	unsigned long pass = 0;

	/*
	 * Test -0+0*0
	 */
	TEST_VFP_DOUBLE("fmscd %P[result], %P[num1], %P[num2] \n");
	report("%s[%s]", (result.d == 0 && pass == FPSCR_NO_EXCEPTION),
		testname, "0");

	/*
	 * Test -0+inf*1
	 */
	result.d = 0;
	num1.input = DOUBLE_PLUS_INF;
	num2.d = 1;
	TEST_VFP_DOUBLE("fmscd %P[result], %P[num1], %P[num2] \n");
	report("%s[%s]", (result.input == DOUBLE_PLUS_INF && pass == FPSCR_NO_EXCEPTION),
		testname, "INF");

	/*
	 * Test cancel INF
	 * +Inf-inf*inf
	 */
	num1.input = result.input = DOUBLE_MINUS_INF;
	num2.input = DOUBLE_PLUS_INF;
	TEST_VFP_DOUBLE("fmscd %P[result], %P[num1], %P[num2] \n");
	report("%s[%s]", (pass == FPSCR_IOC), testname, "Canceling inf");

	/*
	 * Test calculating with NaN
	 */
	result.d = 9455659;
	num1.d = 12348.5;
	num2.input = DOUBLE_PLUS_NAN;
	TEST_VFP_DOUBLE("fmscd %P[result], %P[num1], %P[num2] \n");
	report("%s[%s]", (pass == FPSCR_IOC), testname, "NaN");
}

static void test_fmscs()
{
	FLOAT_UNION(num1, 0UL);
	FLOAT_UNION(num2, 0UL);
	FLOAT_UNION(result, 0UL);
	unsigned long pass = 0UL;

	/*
	 * Test -0+0*0
	 */
	TEST_VFP_FLOAT("fmscs %[result], %[num1], %[num2] \n");
	report("%s[%s]", (result.f == 0 && pass == FPSCR_NO_EXCEPTION),
		testname, "0");

	/*
	 * Test -0+inf*1
	 */
	result.f = 0;
	num1.input = FLOAT_PLUS_INF;
	num2.f = 1;
	TEST_VFP_FLOAT("fmscs %[result], %[num1], %[num2] \n");
	report("%s[%s]", (result.input == FLOAT_PLUS_INF && pass == FPSCR_NO_EXCEPTION),
		testname, "INF");

	/*
	 * Test cancel INF
	 * +Inf-inf*inf
	 */
	num1.input = result.input = FLOAT_MINUS_INF;
	num2.input = FLOAT_PLUS_INF;
	TEST_VFP_FLOAT("fmscs %[result], %[num1], %[num2] \n");
	report("%s[%s]", (pass == FPSCR_IOC), testname, "Canceling inf");

	/*
	 * Test calculating with NaN
	 */
	result.f = 9455659;
	num1.f = 12348.5;
	num2.input = FLOAT_PLUS_NAN;
	TEST_VFP_FLOAT("fmscs %[result], %[num1], %[num2] \n");
	report("%s[%s]", (pass == FPSCR_IOC), testname, "NaN");
}

static void test_fmsr()
{
	unsigned long num1 = FLOAT_PLUS_NULL;
	FLOAT_UNION(num2, 0xFFFFFFFF);

	/* Copy nulls from single to rX */
	asm volatile("fmsr %[num1], %[num2]"
	: [num1]"=t" (num2.f)
	: [num2]"r" (num1)
	);
	report("%s[%s]", (num2.input == FLOAT_PLUS_NULL), testname, "Null");

	/* Copy -INF from single to rX */
	num1 = FLOAT_MINUS_INF;
	asm volatile("fmsr %[num1], %[num2]"
	: [num1]"=t" (num2.f)
	: [num2]"r" (num1)
	);
	report("%s[%s]", (num2.input == FLOAT_MINUS_INF), testname, "-Inf");

	/* Copy +NaN from single to rX */
	num1 = FLOAT_PLUS_NAN;
	asm volatile("fmsr %[num1], %[num2]"
	: [num1]"=t" (num2.f)
	: [num2]"r" (num1)
	);
	report("%s[%s]", (num2.input == FLOAT_PLUS_NAN), testname, "+NaN");
}

static void test_fmsrr()
{
	unsigned long num1 = 0xFFFFFFFF, num2 = 0xFFFFFFFF;
	FLOAT_UNION(num3, FLOAT_PLUS_NULL);
	FLOAT_UNION(num4, FLOAT_PLUS_NULL);

	/* Copy 1s to single precision registers */
	asm volatile(
		"fmsrr {s0, s1}, %[num1], %[num2]"	"\n"
		"fcpys %[num3], s0"			"\n"
		"fcpys %[num4], s1"
	: [num3]"=t" (num3.f),
	  [num4]"=t" (num4.f)
	: [num1]"r" (num1),
	  [num2]"r" (num2)
	: "s0","s1" 
	);
	report("%s[%s]", (num3.input == 0xFFFFFFFF &&
			  num4.input == 0xFFFFFFFF ), testname, "1s");

	num1 = FLOAT_MINUS_NAN;
	num2 = FLOAT_PLUS_INF;
	/* Copy -NaN and +Inf to sX registers */
	asm volatile(
		"fmsrr {s0, s1}, %[num1], %[num2]"	"\n"
		"fcpys %[num3], s0"			"\n"
		"fcpys %[num4], s1"
	: [num3]"=t" (num3.f),
	  [num4]"=t" (num4.f)
	: [num1]"r" (num1),
	  [num2]"r" (num2)
	: "s0","s1" 
	);
	report("%s[%s]", (num3.input == FLOAT_MINUS_NAN &&
			  num4.input == FLOAT_PLUS_INF ),
		testname, "Inf and -NaN");
}

static void test_fnegd()
{
	DOUBLE_UNION(num, 0ULL);
	DOUBLE_UNION(result, 0ULL);
	unsigned int pass = 0;
	
	/*
	 * +Number
	 * Negate number with maximal fract bit range
	 * 001111111111111111010110110101101111100000011011000011101001101
	 * Expected result is
	 * 101111111111111111010110110101101111100000011011000011101001101
	 */
	num.d = FNEGD_NUM;
	TEST_VFP_EXCEPTION("fnegd %P[result], %P[num1]",
		pass, result.d, num.d, NULL, FPSCR_NO_EXCEPTION);
	report("%s[%s]", (result.d == -FNEGD_NUM && pass), testname, "+num");

	/*
	 * -Inf
	 * Negate -infinity to +infinity
	 */
	num.input = DOUBLE_PLUS_INF;
	TEST_VFP_EXCEPTION("fnegd %P[result], %P[num1]",
		pass, result.d, num.d, NULL, FPSCR_NO_EXCEPTION);
	report("%s[%s]", (result.input == DOUBLE_MINUS_INF && pass), testname, "-inf");

	/*
	 * +NaN and check status register
	 * Try negate +NaN. Result should be same NaN with changed sign bit
	 * and exception bits should be 0
	 */
	num.input = DOUBLE_PLUS_NAN;
	TEST_VFP_EXCEPTION("fnegd %P[result], %P[num1]",
		pass, result.d, num.d, NULL, FPSCR_NO_EXCEPTION);
	report("%s[%s]", (result.input == DOUBLE_MINUS_NAN && pass), testname, "+NaN");
}

static void test_fnegs()
{
	FLOAT_UNION(num, 0ULL);
	FLOAT_UNION(result, 0ULL);
	unsigned int pass = 0;
	
	/*
	 * +Number
	 * Negate number with maximal fract bit range
	 */
	num.f = FNEGS_NUM;
	TEST_VFP_EXCEPTION("fnegs %[result], %[num1]",
		pass, result.f, num.f, NULL, FPSCR_NO_EXCEPTION);
	report("%s[%s]", (result.f == -FNEGS_NUM && pass), testname, "+num");

	/*
	 * -Inf
	 * Negate -infinity to +infinity
	 */
	num.input = FLOAT_PLUS_INF;
	TEST_VFP_EXCEPTION("fnegs %[result], %[num1]",
		pass, result.f, num.f, NULL, FPSCR_NO_EXCEPTION);
	report("%s[%s]", (result.input == FLOAT_MINUS_INF && pass), testname, "-inf");

	/*
	 * +NaN and check status register
	 * Try negate +NaN. Result should be same NaN with changed sign bit
	 * and exception bits should be 0
	 */
	num.input = FLOAT_PLUS_NAN;
	TEST_VFP_EXCEPTION("fnegs %[result], %[num1]",
		pass, result.f, num.f, NULL, FPSCR_NO_EXCEPTION);
	report("%s[%s]", (result.input == FLOAT_MINUS_NAN && pass), testname, "+NaN");
}


static void test_fnmacd()
{
	DOUBLE_UNION(num1, 0ULL);
	DOUBLE_UNION(num2, 0ULL);
	DOUBLE_UNION(result, 0ULL);
	unsigned long pass = 0;

	/*
	 * Test 0-0*0
	 */
	TEST_VFP_DOUBLE("fnmacd %P[result], %P[num1], %P[num2] \n");
	report("%s[%s]", (result.d == 0 && pass == FPSCR_NO_EXCEPTION),
		testname, "0");

	/*
	 * Test 0-inf*1
	 */
	result.d = 0;
	num1.input = DOUBLE_PLUS_INF;
	num2.d = 1;
	TEST_VFP_DOUBLE("fnmacd %P[result], %P[num1], %P[num2] \n");
	report("%s[%s]", (result.input == DOUBLE_MINUS_INF && pass == FPSCR_NO_EXCEPTION),
		testname, "INF");

	/*
	 * Test cancel INF
	 * Inf-inf*inf
	 */
	num2.input = num1.input = result.input = DOUBLE_PLUS_INF;
	TEST_VFP_DOUBLE("fnmacd %P[result], %P[num1], %P[num2] \n");
	report("%s[%s]", (pass == FPSCR_IOC), testname, "Canceling inf");

	/*
	 * Test calculating with NaN
	 */
	result.d = 9455659;
	num1.d = 12348.5;
	num2.input = DOUBLE_PLUS_NAN;
	TEST_VFP_DOUBLE("fnmacd %P[result], %P[num1], %P[num2] \n");
	report("%s[%s]", (pass == FPSCR_IOC), testname, "NaN");
}

static void test_fnmacs()
{
	FLOAT_UNION(num1, 0UL);
	FLOAT_UNION(num2, 0UL);
	FLOAT_UNION(result, 0UL);
	unsigned long pass = 0UL;

	/*
	 * Test 0-0*0
	 */
	TEST_VFP_FLOAT("fnmacs %[result], %[num1], %[num2] \n");
	report("%s[%s]", (result.f == 0 && pass == FPSCR_NO_EXCEPTION),
		testname, "0");

	/*
	 * Test 0-inf*1
	 */
	result.f = 0;
	num1.input = FLOAT_PLUS_INF;
	num2.f = 1;
	TEST_VFP_FLOAT("fnmacs %[result], %[num1], %[num2] \n");
	report("%s[%s]", (result.input == FLOAT_MINUS_INF && pass == FPSCR_NO_EXCEPTION),
		testname, "INF");

	/*
	 * Test cancel INF
	 * Inf-inf*inf
	 */
	num2.input = num1.input =  result.input = FLOAT_PLUS_INF;
	TEST_VFP_FLOAT("fnmacs %[result], %[num1], %[num2] \n");
	report("%s[%s]", (pass == FPSCR_IOC), testname, "Canceling inf");

	/*
	 * Test calculating with NaN
	 */
	result.f = 9455659;
	num1.f = 12348.5;
	num2.input = FLOAT_PLUS_NAN;
	TEST_VFP_FLOAT("fnmacs %[result], %[num1], %[num2] \n");
	report("%s[%s]", (pass == FPSCR_IOC), testname, "NaN");
}

static void test_fnmscd()
{
	DOUBLE_UNION(num1, 0ULL);
	DOUBLE_UNION(num2, 0ULL);
	DOUBLE_UNION(result, 0ULL);
	unsigned long pass = 0;

	/*
	 * Test -0-0*0
	 */
	TEST_VFP_DOUBLE("fnmscd %P[result], %P[num1], %P[num2] \n");
	report("%s[%s]", (result.d == 0 && pass == FPSCR_NO_EXCEPTION),
		testname, "0");

	/*
	 * Test -0-inf*1
	 */
	result.d = 0;
	num1.input = DOUBLE_PLUS_INF;
	num2.d = 1;
	TEST_VFP_DOUBLE("fnmscd %P[result], %P[num1], %P[num2] \n");
	report("%s[%s]", (result.input == DOUBLE_MINUS_INF && pass == FPSCR_NO_EXCEPTION),
		testname, "INF");

	/*
	 * Test cancel INF
	 * -Inf+inf*inf
	 */
	num1.input = result.input = DOUBLE_PLUS_INF;
	num2.input = DOUBLE_MINUS_INF;
	TEST_VFP_DOUBLE("fnmscd %P[result], %P[num1], %P[num2] \n");
	report("%s[%s]", (pass == FPSCR_IOC), testname, "Canceling inf");

	/*
	 * Test calculating with NaN
	 */
	result.d = 9455659;
	num1.d = 12348.5;
	num2.input = DOUBLE_PLUS_NAN;
	TEST_VFP_DOUBLE("fnmscd %P[result], %P[num1], %P[num2] \n");
	report("%s[%s]", (pass == FPSCR_IOC), testname, "NaN");
}

static void test_fnmscs()
{
	FLOAT_UNION(num1, 0UL);
	FLOAT_UNION(num2, 0UL);
	FLOAT_UNION(result, 0UL);
	unsigned long pass = 0UL;

	/*
	 * Test -0-0*0
	 */
	TEST_VFP_FLOAT("fnmscs %[result], %[num1], %[num2] \n");
	report("%s[%s]", (result.f == 0 && pass == FPSCR_NO_EXCEPTION),
		testname, "0");

	/*
	 * Test -0-inf*1
	 */
	result.f = 0;
	num1.input = FLOAT_PLUS_INF;
	num2.f = 1;
	TEST_VFP_FLOAT("fnmscs %[result], %[num1], %[num2] \n");
	report("%s[%s]", (result.input == FLOAT_MINUS_INF && pass == FPSCR_NO_EXCEPTION),
		testname, "INF");

	/*
	 * Test cancel INF
	 * -Inf+inf*inf
	 */
	num1.input =  result.input = FLOAT_PLUS_INF;
	num2.input = FLOAT_MINUS_INF;
	TEST_VFP_FLOAT("fnmscs %[result], %[num1], %[num2] \n");
	report("%s[%s]", (pass == FPSCR_IOC), testname, "Canceling inf");

	/*
	 * Test calculating with NaN
	 */
	result.f = 9455659;
	num1.f = 12348.5;
	num2.input = FLOAT_PLUS_NAN;
	TEST_VFP_FLOAT("fnmscs %[result], %[num1], %[num2] \n");
	report("%s[%s]", (pass == FPSCR_IOC), testname, "NaN");
}

static void test_fsqrtd()
{
	DOUBLE_UNION(num1, 0ULL);
	DOUBLE_UNION(result, 0ULL);
	unsigned long pass = 0;

	/*
	 * Test square for 4, expected value is 2
	 */
	num1.d = 4;
	TEST_VFP_EXCEPTION("fsqrtd %P[result], %P[num1]",
		pass, result.d, num1.d, NULL, FPSCR_NO_EXCEPTION);
	report("%s[%s]", (result.d == 2 && pass), testname, "num");

	/*
	 * Test sqrt with inaccurate result
	 * should throw Inexact exception
	 */
	num1.d = 162.639023;
	TEST_VFP_EXCEPTION("fsqrtd %P[result], %P[num1]",
		pass, result.d, num1.d, NULL, FPSCR_IXC);
	report("%s[%s]", (result.input == 0x4029818949B6B583 && pass),
		testname, "Inaccurate");

	/*
	 * Test sqrt 0
	 */
	num1.d = 0;
	TEST_VFP_EXCEPTION("fsqrtd %P[result], %P[num1]",
		pass, result.d, num1.d, NULL, FPSCR_NO_EXCEPTION);
	report("%s[%s]", (result.d == 0 && pass), testname, "0");

	/*
	 * Test sqrt -0
	 */
	num1.input = DOUBLE_MINUS_NULL;
	TEST_VFP_EXCEPTION("fsqrtd %P[result], %P[num1]",
		pass, result.d, num1.d, NULL, FPSCR_NO_EXCEPTION);
	report("%s[%s]", (result.d == 0 && pass), testname, "-0");

	/*
	 * Test sqrt -1
	 * should throw Invalid operation
	 */
	num1.d = -1;
	TEST_VFP_EXCEPTION("fsqrtd %P[result], %P[num1]",
		pass, result.d, num1.d, NULL, FPSCR_IOC);
	report("%s[%s]", (pass), testname, "-1");

	/*
	 * NaN, should throw Invalid operation exception
	 */
	num1.input = DOUBLE_PLUS_NAN;
	TEST_VFP_EXCEPTION("fsqrtd %P[result], %P[num1]",
		pass, result.d, num1.d, NULL, FPSCR_IOC);
	report("%s[%s]", (pass), testname, "+NaN");

	/*
	 * -NaN, should throw Invalid operation exception
	 */
	num1.input = DOUBLE_MINUS_NAN;
	TEST_VFP_EXCEPTION("fsqrtd %P[result], %P[num1]",
		pass, result.d, num1.d, NULL, FPSCR_IOC);
	report("%s[%s]", (pass), testname, "-NaN");

	/*
	 * +Inf, should not thow any exception
	 */
	num1.input = DOUBLE_PLUS_INF;
	TEST_VFP_EXCEPTION("fsqrtd %P[result], %P[num1]",
		pass, result.d, num1.d, NULL, FPSCR_NO_EXCEPTION);
	report("%s[%s]", (result.input == DOUBLE_PLUS_INF && pass),
		testname, "+Inf");

	/*
	 * -Inf, should throw invalid operation
	 */
	num1.input = DOUBLE_MINUS_INF;
	TEST_VFP_EXCEPTION("fsqrtd %P[result], %P[num1]",
		pass, result.d, num1.d, NULL, FPSCR_IOC);
	report("%s[%s]", (pass), testname, "-Inf");
}

static void test_fsqrts()
{
	FLOAT_UNION(num1, 0ULL);
	FLOAT_UNION(result, 0ULL);
	unsigned long pass = 0;

	/*
	 * Test square for 4, expected value is 2
	 */
	num1.f = 4;
	TEST_VFP_EXCEPTION("fsqrts %[result], %[num1]",
		pass, result.f, num1.f, NULL, FPSCR_NO_EXCEPTION);
	report("%s[%s]", (result.f == 2 && pass), testname, "num");

	/*
	 * Test sqrt with inaccurate result
	 * should throw Inexact exception
	 */
	num1.f = 833118.0;
	TEST_VFP_EXCEPTION("fsqrts %[result], %[num1]",
		pass, result.f, num1.f, NULL, FPSCR_IXC);
	report("%s[%s]", (pass), testname, "Inaccurate");

	/*
	 * Test sqrt 0
	 */
	num1.f = 0;
	TEST_VFP_EXCEPTION("fsqrts %[result], %[num1]",
		pass, result.f, num1.f, NULL, FPSCR_NO_EXCEPTION);
	report("%s[%s]", (result.f == 0 && pass), testname, "0");

	/*
	 * Test sqrt -0
	 */
	num1.input = FLOAT_MINUS_NULL;
	TEST_VFP_EXCEPTION("fsqrts %[result], %[num1]",
		pass, result.f, num1.f, NULL, FPSCR_NO_EXCEPTION);
	report("%s[%s]", (result.f == 0 && pass), testname, "-0");

	/*
	 * Test sqrt -1
	 * should throw Invalid operation
	 */
	num1.f = -1;
	TEST_VFP_EXCEPTION("fsqrts %[result], %[num1]",
		pass, result.f, num1.f, NULL, FPSCR_IOC);
	report("%s[%s]", (pass), testname, "-1");

	/*
	 * NaN, should throw Invalid operation exception
	 */
	num1.input = FLOAT_PLUS_NAN;
	TEST_VFP_EXCEPTION("fsqrts %[result], %[num1]",
		pass, result.f, num1.f, NULL, FPSCR_IOC);
	report("%s[%s]", (pass), testname, "+NaN");

	/*
	 * -NaN, should throw Invalid operation exception
	 */
	num1.input = FLOAT_MINUS_NAN;
	TEST_VFP_EXCEPTION("fsqrts %[result], %[num1]",
		pass, result.f, num1.f, NULL, FPSCR_IOC);
	report("%s[%s]", (pass), testname, "-NaN");

	/*
	 * +Inf, should not thow any exception
	 */
	num1.input = FLOAT_PLUS_INF;
	TEST_VFP_EXCEPTION("fsqrts %[result], %[num1]",
		pass, result.f, num1.f, NULL, FPSCR_NO_EXCEPTION);
	report("%s[%s]", (result.input == FLOAT_PLUS_INF && pass),
		testname, "+Inf");

	/*
	 * -Inf, should throw invalid operation
	 */
	num1.input = FLOAT_MINUS_INF;
	TEST_VFP_EXCEPTION("fsqrts %[result], %[num1]",
		pass, result.f, num1.f, NULL, FPSCR_IOC);
	report("%s[%s]", (pass), testname, "-Inf");
}

static void test_fsubd()
{
	DOUBLE_UNION(num1, 0ULL);
	DOUBLE_UNION(num2, 0ULL);
	DOUBLE_UNION(result, 0ULL);
	unsigned long pass = 0;
	
	/*
	 * Test two numbers
	 * Substract one nomber from another.
	 * Expected result is 2.5
	 */
	num1.d = 2.75;
	num2.d = 0.25;
	TEST_VFP_EXCEPTION("fsubd %P[result], %P[num1], %P[num2]",
		pass, result.d, num1.d, num2.d, FPSCR_NO_EXCEPTION);
	report("%s[%s]", (result.d == 2.5 && pass), testname, "num");

	/*
	 * Testing fsubd for maximal precision
	 *  1.11000101110000010100101000011001011010000001010101111
	 * -111.111110101010011001101011101000010110001100110100010
	 * Expecting result is
	 * 110000000001100011010011100101001000011000011111111011000111101
	 */
	num1.d = 1.77248061294820569155916700765374116599559783935546875;
	num2.d = 7.97910187425732519983512247563339769840240478515625;
	TEST_VFP_EXCEPTION("fsubd %P[result], %P[num1], %P[num2]",
		pass, result.d, num1.d, num2.d, FPSCR_NO_EXCEPTION);
	report("%s[%s]", (result.d == -6.20662126130911950827595546797965653240680694580078125
		&& pass), testname, "max precision");

	/*
	 * Test (-inf)-(+inf)
	 * Substracting positive infinity from negative infinity
	 * Expected result is negative inifnity
	 */
	num1.input = DOUBLE_MINUS_INF;
	num2.input = DOUBLE_PLUS_INF;
	TEST_VFP_EXCEPTION("fsubd %P[result], %P[num1], %P[num2]",
		pass, result.d, num1.d, num2.d, FPSCR_NO_EXCEPTION);
	report("%s[%s]", (result.input == DOUBLE_MINUS_INF && pass),
		testname, "(-inf)-(+inf)");

	/*
	 * Test (inf)-(nan)
	 * Substracting NaN from infinity should set IOC bit to 1
	 */
	num1.input = DOUBLE_PLUS_INF;
	num2.input = DOUBLE_PLUS_NAN;
	TEST_VFP_EXCEPTION("fsubd %P[result], %P[num1], %P[num2]",
		pass, result.d, num1.d, num2.d, FPSCR_IOC);
	report("%s[%s]", (pass), testname, "(inf)-(NaN)");
}

static void test_fsubs()
{
	FLOAT_UNION(num1, 0ULL);
	FLOAT_UNION(num2, 0ULL);
	FLOAT_UNION(result, 0ULL);
	unsigned long pass = 0;
	
	/*
	 * Test two numbers
	 * Substract one nomber from another.
	 * Expected result is 2.5
	 */
	num1.f = 2.75;
	num2.f = 0.25;
	TEST_VFP_EXCEPTION("fsubs %[result], %[num1], %[num2]",
		pass, result.f, num1.f, num2.f, FPSCR_NO_EXCEPTION);
	report("%s[%s]", (result.f == 2.5 && pass), testname, "num");

	/*
	 * Testing fsubs for maximal precision
	 */
	num1.f = 30910960529778934627851829249.0;
	num2.f = 1;
	TEST_VFP_EXCEPTION("fsubs %[result], %[num1], %[num2]",
		pass, result.f, num1.f, num2.f, FPSCR_NO_EXCEPTION);
	report("%s[%s]", (result.f == 30910960529778934627851829248.0
		&& pass), testname, "max precision");

	/*
	 * Test (-inf)-(+inf)
	 * Substracting positive infinity from negative infinity
	 * Expected result is negative inifnity
	 */
	num1.input = FLOAT_MINUS_INF;
	num2.input = FLOAT_PLUS_INF;
	TEST_VFP_EXCEPTION("fsubs %[result], %[num1], %[num2]",
		pass, result.f, num1.f, num2.f, FPSCR_NO_EXCEPTION);
	report("%s[%s]", (result.input == FLOAT_MINUS_INF && pass),
		testname, "(-inf)-(+inf)");

	/*
	 * Test (inf)-(nan)
	 * Substracting NaN from infinity should set IOC bit to 1
	 */
	num1.input = FLOAT_PLUS_INF;
	num2.input = FLOAT_PLUS_NAN;
	TEST_VFP_EXCEPTION("fsubs %[result], %[num1], %[num2]",
		pass, result.f, num1.f, num2.f, FPSCR_IOC);
	report("%s[%s]", (pass), testname, "(inf)-(NaN)");
}


static void test_disabled(void)
{
	disable_vfp();
	
	/* Fabsd with disabled VFP */
	handled = 0;
	install_exception_handler(EXCPTN_UND, und_handler);
	test_exception("", "fabsd d0, d0", "");
	report("%s[%s]", handled, testname, "fabsd");
	
	/* Fdivd with disabled VFP */
	handled = 0;
	install_exception_handler(EXCPTN_UND, und_handler);
	test_exception("", "fdivd d0, d1, d2", "");
	report("%s[%s]", handled, testname, "fdivd");

	/* Fcmpd with disabled VFP */
	handled = 0;
	install_exception_handler(EXCPTN_UND, und_handler);
	test_exception("", "fcmpd d0, d1", "");
	report("%s[%s]", handled, testname, "fcmpd");

	/* Fmrx fpexc with disabled VFP */
	handled = 0;
	install_exception_handler(EXCPTN_UND, und_handler);
	test_exception("", "fmrx r0, fpexc", "");
	report("%s[%s]", !handled, testname, "fmrx fpexc");

	/* Fnegs with disabled VFP */
	handled = 0;
	install_exception_handler(EXCPTN_UND, und_handler);
	test_exception("", "fnegs s0, s1", "");
	report("%s[%s]", handled, testname, "fnegs");

	/* Fmrx fpscr with disabled VFP */
	handled = 0;
	install_exception_handler(EXCPTN_UND, und_handler);
	test_exception("", "fmrx r0, fpscr", "");
	report("%s[%s]", handled, testname, "fmrx fpscr");
	
	install_exception_handler(EXCPTN_UND, NULL);
}

int main(int argc, char **argv)
{
	testname_set(NULL,NULL);
	assert_args(argc, 1);
	testname_set(TESTGRP,argv[0]);
	enable_vfp();

	if (strcmp(argv[0], "available") == 0)
		test_available();
	else if (strcmp(argv[0], "cumulative") == 0)
		test_cumulative();
	else if (strcmp(argv[0], "disabled") == 0)
		test_disabled();
	else if (strcmp(argv[0], "fabsd") == 0)
		test_fabsd();
	else if (strcmp(argv[0], "fabss") == 0)
		test_fabss();
	else if (strcmp(argv[0], "faddd") == 0)
		test_faddd();
	else if (strcmp(argv[0], "fadds") == 0)
		test_fadds();
	else if (strcmp(argv[0], "fcmpd") == 0)
		test_fcmpd();
	else if (strcmp(argv[0], "fcmps") == 0)
		test_fcmps();
	else if (strcmp(argv[0], "fcpyd") == 0)
		test_fcpyd();
	else if (strcmp(argv[0], "fcpys") == 0)
		test_fcpys();
	else if (strcmp(argv[0], "fcvtds") == 0)
		test_fcvtds();
	else if (strcmp(argv[0], "fcvtsd") == 0)
		test_fcvtsd();
	else if (strcmp(argv[0], "fdivd") == 0)
		test_fdivd();
	else if (strcmp(argv[0], "fdivs") == 0)
		test_fdivs();
	else if (strcmp(argv[0], "fmacd") == 0)
		test_fmacd();
	else if (strcmp(argv[0], "fmacs") == 0)
		test_fmacs();
	else if (strcmp(argv[0], "fmdhr") == 0)
		test_fmdhr();
	else if (strcmp(argv[0], "fmdlr") == 0)
		test_fmdlr();
	else if (strcmp(argv[0], "fmrdh") == 0)
		test_fmrdh();
	else if (strcmp(argv[0], "fmrdl") == 0)
		test_fmrdl();
	else if (strcmp(argv[0], "fmdrr") == 0)
		test_fmdrr();
	else if (strcmp(argv[0], "fmrrd") == 0)
		test_fmrrd();
	else if (strcmp(argv[0], "fmrrs") == 0)
		test_fmrrs();
	else if (strcmp(argv[0], "fmrs") == 0)
		test_fmrs();
	else if (strcmp(argv[0], "fmscd") == 0)
		test_fmscd();
	else if (strcmp(argv[0], "fmscs") == 0)
		test_fmscs();
	else if (strcmp(argv[0], "fmsr") == 0)
		test_fmsr();
	else if (strcmp(argv[0], "fmsrr") == 0)
		test_fmsrr();
	else if (strcmp(argv[0], "fnegd") == 0)
		test_fnegd();
	else if (strcmp(argv[0], "fnmacd") == 0)
		test_fnmacd();
	else if (strcmp(argv[0], "fnmacs") == 0)
		test_fnmacs();
	else if (strcmp(argv[0], "fnmscd") == 0)
		test_fnmscd();
	else if (strcmp(argv[0], "fnmscs") == 0)
		test_fnmscs();
	else if (strcmp(argv[0], "fnegs") == 0)
		test_fnegs();
	else if (strcmp(argv[0], "fsqrtd") == 0)
		test_fsqrtd();
	else if (strcmp(argv[0], "fsqrts") == 0)
		test_fsqrts();
	else if (strcmp(argv[0], "fsubd") == 0)
		test_fsubd();
	else if (strcmp(argv[0], "fsubs") == 0)
		test_fsubs();
	else 
		printf("Error: Unkown test\n");

	return report_summary();
}
