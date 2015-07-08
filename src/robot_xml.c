#include "robot_xml.h"
#include <string.h>
#include <stdarg.h>
#include <stdio.h>


struct _RobotXmlPrivate {
	/* Tag name: */
	gchar *name;
	/* Attributes: */
	GTree *attrs;
	/* Weak reference to parent: */
	GWeakRef* parent;
	/* Array of children: */
	GPtrArray *children;
	/* Text node text: */
	gchar *text;
	/* Text node len: */
	gsize text_len;
};

G_DEFINE_TYPE_WITH_PRIVATE(RobotXml, robot_xml, G_TYPE_OBJECT)

static void dispose(GObject *obj)
{
	RobotXml *self = ROBOT_XML(obj);

	g_ptr_array_free(self->priv->children, TRUE);
	self->priv->children = NULL;

	if (self->priv->parent) {
		g_weak_ref_clear(self->priv->parent);
		self->priv->parent = NULL;
	}
}

static void finalize(GObject *obj)
{
	RobotXml *self = ROBOT_XML(obj);

	g_free(self->priv->name);
	g_tree_unref(self->priv->attrs);
	self->priv = NULL;
}

static void robot_xml_class_init(RobotXmlClass *klass)
{
	GObjectClass *objcls = G_OBJECT_CLASS(klass);
	objcls->dispose = dispose;
	objcls->finalize = finalize;
}

static gint str_compare(gconstpointer v1, gconstpointer v2, gpointer data)
{
	return strcmp(v1, v2);
}

static void robot_xml_init(RobotXml *self)
{
	self->priv = robot_xml_get_instance_private(self);
	self->priv->name = NULL;
	self->priv->children = g_ptr_array_new_full(0, g_object_unref);
	self->priv->attrs = g_tree_new_full(str_compare, NULL, g_free, g_free);
	self->priv->parent = NULL;
	self->priv->text = NULL;
	self->priv->text_len = 0;
}

RobotXml* robot_xml_new(const gchar *name)
{
	RobotXml* self = g_object_new(ROBOT_TYPE_XML, NULL);

	self->priv->name = g_strdup(name);

	return self;
}

RobotXml* robot_xml_new_with_attributes(const gchar *name, const gchar *first_attr_name, ...)
{
	RobotXml *self = robot_xml_new(name);
	va_list args;
	const gchar *nm;
	const gchar *value;

	nm = first_attr_name;
	va_start(args, first_attr_name);
	while (nm) {
		value = va_arg(args, const gchar*);

		robot_xml_set_attribute(self, nm, value);

		nm = va_arg(args, const gchar*);
	}
	va_end(args);

	return self;
}

RobotXml* robot_xml_new_text(const gchar *text, gssize len)
{
	RobotXml *self = robot_xml_new("");

	robot_xml_set_text(self, text, len);

	return self;
}

void robot_xml_set_parent(RobotXml *self, RobotXml *parent)
{
	if (self->priv->parent) {
		g_weak_ref_clear(self->priv->parent);
		self->priv->parent = NULL;
	}

	if (parent) {
		g_object_ref(parent);
		self->priv->parent = g_malloc0(sizeof(GWeakRef));
		g_weak_ref_init(self->priv->parent, parent);
		g_object_unref(parent);
	}
}

RobotXml* robot_xml_get_parent(RobotXml *self)
{
	if (self->priv->parent) {
		return g_weak_ref_get(self->priv->parent);
	}
	return NULL;
}

const gchar* robot_xml_get_name(RobotXml *self)
{
	return self->priv->name;
}

const gchar* robot_xml_get_text(RobotXml *self)
{
	return self->priv->text;
}

gsize robot_xml_get_text_len(RobotXml *self)
{
	return self->priv->text_len;
}

void robot_xml_set_text(RobotXml *self, const gchar *text, gssize text_len)
{
	g_free(self->priv->text);
	if (text_len < 0)
		text_len = strlen(text);

	self->priv->text = g_malloc(text_len + 1);
	memcpy(self->priv->text, text, text_len);
	self->priv->text[text_len] = 0;
	self->priv->text_len = text_len;
}

void robot_xml_append_child(RobotXml *self, RobotXml *child)
{
	if (!child)
		return;

	g_ptr_array_add(self->priv->children, child);
	robot_xml_set_parent(child, self);
}

void robot_xml_prepend_child(RobotXml *self, RobotXml *child)
{
	if (!child)
		return;

	g_ptr_array_insert(self->priv->children, 0, child);
	robot_xml_set_parent(child, self);
}

void robot_xml_insert_child(RobotXml *self, guint idx, RobotXml *child)
{
	if (!child)
		return;
	if (idx >= self->priv->children->len)
		return;

	g_ptr_array_insert(self->priv->children, idx, child);
	robot_xml_set_parent(child, self);
}

void robot_xml_remove_child(RobotXml *self, guint idx)
{
	g_ptr_array_remove_index(self->priv->children, idx);
}

RobotXml* robot_xml_get_child(RobotXml *self, guint idx)
{
	if (idx >= self->priv->children->len)
		return NULL;
	return g_ptr_array_index(self->priv->children, idx);
}

guint robot_xml_get_children_count(RobotXml *self)
{
	return self->priv->children->len;
}

const gchar* robot_xml_get_attribute(RobotXml *self, const gchar* name)
{
	return g_tree_lookup(self->priv->attrs, name);
}

void robot_xml_set_attribute(RobotXml *self, const gchar *name, const gchar *value)
{
	if (!name || !value)
		return;
	g_tree_insert(self->priv->attrs, g_strdup(name), g_strdup(value));
}

void robot_xml_del_attribute(RobotXml *self, const gchar *name)
{
	g_tree_remove(self->priv->attrs, name);
}

struct strlist {
	gchar **array;
	guint cnt;
	guint allocated;
};

static gboolean traverse(gpointer key, gpointer val, gpointer data)
{
	struct strlist *lst = data;
	if (lst->cnt + 2 >= lst->allocated) {
		lst->allocated += 16;
		lst->array = g_realloc(lst->array, lst->allocated * sizeof(gchar*));
	}
	lst->array[lst->cnt++] = g_strdup(key);
	return TRUE;
}

/** Get NULL-terminated list of attributes. Use g_strfreev to free one. */
gchar** robot_xml_list_attributes(RobotXml *self)
{
	struct strlist lst = { g_malloc(sizeof(gchar*) * 16), 0, 16 };

	g_tree_foreach(self->priv->attrs, traverse, &lst);

	lst.array[lst.cnt] = NULL;

	return lst.array;
}

struct xml_pctx {
	RobotXml *root;
	RobotXml *x;
};

/* Parse callbacks: */
static void x_start_element(
		GMarkupParseContext *context,
		const gchar         *element_name,
		const gchar        **attribute_names,
		const gchar        **attribute_values,
		gpointer             user_data,
		GError             **error)
{
	RobotXml *xml = robot_xml_new(element_name);
	struct xml_pctx *ctx = user_data;
	int i;

	for (i = 0; attribute_names[i] && attribute_values[i]; i++)
		robot_xml_set_attribute(xml, attribute_names[i], attribute_values[i]);

	if (ctx->root) {
		robot_xml_append_child(ctx->x, xml);
		ctx->x = xml;
	} else {
		ctx->root = xml;
		ctx->x = xml;
	}

	printf("START: %s\n", element_name);
}

static void x_end_element(
		GMarkupParseContext *context,
		const gchar         *element_name,
		gpointer             user_data,
		GError             **error)
{
	struct xml_pctx *ctx = user_data;

	g_return_if_fail(strcmp(robot_xml_get_name(ctx->x), element_name) == 0);
	g_return_if_fail(ctx->x != NULL);

	ctx->x = robot_xml_get_parent(ctx->x);
	if (ctx->x)
		g_object_unref(ctx->x);

	printf("END: %s\n", element_name);
}

static void x_text(
		GMarkupParseContext *context,
		const gchar         *text,
		gsize                text_len,
		gpointer             user_data,
		GError             **error)
{
	struct xml_pctx *ctx = user_data;
	RobotXml *xml;
	gsize i;

	g_return_if_fail(ctx->x != NULL);

	for (i = 0; i < text_len; i++)
		if (!g_ascii_isspace(text[i]))
			break;
	if (i == text_len) /* It is empty string */
		return;

	xml = robot_xml_new_text(text, text_len);
	robot_xml_append_child(ctx->x, xml);

	printf("TEXT:\n");
}

static void x_passthrough(
		GMarkupParseContext *context,
		const gchar         *passthrough_text,
		gsize                text_len,
		gpointer             user_data,
		GError             **error)
{
	/* Do nothing */
}

static void x_error(
		GMarkupParseContext *context,
		GError              *error,
		gpointer             user_data)
{
	/* Do nothing */
}

static GMarkupParser x_parser = {
	x_start_element,
	x_end_element,
	x_text,
	x_passthrough,
	x_error
};

RobotXml* robot_xml_parse(const gchar* xml, gssize len, GError **error)
{
	struct xml_pctx pctx = { NULL, NULL };
	GMarkupParseContext *ctx = g_markup_parse_context_new(&x_parser, G_MARKUP_PREFIX_ERROR_POSITION, &pctx, NULL);

	if (!g_markup_parse_context_parse(ctx, xml, len, error) ||
			!g_markup_parse_context_end_parse(ctx, error)) {
		g_markup_parse_context_free(ctx);
		if (pctx.root)
			g_object_unref(pctx.root);
		return NULL;
	}

	g_markup_parse_context_free(ctx);

	return pctx.root;
}

static void p_offset(GString *s, guint offset)
{
	guint i;

	for (i = 0; i < offset; i++)
		g_string_append(s, "\t");
}

static gboolean node_to_string(GString *s, RobotXml *node, guint offset, GError **error);
static gboolean children_xml(GString *s, RobotXml *node, guint offset, GError **error)
{
	guint cnt = robot_xml_get_children_count(node);
	guint i;
	RobotXml *c;

	for (i = 0; i < cnt; i++) {
		c = robot_xml_get_child(node, i);
		if (c)
			if (!node_to_string(s, c, offset, error))
				return FALSE;
	}

	return TRUE;
}

static void attributes_xml(GString *s, RobotXml *node)
{
	gchar **names;
	const char *val;
	guint i;

	names = robot_xml_list_attributes(node);
	if (!names) {
		return;
	}

	for (i = 0; names[i]; i++) {
		val = robot_xml_get_attribute(node, names[i]);
		if (val) {
			/* TODO: escape */
			g_string_append_printf(s, " %s=\"%s\"", names[i], val);
		}
	}

	g_strfreev(names);
}

static gboolean node_to_string(GString *s, RobotXml *node, guint offset, GError **error)
{
	const gchar *txt;
	gsize txt_len;
	guint ccnt;

	txt = robot_xml_get_text(node);
	txt_len = robot_xml_get_text_len(node);
	ccnt = robot_xml_get_children_count(node);

	if (txt && ccnt > 0) {
		g_set_error(error, G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT, "Text node with attributes");
		return FALSE;
	}

	if (txt) {
		g_string_append_len(s, txt, txt_len);
	} else {
		p_offset(s, offset);
		g_string_append_printf(s, "<%s", robot_xml_get_name(node));
		attributes_xml(s, node);
		if (ccnt == 0) {
			g_string_append_printf(s, "/>\n");
		} else {
			g_string_append_printf(s, ">\n");

			if (!children_xml(s, node, offset + 1, error))
				return FALSE;

			p_offset(s, offset);
			g_string_append_printf(s, "</%s>\n", robot_xml_get_name(node));
		}
	}

	return TRUE;
}

gchar* robot_xml_to_string(RobotXml *self, gboolean add_prefix, GError **error)
{
	GString *s = g_string_new("");

	if (add_prefix)
		g_string_printf(s, "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n");

	if (!node_to_string(s, self, 0, error)) {
		g_string_free(s, TRUE);
		return NULL;
	}

	return g_string_free(s, FALSE);
}


