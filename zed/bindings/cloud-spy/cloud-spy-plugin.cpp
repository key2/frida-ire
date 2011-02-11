#include "cloud-spy-plugin.h"

#include "cloud-spy.h"
#include "cloud-spy-object.h"

#include <glib-object.h>
#define VC_EXTRALEAN
#include <windows.h>
#include <tchar.h>
#include "npfunctions.h"

NPNetscapeFuncs * cloud_spy_nsfuncs = NULL;
GMainContext * cloud_spy_main_context = NULL;
static GMainLoop * cloud_spy_main_loop = NULL;
static GThread * cloud_spy_main_thread = NULL;
static NPObject * cloud_spy_root_object = NULL;

static gpointer
cloud_spy_run_main_loop (gpointer data)
{
  (void) data;

  g_main_context_push_thread_default (cloud_spy_main_context);
  g_main_loop_run (cloud_spy_main_loop);
  g_main_context_pop_thread_default (cloud_spy_main_context);

  return NULL;
}

static gboolean
cloud_spy_stop_main_loop (gpointer data)
{
  (void) data;

  g_main_loop_quit (cloud_spy_main_loop);

  return FALSE;
}

static NPError
cloud_spy_plugin_new (NPMIMEType plugin_type, NPP instance, uint16_t mode, int16_t argc, char * argn[], char * argv[],
    NPSavedData * saved)
{
  (void) plugin_type;
  (void) instance;
  (void) mode;
  (void) argc;
  (void) argn;
  (void) argv;
  (void) saved;

  return NPERR_NO_ERROR;
}

static NPError
cloud_spy_plugin_destroy (NPP instance, NPSavedData ** saved)
{
  (void) instance;
  (void) saved;

  if (cloud_spy_root_object != NULL)
    cloud_spy_nsfuncs->releaseobject (cloud_spy_root_object);
  cloud_spy_root_object = NULL;

  return NPERR_NO_ERROR;
}

static NPError
cloud_spy_plugin_set_window (NPP instance, NPWindow * window)
{
  (void) instance;
  (void) window;

  return NPERR_NO_ERROR;
}

static int16_t
cloud_spy_plugin_handle_event (NPP instance, void * ev)
{
  (void) instance;
  (void) ev;

  return NPERR_NO_ERROR;
}

static NPError
cloud_spy_plugin_get_value (NPP instance, NPPVariable variable, void * value)
{
  (void) instance;

  switch (variable)
  {
    case NPPVpluginNameString:
      *((char **) value) = "CloudSpy";
      break;
    case NPPVpluginDescriptionString:
      *((char **) value) = "<a href=\"http://apps.facebook.com/cloud_spy/\">CloudSpy</a> plugin.";
      break;
    case NPPVpluginScriptableNPObject:
      if (cloud_spy_root_object == NULL)
        cloud_spy_root_object = cloud_spy_nsfuncs->createobject (instance, static_cast<NPClass *> (cloud_spy_object_type_get_np_class (CLOUD_SPY_TYPE_ROOT)));
      cloud_spy_nsfuncs->retainobject (cloud_spy_root_object);
      *((NPObject **) value) = cloud_spy_root_object;
      break;
    default:
      return NPERR_GENERIC_ERROR;
  }

  return NPERR_NO_ERROR;
}

NPError OSCALL
NP_GetEntryPoints (NPPluginFuncs * pf)
{
  g_type_init ();

  pf->version = (NP_VERSION_MAJOR << 8) | NP_VERSION_MINOR;
  pf->newp = cloud_spy_plugin_new;
  pf->destroy = cloud_spy_plugin_destroy;
  pf->setwindow = cloud_spy_plugin_set_window;
  pf->event = cloud_spy_plugin_handle_event;
  pf->getvalue = cloud_spy_plugin_get_value;

  return NPERR_NO_ERROR;
}

NPError OSCALL
NP_Initialize (NPNetscapeFuncs * nf)
{
  if (nf == NULL)
    return NPERR_INVALID_FUNCTABLE_ERROR;

  if (HIBYTE (nf->version) > NP_VERSION_MAJOR)
    return NPERR_INCOMPATIBLE_VERSION_ERROR;

  cloud_spy_object_type_init ();

  cloud_spy_nsfuncs = nf;

  cloud_spy_main_context = g_main_context_new ();
  cloud_spy_main_loop = g_main_loop_new (cloud_spy_main_context, FALSE);
  cloud_spy_main_thread = g_thread_create (cloud_spy_run_main_loop, NULL, TRUE, NULL);

  return NPERR_NO_ERROR;
}

NPError OSCALL
NP_Shutdown (void)
{
  GSource * source;

  source = g_idle_source_new ();
  g_source_set_priority (source, G_PRIORITY_LOW);
  g_source_set_callback (source, cloud_spy_stop_main_loop, NULL, NULL);
  g_source_attach (source, cloud_spy_main_context);
  g_source_unref (source);

  g_thread_join (cloud_spy_main_thread);
  cloud_spy_main_thread = NULL;
  g_main_loop_unref (cloud_spy_main_loop);
  cloud_spy_main_loop = NULL;
  g_main_context_unref (cloud_spy_main_context);
  cloud_spy_main_context = NULL;

  cloud_spy_nsfuncs = NULL;

  cloud_spy_object_type_deinit ();

  return NPERR_NO_ERROR;
}

char *
NP_GetMIMEDescription (void)
{
  return "application/x-vnd-cloud-spy:.cspy:oleavr@gmail.com";
}

void
cloud_spy_init_npvariant_with_string (NPVariant * var, const gchar * str)
{
  gsize len = strlen (str);
  NPUTF8 * str_copy = static_cast<NPUTF8 *> (cloud_spy_nsfuncs->memalloc (len));
  memcpy (str_copy, str, len);
  STRINGN_TO_NPVARIANT (str_copy, len, *var);
}