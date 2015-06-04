#include "robot_vm.h"
#include <string.h>

GQuark robot_error_get(void)
{
	static GQuark e = 0;

	if (!e) {
		e = g_quark_from_string("ROBOT");
	}

	return e;
}

typedef struct _Symbol {
	gchar name[256];
	RobotVMWord address;
	RobotVMFunc func;
	gpointer userdata;
	GDestroyNotify free_userdata;
} Symbol;

static void symbol_clear(gpointer p)
{
	Symbol *s = p;
	if (s->free_userdata)
		s->free_userdata(s->userdata);
}

typedef struct _Chunk {
	unsigned char *data;
	size_t len;
} Chunk;

struct _RobotVMPrivate {
	GArray *symtable;
};

G_DEFINE_TYPE_WITH_PRIVATE(RobotVM, robot_vm, G_TYPE_OBJECT)

static void dispose(GObject *obj)
{
	RobotVM *self = ROBOT_VM(obj);

	g_array_unref(self->priv->symtable);
	self->priv->symtable = NULL;

	g_byte_array_unref(self->stack.stack);
	self->stack.stack = NULL;

	g_byte_array_unref(self->rstack.stack);
	self->rstack.stack = NULL;
}

static void finalize(GObject *obj)
{
	RobotVM *self = ROBOT_VM(obj);

	self->priv = NULL;
}

static void robot_vm_class_init(RobotVMClass *klass)
{
	GObjectClass *objcls = G_OBJECT_CLASS(klass);
	objcls->dispose = dispose;
	objcls->finalize = finalize;
}

static void robot_vm_init(RobotVM *self)
{
	self->priv = robot_vm_get_instance_private(self);
	self->priv->symtable = g_array_new(FALSE, TRUE, sizeof(Symbol));

	g_array_set_clear_func(self->priv->symtable, symbol_clear);

	self->PC = 0;
	self->stack.stack = g_byte_array_new();
	self->stack.top = 0;
	self->rstack.stack = g_byte_array_new();
	self->rstack.top = 0;
	self->memory = g_byte_array_new();
}

RobotVM* robot_vm_new(void)
{
	RobotVM *self = robot_vm_new_empty();

	robot_vm_add_standard_functions(self);

	return self;
}

RobotVM* robot_vm_new_empty(void)
{
	RobotVM* self = g_object_new(ROBOT_TYPE_VM, NULL);

	return self;
}

/* Callback manipulation: */
guint robot_vm_add_function(RobotVM *self, const char *name, RobotVMFunc func, gpointer userdata, GDestroyNotify free_userdata)
{
	guint i;
	Symbol *sym;

	for (i = 0; i < self->priv->symtable->len; i++) {
		sym = &g_array_index(self->priv->symtable, Symbol, i);

		if (!strcmp(sym->name, name)) { /* Replace function */
			if (sym->free_userdata)
				sym->free_userdata(sym->userdata);
			sym->func = func;
			sym->userdata = userdata;
			sym->free_userdata = free_userdata;
			sym->address = 0;

			return i;
		}
	}

	self->priv->symtable = g_array_set_size(self->priv->symtable, self->priv->symtable->len + 1);
	sym = &g_array_index(self->priv->symtable, Symbol, self->priv->symtable->len - 1);

	sym->func = func;
	sym->userdata = userdata;
	sym->free_userdata = free_userdata;
	sym->address = 0;

	return self->priv->symtable->len - 1;
}

gboolean robot_vm_has_function(RobotVM *self, const char *name)
{
	return robot_vm_get_function(self, name) >= 0;
}

gint robot_vm_get_function(RobotVM *self, const char *name)
{
	guint i;
	Symbol *sym;

	for (i = 0; i < self->priv->symtable->len; i++) {
		sym = &g_array_index(self->priv->symtable, Symbol, i);

		if (!strcmp(sym->name, name)) {
			return i;
		}
	}

	return -1;
}

/* Execute one instruction: */
static inline gboolean exec(RobotVM *self, GError **error)
{
	RobotVMWord a;
	Symbol *sym;

	if (self->PC >= self->memory->len) {
		g_set_error(error, ROBOT_ERROR, ROBOT_ERROR_EXECUTION_FAULT,
				"Invalid value of PC (%x)", (unsigned)self->PC);
		return FALSE;
	}

	switch (self->memory->data[self->PC]) {
		case ROBOT_VM_NOP:    /* No operation                              */
			++self->PC;
			break;

		case ROBOT_VM_JUMP:   /* Jump to address                           */
			self->PC = self->A;
			break;

		case ROBOT_VM_JIF:    /* Jump if here are non zero on top of stack */
			if (!robot_vm_stack_pop(&self->stack, &a, sizeof(RobotVMWord), error)) {
				return FALSE;
			}
			if (a)
				self->PC = self->A;
			else
				++self->PC;
			break;

		case ROBOT_VM_SYS:   /* Call function by number in symtable       */
			if (self->priv->symtable->len <= self->A) {
				g_set_error(error, ROBOT_ERROR, ROBOT_ERROR_INVALID_ADDRESS,
						"Invalid function reference: %u\n", (unsigned)self->A);
				return FALSE;
			}
			sym = &g_array_index(self->priv->symtable, Symbol, self->A);
			if (!sym->func) {
				g_set_error(error, ROBOT_ERROR, ROBOT_ERROR_EXECUTION_FAULT, "Invalid function");
				return FALSE;
			}

			++self->PC;
			return sym->func(self, sym->userdata, error);

		case ROBOT_VM_CALL:   /* Call function by address       */
			++self->PC;
			if (!robot_vm_stack_push(&self->rstack, &self->PC, sizeof(RobotVMWord), error)) {
				return FALSE;
			}
			self->PC = self->A;
			break;

		case ROBOT_VM_RET:    /* Return from function                      */
			if (!robot_vm_stack_pop(&self->rstack, &self->PC, sizeof(RobotVMWord), error)) {
				return FALSE;
			}
			break;

		case ROBOT_VM_PUSH:   /* Push value from A to stack                */
			if (!robot_vm_stack_push(&self->stack, &self->A, sizeof(RobotVMWord), error)) {
				return FALSE;
			}
			++self->PC;
			break;

		case ROBOT_VM_NTH:    /* Get value from stack to A                 */
			if (!robot_vm_stack_nth(&self->stack, self->A, &self->A, sizeof(RobotVMWord), error)) {
				return FALSE;
			}
			++self->PC;
			break;

		case ROBOT_VM_POP:    /* Pop value from stack to A                 */
			if (!robot_vm_stack_pop(&self->stack, &self->A, sizeof(RobotVMWord), error)) {
				return FALSE;
			}
			++self->PC;
			break;

		case ROBOT_VM_CONST:    /* Load constant to A from memory */
			++self->PC;
			if (self->PC + sizeof(RobotVMWord) >= self->memory->len) {
				g_set_error(error, ROBOT_ERROR, ROBOT_ERROR_INVALID_ADDRESS,
						"Out of memory: %x", (unsigned)self->PC);
				return FALSE;
			}

			self->A =
				(self->memory->data[self->PC] << 24) |
				(self->memory->data[self->PC + 1] << 16) |
				(self->memory->data[self->PC + 2] << 8) |
				(self->memory->data[self->PC + 3]);
			self->PC += sizeof(RobotVMWord);
			break;

		default:
			g_set_error(error, ROBOT_ERROR, ROBOT_ERROR_INVALID_INSTRUCTION,
					"Invalid instruction %02x at %x", self->memory->data[self->PC], (unsigned)self->PC);
			return FALSE;
	}
	return TRUE;
}

gboolean robot_vm_stack_push(RobotVMStack *stack, gconstpointer data, gsize len, GError **error)
{
	if (stack->top + len >= stack->stack->len)
		g_byte_array_set_size(stack->stack, stack->top + len);
	memcpy(stack->stack->data + stack->top, data, len);
	stack->top += len;

	return TRUE;
}

gboolean robot_vm_stack_pop(RobotVMStack *stack, gpointer data, gsize len, GError **error)
{
	if (stack->top < len) {
		g_set_error(error, ROBOT_ERROR, ROBOT_ERROR_STACK,
				"Trying to pop %u bytes when here are only %u bytes on stack", (unsigned)len, (unsigned)stack->top);
		return FALSE;
	}

	stack->top -= len;
	memcpy(data, stack->stack->data + stack->top, len);

	return TRUE;
}

gboolean robot_vm_stack_nth(RobotVMStack *stack, guint idx, gpointer data, gsize len, GError **error)
{
	if (stack->top < len + idx) {
		g_set_error(error, ROBOT_ERROR, ROBOT_ERROR_STACK,
				"Trying to pop %u bytes when here are only %u bytes on stack", (unsigned)len, (unsigned)stack->top);
		return FALSE;
	}

	memcpy(data, stack->stack->data + (stack->top - len - idx), len);

	return TRUE;
}

#define UNFUNC(nm, CODE) \
	static gboolean rvm_##nm(RobotVM *self, gpointer userdata, GError **error) \
	{ \
		RobotVMWord a, r; \
		if (!robot_vm_stack_pop(&self->stack, &a, sizeof(RobotVMWord), error)) { \
			return FALSE; /* Error was set in pop */ \
		} \
		CODE; \
		if (!robot_vm_stack_push(&self->stack, &r, sizeof(RobotVMWord), error)) { \
			return FALSE; \
		} \
		return TRUE; \
	}

#define BINFUNC(nm, CODE) \
	static gboolean rvm_##nm(RobotVM *self, gpointer userdata, GError **error) \
	{ \
		RobotVMWord a, b, r; \
		if (!robot_vm_stack_pop(&self->stack, &a, sizeof(RobotVMWord), error)) { \
			return FALSE; /* Error was set in pop */ \
		} \
		if (!robot_vm_stack_pop(&self->stack, &b, sizeof(RobotVMWord), error)) { \
			return FALSE; /* Error was set in pop */ \
		} \
		CODE; \
		if (!robot_vm_stack_push(&self->stack, &r, sizeof(RobotVMWord), error)) { \
			return FALSE; \
		} \
		return TRUE; \
	}

#define ERR(e, ...) \
	do { \
		g_set_error(error, ROBOT_ERROR, e, __VA_ARGS__); \
		return FALSE; \
	} while (0)

/* Arithmetic: */
BINFUNC(add, r = a + b;)
BINFUNC(sub, r = a - b;)
BINFUNC(mul, r = a * b;)
BINFUNC(div, if (b == 0) ERR(ROBOT_ERROR_DIVISION_BY_ZERO, "Division by zero"); r = a / b;)
BINFUNC(mod, if (b == 0) ERR(ROBOT_ERROR_DIVISION_BY_ZERO, "Division by zero"); r = a % b;)

/* Logical: */
UNFUNC(not, r = !a;)
BINFUNC(and, r = a && b;)
BINFUNC(or, r = a || b;)

/* Compare: */
BINFUNC(eq, r = (a == b);)
BINFUNC(less, r = (a < b);)
BINFUNC(leq, r = (a <= b);)

static struct std_func {
	const char *name;
	RobotVMFunc func;
	gpointer userdata;
} std_funcs[] = {
#define F(nm) \
	{ "@" #nm, rvm_##nm, NULL }

	F(add), F(sub), F(div), F(mul), F(mod),

	F(not), F(and), F(or),

	F(eq), F(less), F(leq),

#undef F
	{ NULL, NULL, NULL }
};

void robot_vm_add_standard_functions(RobotVM *self)
{
	guint i;

	for (i = 0; std_funcs[i].name; i++) {
		robot_vm_add_function(self, std_funcs[i].name, std_funcs[i].func, std_funcs[i].userdata, NULL);
	}
}

