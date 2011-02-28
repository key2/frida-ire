#include "Session.hpp"

#include "Marshal.hpp"

using System::Windows::Threading::DispatcherPriority;

G_BEGIN_DECLS

struct _GVariant
{
  gpointer foo;
};

G_END_DECLS

namespace Frida
{
  static void OnSessionClosed (FridaSession * session, gpointer user_data);
  static void OnSessionGLogMessage (FridaSession * session, guint64 timestamp, const gchar * domain, guint level, const gchar * message, gpointer user_data);
  static void OnSessionGstPadStats (FridaSession * session, const ZedGstPadStats * stats, guint n_stats, gpointer user_data);
  static void OnScriptMessage (FridaScript * script, GVariant * msg, gpointer user_data);

  Session::Session (void * handle, Dispatcher ^ dispatcher)
    : handle (FRIDA_SESSION (handle)),
      dispatcher (dispatcher)
  {
    selfHandle = new msclr::gcroot<Session ^> (this);

    onClosedHandler = gcnew EventHandler (this, &Session::OnClosed);
    onLogMessageHandler = gcnew LogMessageHandler (this, &Session::OnLogMessage);
    onGstPadStatsHandler = gcnew GstPadStatsHandler (this, &Session::OnGstPadStats);

    g_signal_connect (handle, "closed", G_CALLBACK (OnSessionClosed), selfHandle);
    g_signal_connect (handle, "glog-message", G_CALLBACK (OnSessionGLogMessage), selfHandle);
    g_signal_connect (handle, "gst-pad-stats", G_CALLBACK (OnSessionGstPadStats), selfHandle);
  }

  Session::~Session ()
  {
    g_object_unref (handle);
    handle = NULL;

    delete selfHandle;
    selfHandle = NULL;
  }

  void
  Session::Close ()
  {
    frida_session_close (handle);
  }

  Script ^
  Session::CompileScript (String ^ text)
  {
    FridaScript * scriptHandle;

    GError * error = NULL;
    gchar * textUtf8 = Marshal::ClrStringToUTF8CString (text);
    scriptHandle = frida_session_compile_script (handle, textUtf8, &error);
    g_free (textUtf8);
    Marshal::ThrowGErrorIfSet (&error);

    return gcnew Script (scriptHandle, dispatcher);
  }

  Address
  Session::ResolveModuleBase (String ^ moduleName)
  {
    Address address;

    GError * error = NULL;
    gchar * moduleNameUtf8 = Marshal::ClrStringToUTF8CString (moduleName);
    address.Value = frida_session_resolve_module_base (handle, moduleNameUtf8, &error);
    g_free (moduleNameUtf8);
    Marshal::ThrowGErrorIfSet (&error);

    return address;
  }

  Address
  Session::ResolveModuleExport (String ^ moduleName, String ^ symbolName)
  {
    Address address;

    GError * error = NULL;
    gchar * moduleNameUtf8 = Marshal::ClrStringToUTF8CString (moduleName);
    gchar * symbolNameUtf8 = Marshal::ClrStringToUTF8CString (symbolName);
    address.Value = frida_session_resolve_module_export (handle, moduleNameUtf8, symbolNameUtf8, &error);
    g_free (symbolNameUtf8);
    g_free (moduleNameUtf8);
    Marshal::ThrowGErrorIfSet (&error);

    return address;
  }

  array<Address> ^
  Session::ScanModuleForCodePattern (String ^ moduleName, String ^ pattern)
  {
    array<Address> ^ result;

    GError * error = NULL;
    gchar * moduleNameUtf8 = Marshal::ClrStringToUTF8CString (moduleName);
    gchar * patternUtf8 = Marshal::ClrStringToUTF8CString (pattern);
    int rawResultLength;
    guint64 * rawResult = frida_session_scan_module_for_code_pattern (handle, moduleNameUtf8, patternUtf8, &rawResultLength, &error);
    g_free (patternUtf8);
    g_free (moduleNameUtf8);
    Marshal::ThrowGErrorIfSet (&error);

    result = gcnew array<Address> (rawResultLength);
    for (int i = 0; i != rawResultLength; i++)
      result[i] = Address (rawResult[i]);

    g_free (rawResult);

    return result;
  }

  void
  Session::InvokeFunction (Address address, String ^ arguments)
  {
    GError * error = NULL;
    gchar * argumentsUtf8 = Marshal::ClrStringToUTF8CString (arguments);
    frida_session_invoke_function (handle, address.Value, argumentsUtf8, &error);
    g_free (argumentsUtf8);
    Marshal::ThrowGErrorIfSet (&error);
  }

  void
  Session::AddGLogPattern (String ^ pattern, LogLevel levels)
  {
    GError * error = NULL;
    gchar * patternUtf8 = Marshal::ClrStringToUTF8CString (pattern);
    frida_session_add_glog_pattern (handle, patternUtf8, static_cast<guint> (levels), &error);
    g_free (patternUtf8);
    Marshal::ThrowGErrorIfSet (&error);
  }

  void
  Session::ClearGLogPatterns ()
  {
    GError * error = NULL;
    frida_session_clear_glog_patterns (handle, &error);
    Marshal::ThrowGErrorIfSet (&error);
  }

  void
  Session::EnableGMainWatchdog (double maxDuration)
  {
    GError * error = NULL;
    frida_session_enable_gmain_watchdog (handle, maxDuration, &error);
    Marshal::ThrowGErrorIfSet (&error);
  }

  void
  Session::DisableGMainWatchdog ()
  {
    GError * error = NULL;
    frida_session_disable_gmain_watchdog (handle, &error);
    Marshal::ThrowGErrorIfSet (&error);
  }

  void
  Session::EnableGstMonitor ()
  {
    GError * error = NULL;
    frida_session_enable_gst_monitor (handle, &error);
    Marshal::ThrowGErrorIfSet (&error);
  }

  void
  Session::DisableGstMonitor ()
  {
    GError * error = NULL;
    frida_session_disable_gst_monitor (handle, &error);
    Marshal::ThrowGErrorIfSet (&error);
  }

  void
  Session::OnClosed (Object ^ sender, EventArgs ^ e)
  {
    if (dispatcher->CheckAccess ())
      Closed (sender, e);
    else
      dispatcher->BeginInvoke (DispatcherPriority::Normal, onClosedHandler, sender, e);
  }

  void
  Session::OnLogMessage (Object ^ sender, LogMessageEventArgs ^ e)
  {
    if (dispatcher->CheckAccess ())
      LogMessage (sender, e);
    else
      dispatcher->BeginInvoke (DispatcherPriority::Normal, onLogMessageHandler, sender, e);
  }

  void
  Session::OnGstPadStats (Object ^ sender, GstPadStatsEventArgs ^ e)
  {
    if (dispatcher->CheckAccess ())
      GstPadStats (sender, e);
    else
      dispatcher->BeginInvoke (DispatcherPriority::Normal, onGstPadStatsHandler, sender, e);
  }

  static void
  OnSessionClosed (FridaSession * session, gpointer user_data)
  {
    (void) session;

    msclr::gcroot<Session ^> * wrapper = static_cast<msclr::gcroot<Session ^> *> (user_data);
    (*wrapper)->OnClosed (*wrapper, EventArgs::Empty);
  }

  static void
  OnSessionGLogMessage (FridaSession * session, guint64 timestamp, const gchar * domain, guint level, const gchar * message, gpointer user_data)
  {
    (void) session;

    msclr::gcroot<Session ^> * wrapper = static_cast<msclr::gcroot<Session ^> *> (user_data);
    LogMessageEventArgs ^ e = gcnew LogMessageEventArgs (
        TimeSpan (timestamp / 100),
        Marshal::UTF8CStringToClrString (domain),
        static_cast<LogLevel> (level),
        Marshal::UTF8CStringToClrString (message));
    (*wrapper)->OnLogMessage (*wrapper, e);
  }

  static void
  OnSessionGstPadStats (FridaSession * session, const ZedGstPadStats * stats, guint n_stats, gpointer user_data)
  {
    (void) session;

    msclr::gcroot<Session ^> * wrapper = static_cast<msclr::gcroot<Session ^> *> (user_data);
    array<GstPadStats ^> ^ entries = gcnew array<GstPadStats ^> (n_stats);
    for (guint i = 0; i != n_stats; i++)
    {
      const ZedGstPadStats * s = &stats[i];
      entries[i] = gcnew GstPadStats (Marshal::UTF8CStringToClrString (s->_pad_name), s->_buffers_per_second, Marshal::UTF8CStringToClrString (s->_timing_history));
    }
    GstPadStatsEventArgs ^ e = gcnew GstPadStatsEventArgs (entries);
    (*wrapper)->OnGstPadStats (*wrapper, e);
  }

  Script::Script (FridaScript * handle, Dispatcher ^ dispatcher)
    : handle (handle),
      dispatcher (dispatcher)
  {
    selfHandle = new msclr::gcroot<Script ^> (this);

    onMessageHandler = gcnew ScriptMessageHandler (this, &Script::OnMessage);

    g_signal_connect (handle, "message", G_CALLBACK (OnScriptMessage), selfHandle);
  }

  Script::~Script ()
  {
    g_object_unref (handle);
    handle = NULL;

    delete selfHandle;
    selfHandle = NULL;
  }

  void
  Script::Destroy ()
  {
    GError * error = NULL;
    frida_script_destroy (handle, &error);
    Marshal::ThrowGErrorIfSet (&error);
  }

  void
  Script::AttachTo (Address address)
  {
    GError * error = NULL;
    frida_script_attach_to (handle, address.Value, &error);
    Marshal::ThrowGErrorIfSet (&error);
  }

  void
  Script::OnMessage (Object ^ sender, ScriptMessageEventArgs ^ e)
  {
    if (dispatcher->CheckAccess ())
      Message (sender, e);
    else
      dispatcher->BeginInvoke (DispatcherPriority::Normal, onMessageHandler, sender, e);
  }

  static void
  OnScriptMessage (FridaScript * script, GVariant * msg, gpointer user_data)
  {
    (void) script;

    msclr::gcroot<Script ^> * wrapper = static_cast<msclr::gcroot<Script ^> *> (user_data);
    gchar * msg_str = g_variant_print (msg, false);
    ScriptMessageEventArgs ^ e = gcnew ScriptMessageEventArgs (
        Marshal::UTF8CStringToClrString (msg_str));
    g_free (msg_str);
   (*wrapper)->OnMessage (*wrapper, e);
  }
}