/*
 * Copyright (C) 2010-2011 Ole Andr� Vadla Ravn�s <ole.andre.ravnas@tillitech.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "gumscript.h"

#include "guminterceptor.h"
#include "gumsymbolutil.h"
#include "gumtls.h"
#ifdef G_OS_WIN32
# include "backend-windows/gumwinexceptionhook.h"
#endif

#include <gio/gio.h>
#include <setjmp.h>
#include <string.h>
#include <v8.h>
#include <wchar.h>
#ifdef G_OS_WIN32
# define VC_EXTRALEAN
# include <windows.h>
#else
# include <signal.h>
# if defined (HAVE_DARWIN) && defined (HAVE_ARM)
#  define GUM_INVALID_ACCESS_SIGNAL SIGBUS
# else
#  define GUM_INVALID_ACCESS_SIGNAL SIGSEGV
# endif
#endif

#define GUM_SCRIPT_RUNTIME_SOURCE_LINE_COUNT 1

using namespace v8;

typedef struct _GumMessageSink GumMessageSink;
typedef struct _GumScriptAttachEntry GumScriptAttachEntry;
typedef struct _GumMemoryAccessScope GumMemoryAccessScope;
typedef guint GumMemoryValueType;

struct _GumScriptPrivate
{
  GumInterceptor * interceptor;

  Persistent<Context> context;
  Persistent<Script> raw_script;
  Persistent<ObjectTemplate> args_template;

  GumScriptMessageHandler message_handler_func;
  gpointer message_handler_data;
  GDestroyNotify message_handler_notify;

  GMutex * event_mutex;
  GCond * event_cond;
  guint event_count;

  GumMessageSink * incoming_message_sink;

  GQueue * attach_entries;
  GQueue * heap_blocks;
};

struct _GumMessageSink
{
  Persistent<Function> callback;
  Persistent<Object> receiver;
};

struct _GumScriptAttachEntry
{
  Persistent<Function> on_enter;
  Persistent<Function> on_leave;
  Persistent<Object> receiver;
};

struct _GumMemoryAccessScope
{
  gboolean exception_occurred;
  gpointer address;
  jmp_buf env;
};

enum _GumMemoryValueType
{
  GUM_MEMORY_VALUE_SWORD,
  GUM_MEMORY_VALUE_UWORD,
  GUM_MEMORY_VALUE_S8,
  GUM_MEMORY_VALUE_U8,
  GUM_MEMORY_VALUE_S16,
  GUM_MEMORY_VALUE_U16,
  GUM_MEMORY_VALUE_S32,
  GUM_MEMORY_VALUE_U32,
  GUM_MEMORY_VALUE_S64,
  GUM_MEMORY_VALUE_U64,
  GUM_MEMORY_VALUE_UTF8_STRING,
  GUM_MEMORY_VALUE_UTF16_STRING,
  GUM_MEMORY_VALUE_ANSI_STRING
};

static void gum_script_listener_iface_init (gpointer g_iface,
    gpointer iface_data);

static void gum_script_dispose (GObject * object);
static void gum_script_finalize (GObject * object);
static void gum_script_create_context (GumScript * self);

static Handle<Value> gum_script_on_console_log (const Arguments & args);
static Handle<Value> gum_script_on_send (const Arguments & args);
static Handle<Value> gum_script_on_set_incoming_message_callback (
    const Arguments & args);
static Handle<Value> gum_script_on_wait_for_event (const Arguments & args);
static GumMessageSink * gum_message_sink_new (Handle<Function> callback,
    Handle<Object> receiver);
static void gum_message_sink_free (GumMessageSink * sink);
static void gum_message_sink_handle_message (GumMessageSink * self,
    const gchar * msg);
static Handle<Value> gum_script_on_process_find_module_export_by_name (
    const Arguments & args);
static Handle<Value> gum_script_on_interceptor_attach (const Arguments & args);
static gboolean gum_script_attach_callbacks_get (Handle<Object> callbacks,
    const gchar * name, Local<Function> * callback_function);
static Handle<Value> gum_script_on_int32_cast (const Arguments & args);
#ifdef G_OS_WIN32
static gboolean gum_script_memory_on_exception (
    EXCEPTION_RECORD * exception_record, CONTEXT * context,
    gpointer user_data);
#else
static void gum_script_memory_on_invalid_access (int sig, siginfo_t * siginfo,
    void * context);
#endif
static Handle<Value> gum_script_on_memory_read_sword (const Arguments & args);
static Handle<Value> gum_script_on_memory_read_uword (const Arguments & args);
static Handle<Value> gum_script_on_memory_read_s8 (const Arguments & args);
static Handle<Value> gum_script_on_memory_read_u8 (const Arguments & args);
static Handle<Value> gum_script_on_memory_read_s16 (const Arguments & args);
static Handle<Value> gum_script_on_memory_read_u16 (const Arguments & args);
static Handle<Value> gum_script_on_memory_read_s32 (const Arguments & args);
static Handle<Value> gum_script_on_memory_read_u32 (const Arguments & args);
static Handle<Value> gum_script_on_memory_read_s64 (const Arguments & args);
static Handle<Value> gum_script_on_memory_read_u64 (const Arguments & args);
static Handle<Value> gum_script_on_memory_read_utf8_string (
    const Arguments & args);
static Handle<Value> gum_script_on_memory_read_utf16_string (
    const Arguments & args);
#ifdef G_OS_WIN32
static Handle<Value> gum_script_on_memory_read_ansi_string (
    const Arguments & args);
static Handle<Value> gum_script_on_memory_alloc_ansi_string (
    const Arguments & args);
static gchar * gum_ansi_string_to_utf8 (const gchar * str_ansi, gint length);
static gchar * gum_ansi_string_from_utf8 (const gchar * str_utf8);
#endif
static Handle<Value> gum_script_on_memory_alloc_utf8_string (
    const Arguments & args);
static Handle<Value> gum_script_on_memory_alloc_utf16_string (
    const Arguments & args);

static void gum_script_on_enter (GumInvocationListener * listener,
    GumInvocationContext * context);
static void gum_script_on_leave (GumInvocationListener * listener,
    GumInvocationContext * context);

static Handle<Value> gum_script_args_on_get_nth (uint32_t index,
    const AccessorInfo & info);
static Handle<Value> gum_script_args_on_set_nth (uint32_t index,
    Local<Value> value, const AccessorInfo & info);

G_DEFINE_TYPE_EXTENDED (GumScript,
                        gum_script,
                        G_TYPE_OBJECT,
                        0,
                        G_IMPLEMENT_INTERFACE (GUM_TYPE_INVOCATION_LISTENER,
                            gum_script_listener_iface_init));

G_LOCK_DEFINE_STATIC (gum_memaccess);
static guint gum_memaccess_refcount = 0;
static GumTlsKey gum_memaccess_scope_tls;
#ifndef G_OS_WIN32
static struct sigaction gum_memaccess_old_action;
#endif

static const gchar * gum_script_runtime_source =
#include "gumscript-runtime.h"
;

static void
gum_script_class_init (GumScriptClass * klass)
{
  GObjectClass * object_class = G_OBJECT_CLASS (klass);

  g_type_class_add_private (klass, sizeof (GumScriptPrivate));

  object_class->dispose = gum_script_dispose;
  object_class->finalize = gum_script_finalize;
}

static void
gum_script_listener_iface_init (gpointer g_iface,
                                gpointer iface_data)
{
  GumInvocationListenerIface * iface = (GumInvocationListenerIface *) g_iface;

  (void) iface_data;

  iface->on_enter = gum_script_on_enter;
  iface->on_leave = gum_script_on_leave;
}

static void
gum_script_init (GumScript * self)
{
  GumScriptPrivate * priv;

  priv = self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
      GUM_TYPE_SCRIPT, GumScriptPrivate);

  priv->interceptor = gum_interceptor_obtain ();

  priv->event_mutex = g_mutex_new ();
  priv->event_cond = g_cond_new ();

  priv->attach_entries = g_queue_new ();
  priv->heap_blocks = g_queue_new ();

  G_LOCK (gum_memaccess);
  if (gum_memaccess_refcount++ == 0)
  {
    GUM_TLS_KEY_INIT (&gum_memaccess_scope_tls);

#ifndef G_OS_WIN32
    struct sigaction action;
    action.sa_sigaction = gum_script_memory_on_invalid_access;
    sigemptyset (&action.sa_mask);
    action.sa_flags = SA_SIGINFO;
    sigaction (GUM_INVALID_ACCESS_SIGNAL, &action, &gum_memaccess_old_action);
#endif
  }
  G_UNLOCK (gum_memaccess);

#ifdef G_OS_WIN32
  gum_win_exception_hook_add (gum_script_memory_on_exception, self);
#endif

  gum_script_create_context (self);
}

static void
gum_script_dispose (GObject * object)
{
  GumScript * self = GUM_SCRIPT (object);
  GumScriptPrivate * priv = self->priv;

#ifdef G_OS_WIN32
  gum_win_exception_hook_remove (gum_script_memory_on_exception);
#endif

  if (priv->interceptor != NULL)
  {
    gum_script_unload (self);

    g_object_unref (priv->interceptor);
    priv->interceptor = NULL;

    gum_message_sink_free (priv->incoming_message_sink);
    priv->incoming_message_sink = NULL;

    while (!g_queue_is_empty (priv->attach_entries))
    {
      GumScriptAttachEntry * entry = static_cast<GumScriptAttachEntry *> (
          g_queue_pop_tail (priv->attach_entries));
      entry->on_enter.Clear ();
      entry->on_leave.Clear ();
      entry->receiver.Clear ();
      g_slice_free (GumScriptAttachEntry, entry);
    }

    while (!g_queue_is_empty (priv->heap_blocks))
      g_free (g_queue_pop_tail (priv->heap_blocks));

    priv->args_template.Dispose ();
    priv->args_template.Clear ();
    priv->raw_script.Dispose ();
    priv->raw_script.Clear ();
    priv->context.Dispose ();
    priv->context.Clear ();
  }

  G_OBJECT_CLASS (gum_script_parent_class)->dispose (object);
}

static void
gum_script_finalize (GObject * object)
{
  GumScript * self = GUM_SCRIPT (object);
  GumScriptPrivate * priv = self->priv;

  if (priv->message_handler_notify != NULL)
    priv->message_handler_notify (priv->message_handler_data);

  G_LOCK (gum_memaccess);
  if (--gum_memaccess_refcount == 0)
  {
#ifndef G_OS_WIN32
    sigaction (GUM_INVALID_ACCESS_SIGNAL, &gum_memaccess_old_action, NULL);
    memset (&gum_memaccess_old_action, 0, sizeof (gum_memaccess_old_action));
#endif

    GUM_TLS_KEY_FREE (gum_memaccess_scope_tls);
    gum_memaccess_scope_tls = 0;
  }
  G_UNLOCK (gum_memaccess);

  g_mutex_free (priv->event_mutex);
  g_cond_free (priv->event_cond);

  g_queue_free (priv->attach_entries);
  g_queue_free (priv->heap_blocks);

  G_OBJECT_CLASS (gum_script_parent_class)->finalize (object);
}

static void
gum_script_create_context (GumScript * self)
{
  GumScriptPrivate * priv = self->priv;
  Locker l;
  HandleScope handle_scope;

  Handle<ObjectTemplate> global_templ = ObjectTemplate::New ();

  Handle<ObjectTemplate> console_templ = ObjectTemplate::New ();
  console_templ->Set (String::New ("log"), FunctionTemplate::New (
      gum_script_on_console_log, External::Wrap (self)));
  global_templ->Set (String::New ("console"), console_templ);

  global_templ->Set (String::New ("_send"),
      FunctionTemplate::New (gum_script_on_send, External::Wrap (self)));
  global_templ->Set (String::New ("_setIncomingMessageCallback"),
      FunctionTemplate::New (gum_script_on_set_incoming_message_callback,
          External::Wrap (self)));
  global_templ->Set (String::New ("_waitForEvent"),
      FunctionTemplate::New (gum_script_on_wait_for_event,
          External::Wrap (self)));

  Handle<ObjectTemplate> interceptor_templ = ObjectTemplate::New ();
  interceptor_templ->Set (String::New ("attach"), FunctionTemplate::New (
      gum_script_on_interceptor_attach, External::Wrap (self)));
  global_templ->Set (String::New ("Interceptor"), interceptor_templ);

  Handle<ObjectTemplate> process_templ = ObjectTemplate::New ();
  process_templ->Set (String::New ("findModuleExportByName"),
      FunctionTemplate::New (
          gum_script_on_process_find_module_export_by_name));
  global_templ->Set (String::New ("Process"), process_templ);

  global_templ->Set (String::New ("Int32"),
      FunctionTemplate::New (gum_script_on_int32_cast, External::Wrap (self)));

  Handle<ObjectTemplate> memory_templ = ObjectTemplate::New ();
  memory_templ->Set (String::New ("readSWord"),
      FunctionTemplate::New (gum_script_on_memory_read_sword,
          External::Wrap (self)));
  memory_templ->Set (String::New ("readUWord"),
      FunctionTemplate::New (gum_script_on_memory_read_uword,
          External::Wrap (self)));
  memory_templ->Set (String::New ("readS8"),
      FunctionTemplate::New (gum_script_on_memory_read_s8,
          External::Wrap (self)));
  memory_templ->Set (String::New ("readU8"),
      FunctionTemplate::New (gum_script_on_memory_read_u8,
          External::Wrap (self)));
  memory_templ->Set (String::New ("readS16"),
      FunctionTemplate::New (gum_script_on_memory_read_s16,
          External::Wrap (self)));
  memory_templ->Set (String::New ("readU16"),
      FunctionTemplate::New (gum_script_on_memory_read_u16,
          External::Wrap (self)));
  memory_templ->Set (String::New ("readS32"),
      FunctionTemplate::New (gum_script_on_memory_read_s32,
          External::Wrap (self)));
  memory_templ->Set (String::New ("readU32"),
      FunctionTemplate::New (gum_script_on_memory_read_u32,
          External::Wrap (self)));
  memory_templ->Set (String::New ("readS64"),
      FunctionTemplate::New (gum_script_on_memory_read_s64,
          External::Wrap (self)));
  memory_templ->Set (String::New ("readU64"),
      FunctionTemplate::New (gum_script_on_memory_read_u64,
          External::Wrap (self)));
  memory_templ->Set (String::New ("readUtf8String"),
      FunctionTemplate::New (gum_script_on_memory_read_utf8_string,
          External::Wrap (self)));
  memory_templ->Set (String::New ("readUtf16String"),
      FunctionTemplate::New (gum_script_on_memory_read_utf16_string,
          External::Wrap (self)));
#ifdef G_OS_WIN32
  memory_templ->Set (String::New ("readAnsiString"),
      FunctionTemplate::New (gum_script_on_memory_read_ansi_string,
          External::Wrap (self)));
  memory_templ->Set (String::New ("allocAnsiString"),
      FunctionTemplate::New (gum_script_on_memory_alloc_ansi_string,
          External::Wrap (self)));
#endif
  memory_templ->Set (String::New ("allocUtf8String"),
      FunctionTemplate::New (gum_script_on_memory_alloc_utf8_string,
          External::Wrap (self)));
  memory_templ->Set (String::New ("allocUtf16String"),
      FunctionTemplate::New (gum_script_on_memory_alloc_utf16_string,
          External::Wrap (self)));
  global_templ->Set (String::New ("Memory"), memory_templ);

  priv->context = Context::New (NULL, global_templ);

  Context::Scope context_scope (priv->context);

  Handle<ObjectTemplate> args_templ = ObjectTemplate::New ();
  args_templ->SetInternalFieldCount (1);
  args_templ->SetIndexedPropertyHandler (gum_script_args_on_get_nth,
      gum_script_args_on_set_nth);
  priv->args_template = Persistent<ObjectTemplate>::New (args_templ);
}

class ScriptScope
{
public:
  ScriptScope (GumScript * parent)
    : parent (parent),
      context_scope (parent->priv->context)
  {
  }

  ~ScriptScope ()
  {
    GumScriptPrivate * priv = parent->priv;

    if (trycatch.HasCaught () && priv->message_handler_func != NULL)
    {
      Handle<Message> message = trycatch.Message ();
      Handle<Value> exception = trycatch.Exception ();
      String::AsciiValue exception_str (exception);
      gchar * error = g_strdup_printf (
          "{\"type\":\"error\",\"lineNumber\":%d,\"description\":\"%s\"}",
          message->GetLineNumber () - GUM_SCRIPT_RUNTIME_SOURCE_LINE_COUNT,
          *exception_str);
      priv->message_handler_func (parent, error, priv->message_handler_data);
      g_free (error);
    }
  }

private:
  GumScript * parent;
  Locker l;
  HandleScope handle_scope;
  Context::Scope context_scope;
  TryCatch trycatch;
};

GumScript *
gum_script_from_string (const gchar * source,
                        GError ** error)
{
  GumScript * script = GUM_SCRIPT (g_object_new (GUM_TYPE_SCRIPT, NULL));

  Locker l;
  HandleScope handle_scope;
  Context::Scope context_scope (script->priv->context);

  gchar * combined_source = g_strconcat (gum_script_runtime_source, "\n",
      source, NULL);
  Handle<String> source_value = String::New (combined_source);
  g_free (combined_source);
  TryCatch trycatch;
  Handle<Script> raw_script = Script::Compile (source_value);
  if (raw_script.IsEmpty())
  {
    g_object_unref (script);

    Handle<Message> message = trycatch.Message ();
    Handle<Value> exception = trycatch.Exception ();
    String::AsciiValue exception_str (exception);
    g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED, "Script(line %d): %s",
        message->GetLineNumber () - GUM_SCRIPT_RUNTIME_SOURCE_LINE_COUNT,
        *exception_str);

    return NULL;
  }

  script->priv->raw_script = Persistent<Script>::New (raw_script);

  return script;
}

void
gum_script_set_message_handler (GumScript * self,
                                GumScriptMessageHandler func,
                                gpointer data,
                                GDestroyNotify notify)
{
  self->priv->message_handler_func = func;
  self->priv->message_handler_data = data;
  self->priv->message_handler_notify = notify;
}

void
gum_script_load (GumScript * self)
{
  ScriptScope scope (self);

  self->priv->raw_script->Run ();
}

void
gum_script_unload (GumScript * self)
{
  gum_interceptor_detach_listener (self->priv->interceptor,
      GUM_INVOCATION_LISTENER (self));
}

void
gum_script_post_message (GumScript * self,
                         const gchar * msg)
{
  if (self->priv->incoming_message_sink != NULL)
  {
    GumScriptPrivate * priv = self->priv;
    
    {
      ScriptScope scope (self);
      gum_message_sink_handle_message (self->priv->incoming_message_sink, msg);
      priv->event_count++;
    }

    g_mutex_lock (priv->event_mutex);
    g_cond_broadcast (priv->event_cond);
    g_mutex_unlock (priv->event_mutex);
  }
}

static Handle<Value>
gum_script_on_console_log (const Arguments & args)
{
  String::Utf8Value message (args[0]);
  g_print ("%s\n", *message);

  return Undefined ();
}

static Handle<Value>
gum_script_on_send (const Arguments & args)
{
  GumScript * self = GUM_SCRIPT_CAST (External::Unwrap (args.Data ()));
  GumScriptPrivate * priv = self->priv;

  if (priv->message_handler_func != NULL)
  {
    String::Utf8Value message (args[0]);
    priv->message_handler_func (self, *message, priv->message_handler_data);
  }

  return Undefined ();
}

static Handle<Value>
gum_script_on_set_incoming_message_callback (const Arguments & args)
{
  GumScript * self = GUM_SCRIPT_CAST (External::Unwrap (args.Data ()));
  GumScriptPrivate * priv = self->priv;

  if (args.Length () > 1)
  {
    ThrowException (Exception::TypeError (String::New (
        "invalid argument count")));
    return Undefined ();
  }

  gum_message_sink_free (priv->incoming_message_sink);
  priv->incoming_message_sink = NULL;

  if (args.Length () == 1)
  {
    priv->incoming_message_sink =
        gum_message_sink_new (Local<Function>::Cast (args[0]), args.This ());
  }

  return Undefined ();
}

static Handle<Value>
gum_script_on_wait_for_event (const Arguments & args)
{
  GumScript * self = GUM_SCRIPT_CAST (External::Unwrap (args.Data ()));
  GumScriptPrivate * priv = self->priv;
  guint start_count;

  start_count = priv->event_count;
  while (priv->event_count == start_count)
  {
    Unlocker ul;

    g_mutex_lock (priv->event_mutex);
    g_cond_wait (priv->event_cond, priv->event_mutex);
    g_mutex_unlock (priv->event_mutex);
  }

  return Undefined ();
}

static GumMessageSink *
gum_message_sink_new (Handle<Function> callback,
                      Handle<Object> receiver)
{
  GumMessageSink * sink;

  sink = g_slice_new (GumMessageSink);
  sink->callback = Persistent<Function>::New (callback);
  sink->receiver = Persistent<Object>::New (receiver);

  return sink;
}

static void
gum_message_sink_free (GumMessageSink * sink)
{
  if (sink == NULL)
    return;

  sink->callback.Clear ();
  sink->receiver.Clear ();

  g_slice_free (GumMessageSink, sink);
}

static void
gum_message_sink_handle_message (GumMessageSink * self,
                                 const gchar * msg)
{
  Handle<Value> argv[] = { String::New (msg) };
  self->callback->Call (self->receiver, 1, argv);
}

static Handle<Value>
gum_script_on_process_find_module_export_by_name (const Arguments & args)
{
  Local<Value> module_name_val = args[0];
  if (!module_name_val->IsString ())
  {
    ThrowException (Exception::TypeError (String::New (
        "Process.findModuleExportByName: first argument must be a string "
        "specifying module name")));
    return Undefined ();
  }
  String::Utf8Value module_name (module_name_val);

  Local<Value> symbol_name_val = args[1];
  if (!symbol_name_val->IsString ())
  {
    ThrowException (Exception::TypeError (String::New (
        "Process.findModuleExportByName: second argument must be a string "
        "specifying name of exported symbol")));
    return Undefined ();
  }
  String::Utf8Value symbol_name (symbol_name_val);

  gpointer raw_address =
      gum_module_find_export_by_name (*module_name, *symbol_name);
  if (raw_address == NULL)
    return Undefined ();

  return Number::New (GPOINTER_TO_SIZE (raw_address));
}

static Handle<Value>
gum_script_on_interceptor_attach (const Arguments & args)
{
  GumScript * self = GUM_SCRIPT_CAST (External::Unwrap (args.Data ()));
  GumScriptPrivate * priv = self->priv;

  Local<Value> target_spec = args[0];
  if (!target_spec->IsNumber ())
  {
    ThrowException (Exception::TypeError (String::New ("Interceptor.attach: "
        "first argument must be a memory address")));
    return Undefined ();
  }

  Local<Value> callbacks_value = args[1];
  if (!callbacks_value->IsObject ())
  {
    ThrowException (Exception::TypeError (String::New ("Interceptor.attach: "
        "second argument must be a callback object")));
    return Undefined ();
  }

  Local<Function> on_enter, on_leave;

  Local<Object> callbacks = Local<Object>::Cast (callbacks_value);
  if (!gum_script_attach_callbacks_get (callbacks, "onEnter", &on_enter))
    return Undefined ();
  if (!gum_script_attach_callbacks_get (callbacks, "onLeave", &on_leave))
    return Undefined ();

  GumScriptAttachEntry * entry = g_slice_new (GumScriptAttachEntry);
  entry->on_enter = Persistent<Function>::New (on_enter);
  entry->on_leave = Persistent<Function>::New (on_leave);
  entry->receiver = Persistent<Object>::New (callbacks);

  gpointer function_address = GSIZE_TO_POINTER (target_spec->IntegerValue ());
  GumAttachReturn attach_ret = gum_interceptor_attach_listener (
      priv->interceptor, function_address, GUM_INVOCATION_LISTENER (self),
      entry);

  g_queue_push_tail (priv->attach_entries, entry);

  return (attach_ret == GUM_ATTACH_OK) ? True () : False ();
}

static gboolean
gum_script_attach_callbacks_get (Handle<Object> callbacks,
                                 const gchar * name,
                                 Local<Function> * callback_function)
{
  Local<Value> val = callbacks->Get (String::New (name));
  if (!val->IsUndefined ())
  {
    if (!val->IsFunction ())
    {
      gchar * message =
          g_strdup_printf ("Interceptor.attach: %s must be a function", name);
      ThrowException (Exception::TypeError (String::New (message)));
      g_free (message);

      return FALSE;
    }

    *callback_function = Local<Function>::Cast (val);
  }

  return TRUE;
}

static Handle<Value>
gum_script_on_int32_cast (const Arguments & args)
{
  return Number::New (args[0]->Int32Value ());
}

#ifdef _MSC_VER
# pragma warning (push)
# pragma warning (disable: 4611)
#endif

static Handle<Value>
gum_script_memory_do_read (const Arguments & args,
                           GumMemoryValueType type)
{
  GumMemoryAccessScope scope;
  Handle<Value> address = args[0];
  Handle<Value> result;

  scope.exception_occurred = FALSE;

  GUM_TLS_KEY_SET_VALUE (gum_memaccess_scope_tls, &scope);

  if (setjmp (scope.env) == 0)
  {
    switch (type)
    {
      case GUM_MEMORY_VALUE_SWORD:
        result = Integer::New (*static_cast<const int *> (
            GSIZE_TO_POINTER (args[0]->IntegerValue ())));
        break;
      case GUM_MEMORY_VALUE_UWORD:
        result = Integer::New (*static_cast<const unsigned int *> (
            GSIZE_TO_POINTER (args[0]->IntegerValue ())));
        break;
      case GUM_MEMORY_VALUE_S8:
        result = Integer::New (*static_cast<const gint8 *> (
            GSIZE_TO_POINTER (args[0]->IntegerValue ())));
        break;
      case GUM_MEMORY_VALUE_U8:
        result = Integer::NewFromUnsigned (*static_cast<const guint8 *> (
            GSIZE_TO_POINTER (address->IntegerValue ())));
        break;
      case GUM_MEMORY_VALUE_S16:
        result = Integer::New (*static_cast<const gint16 *> (
            GSIZE_TO_POINTER (args[0]->IntegerValue ())));
        break;
      case GUM_MEMORY_VALUE_U16:
        result = Integer::NewFromUnsigned (*static_cast<const guint16 *> (
            GSIZE_TO_POINTER (args[0]->IntegerValue ())));
        break;
      case GUM_MEMORY_VALUE_S32:
        result = Integer::New (*static_cast<const gint32 *> (
            GSIZE_TO_POINTER (args[0]->IntegerValue ())));
        break;
      case GUM_MEMORY_VALUE_U32:
        result = Integer::NewFromUnsigned (*static_cast<const guint32 *> (
            GSIZE_TO_POINTER (args[0]->IntegerValue ())));
        break;
      case GUM_MEMORY_VALUE_S64:
        result = Number::New (*static_cast<const gint64 *> (
            GSIZE_TO_POINTER (args[0]->IntegerValue ())));
        break;
      case GUM_MEMORY_VALUE_U64:
        result = Number::New (*static_cast<const guint64 *> (
            GSIZE_TO_POINTER (args[0]->IntegerValue ())));
        break;
      case GUM_MEMORY_VALUE_UTF8_STRING:
      {
        const char * data = static_cast<const char *> (
            GSIZE_TO_POINTER (args[0]->IntegerValue ()));
        if (data == NULL)
        {
          result = Null ();
          break;
        }

        int64_t length = -1;
        if (args.Length () > 1)
          length = args[1]->IntegerValue();
        if (length < 0)
          length = g_utf8_strlen (data, -1);

        if (length != 0)
        {
          int size = g_utf8_offset_to_pointer (data, length) - data;
          result = String::New (data, size);
        }
        else
        {
          result = String::Empty ();
        }

        break;
      }
      case GUM_MEMORY_VALUE_UTF16_STRING:
      {
        gconstpointer data = static_cast<gconstpointer> (
            GSIZE_TO_POINTER (args[0]->IntegerValue ()));
        if (data == NULL)
        {
          result = Null ();
          break;
        }

        int64_t length = -1;
        if (args.Length () > 1)
          length = args[1]->IntegerValue();
        if (length < 0)
          length = wcslen (static_cast<const wchar_t *> (data));

        if (length != 0)
        {
          guint16 dummy_to_trap_bad_pointer_early;
          memcpy (&dummy_to_trap_bad_pointer_early, data, sizeof (guint16));

          result = String::New (static_cast<const uint16_t *> (data), length);
        }
        else
        {
          result = String::Empty ();
        }

        break;
      }
#ifdef G_OS_WIN32
      case GUM_MEMORY_VALUE_ANSI_STRING:
      {
        const char * str_ansi = static_cast<const char *> (
            GSIZE_TO_POINTER (args[0]->IntegerValue ()));
        if (str_ansi == NULL)
        {
          result = Null ();
          break;
        }

        int64_t length = -1;
        if (args.Length () > 1)
          length = args[1]->IntegerValue();

        if (length != 0)
        {
          guint8 dummy_to_trap_bad_pointer_early;
          memcpy (&dummy_to_trap_bad_pointer_early, str_ansi, sizeof (guint8));

          gchar * str_utf8 = gum_ansi_string_to_utf8 (str_ansi, length);
          int size = g_utf8_offset_to_pointer (str_utf8,
              g_utf8_strlen (str_utf8, -1)) - str_utf8;
          result = String::New (str_utf8, size);
          g_free (str_utf8);
        }
        else
        {
          result = String::Empty ();
        }

        break;
      }
#endif
      default:
        g_assert_not_reached ();
    }
  }

  GUM_TLS_KEY_SET_VALUE (gum_memaccess_scope_tls, NULL);

  if (scope.exception_occurred)
  {
    gchar * message = g_strdup_printf (
        "access violation reading 0x%" G_GSIZE_MODIFIER "x",
        GPOINTER_TO_SIZE (scope.address));
    ThrowException (Exception::Error (String::New (message)));
    g_free (message);

    result = Undefined ();
  }

  return result;
}

#ifdef G_OS_WIN32

#ifdef _MSC_VER
# pragma warning (pop)
#endif

static void
gum_script_memory_do_longjmp (jmp_buf * env)
{
  longjmp (*env, 1);
}

static gboolean
gum_script_memory_on_exception (EXCEPTION_RECORD * exception_record,
                                CONTEXT * context,
                                gpointer user_data)
{
  GumMemoryAccessScope * scope;

  (void) user_data;

  if (exception_record->ExceptionCode != STATUS_ACCESS_VIOLATION)
    return FALSE;

  /* must be a READ */
  if (exception_record->ExceptionInformation[0] != 0)
    return FALSE;

  scope = (GumMemoryAccessScope *)
      GUM_TLS_KEY_GET_VALUE (gum_memaccess_scope_tls);
  if (scope == NULL)
    return FALSE;

  if (!scope->exception_occurred)
  {
    scope->exception_occurred = TRUE;

    scope->address = (gpointer) exception_record->ExceptionInformation[1];

#if GLIB_SIZEOF_VOID_P == 4
    context->Esp -= 8;
    *((jmp_buf **) (context->Esp + 4)) = &scope->env;
    *((jmp_buf **) (context->Esp + 0)) = NULL;
    context->Eip = (DWORD) gum_script_memory_do_longjmp;
#else
    context->Rsp -= 16;
    context->Rcx = (DWORD64) &scope->env;
    *((void **) (context->Rsp + 0)) = NULL;
    context->Rip = (DWORD64) gum_script_memory_do_longjmp;
#endif

    return TRUE;
  }

  return FALSE;
}

#else

static void
gum_script_memory_on_invalid_access (int sig,
                                     siginfo_t * siginfo,
                                     void * context)
{
  GumMemoryAccessScope * scope;

  scope = (GumMemoryAccessScope *)
      GUM_TLS_KEY_GET_VALUE (gum_memaccess_scope_tls);
  if (scope == NULL)
    goto not_our_fault;

  if (!scope->exception_occurred)
  {
    scope->exception_occurred = TRUE;

    scope->address = siginfo->si_addr;
    longjmp (scope->env, 1);
  }

not_our_fault:
  raise (GUM_INVALID_ACCESS_SIGNAL);
}

#endif

static Handle<Value>
gum_script_on_memory_read_sword (const Arguments & args)
{
  return gum_script_memory_do_read (args, GUM_MEMORY_VALUE_SWORD);
}

static Handle<Value>
gum_script_on_memory_read_uword (const Arguments & args)
{
  return gum_script_memory_do_read (args, GUM_MEMORY_VALUE_UWORD);
}

static Handle<Value>
gum_script_on_memory_read_s8 (const Arguments & args)
{
  return gum_script_memory_do_read (args, GUM_MEMORY_VALUE_S8);
}

static Handle<Value>
gum_script_on_memory_read_u8 (const Arguments & args)
{
  return gum_script_memory_do_read (args, GUM_MEMORY_VALUE_U8);
}

static Handle<Value>
gum_script_on_memory_read_s16 (const Arguments & args)
{
  return gum_script_memory_do_read (args, GUM_MEMORY_VALUE_S16);
}

static Handle<Value>
gum_script_on_memory_read_u16 (const Arguments & args)
{
  return gum_script_memory_do_read (args, GUM_MEMORY_VALUE_U16);
}

static Handle<Value>
gum_script_on_memory_read_s32 (const Arguments & args)
{
  return gum_script_memory_do_read (args, GUM_MEMORY_VALUE_S32);
}

static Handle<Value>
gum_script_on_memory_read_u32 (const Arguments & args)
{
  return gum_script_memory_do_read (args, GUM_MEMORY_VALUE_U32);
}

static Handle<Value>
gum_script_on_memory_read_s64 (const Arguments & args)
{
  return gum_script_memory_do_read (args, GUM_MEMORY_VALUE_S64);
}

static Handle<Value>
gum_script_on_memory_read_u64 (const Arguments & args)
{
  return gum_script_memory_do_read (args, GUM_MEMORY_VALUE_U64);
}

static Handle<Value>
gum_script_on_memory_read_utf8_string (const Arguments & args)
{
  return gum_script_memory_do_read (args, GUM_MEMORY_VALUE_UTF8_STRING);
}

static Handle<Value>
gum_script_on_memory_read_utf16_string (const Arguments & args)
{
  return gum_script_memory_do_read (args, GUM_MEMORY_VALUE_UTF16_STRING);
}

#ifdef G_OS_WIN32

static Handle<Value>
gum_script_on_memory_read_ansi_string (const Arguments & args)
{
  return gum_script_memory_do_read (args, GUM_MEMORY_VALUE_ANSI_STRING);
}

static Handle<Value>
gum_script_on_memory_alloc_ansi_string (const Arguments & args)
{
  GumScript * self = GUM_SCRIPT_CAST (External::Unwrap (args.Data ()));

  String::Utf8Value str (args[0]);
  gchar * str_heap = gum_ansi_string_from_utf8 (*str);
  g_queue_push_tail (self->priv->heap_blocks, str_heap);

  return Number::New (GPOINTER_TO_SIZE (str_heap));
}

static gchar *
gum_ansi_string_to_utf8 (const gchar * str_ansi,
                         gint length)
{
  guint str_utf16_size;
  WCHAR * str_utf16;
  gchar * str_utf8;

  if (length < 0)
    length = (gint) strlen (str_ansi);

  str_utf16_size = (guint) (length + 1) * sizeof (WCHAR);
  str_utf16 = (WCHAR *) g_malloc (str_utf16_size);
  MultiByteToWideChar (CP_ACP, 0, str_ansi, length, str_utf16, str_utf16_size);
  str_utf16[length] = L'\0';
  str_utf8 = g_utf16_to_utf8 ((gunichar2 *) str_utf16, -1, NULL, NULL, NULL);
  g_free (str_utf16);

  return str_utf8;
}

static gchar *
gum_ansi_string_from_utf8 (const gchar * str_utf8)
{
  gunichar2 * str_utf16;
  gchar * str_ansi;
  guint str_ansi_size;

  str_utf16 = g_utf8_to_utf16 (str_utf8, -1, NULL, NULL, NULL);
  str_ansi_size = WideCharToMultiByte (CP_ACP, 0, (LPCWSTR) str_utf16, -1,
      NULL, 0, NULL, NULL);
  str_ansi = (gchar *) g_malloc (str_ansi_size);
  WideCharToMultiByte (CP_ACP, 0, (LPCWSTR) str_utf16, -1,
      str_ansi, str_ansi_size, NULL, NULL);
  g_free (str_utf16);

  return str_ansi;
}

#endif

static Handle<Value>
gum_script_on_memory_alloc_utf8_string (const Arguments & args)
{
  GumScript * self = GUM_SCRIPT_CAST (External::Unwrap (args.Data ()));

  String::Utf8Value str (args[0]);
  gchar * str_heap = g_strdup (*str);
  g_queue_push_tail (self->priv->heap_blocks, str_heap);

  return Number::New (GPOINTER_TO_SIZE (str_heap));
}

static Handle<Value>
gum_script_on_memory_alloc_utf16_string (const Arguments & args)
{
  GumScript * self = GUM_SCRIPT_CAST (External::Unwrap (args.Data ()));

  String::Utf8Value str (args[0]);
  gunichar2 * str_heap = g_utf8_to_utf16 (*str, -1, NULL, NULL, NULL);
  g_queue_push_tail (self->priv->heap_blocks, str_heap);

  return Number::New (GPOINTER_TO_SIZE (str_heap));
}

static void
gum_script_on_enter (GumInvocationListener * listener,
                     GumInvocationContext * context)
{
  GumScriptAttachEntry * entry = static_cast<GumScriptAttachEntry *> (
      gum_invocation_context_get_listener_function_data (context));
  if (!entry->on_enter.IsEmpty ())
  {
    GumScript * self = GUM_SCRIPT_CAST (listener);

    ScriptScope scope (self);

    Local<Object> args = self->priv->args_template->NewInstance ();
    args->SetPointerInInternalField (0, context);

    Handle<Value> argv[] = { args };
    entry->on_enter->Call (entry->receiver, 1, argv);
  }
}

static void
gum_script_on_leave (GumInvocationListener * listener,
                     GumInvocationContext * context)
{
  GumScriptAttachEntry * entry = static_cast<GumScriptAttachEntry *> (
      gum_invocation_context_get_listener_function_data (context));
  if (!entry->on_leave.IsEmpty ())
  {
    GumScript * self = GUM_SCRIPT_CAST (listener);

    ScriptScope scope (self);

    gpointer raw_value = gum_invocation_context_get_return_value (context);
    Local<Number> return_value (Number::New (GPOINTER_TO_SIZE (raw_value)));

    Handle<Value> argv[] = { return_value };
    entry->on_leave->Call (entry->receiver, 1, argv);
  }
}

static Handle<Value>
gum_script_args_on_get_nth (uint32_t index,
                            const AccessorInfo & info)
{
  GumInvocationContext * ctx = static_cast<GumInvocationContext *> (
      info.This ()->GetPointerFromInternalField (0));

  gpointer raw_value = gum_invocation_context_get_nth_argument (ctx, index);

  return Number::New (GPOINTER_TO_SIZE (raw_value));
}

static Handle<Value>
gum_script_args_on_set_nth (uint32_t index,
                            Local<Value> value,
                            const AccessorInfo & info)
{
  GumInvocationContext * ctx = static_cast<GumInvocationContext *> (
      info.This ()->GetPointerFromInternalField (0));

  if (!value->IsNumber ())
  {
    ThrowException (Exception::TypeError (
        String::New ("can only assign a number")));
    return Undefined ();
  }

  gpointer raw_value = GSIZE_TO_POINTER (value->IntegerValue ());
  gum_invocation_context_replace_nth_argument (ctx, index, raw_value);

  return value;
}
