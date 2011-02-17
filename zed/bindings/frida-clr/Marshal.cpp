#include "Marshal.hpp"

#include <glib.h>
#include <msclr/marshal.h>

namespace Frida
{
  System::String ^
  Marshal::UTF8CStringToClrString (const char * str)
  {
    wchar_t * strUtf16 = reinterpret_cast<wchar_t *> (g_utf8_to_utf16 (str, -1, NULL, NULL, NULL));
    System::String ^ result = gcnew System::String (strUtf16);
    g_free (strUtf16);
    return result;
  }

  char *
  Marshal::ClrStringToUTF8CString (System::String ^ str)
  {
    msclr::interop::marshal_context ^ context = gcnew msclr::interop::marshal_context ();
    const wchar_t * strUtf16 = context->marshal_as<const wchar_t *> (str);
    gchar * strUtf8 = g_utf16_to_utf8 (reinterpret_cast<const gunichar2 *> (strUtf16), -1, NULL, NULL, NULL);
    delete context;
    return strUtf8;
  }

  void
  Marshal::ThrowGErrorIfSet (GError ** error)
  {
    if (*error == NULL)
      return;
    System::String ^ message = UTF8CStringToClrString ((*error)->message);
    g_clear_error (error);
    throw gcnew System::Exception (message);
  }
}