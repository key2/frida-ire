#include "Application.hpp"

#include <glib-object.h>
#include <gio/gio.h>
#include <gum/gum.h>

G_BEGIN_DECLS

struct _GMainLoop
{
  gpointer foo;
};

struct _GMainContext
{
  gpointer foo;
};

G_END_DECLS

namespace Frida
{
  class ApplicationContext
  {
  public:
    ApplicationContext ()
    {
      g_type_init ();
      gum_init_with_features ((GumFeatureFlags)
          (GUM_FEATURE_ALL & ~GUM_FEATURE_SYMBOL_LOOKUP));

      mainContext = g_main_context_new ();
      mainLoop = g_main_loop_new (mainContext, FALSE);
      mainThread = g_thread_create (MainLoopWrapper, this, TRUE, NULL);
    }

    ~ApplicationContext ()
    {
      GSource * source = g_idle_source_new ();
      g_source_set_priority (source, G_PRIORITY_LOW);
      g_source_set_callback (source, StopMainLoopWrapper, this, NULL);
      g_source_attach (source, mainContext);
      g_source_unref (source);

      g_thread_join (mainThread);
      g_main_loop_unref (mainLoop);
      g_main_context_unref (mainContext);

      g_io_deinit ();

      gum_deinit ();
      g_type_deinit ();
      g_thread_deinit ();
      g_mem_deinit ();
    }

    GMainContext *
    MainContext () const
    {
      return mainContext;
    }

  private:
    void MainLoop ()
    {
      g_main_context_push_thread_default (mainContext);
      g_main_loop_run (mainLoop);
      g_main_context_pop_thread_default (mainContext);
    }

    void
    StopMainLoop ()
    {
      g_main_loop_quit (mainLoop);
    }

    static gpointer
    MainLoopWrapper (gpointer data)
    {
      static_cast<ApplicationContext *> (data)->MainLoop ();
      return NULL;
    }

    static gboolean
    StopMainLoopWrapper (gpointer data)
    {
      static_cast<ApplicationContext *> (data)->StopMainLoop ();
      return FALSE;
    }

    GThread * mainThread;
    GMainLoop * mainLoop;
    GMainContext * mainContext;
  };
  static ApplicationContext applicationContext;

  void *
  Application::MainContext ()
  {
    return applicationContext.MainContext ();
  }
}