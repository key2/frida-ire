#include <glib.h>
#include <windows.h>

#ifdef _M_X64
#define WINJECTOR_SERVICE_ARCH "64"
#else
#define WINJECTOR_SERVICE_ARCH "32"
#endif

typedef struct _WinjectorServiceContext WinjectorServiceContext;

struct _WinjectorServiceContext
{
  gchar * service_basename;

  SC_HANDLE scm;
  SC_HANDLE service32;
  SC_HANDLE service64;
};

static void WINAPI winjector_managed_service_main (DWORD argc, WCHAR ** argv);
static DWORD WINAPI winjector_managed_service_handle_control_code (
    DWORD control, DWORD event_type, void * event_data, void * context);
static void winjector_managed_service_report_status (DWORD current_state,
    DWORD exit_code, DWORD wait_hint);

static gboolean register_and_start_services (WinjectorServiceContext * self);
static void stop_and_unregister_services (WinjectorServiceContext * self);
static gboolean spawn_standalone_services (WinjectorServiceContext * self);
static void join_standalone_services (WinjectorServiceContext * self);

static gboolean register_services (WinjectorServiceContext * self);
static gboolean unregister_services (WinjectorServiceContext * self);
static gboolean start_services (WinjectorServiceContext * self);
static gboolean stop_services (WinjectorServiceContext * self);

static SC_HANDLE register_service (WinjectorServiceContext * self,
    const gchar * suffix);
static gboolean unregister_service (WinjectorServiceContext * self,
    SC_HANDLE handle);
static gboolean start_service (WinjectorServiceContext * self,
    SC_HANDLE handle);
static gboolean stop_service (WinjectorServiceContext * self,
    SC_HANDLE handle);

static WinjectorServiceContext * winjector_service_context_new (
    const gchar * service_basename);
static void winjector_service_context_free (WinjectorServiceContext * self);

static WCHAR * winjector_managed_service_name = NULL;
static SERVICE_STATUS_HANDLE winjector_managed_service_status_handle = NULL;

gboolean
winjector_manager_system_is_x64 (void)
{
  static gboolean initialized = FALSE;
  static gboolean system_is_x64;

  if (!initialized) {
    SYSTEM_INFO si;

    GetNativeSystemInfo (&si);
    system_is_x64 = si.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_AMD64;

    initialized = TRUE;
  }

  return system_is_x64;
}

void *
winjector_manager_start_services (const char * service_basename)
{
  WinjectorServiceContext * self;

  self = winjector_service_context_new (service_basename);

  self->scm = OpenSCManager (NULL, NULL, SC_MANAGER_ALL_ACCESS);
  if (self->scm != NULL)
  {
    if (!register_and_start_services (self))
    {
      CloseServiceHandle (self->scm);
      self->scm = NULL;
    }
  }

  if (self->scm == NULL)
  {
    if (!spawn_standalone_services (self))
    {
      winjector_service_context_free (self);
      self = NULL;
    }
  }

  return self;
}

void
winjector_manager_stop_services (void * context)
{
  WinjectorServiceContext * self = context;

  if (self->scm != NULL)
    stop_and_unregister_services (self);
  else
    join_standalone_services (self);

  winjector_service_context_free (self);
}

char *
winjector_service_derive_basename (void)
{
  WCHAR filename_utf16[MAX_PATH + 1] = { 0, };
  gchar * name, * tmp;

  GetModuleFileNameW (NULL, filename_utf16, MAX_PATH);

  name = g_utf16_to_utf8 (filename_utf16, -1, NULL, NULL, NULL);

  tmp = g_path_get_dirname (name);
  g_free (name);
  name = tmp;

  tmp = g_path_get_basename (name);
  g_free (name);
  name = tmp;

  tmp = g_strconcat (name, "-", NULL);
  g_free (name);
  name = tmp;

  return name;
}

char *
winjector_service_derive_filename_for_suffix (const char * suffix)
{
  WCHAR filename_utf16[MAX_PATH + 1] = { 0, };
  gchar * name, * tmp;
  glong len;

  GetModuleFileNameW (NULL, filename_utf16, MAX_PATH);

  name = g_utf16_to_utf8 (filename_utf16, -1, NULL, &len, NULL);
  if (g_str_has_suffix (name, "-32.exe") || g_str_has_suffix (name, "-64.exe"))
  {
    name[len - 6] = '\0';
    tmp = g_strconcat (name, suffix, ".exe", NULL);
    g_free (name);
    name = tmp;
  }
  else
  {
    g_critical ("Unexpected filename: %s", name);
  }

  return name;
}

char *
winjector_service_derive_svcname_for_self (void)
{
  gchar * basename, * name;

  basename = winjector_service_derive_basename ();
  name = g_strconcat (basename, WINJECTOR_SERVICE_ARCH, NULL);
  g_free (basename);

  return name;
}

char *
winjector_service_derive_svcname_for_suffix (const char * suffix)
{
  gchar * basename, * name;

  basename = winjector_service_derive_basename ();
  name = g_strconcat (basename, suffix, NULL);
  g_free (basename);

  return name;
}

void
winjector_managed_service_enter_dispatcher_and_main_loop (void)
{
  SERVICE_TABLE_ENTRYW dispatch_table[2] = { 0, };
  gchar * name;

  name = winjector_service_derive_svcname_for_self ();
  winjector_managed_service_name = g_utf8_to_utf16 (name, -1, NULL, NULL,
      NULL);
  g_free (name);

  dispatch_table[0].lpServiceName = winjector_managed_service_name;
  dispatch_table[0].lpServiceProc = winjector_managed_service_main;

  StartServiceCtrlDispatcherW (dispatch_table);

  winjector_managed_service_status_handle = NULL;

  g_free (winjector_managed_service_name);
  winjector_managed_service_name = NULL;
}

static void WINAPI
winjector_managed_service_main (DWORD argc, WCHAR ** argv)
{
  GMainLoop * loop;

  loop = g_main_loop_new (NULL, FALSE);

  winjector_managed_service_status_handle = RegisterServiceCtrlHandlerExW (
      winjector_managed_service_name,
      winjector_managed_service_handle_control_code,
      loop);

  winjector_managed_service_report_status (SERVICE_START_PENDING, NO_ERROR, 0);

  winjector_managed_service_report_status (SERVICE_RUNNING, NO_ERROR, 0);
  g_main_loop_run (loop);
  winjector_managed_service_report_status (SERVICE_STOPPED, NO_ERROR, 0);

  g_main_loop_unref (loop);
}

static gboolean
winjector_managed_service_stop (gpointer data)
{
  GMainLoop * loop = data;

  g_main_loop_quit (loop);

  return FALSE;
}

static DWORD WINAPI
winjector_managed_service_handle_control_code (DWORD control, DWORD event_type,
    void * event_data, void * context)
{
  GMainLoop * loop = context;

  switch (control)
  {
    case SERVICE_CONTROL_STOP:
      winjector_managed_service_report_status (SERVICE_STOP_PENDING, NO_ERROR,
          0);
      g_idle_add (winjector_managed_service_stop, loop);
      return NO_ERROR;

    case SERVICE_CONTROL_INTERROGATE:
      return NO_ERROR;

    default:
      return ERROR_CALL_NOT_IMPLEMENTED;
  }
}

static void
winjector_managed_service_report_status (DWORD current_state, DWORD exit_code,
    DWORD wait_hint)
{
  SERVICE_STATUS status;
  static DWORD checkpoint = 1;

  status.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
  status.dwCurrentState = current_state;

  if (current_state == SERVICE_START_PENDING)
    status.dwControlsAccepted = 0;
  else
    status.dwControlsAccepted = SERVICE_ACCEPT_STOP;

  status.dwWin32ExitCode = exit_code;
  status.dwServiceSpecificExitCode = 0;

  if (current_state == SERVICE_RUNNING || current_state == SERVICE_STOPPED)
  {
    status.dwCheckPoint = 0;
  }
  else
  {
    status.dwCheckPoint = checkpoint++;
  }

  status.dwWaitHint = wait_hint;

  SetServiceStatus (winjector_managed_service_status_handle, &status);
}

static gboolean
register_and_start_services (WinjectorServiceContext * self)
{
  if (!register_services (self))
    return FALSE;

  if (!start_services (self))
  {
    unregister_services (self);
    return FALSE;
  }

  return TRUE;
}

static void
stop_and_unregister_services (WinjectorServiceContext * self)
{
  stop_services (self);
  unregister_services (self);
}

static gboolean
spawn_standalone_services (WinjectorServiceContext * self)
{
  g_assert_not_reached ();
  return FALSE;
}

static void
join_standalone_services (WinjectorServiceContext * self)
{
  g_assert_not_reached ();
}

static gboolean
register_services (WinjectorServiceContext * self)
{
  SC_HANDLE service32, service64;

  service32 = register_service (self, "32");
  if (service32 == NULL)
    return FALSE;

  if (winjector_manager_system_is_x64 ())
  {
    service64 = register_service (self, "64");
    if (service64 == NULL)
    {
      unregister_service (self, service32);
      CloseServiceHandle (service32);
      return FALSE;
    }
  }
  else
  {
    service64 = NULL;
  }

  self->service32 = service32;
  self->service64 = service64;

  return TRUE;
}

static gboolean
unregister_services (WinjectorServiceContext * self)
{
  gboolean success = TRUE;

  if (winjector_manager_system_is_x64 ())
  {
    success &= unregister_service (self, self->service64);
    CloseServiceHandle (self->service64);
    self->service64 = NULL;
  }

  success &= unregister_service (self, self->service32);
  CloseServiceHandle (self->service32);
  self->service32 = NULL;

  return success;
}

static gboolean
start_services (WinjectorServiceContext * self)
{
  if (!start_service (self, self->service32))
    return FALSE;

  if (winjector_manager_system_is_x64 ())
  {
    if (!start_service (self, self->service64))
    {
      stop_service (self, self->service32);
      return FALSE;
    }
  }

  return TRUE;
}

static gboolean
stop_services (WinjectorServiceContext * self)
{
  gboolean success = TRUE;

  if (winjector_manager_system_is_x64 ())
    success &= stop_service (self, self->service64);

  success &= stop_service (self, self->service32);

  return success;
}

static SC_HANDLE
register_service (WinjectorServiceContext * self, const gchar * suffix)
{
  SC_HANDLE handle;
  gchar * servicename_utf8;
  WCHAR * servicename;
  gchar * displayname_utf8;
  WCHAR * displayname;
  gchar * filename_utf8;
  WCHAR * filename;

  servicename_utf8 = g_strconcat (self->service_basename, suffix, NULL);
  servicename = g_utf8_to_utf16 (servicename_utf8, -1, NULL, NULL, NULL);

  displayname_utf8 = g_strdup_printf ("Zed Winjector %s-bit helper (%s)",
      suffix, servicename_utf8);
  displayname = g_utf8_to_utf16 (displayname_utf8, -1, NULL, NULL, NULL);

  filename_utf8 = winjector_service_derive_filename_for_suffix (suffix);
  filename = g_utf8_to_utf16 (filename_utf8, -1, NULL, NULL, NULL);

  handle = CreateServiceW (self->scm,
      servicename,
      displayname,
      SERVICE_ALL_ACCESS,
      SERVICE_WIN32_OWN_PROCESS,
      SERVICE_DEMAND_START,
      SERVICE_ERROR_NORMAL,
      filename,
      NULL,
      NULL,
      NULL,
      NULL,
      NULL);

  g_free (filename);
  g_free (filename_utf8);

  g_free (displayname);
  g_free (displayname_utf8);

  g_free (servicename);
  g_free (servicename_utf8);

  return handle;
}

static gboolean
unregister_service (WinjectorServiceContext * self, SC_HANDLE handle)
{
  return DeleteService (handle);
}

static gboolean
start_service (WinjectorServiceContext * self, SC_HANDLE handle)
{
  return StartService (handle, 0, NULL);
}

static gboolean
stop_service (WinjectorServiceContext * self, SC_HANDLE handle)
{
  SERVICE_STATUS status = { 0, };
  return ControlService (handle, SERVICE_CONTROL_STOP, &status);
}

static WinjectorServiceContext *
winjector_service_context_new (const gchar * service_basename)
{
  WinjectorServiceContext * self;

  self = g_new0 (WinjectorServiceContext, 1);
  self->service_basename = g_strdup (service_basename);

  return self;
}

static void
winjector_service_context_free (WinjectorServiceContext * self)
{
  g_assert (self->service64 == NULL);
  g_assert (self->service32 == NULL);

  if (self->scm != NULL)
    CloseServiceHandle (self->scm);

  g_free (self->service_basename);

  g_free (self);
}
