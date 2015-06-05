#include "robot_obj_file.h"
#include <string.h>

G_DEFINE_TYPE(RobotObjFile, robot_obj_file, G_TYPE_OBJECT)

static void finalize(GObject *obj)
{
	RobotObjFile *self = ROBOT_OBJ_FILE(obj);
	g_byte_array_unref(self->data);
	g_byte_array_unref(self->text);
	g_array_unref(self->sym);
	g_array_unref(self->reallocation);
	g_array_unref(self->depends);

	self->data = NULL;
	self->text = NULL;
	self->sym = NULL;
	self->reallocation = NULL;
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
	self->reallocation = g_array_new(FALSE, TRUE, sizeof(RobotVMWord));
}

RobotObjFile* robot_obj_file_new(void)
{
	RobotObjFile* self = g_object_new(ROBOT_TYPE_OBJ_FILE, NULL);

	return self;
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

		g_byte_array_append(data, &c, 1);

		p = skip_ws(s, line);
		if (p == s && *s != '}') {
			g_set_error(error, ROBOT_ERROR, ROBOT_ERROR_SYNTAX, "Invalid symbol at line %d `%16s'", *line, s);
			return NULL;
		}

		s = p;
	}

	return NULL;
}

static const gchar* read_string(const gchar *s, GByteArray *data, int *line, GError **error)
{
	/* TODO: */
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
	struct instruction *p;
	GByteArray *array = NULL;
	static int ROBOT_VM_DATA = ROBOT_VM_COMMAND_COUNT + 1;

	if (!prog) {
		g_array_unref(code);
		g_set_error(error, ROBOT_ERROR, ROBOT_ERROR_GENERAL, "Null argument");
		return FALSE;
	}
	g_array_set_clear_func(code, instr_clear);

	/* Clear all the data: */
	self->flags = 0;
	self->reserved0 = 0;
	self->reserved1 = 0;
	self->reserved2 = 0;
	self->reserved3 = 0;
	g_byte_array_set_size(self->text, 0);
	g_byte_array_set_size(self->data, 0);
	g_array_set_size(self->sym, 0);
	g_array_set_size(self->reallocation, 0);
	g_array_set_size(self->depends, 0);

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

			if (!robot_obj_file_add_symbol(self, cur.name, loc, error)) {
				g_array_unref(code);
				return FALSE;
			}

		} else if (*s == '{') {
			cur.code = ROBOT_VM_DATA;
			cur.data = g_byte_array_new();
			s = read_data(s, cur.data, &line, error);
			if (!s) {
				g_array_unref(code);
				return FALSE;
			}

			loc += cur.data->len;
		} else if (*s == '"') {
			cur.code = ROBOT_VM_DATA;
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

	array = g_byte_array_new();

	/* Ok now we can assemble it. */
	for (i = 0; i < code->len; i++) {
		p = &g_array_index(code, struct instruction, i);

		if (p->code != ROBOT_VM_COMMAND_COUNT) {
			buf[0] = p->code;
			g_byte_array_append(array, buf, 1);
		} else {
			if (p->data) {
				g_byte_array_append(array, p->data->data, p->data->len);
			}
		}

		if (p->code == ROBOT_VM_CONST) {
			if (p->name[0]) {
				robot_obj_file_add_reference(self, p->name, p->loc);
			} else if (p->sys[0]) {
				robot_obj_file_add_syscall(self, p->sys, p->loc);
				p->addr = 0;
			} else {
				robot_obj_file_add_reallocation(self, p->loc);
			}

			buf[0] = (p->addr >> 24) & 0xff;
			buf[1] = (p->addr >> 16) & 0xff;
			buf[2] = (p->addr >>  8) & 0xff;
			buf[3] = (p->addr      ) & 0xff;
			g_byte_array_append(array, buf, 4);
		}
	}

	g_array_unref(code);

	return TRUE;
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
			robot_obj_file_add_reallocation(self, addr);
			return;
		}
	}

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

void robot_obj_file_add_reallocation(RobotObjFile *self, RobotVMWord addr)
{
	g_array_append_val(self->reallocation, addr);
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
	append_word(res, self->reserved0);
	append_word(res, self->reserved1);
	append_word(res, self->reserved2);
	append_word(res, self->reserved3);

	/* Sizes of sections: */
	append_word(res, self->text->len);
	append_word(res, self->data->len);
	append_word(res, sym->len);
	append_word(res, self->reallocation->len * 4);
	append_word(res, depends->len);

	/* Dump sections: */
	g_byte_array_append(res, self->text->data, self->text->len);
	g_byte_array_append(res, self->data->data, self->data->len);
	g_byte_array_append(res, sym->data, sym->len);
	for (i = 0; i < self->reallocation->len; i++) {
		append_word(res, g_array_index(self->reallocation, RobotVMWord, i));
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
	RobotVMWord reallocation_len;
	RobotVMWord depends_len;

	/* Clear all the data: */
	self->flags = 0;
	self->reserved0 = 0;
	self->reserved1 = 0;
	self->reserved2 = 0;
	self->reserved3 = 0;
	g_byte_array_set_size(self->text, 0);
	g_byte_array_set_size(self->data, 0);
	g_array_set_size(self->sym, 0);
	g_array_set_size(self->reallocation, 0);
	g_array_set_size(self->depends, 0);

	/* 1. loading flags and reserved: */
	if (
			!load_word(data, &idx, &self->flags, error) ||
			!load_word(data, &idx, &self->reserved0, error) ||
			!load_word(data, &idx, &self->reserved1, error) ||
			!load_word(data, &idx, &self->reserved2, error) ||
			!load_word(data, &idx, &self->reserved3, error) ||
			!load_word(data, &idx, &text_len, error) ||
			!load_word(data, &idx, &data_len, error) ||
			!load_word(data, &idx, &sym_len, error) ||
			!load_word(data, &idx, &reallocation_len, error) ||
			!load_word(data, &idx, &depends_len, error)
	   ) {
		return FALSE;
	}

	if (text_len + data_len + sym_len + reallocation_len + depends_len + idx != data->len) {
		g_set_error(error, ROBOT_ERROR, ROBOT_ERROR_GENERAL, "Invalid object file format (%u != %u)!",
				(unsigned)text_len + data_len + sym_len + reallocation_len + depends_len + idx,
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

	end = idx + reallocation_len;
	while (idx < end) {
		if (!load_word(data, &idx, &w, error)) {
			return FALSE;
		}

		g_array_append_val(self->reallocation, w);
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

