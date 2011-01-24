#include "cloud-spy-object.h"

#define VC_EXTRALEAN
#include <windows.h>
#include "npfunctions.h"

typedef struct _CloudSpyObjectPrivate CloudSpyObjectPrivate;

typedef struct _CloudSpyNPObject CloudSpyNPObject;
typedef struct _CloudSpyNPClass CloudSpyNPClass;

struct _CloudSpyObjectPrivate
{
  guint foo;
};

struct _CloudSpyNPObject
{
  NPObject np_object;
  CloudSpyObject * g_object;
};

struct _CloudSpyNPClass
{
  NPClass np_class;
  GType g_type;
  CloudSpyObjectClass * g_class;
};

G_DEFINE_TYPE (CloudSpyObject, cloud_spy_object, G_TYPE_OBJECT);

static NPNetscapeFuncs * cloud_spy_nsfuncs = NULL;

static void
cloud_spy_object_class_init (CloudSpyObjectClass * klass)
{
  g_type_class_add_private (klass, sizeof (CloudSpyObjectPrivate));
}

static void
cloud_spy_object_init (CloudSpyObject * self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, CLOUD_SPY_TYPE_OBJECT, CloudSpyObjectPrivate);
}

static NPObject *
cloud_spy_object_allocate (NPP npp, NPClass * klass)
{
  CloudSpyNPClass * np_class = reinterpret_cast<CloudSpyNPClass *> (klass);
  CloudSpyNPObject * obj;

  (void) npp;

  obj = g_slice_new (CloudSpyNPObject);
  obj->g_object = CLOUD_SPY_OBJECT (g_object_new (np_class->g_type, NULL));

  return &obj->np_object;
}

static void
cloud_spy_object_deallocate (NPObject * npobj)
{
  CloudSpyNPObject * np_object = reinterpret_cast<CloudSpyNPObject *> (npobj);

  g_assert (np_object->g_object == NULL);
  g_slice_free (CloudSpyNPObject, np_object);
}

static void
cloud_spy_object_invalidate (NPObject * npobj)
{
  CloudSpyNPObject * np_object = reinterpret_cast<CloudSpyNPObject *> (npobj);

  g_assert (np_object->g_object != NULL);
  g_object_unref (np_object->g_object);
  np_object->g_object = NULL;
}

static bool
cloud_spy_object_has_method (NPObject * npobj, NPIdentifier name)
{
  (void) npobj;
  (void) name;

  return false;
}

static bool
cloud_spy_object_invoke (NPObject * npobj, NPIdentifier name, const NPVariant * args, uint32_t arg_count, NPVariant * result)
{
  (void) npobj;
  (void) name;
  (void) args;
  (void) arg_count;
  (void) result;

  cloud_spy_nsfuncs->setexception (npobj, "invoke() is not yet implemented");
  return false;
}

static bool
cloud_spy_object_invoke_default (NPObject * npobj, const NPVariant * args, uint32_t arg_count, NPVariant * result)
{
  (void) npobj;
  (void) args;
  (void) arg_count;
  (void) result;

  cloud_spy_nsfuncs->setexception (npobj, "invoke_default() is not yet implemented");
  return false;
}

static bool
cloud_spy_object_has_property (NPObject * npobj, NPIdentifier name)
{
  CloudSpyNPClass * np_class = reinterpret_cast<CloudSpyNPClass *> (npobj->_class);
  NPString * name_str = static_cast<NPString *> (name);

  return g_object_class_find_property (G_OBJECT_CLASS (np_class->g_class), name_str->UTF8Characters) != NULL;
}

static bool
cloud_spy_object_get_property (NPObject * npobj, NPIdentifier name, NPVariant * result)
{
  CloudSpyNPObject * np_object = reinterpret_cast<CloudSpyNPObject *> (npobj);
  CloudSpyNPClass * np_class = reinterpret_cast<CloudSpyNPClass *> (npobj->_class);
  NPString * name_str = static_cast<NPString *> (name);
  GParamSpec * spec;
  GValue val = { 0, };

  spec = g_object_class_find_property (G_OBJECT_CLASS (np_class->g_class), name_str->UTF8Characters);
  if (spec == NULL)
    goto no_such_property;

  g_value_init (&val, spec->value_type);
  g_object_get_property (G_OBJECT (np_object->g_object), name_str->UTF8Characters, &val);

  switch (spec->value_type)
  {
    case G_TYPE_BOOLEAN:
      BOOLEAN_TO_NPVARIANT (g_value_get_boolean (&val), *result);
      break;
    case G_TYPE_INT:
      INT32_TO_NPVARIANT (g_value_get_int (&val), *result);
      break;
    case G_TYPE_FLOAT:
      DOUBLE_TO_NPVARIANT ((double) g_value_get_float (&val), *result);
      break;
    case G_TYPE_DOUBLE:
      DOUBLE_TO_NPVARIANT (g_value_get_double (&val), *result);
      break;
    case G_TYPE_STRING:
    {
      const gchar * str = g_value_get_string (&val);
      guint len = strlen (str);
      NPUTF8 * str_copy = static_cast<NPUTF8 *> (cloud_spy_nsfuncs->memalloc (len));
      memcpy (str_copy, str, len);
      STRINGN_TO_NPVARIANT (str_copy, len, *result);
      break;
    }
    default:
      goto cannot_marshal;
  }

  g_value_unset (&val);

  return true;

  /* ERRORS */
no_such_property:
  {
    cloud_spy_nsfuncs->setexception (npobj, "no such property");
    return false;
  }
cannot_marshal:
  {
    cloud_spy_nsfuncs->setexception (npobj, "type cannot be marshalled");
    return false;
  }
}

static NPClass cloud_spy_object_template_np_class =
{
  NP_CLASS_STRUCT_VERSION,
  cloud_spy_object_allocate,
  cloud_spy_object_deallocate,
  cloud_spy_object_invalidate,
  cloud_spy_object_has_method,
  cloud_spy_object_invoke,
  cloud_spy_object_invoke_default,
  cloud_spy_object_has_property,
  cloud_spy_object_get_property,
  NULL,
  NULL
};

G_LOCK_DEFINE_STATIC (np_class_by_gobject_class);
static GHashTable * np_class_by_gobject_class = NULL;

void
cloud_spy_object_type_init (gpointer nsfuncs)
{
  cloud_spy_nsfuncs = static_cast<NPNetscapeFuncs *> (nsfuncs);

  np_class_by_gobject_class = g_hash_table_new_full (g_direct_hash, g_direct_equal, g_type_class_unref, g_free);
}

void
cloud_spy_object_type_deinit (void)
{
  if (np_class_by_gobject_class != NULL)
  {
    g_hash_table_unref (np_class_by_gobject_class);
    np_class_by_gobject_class = NULL;
  }

  cloud_spy_nsfuncs = NULL;
}

gpointer
cloud_spy_object_type_get_np_class (GType gtype)
{
  CloudSpyObjectClass * gobject_class;
  CloudSpyNPClass * np_class;

  g_assert (g_type_is_a (gtype, CLOUD_SPY_TYPE_OBJECT));

  gobject_class = CLOUD_SPY_OBJECT_CLASS (g_type_class_ref (gtype));

  G_LOCK (np_class_by_gobject_class);

  np_class = static_cast<CloudSpyNPClass *> (g_hash_table_lookup (np_class_by_gobject_class, gobject_class));
  if (np_class == NULL)
  {
    np_class = g_new (CloudSpyNPClass, 1);

    memcpy (np_class, &cloud_spy_object_template_np_class, sizeof (cloud_spy_object_template_np_class));

    np_class->g_type = gtype;
    g_type_class_ref (gtype);
    np_class->g_class = gobject_class;

    g_hash_table_insert (np_class_by_gobject_class, np_class->g_class, np_class);
  }

  G_UNLOCK (np_class_by_gobject_class);

  g_type_class_unref (gobject_class);

  return np_class;
}