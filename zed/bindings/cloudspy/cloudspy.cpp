#include <glib-object.h>
#define VC_EXTRALEAN
#include <windows.h>
#include <tchar.h>
#include "npfunctions.h"

static NPNetscapeFuncs * cloudspy_nsfuncs = NULL;

static NPError
cloudspy_plugin_new (NPMIMEType plugin_type, NPP instance, uint16_t mode, int16_t argc, char * argn[], char * argv[],
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
cloudspy_plugin_destroy (NPP instance, NPSavedData ** saved)
{
  (void) instance;
  (void) saved;

  return NPERR_NO_ERROR;
}

static NPError
cloudspy_plugin_set_window (NPP instance, NPWindow * window)
{
  (void) instance;
  (void) window;

  return NPERR_NO_ERROR;
}

static int16_t
cloudspy_plugin_handle_event (NPP instance, void * ev)
{
  (void) instance;
  (void) ev;

  return NPERR_NO_ERROR;
}

static NPError
cloudspy_plugin_get_value (NPP instance, NPPVariable variable, void * value)
{
  (void) instance;

  switch (variable)
  {
    case NPPVpluginNameString:
      *((char **) value) = "CloudSpyPlugin";
      break;

    case NPPVpluginDescriptionString:
      *((char **) value) = "<a href=\"http://apps.facebook.com/cloudspy/\">CloudSpy</a> plugin.";
      break;

    default:
      return NPERR_GENERIC_ERROR;
  }

  return NPERR_NO_ERROR;
}

NPError OSCALL
NP_GetEntryPoints (NPPluginFuncs * pf)
{
  MessageBox (NULL, _T ("Yo"), _T ("NP_GetEntryPoints"), MB_ICONINFORMATION | MB_OK);

  g_type_init ();

  pf->version = (NP_VERSION_MAJOR << 8) | NP_VERSION_MINOR;
  pf->newp = cloudspy_plugin_new;
  pf->destroy = cloudspy_plugin_destroy;
  pf->setwindow = cloudspy_plugin_set_window;
  pf->event = cloudspy_plugin_handle_event;
  pf->getvalue = cloudspy_plugin_get_value;

  return NPERR_NO_ERROR;
}

NPError OSCALL
NP_Initialize (NPNetscapeFuncs * nf)
{
  MessageBox (NULL, _T ("Yo"), _T ("NP_Initialize"), MB_ICONINFORMATION | MB_OK);

  if (nf == NULL)
    return NPERR_INVALID_FUNCTABLE_ERROR;

  if (HIBYTE (nf->version) > NP_VERSION_MAJOR)
    return NPERR_INCOMPATIBLE_VERSION_ERROR;

  cloudspy_nsfuncs = nf;

  return NPERR_NO_ERROR;
}

NPError OSCALL
NP_Shutdown (void)
{
  MessageBox (NULL, _T ("Yo"), _T ("NP_Shutdown"), MB_ICONINFORMATION | MB_OK);

  cloudspy_nsfuncs = NULL;

  return NPERR_NO_ERROR;
}

char *
NP_GetMIMEDescription (void)
{
  MessageBox (NULL, _T ("Yo"), _T ("NP_GetMIMEDescription"), MB_ICONINFORMATION | MB_OK);

  return "application/x-vnd-cloudspy:.cloudspy:oleavr@gmail.com";
}
