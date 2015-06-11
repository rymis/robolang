#ifndef _ROBOT_VM_H_
#define _ROBOT_VM_H_ 1

#include <glib-object.h>

G_BEGIN_DECLS

GQuark robot_error_get(void);
#define ROBOT_ERROR robot_error_get()

typedef enum _RobotErrorCodes {
	ROBOT_ERROR_NONE,
	ROBOT_ERROR_GENERAL,
	ROBOT_ERROR_INVALID_INSTRUCTION,
	ROBOT_ERROR_INVALID_ADDRESS,
	ROBOT_ERROR_EXECUTION_FAULT,
	ROBOT_ERROR_DIVISION_BY_ZERO,
	ROBOT_ERROR_SYNTAX,
	ROBOT_ERROR_IO,
	ROBOT_ERROR_NAME,
	ROBOT_ERROR_STACK
} RobotErrorCodes;

typedef enum _RobotVMCommand {
	ROBOT_VM_NOP,    /* No operation                              */
	ROBOT_VM_LOAD,   /* Load B C into A                           */
	ROBOT_VM_EXT,    /* Call extension instruction by number      */
	ROBOT_VM_W8,     /* Write byte to address. (*A = B)           */
	ROBOT_VM_R8,     /* Read byte from address. (B = *A)          */
	ROBOT_VM_W16,    /* Write uint16 to address. (*A = B)         */
	ROBOT_VM_R16,    /* Read uint16 from address. (B = *A)        */
	ROBOT_VM_W32,    /* Read word from address.                   */
	ROBOT_VM_R32,    /* Write word to address.                    */
	ROBOT_VM_SWAP,   /* Swap A and B                              */
	ROBOT_VM_MOVE,   /* A = B                                     */
	ROBOT_VM_MOVEIF, /* A = B if C != 0                           */
	ROBOT_VM_STOP,   /* Stop execution. A is return code.         */
	/* Binary operations: */
	ROBOT_VM_LSHIFT, /* A = B << C                                */
	ROBOT_VM_RSHIFT, /* PUSH(POP() >> POP())                      */
	ROBOT_VM_SSHIFT, /* PUSH((signed int)POP() >> POP())          */
	ROBOT_VM_BAND,   /* PUSH(POP() & POP())                       */
	ROBOT_VM_BOR,    /* PUSH(POP() | POP())                       */
	ROBOT_VM_BXOR,   /* PUSH(POP() ^ POP())                       */
	ROBOT_VM_BNEG,   /* PUSH(~POP())                              */
	/* Arithmetic operations: */
	ROBOT_VM_INCR,   /* ++self->A                                 */
	ROBOT_VM_DECR,   /* --self->A                                 */
	ROBOT_VM_ADD,    /* PUSH(POP() + POP())                       */
	ROBOT_VM_SUB,    /* PUSH(POP() - POP())                       */
	ROBOT_VM_MUL,    /* PUSH(POP() * POP())                       */
	ROBOT_VM_DIV,    /* PUSH(POP() / POP())                       */
	/* I/O */
	ROBOT_VM_OUT,    /* Out symbol from stack to console.         */
	ROBOT_VM_IN,     /* Input symbol from console to stack.       */

	ROBOT_VM_COMMAND_COUNT
} RobotVMCommand;

/* Type conversion macroses: */
#define ROBOT_TYPE_VM                   (robot_vm_get_type())
#define ROBOT_VM(obj)                   (G_TYPE_CHECK_INSTANCE_CAST((obj),  ROBOT_TYPE_VM, RobotVM))
#define ROBOT_IS_VM(obj)                (G_TYPE_CHECK_INSTANCE_TYPE ((obj), ROBOT_TYPE_VM))
#define ROBOT_VM_CLASS(klass)           (G_TYPE_CHECK_CLASS_CAST ((klass),  ROBOT_TYPE_VM, RobotVMClass))
#define ROBOT_IS_VM_CLASS(klass)        (G_TYPE_CHECK_CLASS_TYPE ((klass),  ROBOT_TYPE_VM))
#define ROBOT_VM_GET_CLASS(obj)         (G_TYPE_INSTANCE_GET_CLASS ((obj),  ROBOT_TYPE_VM, RobotVMClass))

/* get_type prototype: */
GType robot_vm_get_type(void);

/* Structures definitions: */
typedef struct _RobotVM RobotVM;
typedef struct _RobotVMClass RobotVMClass;
typedef struct _RobotVMPrivate RobotVMPrivate;
typedef gboolean (*RobotVMFunc)(RobotVM *vm, gpointer userdata, GError **error);

typedef guint32 RobotVMWord;

struct _RobotVM {
	GObject parent_instance;

	GByteArray *memory;   /* VM memory.                                         */
	RobotVMWord R[32];    /* Registers                                          */

	/* Some registers has special meaning:
	 * R0 - Instruction counter
	 * R1 - used as stack top
	 * R31 - used to store multiplication and integer overflow and mod of division
	 */

	RobotVMPrivate *priv;
};

struct _RobotVMClass {
	GObjectClass parent_class;
};

/* Object file: */
typedef struct _RobotVMObj {
	RobotVMWord flags;  /* Flags        */
	GByteArray *text;   /* code segment */
	GByteArray *data;   /* data segment */
	GHashTable *sym;    /* symbol table */
	GHashTable *dsym;   /* data symbols */
	GByteArray *reloc;  /* relocation table */
} RobotVMObj;

/** Create new VM and add standard functions */
RobotVM* robot_vm_new(void);

void robot_vm_allocate_memory(RobotVM *self, gsize len);
typedef struct _RobotObjFile RobotObjFile;
gboolean robot_vm_load(RobotVM *self, RobotObjFile *obj, GError **error);

guint robot_vm_add_function(RobotVM *self, const char *name, RobotVMFunc func, gpointer userdata, GDestroyNotify free_userdata);
gboolean robot_vm_has_function(RobotVM *self, const char *name);
gint robot_vm_get_function(RobotVM *self, const char *name);

gboolean robot_vm_stack_push(RobotVM *self, gconstpointer data, gsize len, GError **error);
gboolean robot_vm_stack_pop(RobotVM *self, gpointer data, gsize len, GError **error);
gboolean robot_vm_stack_nth(RobotVM *self, guint idx, gpointer data, gsize len, GError **error);
gboolean robot_vm_stack_pop_word(RobotVM *self, RobotVMWord *w, GError **error);
gboolean robot_vm_stack_push_word(RobotVM *self, RobotVMWord w, GError **error);

/* Execute program throw the end: */
gboolean robot_vm_exec(RobotVM *self, GError **error);
/* Execute one program instruction: */
gboolean robot_vm_step(RobotVM *self, gboolean *stop, GError **error);
/* Execute program until some syscall: */
gboolean robot_vm_next(RobotVM *self, gboolean *stop, GError **error);

G_END_DECLS

#endif /* ROBOT_VM_H */
