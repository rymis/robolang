#ifndef _ROBOT_XML_H_
#define _ROBOT_XML_H_ 1

#include <glib-object.h>

G_BEGIN_DECLS

/* Type conversion macroses: */
#define ROBOT_TYPE_XML                   (robot_xml_get_type())
#define ROBOT_XML(obj)                   (G_TYPE_CHECK_INSTANCE_CAST((obj),  ROBOT_TYPE_XML, RobotXml))
#define ROBOT_IS_XML(obj)                (G_TYPE_CHECK_INSTANCE_TYPE ((obj), ROBOT_TYPE_XML))
#define ROBOT_XML_CLASS(klass)           (G_TYPE_CHECK_CLASS_CAST ((klass),  ROBOT_TYPE_XML, RobotXmlClass))
#define ROBOT_IS_XML_CLASS(klass)        (G_TYPE_CHECK_CLASS_TYPE ((klass),  ROBOT_TYPE_XML))
#define ROBOT_XML_GET_CLASS(obj)         (G_TYPE_INSTANCE_GET_CLASS ((obj),  ROBOT_TYPE_XML, RobotXmlClass))

/* get_type prototype: */
GType robot_xml_get_type(void);

/* Structures definitions: */
typedef struct _RobotXml RobotXml;
typedef struct _RobotXmlClass RobotXmlClass;
typedef struct _RobotXmlPrivate RobotXmlPrivate;

enum _RobotXmlNodeType {
	ROBOT_XML_TAG,
	ROBOT_XML_TEXT
};

struct _RobotXml {
	GObject parent_instance;

	RobotXmlPrivate *priv;
};

struct _RobotXmlClass {
	GObjectClass parent_class;
};

RobotXml* robot_xml_new(const gchar *name);
RobotXml* robot_xml_new_with_attributes(const gchar *name, const gchar *first_attr_name, ...);
RobotXml* robot_xml_new_text(const gchar *text, gssize len);

const gchar* robot_xml_get_name(RobotXml *self);
const gchar* robot_xml_get_text(RobotXml *self);
void robot_xml_set_parent(RobotXml *self, RobotXml *parent);
gsize robot_xml_get_text_len(RobotXml *self);
void robot_xml_set_text(RobotXml *self, const gchar *text, gssize text_len);
RobotXml* robot_xml_get_parent(RobotXml *self);
void robot_xml_append_child(RobotXml *self, RobotXml *child);
void robot_xml_prepend_child(RobotXml *self, RobotXml *child);
void robot_xml_insert_child(RobotXml *self, guint idx, RobotXml *child);
void robot_xml_remove_child(RobotXml *self, guint idx);
RobotXml* robot_xml_get_child(RobotXml *self, guint idx);
guint robot_xml_get_children_count(RobotXml *self);

const gchar* robot_xml_get_attribute(RobotXml *self, const gchar* name);
void robot_xml_set_attribute(RobotXml *self, const gchar *name, const gchar *value);
void robot_xml_del_attribute(RobotXml *self, const gchar *name);
/** Get NULL-terminated list of attributes. Use g_strvfree to free one. */
gchar** robot_xml_list_attributes(RobotXml *self);

RobotXml* robot_xml_parse(const gchar* xml, gssize len, GError **error);
gchar* robot_xml_to_string(RobotXml *self, gboolean add_prefix, GError **error);

G_END_DECLS

#endif /* ROBOT_XML_H */
