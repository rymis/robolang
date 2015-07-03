#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define ROBOT_MATRIX_NO_PREFIX
#include "robot_linalg.h"

static int x_outs(void *ctx, const char *s)
{
	fprintf(ctx? ctx: stderr, "%s", s);
	return 0;
}

static int x_outf(void *ctx, double f)
{
	fprintf(ctx? ctx: stderr, "%f", f);
	return 0;
}

static RobotPrintFunctions x_print_functions = { x_outs, x_outf };

static RobotMatrix2 E2 = {
	{ 1.0, 0.0 },
	{ 0.0, 1.0 }
};

static RobotMatrix3 E3 = {
	{ 1.0, 0.0, 0.0 },
	{ 0.0, 1.0, 0.0 },
	{ 0.0, 0.0, 1.0 }
};

static RobotMatrix4 E4 = {
	{ 1.0, 0.0, 0.0, 0.0 },
	{ 0.0, 1.0, 0.0, 0.0 },
	{ 0.0, 0.0, 1.0, 0.0 },
	{ 0.0, 0.0, 0.0, 1.0 }
};

static void matrix_pow2(RobotMatrix2 *res, const RobotMatrix2 *m, unsigned p)
{
	RobotMatrix2 r, r2;

	if (p == 0) {
		memcpy(res, &E2, sizeof(E2));
		return;
	} else if (p == 1) {
		memcpy(res, m, sizeof(RobotMatrix2));
		return;
	}

	if (p & 1) { /* r = (r ^ (p/2))^2 * r */
		matrix_pow2(&r, m, p / 2);
		matrix_mul2(&r2, &r, &r);
		matrix_mul2(res, &r2, m);
	} else {
		matrix_pow2(&r, m, p / 2);
		matrix_mul2(res, &r, &r);
	}
#if 0
	printf("[[[[%u]]]]\n", p);
	matrix_print2(&x_print_functions, stdout, res);
	matrix_print2(&x_print_functions, stdout, m);
#endif

	return;
}

static void test_print(void)
{
	matrix_print2(&x_print_functions, stdout, &E2); printf("\n");
	matrix_print3(&x_print_functions, stdout, &E3); printf("\n");
	matrix_print4(&x_print_functions, stdout, &E4); printf("\n");
}

static long fibo(int n)
{
	long a = 1;
	long b = 1;

	int i;

	for (i = 2; i < n; i++) {
		b = a + b;
		a = b - a;
	}

	return b;
}

static void test_fibo(void)
{
	int x = rand() % 50 + 20;
	long f = fibo(x);
	RobotMatrix2 F = {
		{ 1.0, 1.0 },
		{ 1.0, 0.0 }
	};
	RobotVector2 v = { 1.0, 1.0 };
	RobotVector2 r;

	RobotMatrix2 F__x;

	matrix_pow2(&F__x, &F, x - 2);
	robot_matrix_mulv2(&r, &F__x, &v);

	printf("Fibonachi(%d) = %ld\n", x, f);
	printf("Fibonachi(%d) = %f (%ld)\n", x, r.x, (long)r.x);

	matrix_print2(&x_print_functions, stdout, &F__x); printf("\n");
	vector_print2(&x_print_functions, stdout, &r); printf("\n");
}

static void test_matrix2(void)
{
	RobotMatrix2 A = {
		{ 1.0, 2.0 },
		{ 3.0, 4.0 }
	};
	RobotMatrix2 B = {
		{ 4.0, 3.0 },
		{ 2.0, 1.0 }
	};
	RobotMatrix2 R; /* { { 8, 5 }, { 20, 13 } } */

	matrix_mul2(&R, &A, &B);
	matrix_print2(&x_print_functions, stdout, &R);
}


int main(int argc, char *argv[])
{
	srand(time(NULL));
	test_print();
	test_matrix2();
	test_fibo();


	return 0;
}

