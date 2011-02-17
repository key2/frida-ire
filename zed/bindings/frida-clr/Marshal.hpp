#pragma once

#include <glib.h>

namespace Frida
{
  private ref class Marshal
  {
  public:
    static System::String ^ UTF8CStringToClrString (const char * str);
    static char * ClrStringToUTF8CString (System::String ^ str);

    static void ThrowGErrorIfSet (GError ** error);
  };
}