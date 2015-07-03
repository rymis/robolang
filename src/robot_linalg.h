#ifndef ROBOT_LINALG_H
#define ROBOT_LINALG_H

/* This library depends only on C compiler */

#ifdef __cplusplus
extern "C" {
#endif /* } */

#ifdef ROBOT_MATRIX_NO_INLINE
# define robot_inline static
#else
# define robot_inline static inline
#endif

#ifdef ROBOT_MATRIX_USE_FLOAT
typedef float RobotNumber;
#else
typedef double RobotNumber;
#endif

struct robot_print_functions {
	int (*outs)(void *ctx, const char *s);
	int (*outf)(void *ctx, RobotNumber n);
};
typedef struct robot_print_functions RobotPrintFunctions;

struct robot_vector2 {
	RobotNumber x, y;
};
typedef struct robot_vector2 RobotVector2;

struct robot_vector3 {
	RobotNumber x, y, z;
};
typedef struct robot_vector3 RobotVector3;

struct robot_vector4 {
	RobotNumber x, y, z, a;
};
typedef struct robot_vector4 RobotVector4;

struct robot_matrix2 {
	RobotVector2 r1, r2;
};
typedef struct robot_matrix2 RobotMatrix2;

struct robot_matrix3 {
	RobotVector3 r1, r2, r3;
};
typedef struct robot_matrix3 RobotMatrix3;

struct robot_matrix4 {
	RobotVector4 r1, r2, r3, r4;
};
typedef struct robot_matrix4 RobotMatrix4;


robot_inline void robot_vector_add2(RobotVector2 *res, const RobotVector2 *a, const RobotVector2 *b)
{
	res->x = a->x + b->x;
	res->y = a->y + b->y;
}

robot_inline void robot_vector_sub2(RobotVector2 *res, const RobotVector2 *a, const RobotVector2 *b)
{
	res->x = a->x - b->x;
	res->y = a->y - b->y;
}

robot_inline RobotNumber robot_vector_smul2(const RobotVector2 *a, const RobotVector2 *b)
{
	return a->x * b->x + a->y * b->y;
}

robot_inline void robot_vector_add3(RobotVector3 *res, const RobotVector3 *a, const RobotVector3 *b)
{
	res->x = a->x + b->x;
	res->y = a->y + b->y;
	res->z = a->z + b->z;
}

robot_inline void robot_vector_sub3(RobotVector3 *res, const RobotVector3 *a, const RobotVector3 *b)
{
	res->x = a->x - b->x;
	res->y = a->y - b->y;
	res->z = a->z - b->z;
}

robot_inline RobotNumber robot_vector_smul3(const RobotVector3 *a, const RobotVector3 *b)
{
	return a->x * b->x + a->y * b->y + a->z * b->z;
}

robot_inline void robot_vector_add4(RobotVector4 *res, const RobotVector4 *a, const RobotVector4 *b)
{
	res->x = a->x + b->x;
	res->y = a->y + b->y;
	res->z = a->z + b->z;
	res->a = a->a + b->a;
}

robot_inline void robot_vector_sub4(RobotVector4 *res, const RobotVector4 *a, const RobotVector4 *b)
{
	res->x = a->x - b->x;
	res->y = a->y - b->y;
	res->z = a->z - b->z;
	res->a = a->a - b->a;
}

robot_inline RobotNumber robot_vector_smul4(const RobotVector4 *a, const RobotVector4 *b)
{
	return a->x * b->x + a->y * b->y + a->z * b->z + a->a * b->a;
}

robot_inline void robot_matrix_add2(RobotMatrix2 *res, const RobotMatrix2 *m1, const RobotMatrix2 *m2)
{
	robot_vector_add2(&res->r1, &m1->r1, &m2->r1);
	robot_vector_add2(&res->r2, &m1->r2, &m2->r2);
}

robot_inline void robot_matrix_sub2(RobotMatrix2 *res, const RobotMatrix2 *m1, const RobotMatrix2 *m2)
{
	robot_vector_sub2(&res->r1, &m1->r1, &m2->r1);
	robot_vector_sub2(&res->r2, &m1->r2, &m2->r2);
}

robot_inline void robot_matrix_transpose2(RobotMatrix2 *res, const RobotMatrix2 *m)
{
	res->r1.x = m->r1.x;
	res->r1.y = m->r2.x;
	res->r2.x = m->r1.y;
	res->r2.y = m->r2.y;
}

robot_inline void robot_matrix_transpose3(RobotMatrix3 *res, const RobotMatrix3 *m)
{
	res->r1.x = m->r1.x;
	res->r1.y = m->r2.x;
	res->r1.z = m->r3.x;

	res->r2.x = m->r1.y;
	res->r2.y = m->r2.y;
	res->r2.z = m->r3.y;

	res->r3.x = m->r1.z;
	res->r3.y = m->r2.z;
	res->r3.z = m->r3.z;
}

robot_inline void robot_matrix_transpose4(RobotMatrix4 *res, const RobotMatrix4 *m)
{
	res->r1.x = m->r1.x;
	res->r1.y = m->r2.x;
	res->r1.z = m->r3.x;
	res->r1.a = m->r4.x;

	res->r2.x = m->r1.y;
	res->r2.y = m->r2.y;
	res->r2.z = m->r3.y;
	res->r2.a = m->r4.y;

	res->r3.x = m->r1.z;
	res->r3.y = m->r2.z;
	res->r3.z = m->r3.z;
	res->r3.a = m->r4.z;

	res->r4.x = m->r1.a;
	res->r4.y = m->r2.a;
	res->r4.z = m->r3.a;
	res->r4.a = m->r4.a;
}

robot_inline void robot_matrix_mul2(RobotMatrix2 *res, const RobotMatrix2 *m1, const RobotMatrix2 *m2)
{
	res->r1.x = m1->r1.x * m2->r1.x + m1->r1.y * m2->r2.x;
	res->r1.y = m1->r1.x * m2->r1.y + m1->r1.y * m2->r2.y;
	res->r2.x = m1->r2.x * m2->r1.x + m1->r2.y * m2->r2.x;
	res->r2.y = m1->r2.x * m2->r1.y + m1->r2.y * m2->r2.y;
}

robot_inline void robot_matrix_mul3(RobotMatrix3 *res, const RobotMatrix3 *m1, const RobotMatrix3 *m2)
{
	res->r1.x = m1->r1.x * m2->r1.x + m1->r1.y * m2->r2.x + m1->r1.z * m2->r3.x;
	res->r1.y = m1->r1.x * m2->r1.y + m1->r1.y * m2->r2.y + m1->r1.z * m2->r3.y;
	res->r1.z = m1->r1.x * m2->r1.z + m1->r1.y * m2->r2.z + m1->r1.z * m2->r3.z;

	res->r2.x = m1->r2.x * m2->r1.x + m1->r2.y * m2->r2.x + m1->r2.z * m2->r3.x;
	res->r2.y = m1->r2.x * m2->r1.y + m1->r2.y * m2->r2.y + m1->r2.z * m2->r3.y;
	res->r2.z = m1->r2.x * m2->r1.z + m1->r2.y * m2->r2.z + m1->r2.z * m2->r3.z;

	res->r3.x = m1->r3.x * m2->r1.x + m1->r3.y * m2->r2.x + m1->r3.z * m2->r3.x;
	res->r3.y = m1->r3.x * m2->r1.y + m1->r3.y * m2->r2.y + m1->r3.z * m2->r3.y;
	res->r3.z = m1->r3.x * m2->r1.z + m1->r3.y * m2->r2.z + m1->r3.z * m2->r3.z;
}

robot_inline void robot_matrix_mul4(RobotMatrix4 *res, const RobotMatrix4 *m1, const RobotMatrix4 *m2)
{
	res->r1.x = m1->r1.x * m2->r1.x + m1->r1.y * m2->r2.x + m1->r1.z * m2->r3.x + m1->r1.a * m2->r4.x;
	res->r1.y = m1->r1.x * m2->r1.y + m1->r1.y * m2->r2.y + m1->r1.z * m2->r3.y + m1->r1.a * m2->r4.y;
	res->r1.z = m1->r1.x * m2->r1.z + m1->r1.y * m2->r2.z + m1->r1.z * m2->r3.z + m1->r1.a * m2->r4.z;
	res->r1.a = m1->r1.x * m2->r1.a + m1->r1.y * m2->r2.a + m1->r1.z * m2->r3.a + m1->r1.a * m2->r4.a;

	res->r2.x = m1->r2.x * m2->r1.x + m1->r2.y * m2->r2.x + m1->r2.z * m2->r3.x + m1->r2.a * m2->r4.x;
	res->r2.y = m1->r2.x * m2->r1.y + m1->r2.y * m2->r2.y + m1->r2.z * m2->r3.y + m1->r2.a * m2->r4.y;
	res->r2.z = m1->r2.x * m2->r1.z + m1->r2.y * m2->r2.z + m1->r2.z * m2->r3.z + m1->r2.a * m2->r4.z;
	res->r2.a = m1->r2.x * m2->r1.a + m1->r2.y * m2->r2.a + m1->r2.z * m2->r3.a + m1->r2.a * m2->r4.a;

	res->r3.x = m1->r3.x * m2->r1.x + m1->r3.y * m2->r2.x + m1->r3.z * m2->r3.x + m1->r3.a * m2->r4.x;
	res->r3.y = m1->r3.x * m2->r1.y + m1->r3.y * m2->r2.y + m1->r3.z * m2->r3.y + m1->r3.a * m2->r4.y;
	res->r3.z = m1->r3.x * m2->r1.z + m1->r3.y * m2->r2.z + m1->r3.z * m2->r3.z + m1->r3.a * m2->r4.z;
	res->r3.a = m1->r3.x * m2->r1.a + m1->r3.y * m2->r2.a + m1->r3.z * m2->r3.a + m1->r3.a * m2->r4.a;

	res->r4.x = m1->r4.x * m2->r1.x + m1->r4.y * m2->r2.x + m1->r4.z * m2->r3.x + m1->r4.a * m2->r4.x;
	res->r4.y = m1->r4.x * m2->r1.y + m1->r4.y * m2->r2.y + m1->r4.z * m2->r3.y + m1->r4.a * m2->r4.y;
	res->r4.z = m1->r4.x * m2->r1.z + m1->r4.y * m2->r2.z + m1->r4.z * m2->r3.z + m1->r4.a * m2->r4.z;
	res->r4.a = m1->r4.x * m2->r1.a + m1->r4.y * m2->r2.a + m1->r4.z * m2->r3.a + m1->r4.a * m2->r4.a;
}

robot_inline void robot_matrix_mulv2(RobotVector2 *res, const RobotMatrix2 *m, const RobotVector2 *v)
{
	res->x = robot_vector_smul2(&m->r1, v);
	res->y = robot_vector_smul2(&m->r2, v);
}

robot_inline void robot_matrix_mulv3(RobotVector3 *res, const RobotMatrix3 *m, const RobotVector3 *v)
{
	res->x = robot_vector_smul3(&m->r1, v);
	res->y = robot_vector_smul3(&m->r2, v);
	res->z = robot_vector_smul3(&m->r3, v);
}

robot_inline void robot_matrix_mulv4(RobotVector4 *res, const RobotMatrix4 *m, const RobotVector4 *v)
{
	res->x = robot_vector_smul4(&m->r1, v);
	res->y = robot_vector_smul4(&m->r2, v);
	res->z = robot_vector_smul4(&m->r3, v);
	res->a = robot_vector_smul4(&m->r4, v);
}

robot_inline int robot_vector_print2(RobotPrintFunctions *f, void *ctx, const RobotVector2 *v)
{
	if (
			f->outs(ctx, "{ ") < 0 ||
			f->outf(ctx, v->x) < 0 ||
			f->outs(ctx, ", ") < 0 ||
			f->outf(ctx, v->y) < 0 ||
			f->outs(ctx, " }") < 0
	   )
		return -1;
	return 0;
}

robot_inline int robot_vector_print3(RobotPrintFunctions *f, void *ctx, const RobotVector3 *v)
{
	if (
			f->outs(ctx, "{ ") < 0 ||
			f->outf(ctx, v->x) < 0 ||
			f->outs(ctx, ", ") < 0 ||
			f->outf(ctx, v->y) < 0 ||
			f->outs(ctx, ", ") < 0 ||
			f->outf(ctx, v->z) < 0 ||
			f->outs(ctx, " }") < 0
	   )
		return -1;
	return 0;
}

robot_inline int robot_vector_print4(RobotPrintFunctions *f, void *ctx, const RobotVector4 *v)
{
	if (
			f->outs(ctx, "{ ") < 0 ||
			f->outf(ctx, v->x) < 0 ||
			f->outs(ctx, ", ") < 0 ||
			f->outf(ctx, v->y) < 0 ||
			f->outs(ctx, ", ") < 0 ||
			f->outf(ctx, v->z) < 0 ||
			f->outs(ctx, ", ") < 0 ||
			f->outf(ctx, v->a) < 0 ||
			f->outs(ctx, " }") < 0
	   )
		return -1;
	return 0;
}

robot_inline int robot_matrix_print2(RobotPrintFunctions *f, void *ctx, const RobotMatrix2 *m)
{
	if (
			f->outs(ctx, "[\n\t") < 0 ||
			robot_vector_print2(f, ctx, &m->r1) < 0 ||
			f->outs(ctx, "\n\t") < 0 ||
			robot_vector_print2(f, ctx, &m->r2) < 0 ||
			f->outs(ctx, "\n]") < 0
	   )
		return -1;
	return 0;
}

robot_inline int robot_matrix_print3(RobotPrintFunctions *f, void *ctx, const RobotMatrix3 *m)
{
	if (
			f->outs(ctx, "[\n\t") < 0 ||
			robot_vector_print3(f, ctx, &m->r1) < 0 ||
			f->outs(ctx, "\n\t") < 0 ||
			robot_vector_print3(f, ctx, &m->r2) < 0 ||
			f->outs(ctx, "\n\t") < 0 ||
			robot_vector_print3(f, ctx, &m->r3) < 0 ||
			f->outs(ctx, "\n]") < 0
	   )
		return -1;
	return 0;
}

robot_inline int robot_matrix_print4(RobotPrintFunctions *f, void *ctx, const RobotMatrix4 *m)
{
	if (
			f->outs(ctx, "[\n\t") < 0 ||
			robot_vector_print4(f, ctx, &m->r1) < 0 ||
			f->outs(ctx, "\n\t") < 0 ||
			robot_vector_print4(f, ctx, &m->r2) < 0 ||
			f->outs(ctx, "\n\t") < 0 ||
			robot_vector_print4(f, ctx, &m->r3) < 0 ||
			f->outs(ctx, "\n\t") < 0 ||
			robot_vector_print4(f, ctx, &m->r4) < 0 ||
			f->outs(ctx, "\n]") < 0
	   )
		return -1;
	return 0;
}

#ifdef ROBOT_MATRIX_NO_PREFIX

#define vector_add2 robot_vector_add2
#define vector_add3 robot_vector_add3
#define vector_add4 robot_vector_add4

#define vector_sub2 robot_vector_sub2
#define vector_sub3 robot_vector_sub3
#define vector_sub4 robot_vector_sub4

#define vector_smul2 robot_vector_smul2
#define vector_smul3 robot_vector_smul3
#define vector_smul4 robot_vector_smul4

#define matrix_add2 robot_matrix_add2
#define matrix_add3 robot_matrix_add3
#define matrix_add4 robot_matrix_add4

#define matrix_sub2 robot_matrix_sub2
#define matrix_sub3 robot_matrix_sub3
#define matrix_sub4 robot_matrix_sub4

#define matrix_mul2 robot_matrix_mul2
#define matrix_mul3 robot_matrix_mul3
#define matrix_mul4 robot_matrix_mul4

#define matrix_mulv2 robot_matrix_mulv2
#define matrix_mulv3 robot_matrix_mulv3
#define matrix_mulv4 robot_matrix_mulv4

#define matrix_print2 robot_matrix_print2
#define matrix_print3 robot_matrix_print3
#define matrix_print4 robot_matrix_print4

#define vector_print2 robot_vector_print2
#define vector_print3 robot_vector_print3
#define vector_print4 robot_vector_print4

#endif

/* extern "C" { */
#ifdef __cplusplus
}
#endif

#endif /* ROBOT_LINALG */

