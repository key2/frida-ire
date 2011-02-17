#include "SessionManager.hpp"

#include "Application.hpp"
#include "Marshal.hpp"
#include "Session.hpp"

using namespace System;

namespace Frida
{
  SessionManager::SessionManager (Dispatcher ^ dispatcher)
    : dispatcher (dispatcher),
      handle (frida_session_manager_new (static_cast<GMainContext *> (Application::GetMainContext ())))
  {
  }

  SessionManager::~SessionManager ()
  {
    g_object_unref (handle);
  }

  Session ^
  SessionManager::ObtainSessionFor (int pid)
  {
    GError * error = NULL;
    FridaSession * session = frida_session_manager_obtain_session_for (handle, pid, &error);
    Marshal::ThrowGErrorIfSet (&error);

    return gcnew Session (session, dispatcher);
  }
}