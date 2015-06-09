#include "robot.h"
#include <string.h>
#include <stdio.h>

static inline RobotVMWord get_word(void *p)
{
	return g_ntohl(((guint32*)p)[0]);
}

static inline void set_word(void *p, RobotVMWord w)
{
	((guint32*)p)[0] = w;
}

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

	gboolean stop;
};

G_DEFINE_TYPE_WITH_PRIVATE(RobotVM, robot_vm, G_TYPE_OBJECT)

static void dispose(GObject *obj)
{
	RobotVM *self = ROBOT_VM(obj);

	g_array_unref(self->priv->symtable);
	self->priv->symtable = NULL;
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
	self->priv->stop = FALSE;

	g_array_set_clear_func(self->priv->symtable, symbol_clear);

	self->PC = 0;
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
	strncpy(sym->name, name, sizeof(sym->name));
	sym->name[sizeof(sym->name) - 1] = 0;

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

#define GET(r, addr) \
	do { \
		if ((addr) + 4 >= self->memory->len) { \
			g_set_error(error, ROBOT_ERROR, ROBOT_ERROR_EXECUTION_FAULT, "out of memory"); \
			return FALSE; \
		} \
		r = g_ntohl(*((RobotVMWord*)(self->memory->data + (addr)))); \
	} while (0)

#define PUT(addr, v) \
	do { \
		if ((addr) + 4 >= self->memory->len) { \
			g_set_error(error, ROBOT_ERROR, ROBOT_ERROR_EXECUTION_FAULT, "out of memory"); \
			return FALSE; \
		} \
		*((RobotVMWord*)(self->memory->data + (addr))) = g_htonl(v); \
	} while (0)

#define POP(r) \
	do { \
		if (self->T >= self->stack_end) { \
			g_set_error(error, ROBOT_ERROR, ROBOT_ERROR_STACK, "Stack underflow"); \
			return FALSE; \
		} \
		r = g_ntohl(*((RobotVMWord*)(self->memory->data + self->T))); \
		self->T += sizeof(RobotVMWord); \
	} while (0)

#define PUSH(v) \
	do { \
		if (self->T < sizeof(RobotVMWord)) { \
			g_set_error(error, ROBOT_ERROR, ROBOT_ERROR_STACK, "Stack overflow"); \
			return FALSE; \
		} \
		self->T -= sizeof(RobotVMWord); \
		*((RobotVMWord*)(self->memory->data + self->T)) = g_htonl(v); \
	} while (0)

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
			if (self->B)
				self->PC = self->A;
			else
				++self->PC;
			break;

		case ROBOT_VM_JIFNOT:    /* Jump if here are non zero on top of stack */
			if (!self->B)
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

			PUSH(self->PC);
			self->PC = self->A;

			break;

		case ROBOT_VM_RET:    /* Return from function                      */
			POP(self->PC);
			break;

		case ROBOT_VM_PUSH:   /* Push value from A to stack                */
			GET(a, self->A);
			PUSH(a);
			++self->PC;
			break;

		case ROBOT_VM_POP:    /* Pop value from stack to A                 */
			POP(a);
			PUT(self->A, a);
			++self->PC;
			break;

		case ROBOT_VM_PUSHA:  /* Push value from A to stack                */
			PUSH(self->A);
			++self->PC;
			break;

		case ROBOT_VM_POPA:   /* Pop value from stack to A                 */
			POP(self->A);
			++self->PC;
			break;

		case ROBOT_VM_PUSHB:  /* Push value from B to stack                */
			PUSH(self->B);
			++self->PC;
			break;

		case ROBOT_VM_POPB:   /* Pop value from stack to B                 */
			POP(self->B);
			++self->PC;
			break;

		case ROBOT_VM_PUSHC:  /* Push value from C to stack                */
			PUSH(self->C);
			++self->PC;
			break;

		case ROBOT_VM_POPC:   /* Pop value from stack to C                 */
			POP(self->C);
			++self->PC;
			break;

		case ROBOT_VM_W8:  /* Write byte to address. (*A = B)           */
			if (self->A >= self->memory->len) {
				g_set_error(error, ROBOT_ERROR, ROBOT_ERROR_EXECUTION_FAULT, "Write out of memory");
				return FALSE;
			}
			self->memory->data[self->A] = self->B;
			++self->PC;
			break;

		case ROBOT_VM_R8:  /* Read byte from address. (B = *A)          */
			if (self->A >= self->memory->len) {
				g_set_error(error, ROBOT_ERROR, ROBOT_ERROR_EXECUTION_FAULT, "Write out of memory");
				return FALSE;
			}
			self->B = self->memory->data[self->A];
			++self->PC;
			break;

		case ROBOT_VM_W16:    /* Write uint16 to address. (*A = B)         */
			if (self->A + 1 >= self->memory->len) {
				g_set_error(error, ROBOT_ERROR, ROBOT_ERROR_EXECUTION_FAULT, "Write out of memory");
				return FALSE;
			}
			self->memory->data[self->A] = self->B >> 8;
			self->memory->data[self->A + 1] = self->B;
			++self->PC;
			break;

		case ROBOT_VM_R16:    /* Read uint16 from address. (B = *A)        */
			if (self->A + 1 >= self->memory->len) {
				g_set_error(error, ROBOT_ERROR, ROBOT_ERROR_EXECUTION_FAULT, "Write out of memory");
				return FALSE;
			}
			self->B = (self->memory->data[self->A] << 8) + self->memory->data[self->A + 1];
			++self->PC;
			break;

		case ROBOT_VM_SWAPAB: /* Swap A and B                              */
			a = self->A;
			self->A = self->B;
			self->B = a;
			++self->PC;
			break;

		case ROBOT_VM_COPY:   /* while (C--) *A = *B;                      */
			++self->PC;
			break;

		case ROBOT_VM_NTH:    /* Get value from stack to A                 */
			/** TODO: **/
			if (!robot_vm_stack_nth(self, self->A, &self->A, sizeof(RobotVMWord), error)) {
				return FALSE;
			}
			++self->PC;
			break;

		case ROBOT_VM_CONST:    /* Load constant to A from memory */
			++self->PC;
			GET(self->A, self->PC);
			self->PC += sizeof(RobotVMWord);
			break;

		case ROBOT_VM_STOP:    /* Stop machine */
			++self->PC;
			self->priv->stop = TRUE;
			break;

		/* Binary operations: */
		case ROBOT_VM_LSHIFT:
			self->A = self->A << (self->B & 31);
			++self->PC;
			break;

		case ROBOT_VM_RSHIFT:
			self->A = self->A >> (self->B & 31);
			++self->PC;
			break;

		case ROBOT_VM_SSHIFT:
			self->A = ((gint32)self->A) >> (self->B & 31);
			++self->PC;
			break;

		case ROBOT_VM_BAND:
			self->A = self->A & self->B;
			++self->PC;
			break;

		case ROBOT_VM_BOR:
			self->A = self->A | self->B;
			++self->PC;
			break;

		case ROBOT_VM_BXOR:
			self->A = self->A ^ self->B;
			++self->PC;
			break;

		case ROBOT_VM_BNEG:
			self->A = ~self->A;
			++self->PC;
			break;

		/* Logical operations: */
		case ROBOT_VM_AND:
			self->A = self->A && self->B;
			++self->PC;
			break;

		case ROBOT_VM_OR:
			self->A = self->A || self->B;
			++self->PC;
			break;

		case ROBOT_VM_NOT:
			self->A = !self->A;
			++self->PC;
			break;

		/* Arithmetic operations: */
		case ROBOT_VM_INCR:   /* ++self->A                                 */
			++self->A;
			++self->PC;
			break;

		case ROBOT_VM_DECR:   /* --self->A                                 */
			--self->A;
			++self->PC;
			break;

		case ROBOT_VM_ADD:
			/* TODO: overflow! */
			self->A += self->B;
			++self->PC;
			break;

		case ROBOT_VM_SUB:
			/* TODO: overflow! */
			self->A -= self->B;
			++self->PC;
			break;

		case ROBOT_VM_MUL:
			/* TODO: overflow! */
			self->A *= self->B;
			++self->PC;
			break;

		case ROBOT_VM_DIV:    /* PUSH(POP() / POP())                       */
			if (!self->B) {
				g_set_error(error, ROBOT_ERROR, ROBOT_ERROR_EXECUTION_FAULT, "Division by zero");
				return FALSE;
			}
			self->A /= self->B;
			++self->PC;
			break;

		case ROBOT_VM_MOD:    /* PUSH(POP() % POP())                       */
			if (!self->B) {
				g_set_error(error, ROBOT_ERROR, ROBOT_ERROR_EXECUTION_FAULT, "Division by zero");
				return FALSE;
			}
			self->A %= self->B;
			++self->PC;
			break;

		/* Stack extendent operations: */
		case ROBOT_VM_RESERVE:/* T += A                                    */
			if (self->T < self->A) {
				g_set_error(error, ROBOT_ERROR, ROBOT_ERROR_STACK, "Stack overflow");
				return FALSE;
			}
			self->T -= self->A;
			++self->PC;
			break;

		case ROBOT_VM_RELEASE:/* T -= A                                    */
			if (self->T + self->A >= self->stack_end) {
				g_set_error(error, ROBOT_ERROR, ROBOT_ERROR_STACK, "Stack overflow");
				return FALSE;
			}
			self->T += self->A;
			++self->PC;
			break;

		/* I/O */
		case ROBOT_VM_OUT:    /* Out symbol from stack to console.         */
			/* TODO: unicode! */
			putchar(self->A);
			++self->PC;
			break;

		case ROBOT_VM_IN:     /* Input symbol from console to stack.       */
			/* TODO: unicode! */
			self->A = getchar();
			++self->PC;
			break;

		default:
			g_set_error(error, ROBOT_ERROR, ROBOT_ERROR_INVALID_INSTRUCTION,
					"Invalid instruction %02x at %x", self->memory->data[self->PC], (unsigned)self->PC);
			return FALSE;
	}
	return TRUE;
}

/* Execute program throw the end: */
gboolean robot_vm_exec(RobotVM *self, GError **error)
{
	self->priv->stop = FALSE;

	while (!self->priv->stop) {
		if (!exec(self, error))
			return FALSE;
	}

	return TRUE;
}

/* Execute one program instruction: */
gboolean robot_vm_step(RobotVM *self, gboolean *stop, GError **error)
{
	gboolean res = exec(self, error);

	if (res && stop) {
		*stop = self->priv->stop;
	}

	return res;
}

/* Execute program until some syscall: */
gboolean robot_vm_next(RobotVM *self, gboolean *stop, GError **error)
{
	self->priv->stop = FALSE;

	while (!self->priv->stop) {
		if (self->PC < self->memory->len &&
				self->memory->data[self->PC] == ROBOT_VM_SYS) {
			break;
		}

		if (!exec(self, error)) {
			return FALSE;
		}
	}

	if (*stop)
		*stop = self->priv->stop;

	return TRUE;
}

gboolean robot_vm_stack_push(RobotVM *self, gconstpointer data, gsize len, GError **error)
{
	if (self->T < len) {
		g_set_error(error, ROBOT_ERROR, ROBOT_ERROR_STACK, "Stack overflow");
		return FALSE;
	}

	self->T -= len;
	memcpy(self->memory->data + self->T, data, len);

	return TRUE;
}

gboolean robot_vm_stack_push_word(RobotVM *self, RobotVMWord w, GError **error)
{
	w = g_htonl(w);
	return robot_vm_stack_push(self, &w, sizeof(RobotVMWord), error);
}

gboolean robot_vm_stack_pop(RobotVM *self, gpointer data, gsize len, GError **error)
{
	if (self->T + len >= self->memory->len) { /* TODO: stack size */
		g_set_error(error, ROBOT_ERROR, ROBOT_ERROR_STACK,
				"Trying to pop %u bytes out of memory", (unsigned)len);
		return FALSE;
	}

	memcpy(data, self->memory->data + self->T, len);
	self->T += len;

	return TRUE;
}

gboolean robot_vm_stack_pop_word(RobotVM *self, RobotVMWord *w, GError **error)
{
	gboolean res = robot_vm_stack_pop(self, w, sizeof(RobotVMWord), error);
	if (res)
		*w = g_ntohl(*w);
	return res;
}

gboolean robot_vm_stack_nth(RobotVM *self, guint idx, gpointer data, gsize len, GError **error)
{
	if (self->T + len + idx >= self->memory->len) { /* TODO: stack size */
		g_set_error(error, ROBOT_ERROR, ROBOT_ERROR_STACK,
				"Trying to pop %u bytes causes execution fault", (unsigned)len);
		return FALSE;
	}

	memcpy(data, self->memory->data + (self->T + idx), len);

	return TRUE;
}

#define UNFUNC(nm, CODE) \
	static gboolean rvm_##nm(RobotVM *self, gpointer userdata, GError **error) \
	{ \
		RobotVMWord a, r; \
		if (!robot_vm_stack_pop(self, &a, sizeof(RobotVMWord), error)) { \
			return FALSE; /* Error was set in pop */ \
		} \
		CODE; \
		if (!robot_vm_stack_push(self, &r, sizeof(RobotVMWord), error)) { \
			return FALSE; \
		} \
		return TRUE; \
	}

#define BINFUNC(nm, CODE) \
	static gboolean rvm_##nm(RobotVM *self, gpointer userdata, GError **error) \
	{ \
		RobotVMWord a, b, r; \
		if (!robot_vm_stack_pop(self, &a, sizeof(RobotVMWord), error)) { \
			return FALSE; /* Error was set in pop */ \
		} \
		if (!robot_vm_stack_pop(self, &b, sizeof(RobotVMWord), error)) { \
			return FALSE; /* Error was set in pop */ \
		} \
		CODE; \
		if (!robot_vm_stack_push(self, &r, sizeof(RobotVMWord), error)) { \
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
	{ "$" #nm "$", rvm_##nm, NULL }

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

void robot_vm_allocate_memory(RobotVM *self, gsize len, gsize stack_size)
{
	if (len > self->memory->len) {
		g_byte_array_set_size(self->memory, len);
	}
	self->stack_end = stack_size;
}

gboolean robot_vm_load(RobotVM *self, RobotObjFile *obj, GError **error)
{
	gsize sz = 2;
	gsize sz2 = 2;
	gsize s;
	guint i;
	RobotVMWord w, r;
	RobotObjFileSymbol *sym;

	for (i = 0; i < obj->depends->len; i++) {
		sym = &g_array_index(obj->depends, RobotObjFileSymbol, i);
		if (sym->name[0] != '%') {
			g_set_error(error, ROBOT_ERROR, ROBOT_ERROR_NAME, "Unresolved: %s", sym->name);
			return FALSE;
		}

		if (!robot_vm_has_function(self, sym->name + 1)) {
			g_set_error(error, ROBOT_ERROR, ROBOT_ERROR_NAME, "Unresolved syscall: %s", sym->name);
			return FALSE;
		}
	}

	while (sz > 1 && sz < obj->text->len)
		sz <<= 1;
	while (sz2 > 1 && sz2 < obj->data->len)
		sz2 <<= 1;

	s = sz + sz2 + 0x1000 + 0x10000;
	if (self->memory->len < s) {
		robot_vm_allocate_memory(self, s, 0x1000);
	}

	self->PC = self->stack_end;
	self->T = self->stack_end;

	/* Load text and data segments: */
	memcpy(self->memory->data + self->stack_end, obj->text->data, obj->text->len);
	memcpy(self->memory->data + self->stack_end + sz, obj->data->data, obj->data->len);

	/* Process relocations: */
	for (i = 0; i < obj->relocation->len; i++) {
		r = g_array_index(obj->relocation, RobotVMWord, i);
		r += self->stack_end;

		if (r < obj->data->len) {
			GET(w, r);
			w += self->stack_end;
			PUT(r, w);
		} else {
			r += sz;
			r -= obj->data->len;

			GET(w, r);
			w += self->stack_end + sz;
			PUT(r, w);
		}
	}

	for (i = 0; i < obj->depends->len; i++) {
		sym = &g_array_index(obj->depends, RobotObjFileSymbol, i);

		PUT(sym->addr + self->stack_end, robot_vm_get_function(self, sym->name + 1));
	}

	return TRUE;
}

