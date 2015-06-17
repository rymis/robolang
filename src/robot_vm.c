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

	self->R[0] = 0;
	self->memory = g_byte_array_new();
}

RobotVM* robot_vm_new(void)
{
	RobotVM *self = g_object_new(ROBOT_TYPE_VM, NULL);

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
	RobotVMCommand cmd;
	guint8 A, B, C;

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

	if (self->R[0] + 4 > self->memory->len) {
		g_set_error(error, ROBOT_ERROR, ROBOT_ERROR_EXECUTION_FAULT,
				"Invalid value of PC (%x)", (unsigned)self->R[0]);
		return FALSE;
	}

	cmd = self->memory->data[self->R[0]++];
	A = self->memory->data[self->R[0]++] & 0x1f;
	B = self->memory->data[self->R[0]++] & 0x1f;
	C = self->memory->data[self->R[0]++] & 0x1f;
	switch (cmd) {
		case ROBOT_VM_NOP:    /* No operation                              */
			break;

		case ROBOT_VM_LOAD:
			GET(a, self->R[0]);
			self->R[0] += 4;
			self->R[A] = a;
			break;

		case ROBOT_VM_EXT:   /* Call function by number in symtable       */
			if (self->priv->symtable->len <= self->R[A]) {
				g_set_error(error, ROBOT_ERROR, ROBOT_ERROR_INVALID_ADDRESS,
						"Invalid function reference: %u\n", (unsigned)self->R[A]);
				return FALSE;
			}
			sym = &g_array_index(self->priv->symtable, Symbol, self->R[A]);
			if (!sym->func) {
				g_set_error(error, ROBOT_ERROR, ROBOT_ERROR_EXECUTION_FAULT, "Invalid function");
				return FALSE;
			}

			return sym->func(self, sym->userdata, error);

		case ROBOT_VM_W8:  /* Write byte to address. (*A = B)           */
			if (self->R[A] >= self->memory->len) {
				g_set_error(error, ROBOT_ERROR, ROBOT_ERROR_EXECUTION_FAULT, "Write out of memory");
				return FALSE;
			}
			self->memory->data[self->R[A]] = self->R[B];
			break;

		case ROBOT_VM_R8:  /* Read byte from address. (B = *A)          */
			if (self->R[B] >= self->memory->len) {
				g_set_error(error, ROBOT_ERROR, ROBOT_ERROR_EXECUTION_FAULT, "Write out of memory");
				return FALSE;
			}
			self->R[A] = self->memory->data[self->R[B]];
			break;

		case ROBOT_VM_W16:    /* Write uint16 to address. (*A = B)         */
			if (self->R[A] + 1 >= self->memory->len) {
				g_set_error(error, ROBOT_ERROR, ROBOT_ERROR_EXECUTION_FAULT, "Write out of memory");
				return FALSE;
			}
			self->memory->data[self->R[A]] = self->R[B] >> 8;
			self->memory->data[self->R[A] + 1] = self->R[B];
			break;

		case ROBOT_VM_R16:    /* Read uint16 from address. (B = *A)        */
			if (self->R[B] + 1 >= self->memory->len) {
				g_set_error(error, ROBOT_ERROR, ROBOT_ERROR_EXECUTION_FAULT, "Write out of memory");
				return FALSE;
			}
			self->R[A] = (self->memory->data[self->R[B]] << 8) + self->memory->data[self->R[B] + 1];
			break;

		case ROBOT_VM_W32:
			PUT(self->R[B], self->R[A]);
			break;

		case ROBOT_VM_R32:
			GET(self->R[A], self->R[B]);
			break;

		case ROBOT_VM_SWAP: /* Swap A and B                              */
			a = self->R[A];
			self->R[A] = self->R[B];
			self->R[B] = a;
			break;

		case ROBOT_VM_MOVE:
			self->R[A] = self->R[B];
			break;

		case ROBOT_VM_MOVEIF:
			if (self->R[C])
				self->R[A] = self->R[B];
			break;

		case ROBOT_VM_MOVEIFZ:
			if (self->R[C] == 0)
				self->R[A] = self->R[B];
			break;

		case ROBOT_VM_STOP:
			self->priv->stop = TRUE;
			break;

		/* Binary operations: */
		case ROBOT_VM_LSHIFT:
			self->R[A] = self->R[B] << (self->R[C] & 31);
			break;

		case ROBOT_VM_RSHIFT:
			self->R[A] = self->R[B] >> (self->R[C] & 31);
			break;

		case ROBOT_VM_SSHIFT:
			self->R[A] = ((gint32)self->R[B]) >> (self->R[C] & 31);
			break;

		case ROBOT_VM_AND:
			self->R[A] = self->R[B] & self->R[C];
			break;

		case ROBOT_VM_OR:
			self->R[A] = self->R[B] | self->R[C];
			break;

		case ROBOT_VM_XOR:
			self->R[A] = self->R[B] ^ self->R[C];
			break;

		case ROBOT_VM_NEG:
			self->R[A] = ~self->R[B];
			break;

		/* Arithmetic operations: */
		case ROBOT_VM_INCR:   /* ++self->A                                 */
			++self->R[A];
			break;

		case ROBOT_VM_DECR:   /* --self->A                                 */
			--self->R[A];
			break;

		case ROBOT_VM_INCR4:   /* ++self->A                                 */
			self->R[A] += 4;
			break;

		case ROBOT_VM_DECR4:   /* --self->A                                 */
			self->R[A] -= 4;
			break;

		case ROBOT_VM_ADD:
			/* TODO: overflow! */
			self->R[A] = self->R[B] + self->R[C];
			break;

		case ROBOT_VM_SUB:
			/* TODO: overflow! */
			self->R[A] = self->R[B] + self->R[C];
			break;

		case ROBOT_VM_MUL:
			/* TODO: overflow! */
			self->R[A] = self->R[B] * self->R[C];
			break;

		case ROBOT_VM_DIV:    /* PUSH(POP() / POP())                       */
			if (!self->R[C]) {
				g_set_error(error, ROBOT_ERROR, ROBOT_ERROR_EXECUTION_FAULT, "Division by zero");
				return FALSE;
			}
			self->R[A] = self->R[B] / self->R[C];
			self->R[31] = self->R[B] % self->R[C];
			break;

		/* I/O */
		case ROBOT_VM_OUT:    /* Out symbol from stack to console.         */
			/* TODO: unicode! */
			/* TODO: channels? */
			putchar(self->R[A]);
			break;

		case ROBOT_VM_IN:     /* Input symbol from console to stack.       */
			/* TODO: unicode! */
			/* TODO: channels? */
			self->R[A] = getchar();
			break;

		default:
			g_set_error(error, ROBOT_ERROR, ROBOT_ERROR_INVALID_INSTRUCTION,
					"Invalid instruction %02x at %x", self->memory->data[self->R[0]], (unsigned)self->R[0]);
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
		if (self->R[0] < self->memory->len &&
				self->memory->data[self->R[0]] == ROBOT_VM_EXT) {
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

void robot_vm_allocate_memory(RobotVM *self, gsize len)
{
	if (len > self->memory->len) {
		g_byte_array_set_size(self->memory, len);
	}
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

	s = sz + sz2 + obj->SS + 0x10000;
	if (self->memory->len < s) {
		robot_vm_allocate_memory(self, s); 
	}

	self->R[0] = obj->SS;
	self->R[1] = obj->SS;

	/* Load text and data segments: */
	memcpy(self->memory->data + self->R[1], obj->text->data, obj->text->len);
	memcpy(self->memory->data + self->R[1] + sz, obj->data->data, obj->data->len);

	/* Process relocations: */
	for (i = 0; i < obj->relocation->len; i++) {
		r = g_array_index(obj->relocation, RobotVMWord, i);

		if (r < obj->text->len) {
			r += self->R[1];
			GET(w, r);
			w += self->R[1];
			PUT(r, w);
		} else {
			r += self->R[1];
			r += sz;
			r -= obj->text->len;

			GET(w, r);
			w += self->R[1] + sz;
			PUT(r, w);
		}
	}

	for (i = 0; i < obj->depends->len; i++) {
		sym = &g_array_index(obj->depends, RobotObjFileSymbol, i);

		PUT(sym->addr + self->R[1], robot_vm_get_function(self, sym->name + 1));
	}

	return TRUE;
}

