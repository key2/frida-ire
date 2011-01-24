#include "cloud-spy-object.h"

#include "cloud-spy.h"

#include <glib-object.h>
#define VC_EXTRALEAN
#include <windows.h>
#include <tchar.h>
#include "npfunctions.h"

static NPNetscapeFuncs * cloud_spy_nsfuncs = NULL;

static NPObject * cloud_spy_root_object = NULL;

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
      *((char **) value) = "CloudSpyPlugin";
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
  /*MessageBox (NULL, _T ("Yo"), _T ("NP_Initialize"), MB_ICONINFORMATION | MB_OK);*/

  if (nf == NULL)
    return NPERR_INVALID_FUNCTABLE_ERROR;

  if (HIBYTE (nf->version) > NP_VERSION_MAJOR)
    return NPERR_INCOMPATIBLE_VERSION_ERROR;

  cloud_spy_nsfuncs = nf;
  cloud_spy_object_type_init (nf);

  return NPERR_NO_ERROR;
}

NPError OSCALL
NP_Shutdown (void)
{
  /*MessageBox (NULL, _T ("Yo"), _T ("NP_Shutdown"), MB_ICONINFORMATION | MB_OK);*/

  cloud_spy_object_type_deinit ();
  cloud_spy_nsfuncs = NULL;

  return NPERR_NO_ERROR;
}

char *
NP_GetMIMEDescription (void)
{
  return "application/x-vnd-cloud-spy:.cloud-spy:oleavr@gmail.com";
}