#include "robot_obj_file.h"
#include <string.h>

G_DEFINE_TYPE(RobotObjFile, robot_obj_file, G_TYPE_OBJECT)

static void finalize(GObject *obj)
{
	RobotObjFile *self = ROBOT_OBJ_FILE(obj);
	g_byte_array_unref(self->data);
	g_byte_array_unref(self->text);
	g_array_unref(self->sym);
	g_array_unref(self->relocation);
	g_array_unref(self->depends);

	self->data = NULL;
	self->text = NULL;
	self->sym = NULL;
	self->relocation = NULL;
	self->depends = NULL;
}

static void robot_obj_file_class_init(RobotObjFileClass *klass)
{
	GObjectClass *objcls = G_OBJECT_CLASS(klass);
	objcls->finalize = finalize;
}

static void robot_obj_file_init(RobotObjFile *self)
{
	self->text = g_byte_array_new();
	self->data = g_byte_array_new();
	self->sym = g_array_new(FALSE, TRUE, sizeof(RobotObjFileSymbol));
	self->depends = g_array_new(FALSE, TRUE, sizeof(RobotObjFileSymbol));
	self->relocation = g_array_new(FALSE, TRUE, sizeof(RobotVMWord));
}

RobotObjFile* robot_obj_file_new(void)
{
	RobotObjFile* self = g_object_new(ROBOT_TYPE_OBJ_FILE, NULL);

	return self;
}

static RobotObjFileSymbol *get_sym(RobotObjFile *self, const char *name);
static void add_depend(RobotObjFile *self, const char *name, RobotVMWord addr);

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

		g_byte_array_append(data, &c, 1);

		p = skip_ws(s, line);
		if (p == s && *s != '}') {
			g_set_error(error, ROBOT_ERROR, ROBOT_ERROR_SYNTAX, "Invalid symbol at line %d `%16s'", *line, s);
			return NULL;
		}

		s = p;
	}

	c = 0;
	while (data->len % 4 != 0)
		g_byte_array_append(data, &c, 1);

	return NULL;
}

static const gchar* read_string(const gchar *s, GByteArray *data, int *line, GError **error)
{
	unsigned char c;

	++s;
	while (*s != '"') {
		if (!*s) {
			g_set_error(error, ROBOT_ERROR, ROBOT_ERROR_SYNTAX, "End of file inside the string.");
			return NULL;
		}

		if (*s == '\n')
			++(*line);

		if (*s == '\\') {
			++s;
			if (!*s) {
				g_set_error(error, ROBOT_ERROR, ROBOT_ERROR_SYNTAX, "End of file inside escape sequence in string.");
				return NULL;
			}

			if (*s == 'n') {
				c = '\n';
			} else if (*s == 'r') {
				c = '\r';
			} else if (*s == 'a') {
				c = '\a';
			} else if (*s == 't') {
				c = '\t';
			} else if (*s == 'x') {
				++s;
				if (!g_ascii_isxdigit(s[0]) || !g_ascii_isxdigit(s[1])) {
					g_set_error(error, ROBOT_ERROR, ROBOT_ERROR_SYNTAX, "Invalid escape sequence.");
					return NULL;
				}
				c = g_ascii_xdigit_value(s[0]) * 16 + g_ascii_xdigit_value(s[1]);
				s++;
			} else if (*s >= '0' && *s <= '3') {
				if (s[1] >= '0' && s[1] <= '8' && s[2] >= '0' && s[2] <= '8') {
					c = (s[0] - '0') * 64 + (s[1] - '0') * 8 + (s[2] - '0');
				} else {
					g_set_error(error, ROBOT_ERROR, ROBOT_ERROR_SYNTAX, "Invalid escape sequence.");
					return NULL;
				}
				s += 2;
			} else {
				c = *s;
			}
		} else {
			c = *s;
		}

		g_byte_array_append(data, &c, 1);
		++s;
	}

	c = 0;
	g_byte_array_append(data, &c, 1);

	while (data->len % 4 != 0)
		g_byte_array_append(data, &c, 1);

	return s + 1;
}

static const gchar* read_reg(const gchar *s, guint8 *r, int *line, GError **error)
{
	guint num = 0;

	s = skip_ws(s, line);
	if (!s || !*s) {
		g_set_error(error, ROBOT_ERROR, ROBOT_ERROR_SYNTAX, "Waiting for register name at line %d", *line);
		return NULL;
	}

	/* Special case: const */
	if (g_ascii_isxdigit(*s) && g_ascii_isxdigit(s[1]) && (s[2] == 0 || g_ascii_isspace(s[2]))) {
		num = g_ascii_xdigit_value(s[0]) * 16 + g_ascii_xdigit_value(s[1]);
		*r = num;
		s += 2;

		return skip_ws(s, line);
	}

	if (*s != 'r' && *s != 'R') {
		g_set_error(error, ROBOT_ERROR, ROBOT_ERROR_SYNTAX, "Waiting for register name at line %d", *line);
		return NULL;
	}

	++s;
	if (!g_ascii_isdigit(*s)) {
		g_set_error(error, ROBOT_ERROR, ROBOT_ERROR_SYNTAX, "Waiting for register name at line %d", *line);
		return NULL;
	}

	num = g_ascii_digit_value(*s);
	++s;
	if (g_ascii_isdigit(*s)) {
		num = num * 10 + g_ascii_digit_value(*s);
		++s;
		if (!*s || g_ascii_isspace(*s)) {
			*r = num;
			if (num > 31) {
				g_set_error(error, ROBOT_ERROR, ROBOT_ERROR_SYNTAX, "We've got only 32 registers (at line %d)", *line);
				return NULL;
			}
			return skip_ws(s, line);
		} else {
			g_set_error(error, ROBOT_ERROR, ROBOT_ERROR_SYNTAX, "Waiting for register name at line %d", *line);
			return NULL;
		}
	} else if (!*s || g_ascii_isspace(*s)) {
		*r = num;
		return skip_ws(s, line);
	} else {
		g_set_error(error, ROBOT_ERROR, ROBOT_ERROR_SYNTAX, "Waiting for register name at line %d", *line);
		return NULL;
	}

	return NULL;
}

struct instruction {
	RobotVMWord    loc;
	RobotVMCommand code;
	guint8 A, B, C;
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
	s->data = NULL;
}

/* Assembler is as simple as possible. Language is:
 * # comment - it is comment
 * ; comment - and this is comment too
 * .text, .data - section names. Here are only 2 variants possible.
 * .stack number - could be used to set stack size
 * :label - label. This label place could be used in program as address in form @label
 * name [r1 [r2 [r3]]] - instruction
 * { aa bb cc dd } - save data at the address. Will be aligned with 0's
 * "string" - the same as previous but saves zero-ended string
 * Special instruction 'load r0 @name' or 'load r0 %name' or 'load r0 const' loads address or extension number or constant to register.
 */
static struct _iinfo {
	const char *name;
	RobotVMCommand code;
	unsigned argcnt;
} instructions[] = {
	/* Main: */
	{ "nop", ROBOT_VM_NOP, 0 },
	{ "load", ROBOT_VM_LOAD, 1 },
	{ "ext", ROBOT_VM_EXT, 1 },
	{ "write8", ROBOT_VM_W8, 2 },
	{ "read8", ROBOT_VM_R8, 2 },
	{ "write16", ROBOT_VM_W16, 2 },
	{ "read16", ROBOT_VM_R16, 2 },
	{ "write32", ROBOT_VM_W32, 2 },
	{ "read32", ROBOT_VM_R32, 2 },
	{ "stop", ROBOT_VM_STOP, 1 },
	{ "move", ROBOT_VM_MOVE, 2 },
	{ "moveif", ROBOT_VM_MOVEIF, 3 },
	{ "moveifz", ROBOT_VM_MOVEIFZ, 3 },
	{ "swap", ROBOT_VM_SWAP, 2 },
	/* Binary operations: */
	{ "lshift", ROBOT_VM_LSHIFT, 3 },
	{ "rshift", ROBOT_VM_RSHIFT, 3 },
	{ "sshift", ROBOT_VM_SSHIFT, 3 },
	{ "and", ROBOT_VM_AND, 3 },
	{ "or", ROBOT_VM_OR, 3 },
	{ "xor", ROBOT_VM_XOR, 3 },
	{ "neg", ROBOT_VM_NEG, 2 },
	/* Arithmetic operations: */
	{ "incr", ROBOT_VM_INCR, 1 },
	{ "decr", ROBOT_VM_DECR, 1 },
	{ "incr4", ROBOT_VM_INCR4, 1 },
	{ "decr4", ROBOT_VM_DECR4, 1 },
	{ "add", ROBOT_VM_ADD, 3 },
	{ "sub", ROBOT_VM_SUB, 3 },
	{ "mul", ROBOT_VM_MUL, 3 },
	{ "div", ROBOT_VM_DIV, 3 },
	/* I/O */
	{ "out", ROBOT_VM_OUT, 1 },
	{ "in", ROBOT_VM_IN, 1 },

	{ NULL, 0, 0 }
};

gboolean robot_obj_file_compile(RobotObjFile *self, const gchar *prog, GError **error)
{
	int line = 1;
	const gchar *s = prog;
	char name[128];
	unsigned char buf[sizeof(RobotVMWord) + 1];
	GArray *code = g_array_new(FALSE, TRUE, sizeof(struct instruction));
	struct instruction cur;
	RobotVMWord loc = 0;
	guint i;
	int j;
	struct instruction *p;
	GByteArray *array = NULL;

	if (!prog) {
		g_array_unref(code);
		g_set_error(error, ROBOT_ERROR, ROBOT_ERROR_GENERAL, "Null argument");
		return FALSE;
	}
	g_array_set_clear_func(code, instr_clear);

	/* Clear all the data: */
	self->flags = 0;
	self->SS = 1024;
	self->reserved1 = 0;
	self->reserved2 = 0;
	self->reserved3 = 0;
	g_byte_array_set_size(self->text, 0);
	g_byte_array_set_size(self->data, 0);
	g_array_set_size(self->sym, 0);
	g_array_set_size(self->relocation, 0);
	g_array_set_size(self->depends, 0);

	/* Wait for start of .text secion: */
	while (*s) {
		s = skip_ws(s, &line);
		if (!*s) {
			g_set_error(error, ROBOT_ERROR, ROBOT_ERROR_SYNTAX, "Empty assembler file");
			g_array_unref(code);
			return FALSE;
		}

		if (*s != '.') {
			g_set_error(error, ROBOT_ERROR, ROBOT_ERROR_SYNTAX, "Waiting for parameters or end of section at line %d", line);
			g_array_unref(code);
			return FALSE;
		}

		++s;
		if (!(s = read_name(s, name, sizeof(name), line, error))) {
			g_array_unref(code);
			return FALSE;
		}

		if (!strcmp(name, "text")) {
			/* Ok it is start of code: */
			break;
		} else if (!strcmp(name, "stack")) {
			/* Set stack size: */
			s = skip_ws(s, &line);
			if (!*s) {
				g_array_unref(code);
				g_set_error(error, ROBOT_ERROR, ROBOT_ERROR_SYNTAX, "Waiting for stack size at line %d", line);
				return FALSE;
			}

			if (!(s = read_num(s, &self->SS, line, error))) {
				g_array_unref(code);
				return FALSE;
			}
		} else if (!strcmp(name, "data")) {
			g_set_error(error, ROBOT_ERROR, ROBOT_ERROR_SYNTAX, "data section must be placed after .text at line %d", line);
			g_array_unref(code);
			return FALSE;
		} else {
			g_set_error(error, ROBOT_ERROR, ROBOT_ERROR_SYNTAX, "Unknown secion or parameter %s at line %d", name, line);
			g_array_unref(code);
			return FALSE;
		}
	}

	/* .text: */
	while (*s) {
		s = skip_ws(s, &line);
		if (!*s)
			break;

		/* Code atom template: */
		cur.loc = loc;
		cur.code = ROBOT_VM_COMMAND_COUNT;
		cur.addr = 0;
		cur.A = 0;
		cur.B = 0;
		cur.C = 0;
		cur.name[0] = 0;
		cur.sys[0] = 0;
		cur.line = line;
		cur.data = NULL;

		if (*s == ':') { /* Label: */
			++s;
			if (!(s = read_name(s, cur.name, sizeof(cur.name), line, error))) {
				g_array_unref(code);
				return FALSE;
			}

			if (!robot_obj_file_add_symbol(self, cur.name, loc, error)) {
				g_array_unref(code);
				return FALSE;
			}

			continue;
		} else if (*s == '.') { /* Section: */
			++s;
			if (!(s = read_name(s, cur.name, sizeof(cur.name), line, error))) {
				g_array_unref(code);
				return FALSE;
			}

			if (!strcmp(cur.name, "text")) {
				g_array_unref(code);
				g_set_error(error, ROBOT_ERROR, ROBOT_ERROR_SYNTAX, "Invalid section placement at line %d", line);
				return FALSE;
			} else if (!strcmp(cur.name, "data")) {
				/* Now will be data section: */
				break;
			} else {
				g_array_unref(code);
				g_set_error(error, ROBOT_ERROR, ROBOT_ERROR_SYNTAX, "Invalid section: %s at line %d", cur.name, line);
				return FALSE;
			}
		} else if (*s == '{') {
			cur.data = g_byte_array_new();
			s = read_data(s, cur.data, &line, error);
			if (!s) {
				g_array_unref(code);
				return FALSE;
			}

			loc += cur.data->len;
		} else if (*s == '"') {
			cur.data = g_byte_array_new();
			s = read_string(s, cur.data, &line, error);
			if (!s) {
				g_array_unref(code);
				return FALSE;
			}

			loc += cur.data->len;
		} else {
			if (!(s = read_name(s, name, sizeof(name), line, error))) {
				g_array_unref(code);
				return FALSE;
			}

			if (!strcmp(name, "const")) { /* Virtual instruction: */
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
				for (j = 0; instructions[j].name; j++) {
					if (!strcmp(name, instructions[j].name)) {
						cur.code = instructions[j].code;
						loc += sizeof(RobotVMWord);
						break;
					}
				}

				if (!instructions[j].name) {
					g_set_error(error, ROBOT_ERROR, ROBOT_ERROR_SYNTAX,
							"Invalid instruction at %d `%s'", line, name);
					g_array_unref(code);
					return FALSE;
				}

				for (i = 0; i < instructions[j].argcnt; i++) {
					if (!(s = read_reg(s, i == 0? &cur.A: ((i == 1)? &cur.B: &cur.C), &line, error))) {
						g_array_unref(code);
						return FALSE;
					}
				}
			}
		}

		g_array_append_val(code, cur);
	}

	loc = code->len * 4;

	/* .data */
	while (*s) {
		GByteArray *data = NULL;

		s = skip_ws(s, &line);
		if (!*s)
			break;

		if (*s == ':') { /* Label: */
			++s;
			if (!(s = read_name(s, name, sizeof(name), line, error))) {
				g_array_unref(code);
				return FALSE;
			}

			if (!robot_obj_file_add_symbol(self, name, self->text->len + loc, error)) {
				g_array_unref(code);
				return FALSE;
			}

			continue;
		} else if (*s == '{') {
			data = g_byte_array_new();
			s = read_data(s, data, &line, error);
			if (!s) {
				g_array_unref(code);
				return FALSE;
			}

			loc += data->len;
		} else if (*s == '"') {
			data = g_byte_array_new();
			s = read_string(s, data, &line, error);
			if (!s) {
				g_array_unref(code);
				return FALSE;
			}

			loc += data->len;
		} else {
			g_set_error(error, ROBOT_ERROR, ROBOT_ERROR_SYNTAX, "Invalid instruction in data section at line %d", line);
			g_array_unref(code);
			return FALSE;
		}

		g_byte_array_append(self->data, data->data, data->len);

		g_byte_array_unref(data);
		data = NULL;
	}

	array = self->text;

	/* Ok now we can assemble it. */
	for (i = 0; i < code->len; i++) {
		p = &g_array_index(code, struct instruction, i);

		if (p->code < ROBOT_VM_COMMAND_COUNT) {
			buf[0] = p->code;
			buf[1] = p->A;
			buf[2] = p->B;
			buf[3] = p->C;
			g_byte_array_append(array, buf, 4);
		} else {
			if (p->data) {
				g_byte_array_append(array, p->data->data, p->data->len);
			} else {
				if (p->name[0]) {
					RobotObjFileSymbol *s = get_sym(self, p->name);
					if (s) {
						p->addr = s->addr;
						robot_obj_file_add_relocation(self, p->loc);
					} else {
						add_depend(self, p->name, p->loc);
					}
				} else if (p->sys[0]) {
					robot_obj_file_add_syscall(self, p->sys, p->loc);
					p->addr = 0;
				} else {
					/* Here are constant so we don't need to add relocation: */
					/* robot_obj_file_add_relocation(self, p->loc + 1); */
				}

				buf[0] = (p->addr >> 24) & 0xff;
				buf[1] = (p->addr >> 16) & 0xff;
				buf[2] = (p->addr >>  8) & 0xff;
				buf[3] = (p->addr      ) & 0xff;
				g_byte_array_append(array, buf, 4);
			}
		}
	}

	g_array_unref(code);

	return TRUE;
}

const gchar* robot_instruction_to_string(gconstpointer code, gchar *buf, gsize len)
{
	int i;
	const guint8 *p = code;

	if (p[0] >= ROBOT_VM_COMMAND_COUNT) {
		if (buf) {
			snprintf(buf, len, "{ %02x %02x %02x %02x }", buf[0], buf[1], buf[2], buf[3]);
			return buf;
		} else {
			return "const";
		}
	}

	for (i = 0; instructions[i].name; i++) {
		if (instructions[i].code == p[0]) {
			if (buf) {
				if (instructions[i].argcnt == 0) {
					snprintf(buf, len, "%s", instructions[i].name);
				} else if (instructions[i].argcnt == 1) {
					snprintf(buf, len, "%s r%d", instructions[i].name, p[1]);
				} else if (instructions[i].argcnt == 2) {
					snprintf(buf, len, "%s r%d r%d", instructions[i].name, p[1], p[2]);
				} else if (instructions[i].argcnt == 3) {
					snprintf(buf, len, "%s r%d r%d r%d", instructions[i].name, p[1], p[2], p[3]);
				}
				return buf;
			} else {
				return instructions[i].name;
			}
		}
	}

	if (buf) {
		snprintf(buf, len, "${UNKNOWN %02X}$", p[0]);
		return buf;
	} else {
		return "${UNKNOWN INSTRUCTION}$";
	}
}

/* Low level functions. I don't think you need them but why should I hide ones? */
gboolean robot_obj_file_add_symbol(RobotObjFile *self, const char *name, RobotVMWord addr, GError **error)
{
	RobotObjFileSymbol s;
	guint i;

	if (strlen(name) > sizeof(s.name) - 1) {
		g_set_error(error, ROBOT_ERROR, ROBOT_ERROR_SYNTAX, "Symbol name is too long");
		return FALSE;
	}

	strcpy(s.name, name);
	s.addr = addr;

	for (i = 0; i < self->sym->len; i++) {
		if (!strcmp(g_array_index(self->sym, RobotObjFileSymbol, i).name, name)) {
			g_set_error(error, ROBOT_ERROR, ROBOT_ERROR_SYNTAX, "Symbol `%s' is defined twice", name);
			return FALSE;
		}
	}

	g_array_append_val(self->sym, s);

	return TRUE;
}

static gboolean write_word(RobotObjFile *self, RobotVMWord addr, RobotVMWord w)
{
	unsigned char *ptr = NULL;

	if (addr >= self->text->len) {
		addr -= self->text->len;
		if (addr + sizeof(RobotVMWord) > self->data->len)
			return FALSE;
		ptr = self->data->data + addr;
	} else {
		if (addr + sizeof(RobotVMWord) > self->text->len)
			return FALSE;
		ptr = self->text->data + addr;
	}

	*ptr++ = (w >> 24) & 0xff;
	*ptr++ = (w >> 16) & 0xff;
	*ptr++ = (w >>  8) & 0xff;
	*ptr++ = (w      ) & 0xff;

	return TRUE;
}

static RobotObjFileSymbol *get_sym(RobotObjFile *self, const char *name)
{
	RobotObjFileSymbol s;
	guint i;

	strncpy(s.name, name, sizeof(s.name));
	s.name[sizeof(s.name) - 1] = 0;

	for (i = 0; i < self->sym->len; i++) {
		if (!strcmp(g_array_index(self->sym, RobotObjFileSymbol, i).name, name)) {
			return &g_array_index(self->sym, RobotObjFileSymbol, i);
		}
	}

	return NULL;
}

/* Checks if there are this name in current file and adds dependency or reference */
void robot_obj_file_add_reference(RobotObjFile *self, const char *name, RobotVMWord addr)
{
	RobotObjFileSymbol s;
	guint i;

	strncpy(s.name, name, sizeof(s.name));
	s.name[sizeof(s.name) - 1] = 0;
	s.addr = addr;

	for (i = 0; i < self->sym->len; i++) {
		if (!strcmp(g_array_index(self->sym, RobotObjFileSymbol, i).name, name)) {
			write_word(self, addr, g_array_index(self->sym, RobotObjFileSymbol, i).addr);
			robot_obj_file_add_relocation(self, addr);
			return;
		}
	}

	g_array_append_val(self->depends, s);
}

static void add_depend(RobotObjFile *self, const char *name, RobotVMWord addr)
{
	RobotObjFileSymbol s;

	strncpy(s.name, name, sizeof(s.name));
	s.name[sizeof(s.name) - 1] = 0;
	s.addr = addr;

	g_array_append_val(self->depends, s);
}

void robot_obj_file_add_syscall(RobotObjFile *self, const char *name, RobotVMWord addr)
{
	RobotObjFileSymbol s;

	s.name[0] = '%';
	strncpy(s.name + 1, name, sizeof(s.name) - 1);
	s.name[sizeof(s.name) - 1] = 0;
	s.addr = addr;

	g_array_append_val(self->depends, s);
}

void robot_obj_file_add_relocation(RobotObjFile *self, RobotVMWord addr)
{
	g_array_append_val(self->relocation, addr);
}

static void append_word(GByteArray *array, RobotVMWord w)
{
	unsigned char buf[4];

	buf[0] = (w >> 24) & 0xff;
	buf[1] = (w >> 16) & 0xff;
	buf[2] = (w >>  8) & 0xff;
	buf[3] = (w      ) & 0xff;

	g_byte_array_append(array, buf, 4);
}

static GByteArray* sym2bytes(GArray *a)
{
	guint i;
	guint l;
	RobotObjFileSymbol *p;
	GByteArray *res = g_byte_array_new();

	for (i = 0; i < a->len; i++) {
		p = &g_array_index(a, RobotObjFileSymbol, i);
		l = strlen(p->name);
		g_byte_array_append(res, (guint8*)p->name, l + 1);
		append_word(res, p->addr);
	}

	return res;
}

GByteArray* robot_obj_file_to_byte_array(RobotObjFile *self, GError **error)
{
	GByteArray *res;
	GByteArray *sym;
	GByteArray *depends;
	guint i;

	sym = sym2bytes(self->sym);
	depends = sym2bytes(self->depends);

	res = g_byte_array_new();
	append_word(res, self->flags);
	append_word(res, self->SS);
	append_word(res, self->reserved1);
	append_word(res, self->reserved2);
	append_word(res, self->reserved3);

	/* Sizes of sections: */
	append_word(res, self->text->len);
	append_word(res, self->data->len);
	append_word(res, sym->len);
	append_word(res, self->relocation->len * 4);
	append_word(res, depends->len);

	/* Dump sections: */
	g_byte_array_append(res, self->text->data, self->text->len);
	g_byte_array_append(res, self->data->data, self->data->len);
	g_byte_array_append(res, sym->data, sym->len);
	for (i = 0; i < self->relocation->len; i++) {
		append_word(res, g_array_index(self->relocation, RobotVMWord, i));
	}
	g_byte_array_append(res, depends->data, depends->len);

	g_byte_array_unref(sym);
	g_byte_array_unref(depends);

	return res;
}

static gboolean load_word(GByteArray *data, guint *idx_in, RobotVMWord *w, GError **error)
{
	guint idx = *idx_in;

	if (idx + 4 > data->len) {
		g_set_error(error, ROBOT_ERROR, ROBOT_ERROR_GENERAL, "Invalid file format (can not read word)");
		return FALSE;
	}

	*w = (data->data[idx] << 24) | (data->data[idx + 1] << 16) | (data->data[idx + 2] << 8) | data->data[idx + 3];
	*idx_in = idx + 4;

	return TRUE;
}

static gboolean load_string(GByteArray *data, guint *idx_in, char *buf, gsize buf_len, GError **error)
{
	guint idx = *idx_in;
	guint i = 0;

	if (idx >= data->len) {
		g_set_error(error, ROBOT_ERROR, ROBOT_ERROR_GENERAL, "Invalid file format (unexpected end of file)");
		return FALSE;
	}

	for (;;) {
		if (i >= buf_len || idx >= data->len) {
			g_set_error(error, ROBOT_ERROR, ROBOT_ERROR_GENERAL, "Invalid file format (unexpected end of string)");
			return FALSE;
		}

		buf[i] = data->data[idx++];

		if (!buf[i])
			break;
		++i;
	}

	*idx_in = idx;

	return TRUE;
}

gboolean robot_obj_file_from_byte_array(RobotObjFile *self, GByteArray *data, GError **error)
{
	guint idx = 0;
	guint end;
	RobotObjFileSymbol s;
	RobotVMWord w;
	RobotVMWord text_len;
	RobotVMWord data_len;
	RobotVMWord sym_len;
	RobotVMWord relocation_len;
	RobotVMWord depends_len;

	/* Clear all the data: */
	self->flags = 0;
	self->SS = 0;
	self->reserved1 = 0;
	self->reserved2 = 0;
	self->reserved3 = 0;
	g_byte_array_set_size(self->text, 0);
	g_byte_array_set_size(self->data, 0);
	g_array_set_size(self->sym, 0);
	g_array_set_size(self->relocation, 0);
	g_array_set_size(self->depends, 0);

	/* 1. loading flags and reserved: */
	if (
			!load_word(data, &idx, &self->flags, error) ||
			!load_word(data, &idx, &self->SS, error) ||
			!load_word(data, &idx, &self->reserved1, error) ||
			!load_word(data, &idx, &self->reserved2, error) ||
			!load_word(data, &idx, &self->reserved3, error) ||
			!load_word(data, &idx, &text_len, error) ||
			!load_word(data, &idx, &data_len, error) ||
			!load_word(data, &idx, &sym_len, error) ||
			!load_word(data, &idx, &relocation_len, error) ||
			!load_word(data, &idx, &depends_len, error)
	   ) {
		return FALSE;
	}

	if (text_len + data_len + sym_len + relocation_len + depends_len + idx != data->len) {
		g_set_error(error, ROBOT_ERROR, ROBOT_ERROR_GENERAL, "Invalid object file format (%u != %u)!",
				(unsigned)text_len + data_len + sym_len + relocation_len + depends_len + idx,
				(unsigned)data->len);
		return FALSE;
	}

	if (text_len) {
		g_byte_array_set_size(self->text, text_len);
		memcpy(self->text->data, data->data + idx, text_len);
		idx += text_len;
	}

	if (data_len) {
		g_byte_array_set_size(self->data, data_len);
		memcpy(self->data->data, data->data + idx, data_len);
		idx += data_len;
	}

	end = idx + sym_len;
	while (idx < end) {
		if ( !load_string(data, &idx, s.name, sizeof(s.name), error) ||
				!load_word(data, &idx, &s.addr, error)) {
			return FALSE;
		}

		g_array_append_val(self->sym, s);
	}

	end = idx + relocation_len;
	while (idx < end) {
		if (!load_word(data, &idx, &w, error)) {
			return FALSE;
		}

		g_array_append_val(self->relocation, w);
	}

	end = idx + depends_len;
	while (idx < end) {
		if ( !load_string(data, &idx, s.name, sizeof(s.name), error) ||
				!load_word(data, &idx, &s.addr, error)) {
			return FALSE;
		}

		g_array_append_val(self->depends, s);
	}

	return TRUE;
}

gboolean robot_obj_file_dump(RobotObjFile *self, FILE *f, gboolean disasm, GError **error)
{
	guint i, j;
	RobotObjFileSymbol *s;
	char buf[256];
	gboolean load = FALSE;

	fprintf(f, "FLAGS: %08x\n", (unsigned)self->flags);
	fprintf(f, "STACK SIZE: %08x\n", (unsigned)self->SS);
	fprintf(f, "TEXT SIZE: %u\n", (unsigned)self->text->len);
	fprintf(f, "DATA SIZE: %u\n", (unsigned)self->data->len);
	fprintf(f, "SYMBOLS COUNT: %u\n", (unsigned)self->sym->len);
	fprintf(f, "RELOCATIONS COUNT: %u\n", (unsigned)self->relocation->len);
	fprintf(f, "DEPENDS COUNT: %u\n", (unsigned)self->depends->len);

	if (self->depends->len) {
		fprintf(f, "DEPENDS:\n");
		for (i = 0; i < self->depends->len; i++) {
			s = &g_array_index(self->depends, RobotObjFileSymbol, i);
			fprintf(f, "\t[%08x] %s\n", (unsigned)s->addr, s->name);
		}
	}

	if (self->sym->len) {
		fprintf(f, "SYMBOLS:\n");
		for (i = 0; i < self->sym->len; i++) {
			s = &g_array_index(self->sym, RobotObjFileSymbol, i);
			fprintf(f, "\t[%08x] %s\n", (unsigned)s->addr, s->name);
		}
	}

	if (self->relocation->len) {
		fprintf(f, "RELOCATIONS:\n");
		for (i = 0; i < self->relocation->len; i++) {
			fprintf(f, "\t[%08x]", (unsigned)g_array_index(self->relocation, RobotVMWord, i));
			if (i && i % 8 == 0)
				fprintf(f, "\n");
		}
		fprintf(f, "\n");
	}

	if (self->text->len) {
		fprintf(f, "TEXT:\n");
		if (!disasm) {
			for (i = 0; i < self->text->len; i += 4) {
				if (i % 8 == 0)
					fprintf(f, "\n\t");
				fprintf(f, "%02x%02x%02x%02x ",
						self->text->data[i],
						self->text->data[i + 1],
						self->text->data[i + 2],
						self->text->data[i + 3]);
			}
			fprintf(f, "\n");
		} else {
			for (i = 0; i < self->text->len; i += 4) {
				for (j = 0; j < self->sym->len; j++) {
					if (g_array_index(self->sym, RobotObjFileSymbol, j).addr == i) {
						fprintf(f, ":%s\n", g_array_index(self->sym, RobotObjFileSymbol, j).name);
						break;
					}
				}

				fprintf(f, "[%08x] ", i);

				if (load) {
					fprintf(f, "%02x%02x%02x%02x\n",
							self->text->data[i],
							self->text->data[i + 1],
							self->text->data[i + 2],
							self->text->data[i + 3]);
					load = FALSE;
				} else {
					fprintf(f, "%s\n", robot_instruction_to_string(self->text->data + i, buf, sizeof(buf)));
					load = (self->text->data[i] == ROBOT_VM_LOAD);
				}

			}
		}
	}

	if (self->data->len) {
		fprintf(f, "DATA:");
		for (i = 0; i < self->data->len; i++) {
			if (i % 16 == 0)
				fprintf(f, "\n\t");
			fprintf(f, "%02x", self->data->data[i]);
		}
		fprintf(f, "\n");
	}

	return TRUE;
}

gboolean robot_obj_file_merge(RobotObjFile *self, RobotObjFile *other, GError **error)
{
	gsize offset = 0;
	gsize off_data = 0;
	guint i;
	gsize r;
	RobotObjFileSymbol *s;
	GArray *depends = NULL;

	g_return_val_if_fail(self != NULL, FALSE);
	g_return_val_if_fail(other != NULL, FALSE);

	offset = self->text->len;
	off_data = self->data->len;

	g_byte_array_append(self->text, other->text->data, other->text->len);
	g_byte_array_append(self->data, other->data->data, other->data->len);

	/* Process relocations: */
	for (i = 0; i < other->relocation->len; i++) {
		r = g_array_index(other->relocation, RobotVMWord, i);
		if (r >= other->text->len) {
			r += off_data;
		}
		r += offset;

		robot_obj_file_add_relocation(self, r);
	}

	/* Process symbols: */
	for (i = 0; i < other->sym->len; i++) {
		s = &g_array_index(other->sym, RobotObjFileSymbol, i);
		r = s->addr;

		if (r >= other->text->len) {
			r += off_data;
		}
		r += offset;

		if (!robot_obj_file_add_symbol(self, s->name, r, error)) {
			return FALSE;
		}
	}

	/* Resolve symbols: */
	/* 1. Save old dependencies and create new ones: */
	depends = self->depends;
	self->depends = g_array_new(FALSE, TRUE, sizeof(RobotObjFileSymbol));

	/* 2. Add self dependencies: */
	for (i = 0; i < depends->len; i++) {
		s = &g_array_index(depends, RobotObjFileSymbol, i);
		if (s->name[0] == '%') {
			robot_obj_file_add_syscall(self, s->name + 1, s->addr);
		} else {
			robot_obj_file_add_reference(self, s->name, s->addr);
		}
	}

	g_array_unref(depends);

	/* 3. Add others dependencies: */
	for (i = 0; i < other->depends->len; i++) {
		s = &g_array_index(other->depends, RobotObjFileSymbol, i);
		if (s->name[0] == '%') {
			robot_obj_file_add_syscall(self, s->name + 1, s->addr + offset);
		} else {
			robot_obj_file_add_reference(self, s->name, s->addr + offset);
		}
	}

	if (other->SS > self->SS)
		self->SS = other->SS;

	return TRUE;
}

guint robot_obj_file_dependencies_count(RobotObjFile *self)
{
	RobotObjFileSymbol *s;
	guint i;
	guint cnt = 0;

	for (i = 0; i < self->depends->len; i++) {
		s = &g_array_index(self->depends, RobotObjFileSymbol, i);
		if (s->name[0] != '%')
			++cnt;
	};

	return cnt;
}

