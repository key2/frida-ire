#using <mscorlib.dll>

#include "zed-core.h"

#include "Application.hpp"
#include "Marshal.hpp"
#include <msclr/gcroot.h>

using namespace System::Collections::Generic;

namespace Frida
{
  [System::Flags]
  public enum struct LogLevel : System::UInt32
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

  public delegate void LogMessageHandler (System::String ^ domain, LogLevel level, System::String ^ message);
  static void HandleGLogMessage (FridaSession * session, const gchar * domain, guint level, const gchar * message, gpointer user_data);

  public ref class Session
  {
  public:
    event LogMessageHandler ^ LogMessage;

    Session (void * handle)
      : handle (FRIDA_SESSION (handle))
    {
      selfHandle = new msclr::gcroot<Session ^> (this);
      g_signal_connect (handle, "glog-message", G_CALLBACK (HandleGLogMessage), selfHandle);
    }

    ~Session ()
    {
      g_object_unref (handle);
      delete selfHandle;
    }

    void
    AddGLogPattern (System::String ^ pattern, LogLevel levels)
    {
      gchar * patternUtf8 = Marshal::ClrStringToUTF8CString (pattern);
      GError * error = NULL;
      frida_session_add_glog_pattern (handle, patternUtf8, static_cast<guint> (levels), &error);
      g_free (patternUtf8);
      if (error != NULL)
        throw gcnew System::Exception (gcnew System::String (error->message));
    }

    void
    ClearGLogPatterns ()
    {
      GError * error = NULL;
      frida_session_clear_glog_patterns (handle, &error);
      if (error != NULL)
        throw gcnew System::Exception (gcnew System::String (error->message));
    }

    void
    SetGMainWatchdogEnabled (bool enable)
    {
      GError * error = NULL;
      frida_session_set_gmain_watchdog_enabled (handle, enable, &error);
      if (error != NULL)
        throw gcnew System::Exception (gcnew System::String (error->message));
    }

    void
    OnLogMessage (System::String ^ domain, LogLevel level, System::String ^ message)
    {
      LogMessage (domain, level, message);
    }

  private:
    FridaSession * handle;
    msclr::gcroot<Session ^> * selfHandle;
  };

  static void
  HandleGLogMessage (FridaSession * session, const gchar * domain, guint level, const gchar * message, gpointer user_data)
  {
    (void) session;

    System::String ^ domainObj = Marshal::UTF8CStringToClrString (domain);
    LogLevel levelObj = static_cast<LogLevel> (level);
    System::String ^ messageObj = Marshal::UTF8CStringToClrString (message);

    msclr::gcroot<Session ^> * wrapper = static_cast<msclr::gcroot<Session ^> *> (user_data);
    (*wrapper)->OnLogMessage (domainObj, levelObj, messageObj);
  }

  public ref class SessionManager
  {
  public:
    SessionManager ()
    {
      handle = frida_session_manager_new (static_cast<GMainContext *> (Application::GetMainContext ()));
    }

    ~SessionManager ()
    {
      g_object_unref (handle);
    }

    Session ^
    AttachTo (int pid)
    {
      GError * error = NULL;
      FridaSession * session = frida_session_manager_attach_to (handle, pid, &error);

      if (error != NULL)
        throw gcnew System::Exception (gcnew System::String (error->message));

      return gcnew Session (session);
    }

  private:
    FridaSessionManager * handle;
  };
}
