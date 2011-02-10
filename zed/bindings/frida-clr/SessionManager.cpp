#using <mscorlib.dll>

#include "zed-core.h"

#include "Application.hpp"
#include <msclr/marshal.h>

using namespace System::Collections::Generic;

namespace Frida
{
  public ref class Session
  {
  public:
    Session (void * handle)
      : handle (FRIDA_SESSION (handle))
    {
    }

    ~Session ()
    {
      g_object_unref (handle);
    }

    void
    SetGMainWatchdogEnabled (bool enable)
    {
      GError * error = NULL;
      frida_session_set_gmain_watchdog_enabled (handle, enable, &error);
      if (error != NULL)
        throw gcnew System::Exception (gcnew System::String (error->message));
    }

  private:
    FridaSession * handle;
  };

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

#if 0
      msclr::interop::marshal_context ^ context = gcnew msclr::interop::marshal_context ();
      const wchar_t * wideText = context->marshal_as<const wchar_t *> (text);
      gchar * utf8Text = g_utf16_to_utf8 (reinterpret_cast<const gunichar2 *> (wideText), -1, NULL, NULL, NULL);
      delete context;

      g_free (utf8Text);
#endif