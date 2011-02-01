#include "zed-tests.h"

#include <windows.h>
#include <psapi.h>

char *
zed_test_process_backend_filename_of (void * handle)
{
  WCHAR filename_utf16[MAX_PATH + 1];

  GetModuleFileNameExW (handle, NULL, filename_utf16, sizeof (filename_utf16));

  return g_utf16_to_utf8 (filename_utf16, -1, NULL, NULL, NULL);
}

void *
zed_test_process_backend_self_handle (void)
{
  return GetCurrentProcess ();
}

gulong
zed_test_process_backend_self_id (void)
{
  return GetCurrentProcessId ();
}

void
zed_test_process_backend_do_start (const char * filename,
    void ** handle, gulong * id, GError ** error)
{
  LPWSTR filename_utf16;
  STARTUPINFOW startup_info = { 0, };
  PROCESS_INFORMATION process_info = { 0, };
  BOOL success;

  filename_utf16 =
      g_utf8_to_utf16 (filename, -1, NULL, NULL, NULL);

  startup_info.cb = sizeof (startup_info);

  success = CreateProcessW (filename_utf16, NULL, NULL, NULL, FALSE, 0, NULL,
      NULL, &startup_info, &process_info);

  if (success)
  {
    CloseHandle (process_info.hThread);

    *handle = process_info.hProcess;
    *id = process_info.dwProcessId;
  }
  else
  {
    g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
        "CreateProcess failed: 0x%08x\n", GetLastError ());
  }

  g_free (filename_utf16);
}

int
zed_test_process_backend_do_join (void * handle, guint timeout_msec,
    GError ** error)
{
  DWORD exit_code;

  if (WaitForSingleObject (handle,
      (timeout_msec != 0) ? timeout_msec : INFINITE) == WAIT_TIMEOUT)
  {
    g_set_error (error, G_IO_ERROR, G_IO_ERROR_TIMED_OUT, "timed out");
    return -1;
  }

  GetExitCodeProcess (handle, &exit_code);
  CloseHandle (handle);

  return exit_code;
}
