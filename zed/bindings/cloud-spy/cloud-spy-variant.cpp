#include "cloud-spy-variant.h"

typedef struct _CloudSpyVariant CloudSpyVariant;

struct _CloudSpyVariant
{
  NPObject np_object;
  GVariant * variant;
};

static NPObject *
cloud_spy_variant_allocate (NPP npp, NPClass * klass)
{
  CloudSpyVariant * obj;

  (void) npp;
  (void) klass;

  obj = g_slice_new (CloudSpyVariant);
  obj->variant = NULL;

  return &obj->np_object;
}

static void
cloud_spy_variant_deallocate (NPObject * npobj)
{
  CloudSpyVariant * self = reinterpret_cast<CloudSpyVariant *> (npobj);

  if (self->variant != NULL)
    g_variant_unref (self->variant);

  g_slice_free (CloudSpyVariant, self);
}

static void
cloud_spy_variant_invalidate (NPObject * npobj)
{
  (void) npobj;
}

static bool
cloud_spy_variant_has_method (NPObject * npobj, NPIdentifier name)
{
  (void) npobj;

  return strcmp (static_cast<NPString *> (name)->UTF8Characters, "toString") == 0;
}

static bool
cloud_spy_variant_invoke (NPObject * npobj, NPIdentifier name, const NPVariant * args, uint32_t arg_count, NPVariant * result)
{
  CloudSpyVariant * self = reinterpret_cast<CloudSpyVariant *> (npobj);

  (void) args;

  if (strcmp (static_cast<NPString *> (name)->UTF8Characters, "toString") == 0 && arg_count == 0)
  {
    gchar * str;
    guint len;

    /* FIXME: should do this ourself -- this is just a hack: */
    str = g_variant_print (self->variant, FALSE);

    if (str[0] == '(')
      str[0] = '[';

    len = strlen (str);
    if (len != 0 && str[len - 1] == ')')
      str[len - 1] = ']';

    cloud_spy_init_npvariant_with_string (result, str);

    g_free (str);

    return true;
  }
  else
  {
    cloud_spy_nsfuncs->setexception (npobj, "no such method");
    return false;
  }
}

static bool
cloud_spy_variant_invoke_default (NPObject * npobj, const NPVariant * args, uint32_t arg_count, NPVariant * result)
{
  (void) args;
  (void) arg_count;
  (void) result;

  cloud_spy_nsfuncs->setexception (npobj, "invalid operation (invoke_default)");
  return false;
}

static bool
cloud_spy_variant_has_property (NPObject * npobj, NPIdentifier name)
{
  (void) npobj;
  (void) name;

  return false;
}

static bool
cloud_spy_variant_get_property (NPObject * npobj, NPIdentifier name, NPVariant * result)
{
  (void) name;
  (void) result;

  cloud_spy_nsfuncs->setexception (npobj, "get_property not implemented");
  return false;
}

static NPClass cloud_spy_variant_class =
{
  NP_CLASS_STRUCT_VERSION,
  cloud_spy_variant_allocate,
  cloud_spy_variant_deallocate,
  cloud_spy_variant_invalidate,
  cloud_spy_variant_has_method,
  cloud_spy_variant_invoke,
  cloud_spy_variant_invoke_default,
  cloud_spy_variant_has_property,
  cloud_spy_variant_get_property,
  NULL,
  NULL,
  NULL,
  NULL
};

NPObject *
cloud_spy_variant_new (NPP npp, GVariant * variant)
{
  CloudSpyVariant * obj;

  obj = reinterpret_cast<CloudSpyVariant *> (cloud_spy_nsfuncs->createobject (npp, &cloud_spy_variant_class));
  obj->variant = variant;

  return &obj->np_object;
}

NPClass *
cloud_spy_variant_get_class (void)
{
  return &cloud_spy_variant_class;
}