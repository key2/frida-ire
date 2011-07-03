#include <glib.h>
#include <glib-object.h>
#include <cloud-spy-object.h>
#include <stdlib.h>
#include <string.h>
#include <gio/gio.h>

typedef struct _CloudSpyDispatcherInvocation CloudSpyDispatcherInvocation;

struct _CloudSpyDispatcherInvocation
{
  GDBusMessage * call_message;
  GDBusMessage * reply_message;
  GError * error;
};

static GDBusMessage * cloud_spy_dispatcher_invocation_get_message (GDBusMethodInvocation * invocation);
static GDBusConnection * cloud_spy_dispatcher_invocation_get_connection (GDBusMethodInvocation * invocation);
static gboolean cloud_spy_dispatcher_connection_send_message (GDBusConnection * connection, GDBusMessage * message, GDBusSendMessageFlags flags, volatile guint32 * out_serial, GError ** error);
static void cloud_spy_dispatcher_invocation_return_gerror (GDBusMethodInvocation * invocation, const GError * error);

#define g_dbus_method_invocation_get_message(invocation) \
    cloud_spy_dispatcher_invocation_get_message (invocation)
#define g_dbus_method_invocation_get_connection(invocation) \
    cloud_spy_dispatcher_invocation_get_connection (invocation)
#define g_dbus_connection_send_message(connection, message, flags, out_serial, error) \
    cloud_spy_dispatcher_connection_send_message (connection, message, flags, out_serial, error)
#define g_dbus_method_invocation_return_gerror(invocation, error) \
    cloud_spy_dispatcher_invocation_return_gerror (invocation, error)

#pragma warning (push)
#pragma warning (disable: 4054 4100)
#include "cloud-spy-api.c"
#pragma warning (pop)

#undef g_dbus_method_invocation_get_message
#undef g_dbus_method_invocation_get_connection
#undef g_dbus_connection_send_message
#undef g_dbus_method_invocation_return_gerror

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
  GVariant * result = NULL;

  invocation.call_message = g_dbus_message_new_method_call (NULL, "/org/boblycat/frida/Foo", "org.boblycat.Frida.Foo", method->name);
  g_dbus_message_set_serial (invocation.call_message, 1);

  self->dispatch_func (NULL, NULL, NULL, NULL, method->name, parameters, (GDBusMethodInvocation *) &invocation, &self->target_object);

  g_propagate_error (error, invocation.error);

  if (invocation.reply_message != NULL)
  {
    result = g_variant_ref (g_dbus_message_get_body (invocation.reply_message));
    g_object_unref (invocation.reply_message);
  }

  g_object_unref (invocation.call_message);

  return result;
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

static GDBusMessage *
cloud_spy_dispatcher_invocation_get_message (GDBusMethodInvocation * invocation)
{
  return ((CloudSpyDispatcherInvocation *) invocation)->call_message;
}

static GDBusConnection *
cloud_spy_dispatcher_invocation_get_connection (GDBusMethodInvocation * invocation)
{
  return (GDBusConnection *) invocation;
}

static gboolean
cloud_spy_dispatcher_connection_send_message (GDBusConnection * connection, GDBusMessage * message, GDBusSendMessageFlags flags, volatile guint32 * out_serial, GError ** error)
{
  (void) flags;
  (void) out_serial;
  (void) error;

  g_object_ref (message);
  ((CloudSpyDispatcherInvocation *) connection)->reply_message = message;

  return TRUE;
}

static void
cloud_spy_dispatcher_invocation_return_gerror (GDBusMethodInvocation * invocation, const GError * error)
{
  ((CloudSpyDispatcherInvocation *) invocation)->error = (GError *) error;
}