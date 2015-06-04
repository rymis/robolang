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

	gboolean stop;
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
	self->priv->stop = FALSE;

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

		case ROBOT_VM_STOP:    /* Stop machine */
			++self->PC;
			self->priv->stop = TRUE;
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

static const gchar* skip_ws(const gchar* s, int *line)
{
	for (;;) {
		while (*s && g_ascii_isspace(*s)) {
			if (*s == '\n')
				++(*line);
			++s;
		}

		if (*s == ';' || *s == '#') {
			++s;
			while (*s && *s != '\n')
				++s;
			if (!*s)
				return s;
			++(*line);
			++s;
		} else {
			return s;
		}
	}

	return NULL; /* not reached */
}

static const gchar *read_name(const gchar *s, char *id, gsize sz, int line, GError **error)
{
	char b[16];
	gsize i = 0;

	if (!g_ascii_isalpha(s[0]) && s[0] != '$') {
		strncpy(b, s, sizeof(b) - 1);
		b[sizeof(b) - 1] = 0;
		g_set_error(error, ROBOT_ERROR, ROBOT_ERROR_SYNTAX, "Invalid character at line %d `%s'", line, b);
		return NULL;
	}

	while (i < sz && (g_ascii_isalnum(s[i]) || s[i] == '_' || s[i] == '$')) {
		id[i] = s[i];
		i++;
	}

	if (i == sz) {
		id[sz - 1] = 0;
		g_set_error(error, ROBOT_ERROR, ROBOT_ERROR_SYNTAX, "Invalid identifier of size more then %u at line %d `%s...'", (unsigned)sz, line, id);
		return NULL;
	}

	id[i] = 0;

	return s + i;
}

static const gchar* read_num(const gchar *s, RobotVMWord *result, int line, GError **error)
{
	RobotVMWord res = 0;
	RobotVMWord r2 = 0;

	if (s[0] == '0' && s[1] == 'x') { /* Read hex */
		s += 2;
		while (g_ascii_isxdigit(*s)) {
			r2 = res * 16 + g_ascii_xdigit_value(*s);
			++s;

			if (r2 < res) {
				g_set_error(error, ROBOT_ERROR, ROBOT_ERROR_SYNTAX, "Integer overflow at line %d", line);
				return NULL;
			}

			res = r2;
		}
	} else {
		while (g_ascii_isdigit(*s)) {
			r2 = res * 10 + g_ascii_digit_value(*s);
			++s;

			if (r2 < res) {
				g_set_error(error, ROBOT_ERROR, ROBOT_ERROR_SYNTAX, "Integer overflow at line %d", line);
				return NULL;
			}

			res = r2;
		}
	}

	*result = res;

	return s;
}

static const gchar* read_data(const gchar *s, GByteArray *data, int *line, GError **error)
{
	unsigned char c;
	const gchar *p;

	++s;

	while (*s) {
		s = skip_ws(s, line);

		if (*s == '}') {
			++s;
			return s;
		}

		if (!g_ascii_isxdigit(*s)) {
			g_set_error(error, ROBOT_ERROR, ROBOT_ERROR_SYNTAX, "Invalid symbol at line %d `%16s'", *line, s);
			return NULL;
		}

		c = g_ascii_xdigit_value(*s) * 16;
		++s;

		if (!g_ascii_isxdigit(*s)) {
			g_set_error(error, ROBOT_ERROR, ROBOT_ERROR_SYNTAX, "Invalid symbol at line %d `%16s'", *line, s);
			return NULL;
		}

		c += g_ascii_xdigit_value(*s);
		++s;

		p = skip_ws(s, line);
		if (p == s && *s != '}') {
			g_set_error(error, ROBOT_ERROR, ROBOT_ERROR_SYNTAX, "Invalid symbol at line %d `%16s'", *line, s);
			return NULL;
		}

		s = p;
	}

	return NULL;
}

struct instruction {
	RobotVMWord    loc;
	RobotVMCommand code;
	RobotVMWord    addr;
	char name[256];
	char sys[256];
	int line;
	GByteArray *data;
};

static void instr_clear(void *p)
{
	struct instruction *s = p;
	if (s->data)
		g_byte_array_unref(s->data);
}

static gboolean find_label(GArray *array, const char *name, RobotVMWord *addr)
{
	guint i;
	struct instruction *p;

	for (i = 0; i < array->len; i++) {
		p = &g_array_index(array, struct instruction, i);
		if (p->code == ROBOT_VM_COMMAND_COUNT && !strcmp(p->name, name)) {
			*addr = p->loc;
			return TRUE;
		}
	}

	return FALSE;
}

/* Assembler is as simple as possible. Language is:
 * # comment - it is comment
 * ; comment - and this is comment too
 * :label - label. This label place could be used in program as address in form @label
 * nop - no operation instruction
 * jump - jump to address saved in register A
 * jif  - jump to address saved in register A if top of stack contains not zero. This instruction pops value from stack.
 * sys  - call system function with number in A
 * call - call subprogram by address from A
 * ret  - return from subprogram
 * push - push value from memory pointed by A into the stack
 * pop  - pop value from stack into memory pointed by A
 * nth  - get value from stack value by offset in A
 * stop - stop the program
 * load value - load value after this instruction into register A. Value could be @label, %sys or integer constant in forms: 0xhex or 1234
 * { aa bb cc dd } - save data at the address. Must be placed directly after label.
 */
gboolean robot_vm_asm_compile(RobotVM *self, const gchar *prog, GError **error)
{
	int line = 1;
	const gchar *s = prog;
	char name[128];
	unsigned char buf[sizeof(RobotVMWord) + 1];
	GArray *code = g_array_new(FALSE, TRUE, sizeof(struct instruction));
	struct instruction cur;
	RobotVMWord loc = 0;
	guint i;
	struct instruction *p;

	if (!prog) {
		g_array_unref(code);
		g_set_error(error, ROBOT_ERROR, ROBOT_ERROR_GENERAL, "Null argument");
		return FALSE;
	}
	g_array_set_clear_func(code, instr_clear);

	while (*s) {
		s = skip_ws(s, &line);

		if (!*s)
			break;

		cur.loc = loc;
		cur.code = ROBOT_VM_COMMAND_COUNT;
		cur.addr = 0;
		cur.name[0] = 0;
		cur.sys[0] = 0;
		cur.line = line;
		cur.data = NULL;

		if (*s == ':') {
			++s;
			if (!(s = read_name(s, cur.name, sizeof(cur.name), line, error))) {
				g_array_unref(code);
				return FALSE;
			}

			s = skip_ws(s, &line);
			if (*s == '{') {
				cur.data = g_byte_array_new();
				s = read_data(s, cur.data, &line, error);
				if (!s) {
					g_array_unref(code);
					return FALSE;
				}

				loc += cur.data->len;
			}
		} else {
			if (!(s = read_name(s, name, sizeof(name), line, error))) {
				g_array_unref(code);
				return FALSE;
			}

			if (!strcmp(name, "nop")) {
				cur.code = ROBOT_VM_NOP;
				++loc;
			} else if (!strcmp(name, "jump")) {
				cur.code = ROBOT_VM_JUMP;
				++loc;
			} else if (!strcmp(name, "jif")) {
				cur.code = ROBOT_VM_JIF;
				++loc;
			} else if (!strcmp(name, "sys")) {
				cur.code = ROBOT_VM_SYS;
				++loc;
			} else if (!strcmp(name, "call")) {
				cur.code = ROBOT_VM_CALL;
				++loc;
			} else if (!strcmp(name, "ret")) {
				cur.code = ROBOT_VM_RET;
				++loc;
			} else if (!strcmp(name, "push")) {
				cur.code = ROBOT_VM_PUSH;
				++loc;
			} else if (!strcmp(name, "pop")) {
				cur.code = ROBOT_VM_POP;
				++loc;
			} else if (!strcmp(name, "nth")) {
				cur.code = ROBOT_VM_NTH;
				++loc;
			} else if (!strcmp(name, "stop")) {
				cur.code = ROBOT_VM_STOP;
				++loc;
			} else if (!strcmp(name, "load")) {
				cur.code = ROBOT_VM_CONST;
				++loc;

				if (!g_ascii_isspace(*s)) {
					g_set_error(error, ROBOT_ERROR, ROBOT_ERROR_SYNTAX,
							"Invalid character at %d `%6s'", line, s);
					g_array_unref(code);
					return FALSE;
				}

				/* Get argument */
				s = skip_ws(s, &line);
				if (!*s) {
					g_set_error(error, ROBOT_ERROR, ROBOT_ERROR_SYNTAX,
							"Waiting for argument at %d", line);
					g_array_unref(code);
					return FALSE;
				}

				if (*s == '@') {
					++s;
					s = read_name(s, cur.name, sizeof(cur.name), line, error);
					if (!s) {
						g_array_unref(code);
						return FALSE;
					}
				} else if (*s == '%') {
					++s;
					s = read_name(s, cur.sys, sizeof(cur.sys), line, error);
					if (!s) {
						g_array_unref(code);
						return FALSE;
					}
				} else if (g_ascii_isdigit(*s)) {
					s = read_num(s, &cur.addr, line, error);
					if (!s) {
						g_array_unref(code);
						return FALSE;
					}
				} else {
					g_set_error(error, ROBOT_ERROR, ROBOT_ERROR_SYNTAX,
							"Invalid argument at %d `%10s'", line, s);
					g_array_unref(code);
					return FALSE;
				}

				loc += sizeof(RobotVMWord);
			} else {
				g_set_error(error, ROBOT_ERROR, ROBOT_ERROR_SYNTAX,
						"Invalid instruction at %d `%s'", line, name);
				g_array_unref(code);
				return FALSE;
			}
		}

		g_array_append_val(code, cur);
	}

	g_byte_array_set_size(self->memory, 0);

	/* Ok now we can assemble it. */
	for (i = 0; i < code->len; i++) {
		p = &g_array_index(code, struct instruction, i);

		if (p->code != ROBOT_VM_COMMAND_COUNT) {
			buf[0] = p->code;
			g_byte_array_append(self->memory, buf, 1);
		} else {
			if (p->data) {
				g_byte_array_append(self->memory, p->data->data, p->data->len);
			}
		}

		if (p->code == ROBOT_VM_CONST) {
			if (p->name[0]) {
				if (!find_label(code, p->name, &p->addr)) {
					g_set_error(error, ROBOT_ERROR, ROBOT_ERROR_SYNTAX, "Invalid label at line %d `%s'", p->line, p->name);
					g_array_unref(code);
					return FALSE;
				}
			} else if (p->sys[0]) {
				gint x = robot_vm_get_function(self, p->sys);
				if (x < 0) {
					g_set_error(error, ROBOT_ERROR, ROBOT_ERROR_SYNTAX, "Invalid syscall at line %d `%s'", p->line, p->sys);
					g_array_unref(code);
					return FALSE;
				}
				p->addr = x;
			}

			buf[0] = (p->addr >> 24) & 0xff;
			buf[1] = (p->addr >> 16) & 0xff;
			buf[2] = (p->addr >>  8) & 0xff;
			buf[3] = (p->addr      ) & 0xff;
			g_byte_array_append(self->memory, buf, 4);
		}
	}
	g_array_unref(code);

	return TRUE;
}

/* Dump ASM program: */
gchar* robot_vm_asm_dump(RobotVM *self, GError **error);
