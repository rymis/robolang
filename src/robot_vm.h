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
	ROBOT_ERROR_STACK
} RobotErrorCodes;

typedef enum _RobotVMCommand {
	ROBOT_VM_NOP,    /* No operation                              */
	ROBOT_VM_JUMP,   /* Jump to address                           */
	ROBOT_VM_JIF,    /* Jump if here are non zero on top of stack */
	ROBOT_VM_CALL,   /* Call function by number in symtable       */
	ROBOT_VM_RET,    /* Return from function                      */
	ROBOT_VM_PUSH,   /* Push value from A to stack                */
	ROBOT_VM_NTH,    /* Get value from stack at position A to A   */
	ROBOT_VM_POP,    /* Pop value from stack to A                 */

	ROBOT_VM_COMMAND_COUNT
} RobotVMCommand;

typedef struct _RobotVMStack {
	GByteArray *stack;
	gsize top;
} RobotVMStack;

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
	RobotVMStack stack;   /* Stack. It is not mapped to memory.                 */
	RobotVMStack rstack;  /* Return addresses stack                             */
	RobotVMWord PC;       /* Programming counter - current instruction address  */
	RobotVMWord T;        /* Stack top address                                  */
	RobotVMWord A;        /* Address register                                   */

	RobotVMPrivate *priv;
};

struct _RobotVMClass {
	GObjectClass parent_class;
};

RobotVM* robot_vm_new(void);
guint robot_vm_add_function(RobotVM *self, const char *name, RobotVMFunc func, gpointer userdata, GDestroyNotify free_userdata);
gboolean robot_vm_has_function(RobotVM *self, const char *name);
gint robot_vm_get_function(RobotVM *self, const char *name);

gboolean robot_vm_stack_push(RobotVMStack *stack, gconstpointer data, gsize len, GError **error);
gboolean robot_vm_stack_pop(RobotVMStack *stack, gpointer data, gsize len, GError **error);
gboolean robot_vm_stack_nth(RobotVMStack *stack, guint idx, gpointer data, gsize len, GError **error);

G_END_DECLS

#endif /* ROBOT_VM_H */
