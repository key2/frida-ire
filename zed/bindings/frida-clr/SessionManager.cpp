#using <mscorlib.dll>

#include "zed-core.h"

#include "Application.hpp"
#include "Marshal.hpp"
#include <msclr/gcroot.h>

#using <WindowsBase.dll>

using namespace System;
using namespace System::Collections::Generic;
using namespace System::Windows::Threading;

namespace Frida
{
  [Flags]
  public enum struct LogLevel : UInt32
  {
    FlagRecursion = 1 << 0,
    FlagFatal     = 1 << 1,

    Error         = 1 << 2,
    Critical      = 1 << 3,
    Warning       = 1 << 4,
    Message       = 1 << 5,
    Info          = 1 << 6,
    Debug         = 1 << 7,
  };

  public ref class LogMessageEventArgs : public EventArgs
  {
  public:
    property String ^ Domain { String ^ get () { return domain; } };
    property LogLevel Level { LogLevel get () { return level; } };
    property String ^ Message { String ^ get () { return message; } };

    LogMessageEventArgs (String ^ domain, LogLevel level, String ^ message)
    {
      this->domain = domain;
      this->level = level;
      this->message = message;
    }

  private:
    String ^ domain;
    LogLevel level;
    String ^ message;
  };

  public delegate void LogMessageHandler (Object ^ sender, LogMessageEventArgs ^ e);

  static void OnSessionClosed (FridaSession * session, gpointer user_data);
  static void OnSessionGLogMessage (FridaSession * session, const gchar * domain, guint level, const gchar * message, gpointer user_data);
  
  public ref class Session
  {
  public:
    event EventHandler ^ Closed;
    event LogMessageHandler ^ LogMessage;

    Session (void * handle, Dispatcher ^ dispatcher)
      : handle (FRIDA_SESSION (handle)),
        dispatcher (dispatcher)
    {
      selfHandle = new msclr::gcroot<Session ^> (this);

      onClosedHandler = gcnew EventHandler (this, &Session::OnClosed);
      onLogMessageHandler = gcnew LogMessageHandler (this, &Session::OnLogMessage);

      g_signal_connect (handle, "closed", G_CALLBACK (OnSessionClosed), selfHandle);
      g_signal_connect (handle, "glog-message", G_CALLBACK (OnSessionGLogMessage), selfHandle);
    }

    ~Session ()
    {
      g_object_unref (handle);
      handle = NULL;

      delete selfHandle;
      selfHandle = NULL;
    }

    void
    Close ()
    {
      frida_session_close (handle);
    }

    void
    AddGLogPattern (String ^ pattern, LogLevel levels)
    {
      gchar * patternUtf8 = Marshal::ClrStringToUTF8CString (pattern);
      GError * error = NULL;
      frida_session_add_glog_pattern (handle, patternUtf8, static_cast<guint> (levels), &error);
      g_free (patternUtf8);
      if (error != NULL)
        throw gcnew Exception (gcnew String (error->message));
    }

    void
    ClearGLogPatterns ()
    {
      GError * error = NULL;
      frida_session_clear_glog_patterns (handle, &error);
      if (error != NULL)
        throw gcnew Exception (gcnew String (error->message));
    }

    void
    EnableGMainWatchdog (double maxDuration)
    {
      GError * error = NULL;
      frida_session_enable_gmain_watchdog (handle, maxDuration, &error);
      if (error != NULL)
        throw gcnew Exception (gcnew String (error->message));
    }

    void
    DisableGMainWatchdog ()
    {
      GError * error = NULL;
      frida_session_disable_gmain_watchdog (handle, &error);
      if (error != NULL)
        throw gcnew Exception (gcnew String (error->message));
    }

    void
    OnClosed (Object ^ sender, EventArgs ^ e)
    {
      if (dispatcher->CheckAccess ())
        Closed (sender, e);
      else
        dispatcher->BeginInvoke (DispatcherPriority::Normal, onClosedHandler, sender, e);
    }

    void
    OnLogMessage (Object ^ sender, LogMessageEventArgs ^ e)
    {
      if (dispatcher->CheckAccess ())
        LogMessage (sender, e);
      else
        dispatcher->BeginInvoke (DispatcherPriority::Normal, onLogMessageHandler, sender, e);
    }

  private:
    FridaSession * handle;
    msclr::gcroot<Session ^> * selfHandle;

    Dispatcher ^ dispatcher;
    EventHandler ^ onClosedHandler;
    LogMessageHandler ^ onLogMessageHandler;
  };

  static void
  OnSessionClosed (FridaSession * session, gpointer user_data)
  {
    (void) session;

    msclr::gcroot<Session ^> * wrapper = static_cast<msclr::gcroot<Session ^> *> (user_data);
    (*wrapper)->OnClosed (*wrapper, EventArgs::Empty);
  }

  static void
  OnSessionGLogMessage (FridaSession * session, const gchar * domain, guint level, const gchar * message, gpointer user_data)
  {
    (void) session;

    msclr::gcroot<Session ^> * wrapper = static_cast<msclr::gcroot<Session ^> *> (user_data);
    LogMessageEventArgs ^ e = gcnew LogMessageEventArgs (
        Marshal::UTF8CStringToClrString (domain),
        static_cast<LogLevel> (level),
        Marshal::UTF8CStringToClrString (message));
    (*wrapper)->OnLogMessage (*wrapper, e);
  }

  public ref class SessionManager
  {
  public:
    SessionManager (Dispatcher ^ dispatcher)
      : dispatcher (dispatcher),
        handle (frida_session_manager_new (static_cast<GMainContext *> (Application::GetMainContext ())))
    {
    }

    ~SessionManager ()
    {
      g_object_unref (handle);
    }

    Session ^
    ObtainSessionFor (int pid)
    {
      GError * error = NULL;
      FridaSession * session = frida_session_manager_obtain_session_for (handle, pid, &error);

      if (error != NULL)
        throw gcnew Exception (gcnew String (error->message));

      return gcnew Session (session, dispatcher);
    }

  private:
    Dispatcher ^ dispatcher;
    FridaSessionManager * handle;
  };
}
