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
#pragma warning (disable: 4054 4100)
#include "cloud-spy-api.c"
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

void
cloud_spy_dispatcher_validate_argument_list (CloudSpyDispatcher * self, GVariant * args, GDBusMethodInfo * method, GError ** error)
{
  guint actual_arg_count, expected_arg_count;
  GDBusArgInfo ** ai;
  guint i;

  (void) self;

  actual_arg_count = (args != NULL) ? g_variant_n_children (args) : 0;
  expected_arg_count = 0;
  for (ai = method->in_args; *ai != NULL; ai++)
    expected_arg_count++;
  if (actual_arg_count != expected_arg_count)
    goto count_mismatch;

  for (i = 0; i != expected_arg_count; i++)
  {
    GVariant * arg;
    const GVariantType * actual_type;
    GVariantType * expected_type;
    gboolean types_are_equal;

    arg = g_variant_get_child_value (args, i);
    actual_type = g_variant_get_type (arg);
    expected_type = g_variant_type_new (method->in_args[i]->signature);
    types_are_equal = g_variant_type_equal (actual_type, expected_type);
    g_variant_type_free (expected_type);
    g_variant_unref (arg);

    if (!types_are_equal)
      goto type_mismatch;
  }

  return;

  /* ERRORS */
count_mismatch:
  {
    g_set_error (error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT, "argument count mismatch");
    return;
  }
type_mismatch:
  {
    g_set_error (error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT, "argument type mismatch");
    return;
  }
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