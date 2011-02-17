#pragma once

#using <WindowsBase.dll>

#include <zed-core.h>

using System::Windows::Threading::Dispatcher;

namespace Frida
{
  ref class Session;

  public ref class SessionManager
  {
  public:
    SessionManager (Dispatcher ^ dispatcher);
    ~SessionManager ();

    Session ^ ObtainSessionFor (int pid);

  private:
    Dispatcher ^ dispatcher;
    FridaSessionManager * handle;
  };
}