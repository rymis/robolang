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

		case ROBOT_VM_CALL:   /* Call function by number in symtable       */
			if (self->priv->symtable->len <= self->A) {
				g_set_error(error, ROBOT_ERROR, ROBOT_ERROR_INVALID_ADDRESS,
						"Invalid function reference: %u\n", (unsigned)self->A);
				return FALSE;
			}
			sym = &g_array_index(self->priv->symtable, Symbol, self->A);
			if (sym->func) {
				++self->PC;
				return sym->func(self, sym->userdata, error);
			} else {
				++self->PC;
				if (!robot_vm_stack_push(&self->rstack, &self->PC, sizeof(RobotVMWord), error)) {
					return FALSE;
				}
				self->PC = sym->address;
			}
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

