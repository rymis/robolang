#ifndef _ROBOT_OBJ_FILE_H_
#define _ROBOT_OBJ_FILE_H_ 1

#include <glib-object.h>
#include <stdio.h>
#include "robot_vm.h"

G_BEGIN_DECLS

/* Type conversion macroses: */
#define ROBOT_TYPE_OBJ_FILE                   (robot_obj_file_get_type())
#define ROBOT_OBJ_FILE(obj)                   (G_TYPE_CHECK_INSTANCE_CAST((obj),  ROBOT_TYPE_OBJ_FILE, RobotObjFile))
#define ROBOT_IS_OBJ_FILE(obj)                (G_TYPE_CHECK_INSTANCE_TYPE ((obj), ROBOT_TYPE_OBJ_FILE))
#define ROBOT_OBJ_FILE_CLASS(klass)           (G_TYPE_CHECK_CLASS_CAST ((klass),  ROBOT_TYPE_OBJ_FILE, RobotObjFileClass))
#define ROBOT_IS_OBJ_FILE_CLASS(klass)        (G_TYPE_CHECK_CLASS_TYPE ((klass),  ROBOT_TYPE_OBJ_FILE))
#define ROBOT_OBJ_FILE_GET_CLASS(obj)         (G_TYPE_INSTANCE_GET_CLASS ((obj),  ROBOT_TYPE_OBJ_FILE, RobotObjFileClass))

/* get_type prototype: */
GType robot_obj_file_get_type(void);

/* Structures definitions: */
typedef struct _RobotObjFile RobotObjFile;
typedef struct _RobotObjFileClass RobotObjFileClass;
typedef struct _RobotObjFileSymbol RobotObjFileSymbol;

struct _RobotObjFile {
	GObject parent_instance;

	/* Header: */
	RobotVMWord flags;
	RobotVMWord reserved0;
	RobotVMWord reserved1;
	RobotVMWord reserved2;
	RobotVMWord reserved3;

	/* Code section: */
	GByteArray *text;
	/* Data section: */
	GByteArray *data;
	/* Defined functions and data: */
	GArray *sym;
	/* Relocation table: */
	GArray *relocation;
	/* Dependencies of this code:
	 * If dependency name starts with '%' it is system dependency. */
	GArray *depends;
};

struct _RobotObjFileClass {
	GObjectClass parent_class;
};

struct _RobotObjFileSymbol {
	gchar name[256];
	RobotVMWord addr;
};

RobotObjFile* robot_obj_file_new(void);

/* Compile ASM program and returns object file: */
gboolean robot_obj_file_compile(RobotObjFile *self, const gchar *prog, GError **error);
/* Merge two objects into one: */
gboolean robot_obj_file_merge(RobotObjFile *self, RobotObjFile *other , GError **error);

/* Low level functions. I don't think you need them but why should I hide ones? */
gboolean robot_obj_file_add_symbol(RobotObjFile *self, const char *name, RobotVMWord addr, GError **error);
/* Checks if there are this name in current file and adds dependency or reference */
void robot_obj_file_add_reference(RobotObjFile *self, const char *name, RobotVMWord addr);
void robot_obj_file_add_syscall(RobotObjFile *self, const char *name, RobotVMWord addr);
void robot_obj_file_add_relocation(RobotObjFile *self, RobotVMWord addr);

GByteArray* robot_obj_file_to_byte_array(RobotObjFile *self, GError **error);
gboolean robot_obj_file_from_byte_array(RobotObjFile *self, GByteArray *from, GError **error);

gboolean robot_obj_file_dump(RobotObjFile *self, FILE *f, gboolean disasm, GError **error);

G_END_DECLS

#endif /* ROBOT_OBJ_FILE_H */

