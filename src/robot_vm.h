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
	ROBOT_ERROR_STACK
} RobotErrorCodes;

typedef enum _RobotVMCommand {
	ROBOT_VM_NOP,    /* No operation                              */
	ROBOT_VM_JUMP,   /* Jump to address                           */
	ROBOT_VM_JIF,    /* Jump if here are non zero on top of stack */
	ROBOT_VM_SYS,    /* Call function by number in symtable       */
	ROBOT_VM_CALL,   /* Call subprogram by address from A         */
	ROBOT_VM_RET,    /* Return from function                      */
	ROBOT_VM_PUSH,   /* Push value from A to stack                */
	ROBOT_VM_NTH,    /* Get value from stack at position A to A   */
	ROBOT_VM_POP,    /* Pop value from stack to A                 */
	ROBOT_VM_CONST,  /* A = CONST - next memory after instruction */
	ROBOT_VM_STOP,   /* Stop execution. If a == 0 is return code. */

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
	RobotVMWord PC;       /* Programming counter - current instruction address  */
	RobotVMWord T;        /* Stack top address                                  */
	RobotVMWord A;        /* Address register                                   */

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
/** Create VM and do not add standard functions (why???) */
RobotVM* robot_vm_new_empty(void);
void robot_vm_add_standard_functions(RobotVM *self);

guint robot_vm_add_function(RobotVM *self, const char *name, RobotVMFunc func, gpointer userdata, GDestroyNotify free_userdata);
gboolean robot_vm_has_function(RobotVM *self, const char *name);
gint robot_vm_get_function(RobotVM *self, const char *name);

gboolean robot_vm_stack_push(RobotVM *self, gconstpointer data, gsize len, GError **error);
gboolean robot_vm_stack_pop(RobotVM *self, gpointer data, gsize len, GError **error);
gboolean robot_vm_stack_nth(RobotVM *self, guint idx, gpointer data, gsize len, GError **error);

/* Execute program throw the end: */
gboolean robot_vm_exec(RobotVM *self, GError **error);
/* Execute one program instruction: */
gboolean robot_vm_step(RobotVM *self, gboolean *stop, GError **error);
/* Execute program until some syscall: */
gboolean robot_vm_next(RobotVM *self, gboolean *stop, GError **error);

G_END_DECLS

#endif /* ROBOT_VM_H */
