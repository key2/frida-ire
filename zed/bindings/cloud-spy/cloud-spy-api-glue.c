#include <glib.h>
#include <glib-object.h>
#include <cloud-spy-object.h>
#include <stdlib.h>
#include <string.h>
#include <gio/gio.h>

typedef struct _CloudSpyDispatcherInvocation CloudSpyDispatcherInvocation;

struct _CloudSpyDispatcherInvocation
{
  GVariant * return_parameters;
  GError * error;
};

static void cloud_spy_dispatcher_invocation_return_value (GDBusMethodInvocation * invocation, GVariant * parameters);
static void cloud_spy_dispatcher_invocation_return_gerror (GDBusMethodInvocation * invocation, const GError * error);

#define g_dbus_method_invocation_return_value(invocation, parameters) \
    cloud_spy_dispatcher_invocation_return_value(invocation, parameters)
#define g_dbus_method_invocation_return_gerror(invocation, error) \
    cloud_spy_dispatcher_invocation_return_gerror(invocation, error)

#pragma warning (push)
#pragma warning (disable: 4090 4100 4133 4706)
#include "cloud-spy-api-marshal.c"
#pragma warning (pop)

#undef g_dbus_method_invocation_return_gerror
#undef g_dbus_method_invocation_return_value

void
cloud_spy_dispatcher_init_with_object (CloudSpyDispatcher * self, CloudSpyObject * obj)
{
  GType type;

  self->target_object = obj;

  type = G_TYPE_FROM_INSTANCE (obj);

  if (g_type_is_a (type, CLOUD_SPY_TYPE_ROOT_API))
  {
    self->methods = (GDBusMethodInfo **) _cloud_spy_root_api_dbus_method_info;
    self->dispatch_func = cloud_spy_root_api_dbus_interface_method_call;
  }
  else
    g_assert_not_reached ();
}

GVariant *
cloud_spy_dispatcher_do_invoke (CloudSpyDispatcher * self, GDBusMethodInfo * method, GVariant * parameters, GError ** error)
{
  CloudSpyDispatcherInvocation invocation = { 0, };

  self->dispatch_func (NULL, NULL, NULL, NULL, method->name, parameters, (GDBusMethodInvocation *) &invocation, &self->target_object);

  g_propagate_error (error, invocation.error);

  return invocation.return_parameters;
}

static void
cloud_spy_dispatcher_invocation_return_value (GDBusMethodInvocation * invocation, GVariant * parameters)
{
  ((CloudSpyDispatcherInvocation *) invocation)->return_parameters = parameters;
}

static void
cloud_spy_dispatcher_invocation_return_gerror (GDBusMethodInvocation * invocation, const GError * error)
{
  ((CloudSpyDispatcherInvocation *) invocation)->error = (GError *) error;
}