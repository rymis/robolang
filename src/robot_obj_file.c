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
static struct _iinfo {
	const char *name;
	RobotVMCommand code;
} instructions[] = {
	{ "nop", ROBOT_VM_NOP },
	{ "jump", ROBOT_VM_JUMP },
	{ "jif", ROBOT_VM_JIF },
	{ "sys", ROBOT_VM_SYS },
	{ "call", ROBOT_VM_CALL },
	{ "ret", ROBOT_VM_RET },
	{ "push", ROBOT_VM_PUSH },
	{ "pop", ROBOT_VM_POP },
	{ "nth", ROBOT_VM_NTH },
	{ "stop", ROBOT_VM_STOP },

	{ NULL, 0 }
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
	static unsigned ROBOT_VM_DATA = ROBOT_VM_COMMAND_COUNT + 1;
	GByteArray *array = NULL;

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
	g_array_set_size(self->relocation, 0);
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

			if (!strcmp(name, "load")) {
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
				for (j = 0; instructions[j].name; j++) {
					if (!strcmp(name, instructions[j].name)) {
						cur.code = instructions[j].code;
						++loc;
						break;
					}
				}

				if (!instructions[j].name) {
					g_set_error(error, ROBOT_ERROR, ROBOT_ERROR_SYNTAX,
							"Invalid instruction at %d `%s'", line, name);
					g_array_unref(code);
					return FALSE;
				}
			}
		}

		g_array_append_val(code, cur);
	}

	array = self->text;

	/* Ok now we can assemble it. */
	for (i = 0; i < code->len; i++) {
		p = &g_array_index(code, struct instruction, i);

		if (p->code < ROBOT_VM_COMMAND_COUNT) {
			buf[0] = p->code;
			g_byte_array_append(array, buf, 1);
		} else if (p->code == ROBOT_VM_DATA) {
			if (p->data) {
				g_byte_array_append(array, p->data->data, p->data->len);
			}
		}

		if (p->code == ROBOT_VM_CONST) {
			if (p->name[0]) {
				robot_obj_file_add_reference(self, p->name, p->loc + 1);
			} else if (p->sys[0]) {
				robot_obj_file_add_syscall(self, p->sys, p->loc + 1);
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
			robot_obj_file_add_relocation(self, addr);
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
	append_word(res, self->reserved0);
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
	self->reserved0 = 0;
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
			!load_word(data, &idx, &self->reserved0, error) ||
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

	fprintf(f, "FLAGS: %08x\n", (unsigned)self->flags);
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
			for (i = 0; i < self->text->len; i++) {
				if (i % 16 == 0)
					fprintf(f, "\n\t");
				fprintf(f, "%02x ", self->text->data[i]);
			}
			fprintf(f, "\n");
		} else {
			for (i = 0; i < self->text->len; ) {
				for (j = 0; j < self->sym->len; j++) {
					if (g_array_index(self->sym, RobotObjFileSymbol, j).addr == i) {
						fprintf(f, ":%s\n", g_array_index(self->sym, RobotObjFileSymbol, j).name);
						break;
					}
				}

				if (self->text->data[i] >= ROBOT_VM_COMMAND_COUNT) {
					fprintf(f, "{");
					while (i < self->text->len && self->text->data[i] >= ROBOT_VM_COMMAND_COUNT)
						fprintf(f, " %02x", self->text->data[i++]);
					fprintf(f, " }\n");
				} else if (self->text->data[i] == ROBOT_VM_CONST) {
					if (i + 5 <= self->text->len) {
						int ok = 0;
						RobotVMWord w;

						fprintf(f, "load ");
						++i;

						for (j = 0; j < self->depends->len; j++) {
							if (g_array_index(self->depends, RobotObjFileSymbol, j).addr == i) {
								if (g_array_index(self->depends, RobotObjFileSymbol, j).name[0] != '%')
									fprintf(f, "$");
								fprintf(f, "%s\n", g_array_index(self->depends, RobotObjFileSymbol, j).name);
								++ok;
								break;
							}
						}

						if (!ok) {
							w =
								(self->text->data[i] << 24) |
								(self->text->data[i + 1] << 16) |
								(self->text->data[i + 2] << 8) |
								(self->text->data[i + 3]);
							for (j = 0; j < self->sym->len; j++) {
								if (g_array_index(self->sym, RobotObjFileSymbol, j).addr == w) {
									fprintf(f, "$%s\n", g_array_index(self->sym, RobotObjFileSymbol, j).name);
									++ok;
									break;
								}
							}
						}

						if (!ok) {
							fprintf(f, "0x%02x%02x%02x%02x\n",
									self->text->data[i + 0],
									self->text->data[i + 1],
									self->text->data[i + 2],
									self->text->data[i + 3]);
						}

						i += 4;
					} else {
						fprintf(f, "{");
						while (i < self->text->len)
							fprintf(f, " %02x", self->text->data[i++]);
					}
				} else {
					for (j = 0; instructions[j].name; j++)
						if (instructions[j].code == self->text->data[i])
							break;
					if (instructions[j].name) {
						fprintf(f, "%s\n", instructions[j].name);
					} else {
						fprintf(f, "{ %02x }\n", self->text->data[i]);
					}

					i++;
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

