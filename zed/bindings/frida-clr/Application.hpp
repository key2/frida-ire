#pragma once

#include <glib.h>

namespace Frida
{
  struct ApplicationContext;

  public ref class Application
  {
  public:
    static void Init ();
    static void Deinit ();

    static void * GetMainContext ();

  private:
    static ApplicationContext * appCtx;
    static GThread * mainThread = NULL;
  };
}