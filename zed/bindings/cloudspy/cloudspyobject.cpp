#include "cloudspyobject.h"

#define VC_EXTRALEAN
#include <windows.h>
#include "npfunctions.h"

typedef struct _CloudSpyObjectPrivate CloudSpyObjectPrivate;

struct _CloudSpyObjectPrivate
{
  guint foo;
};

G_DEFINE_TYPE (CloudSpyObject, cloud_spy_object, G_TYPE_OBJECT);

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

  return false;
}

static bool
cloud_spy_object_invoke_default (NPObject * npobj, const NPVariant * args, uint32_t arg_count, NPVariant * result)
{
  (void) npobj;
  (void) args;
  (void) arg_count;
  (void) result;

  return false;
}

static bool
cloud_spy_object_has_property (NPObject * npobj, NPIdentifier name)
{
  (void) npobj;
  (void) name;

  return false;
}

static bool
cloud_spy_object_get_property (NPObject * npobj, NPIdentifier name, NPVariant * result)
{
  (void) npobj;
  (void) name;
  (void) result;

  return false;
}

static NPClass cloud_spy_object_np_class =
{
  NP_CLASS_STRUCT_VERSION,
  NULL,
  NULL,
  NULL,
  cloud_spy_object_has_method,
  cloud_spy_object_invoke,
  cloud_spy_object_invoke_default,
  cloud_spy_object_has_property,
  cloud_spy_object_get_property,
  NULL,
  NULL
};

gpointer
cloud_spy_object_get_np_class (void)
{
  return &cloud_spy_object_np_class;
}