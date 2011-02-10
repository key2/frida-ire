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
  static gpointer MainLoop (gpointer data);
  static gboolean StopMainLoop (gpointer data);

  struct ApplicationContext
  {
    GMainLoop * mainLoop;
    GMainContext * mainContext;
  };

  void
  Application::Init ()
  {
    g_type_init ();
    gum_init_with_features ((GumFeatureFlags)
        (GUM_FEATURE_ALL & ~GUM_FEATURE_SYMBOL_LOOKUP));

    appCtx = g_slice_new (ApplicationContext);
    appCtx->mainContext = g_main_context_new ();
    appCtx->mainLoop = g_main_loop_new (appCtx->mainContext, FALSE);
    mainThread = g_thread_create (MainLoop, appCtx, TRUE, NULL);
  }

  void
  Application::Deinit ()
  {
    GSource * source = g_idle_source_new ();
    g_source_set_priority (source, G_PRIORITY_LOW);
    g_source_set_callback (source, StopMainLoop, appCtx, NULL);
    g_source_attach (source, appCtx->mainContext);
    g_source_unref (source);

    g_thread_join (mainThread);
    g_main_loop_unref (appCtx->mainLoop);
    g_main_context_unref (appCtx->mainContext);

    g_slice_free (ApplicationContext, appCtx);

    g_io_deinit ();

    gum_deinit ();
    g_type_deinit ();
    g_thread_deinit ();
    g_mem_deinit ();
  }

  void *
  Application::GetMainContext ()
  {
    return appCtx->mainContext;
  }

  static gpointer
  MainLoop (gpointer data)
  {
    ApplicationContext * appCtx = static_cast<ApplicationContext *> (data);

    g_main_context_push_thread_default (appCtx->mainContext);
    g_main_loop_run (appCtx->mainLoop);
    g_main_context_pop_thread_default (appCtx->mainContext);

    return NULL;
  }

  static gboolean
  StopMainLoop (gpointer data)
  {
    g_main_loop_quit (static_cast<ApplicationContext *> (data)->mainLoop);

    return FALSE;
  }
}