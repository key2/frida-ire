#include "cloud-spy-object.h"

#include "cloud-spy.h"
#include "cloud-spy-plugin.h"
#include "cloud-spy-variant.h"

#include "npfunctions.h"

typedef struct _CloudSpyObjectPrivate CloudSpyObjectPrivate;

typedef struct _CloudSpyNPObject CloudSpyNPObject;
typedef struct _CloudSpyNPObjectClass CloudSpyNPObjectClass;

struct _CloudSpyObjectPrivate
{
  NPP npp;
  CloudSpyDispatcher * dispatcher;
};

struct _CloudSpyNPObject
{
  NPObject np_object;
  CloudSpyObject * g_object;
};

struct _CloudSpyNPObjectClass
{
  NPClass np_class;
  GType g_type;
  CloudSpyObjectClass * g_class;
};

static void cloud_spy_object_constructed (GObject * object);
static void cloud_spy_object_dispose (GObject * object);

GVariant * cloud_spy_object_parse_argument_list (const NPVariant * args, guint arg_count, GError ** err);

G_DEFINE_TYPE (CloudSpyObject, cloud_spy_object, G_TYPE_OBJECT);

static void
cloud_spy_object_class_init (CloudSpyObjectClass * klass)
{
  GObjectClass * object_class = G_OBJECT_CLASS (klass);

  g_type_class_add_private (klass, sizeof (CloudSpyObjectPrivate));

  object_class->constructed = cloud_spy_object_constructed;
  object_class->dispose = cloud_spy_object_dispose;
}

static void
cloud_spy_object_init (CloudSpyObject * self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, CLOUD_SPY_TYPE_OBJECT, CloudSpyObjectPrivate);
}

static void
cloud_spy_object_constructed (GObject * object)
{
  CloudSpyObject * self = CLOUD_SPY_OBJECT (object);

  self->priv->dispatcher = cloud_spy_dispatcher_new_for_object (self);

  if (G_OBJECT_CLASS (cloud_spy_object_parent_class)->constructed != NULL)
    G_OBJECT_CLASS (cloud_spy_object_parent_class)->constructed (object);
}

static void
cloud_spy_object_dispose (GObject * object)
{
  CloudSpyObjectPrivate * priv = CLOUD_SPY_OBJECT (object)->priv;

  if (priv->dispatcher != NULL)
  {
    g_object_unref (priv->dispatcher);
    priv->dispatcher = NULL;
  }

  G_OBJECT_CLASS (cloud_spy_object_parent_class)->dispose (object);
}

static NPObject *
cloud_spy_object_allocate (NPP npp, NPClass * klass)
{
  CloudSpyNPObjectClass * np_class = reinterpret_cast<CloudSpyNPObjectClass *> (klass);
  CloudSpyNPObject * obj;

  (void) npp;

  obj = g_slice_new (CloudSpyNPObject);
  obj->g_object = CLOUD_SPY_OBJECT (g_object_new (np_class->g_type, NULL));
  obj->g_object->priv->npp = npp;

  return &obj->np_object;
}

static void
cloud_spy_object_deallocate (NPObject * npobj)
{
  CloudSpyNPObject * np_object = reinterpret_cast<CloudSpyNPObject *> (npobj);

  g_assert (np_object->g_object != NULL);
  g_object_unref (np_object->g_object);
  g_slice_free (CloudSpyNPObject, np_object);
}

static void
cloud_spy_object_invalidate (NPObject * npobj)
{
  (void) npobj;
}

static bool
cloud_spy_object_has_method (NPObject * npobj, NPIdentifier name)
{
  CloudSpyObjectPrivate * priv = reinterpret_cast<CloudSpyNPObject *> (npobj)->g_object->priv;

  return cloud_spy_dispatcher_has_method (priv->dispatcher, static_cast<NPString *> (name)->UTF8Characters) != NULL;
}

static bool
cloud_spy_object_invoke (NPObject * npobj, NPIdentifier name, const NPVariant * args, uint32_t arg_count, NPVariant * result)
{
  CloudSpyObjectPrivate * priv = reinterpret_cast<CloudSpyNPObject *> (npobj)->g_object->priv;
  GVariant * args_var, * result_var;
  GError * err = NULL;

  (void) args;
  (void) arg_count;

  args_var = cloud_spy_object_parse_argument_list (args, arg_count, &err);
  if (args_var == NULL)
    goto invoke_failed;

  result_var = cloud_spy_dispatcher_invoke (priv->dispatcher, static_cast<NPString *> (name)->UTF8Characters, args_var, &err);
  if (err != NULL)
    goto invoke_failed;

  if (result_var == NULL)
    VOID_TO_NPVARIANT (*result);
  else
    OBJECT_TO_NPVARIANT (cloud_spy_variant_new (priv->npp, result_var), *result);

  g_variant_unref (args_var);

  return true;

invoke_failed:
  {
    g_variant_unref (args_var);
    cloud_spy_nsfuncs->setexception (npobj, err->message);
    g_clear_error (&err);
    return false;
  }
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
  CloudSpyNPObjectClass * np_class = reinterpret_cast<CloudSpyNPObjectClass *> (npobj->_class);
  NPString * name_str = static_cast<NPString *> (name);

  return g_object_class_find_property (G_OBJECT_CLASS (np_class->g_class), name_str->UTF8Characters) != NULL;
}

static bool
cloud_spy_object_get_property (NPObject * npobj, NPIdentifier name, NPVariant * result)
{
  CloudSpyNPObject * np_object = reinterpret_cast<CloudSpyNPObject *> (npobj);
  CloudSpyNPObjectClass * np_class = reinterpret_cast<CloudSpyNPObjectClass *> (npobj->_class);
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
      cloud_spy_init_npvariant_with_string (result, g_value_get_string (&val));
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

GVariant *
cloud_spy_object_parse_argument_list (const NPVariant * args, guint arg_count, GError ** err)
{
  GVariantBuilder builder;
  guint i;

  if (arg_count == 0)
    return NULL;

  g_variant_builder_init (&builder, G_VARIANT_TYPE_ARRAY);

  for (i = 0; i != arg_count; i++)
  {
    const NPVariant * var = &args[i];

    switch (var->type)
    {
      case NPVariantType_Bool:
        g_variant_builder_add_value (&builder, g_variant_new_boolean (NPVARIANT_TO_BOOLEAN (*var)));
        break;
      case NPVariantType_Int32:
        g_variant_builder_add_value (&builder, g_variant_new_int32 (NPVARIANT_TO_INT32 (*var)));
        break;
      case NPVariantType_Double:
        g_variant_builder_add_value (&builder, g_variant_new_double (NPVARIANT_TO_DOUBLE (*var)));
        break;
      case NPVariantType_String:
      {
        gchar * str;

        str = (gchar *) g_malloc (var->value.stringValue.UTF8Length + 1);
        memcpy (str, var->value.stringValue.UTF8Characters, var->value.stringValue.UTF8Length);
        str[var->value.stringValue.UTF8Length] = '\0';

        g_variant_builder_add_value (&builder, g_variant_new_string (str));

        g_free (str);

        break;
      }
      case NPVariantType_Object:
      case NPVariantType_Void:
      case NPVariantType_Null:
        goto invalid_type;
    }
  }

  return g_variant_builder_end (&builder);

invalid_type:
  {
    g_variant_builder_clear (&builder);
    g_set_error (err, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT, "argument has invalid type");
    return NULL;
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
  NULL,
  NULL,
  NULL
};

G_LOCK_DEFINE_STATIC (np_class_by_gobject_class);
static GHashTable * np_class_by_gobject_class = NULL;

void
cloud_spy_object_type_init (void)
{
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
}

gpointer
cloud_spy_object_type_get_np_class (GType gtype)
{
  CloudSpyObjectClass * gobject_class;
  CloudSpyNPObjectClass * np_class;

  g_assert (g_type_is_a (gtype, CLOUD_SPY_TYPE_OBJECT));

  gobject_class = CLOUD_SPY_OBJECT_CLASS (g_type_class_ref (gtype));

  G_LOCK (np_class_by_gobject_class);

  np_class = static_cast<CloudSpyNPObjectClass *> (g_hash_table_lookup (np_class_by_gobject_class, gobject_class));
  if (np_class == NULL)
  {
    np_class = g_new (CloudSpyNPObjectClass, 1);

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