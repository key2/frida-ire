#include "windows-icon-helpers.h"

#include <psapi.h>

typedef struct _FindMainWindowCtx FindMainWindowCtx;

typedef BOOL (WINAPI * Wow64DisableWow64FsRedirectionFunc) (PVOID * OldValue);
typedef BOOL (WINAPI * Wow64RevertWow64FsRedirectionFunc) (PVOID OldValue);

struct _FindMainWindowCtx
{
  DWORD pid;
  HWND main_window;
};

static HWND find_main_window_of_pid (DWORD pid);
static BOOL CALLBACK inspect_window (HWND hwnd, LPARAM lparam);

ZedImageData *
_zed_image_data_from_process_or_file (DWORD pid, WCHAR * filename, ZedIconSize size)
{
  ZedImageData * icon;

  icon = _zed_image_data_from_process (pid, size);
  if (icon == NULL)
    icon = _zed_image_data_from_file (filename, size);

  if (icon == NULL)
  {
    icon = g_new (ZedImageData, 1);
    zed_image_data_init (icon, 0, 0, 0, "");
  }

  return icon;
}

ZedImageData *
_zed_image_data_from_process (DWORD pid, ZedIconSize size)
{
  ZedImageData * result = NULL;
  HICON icon = NULL;
  HWND main_window;

  main_window = find_main_window_of_pid (pid);
  if (main_window != NULL)
  {
    UINT flags, timeout;

    flags = SMTO_ABORTIFHUNG | SMTO_BLOCK;
    timeout = 100;

    if (size == ZED_ICON_SMALL)
    {
      SendMessageTimeout (main_window, WM_GETICON, ICON_SMALL2, 0,
          flags, timeout, (PDWORD_PTR) &icon);

      if (icon == NULL)
      {
        SendMessageTimeout (main_window, WM_GETICON, ICON_SMALL, 0,
            flags, timeout, (PDWORD_PTR) &icon);
      }

      if (icon == NULL)
        icon = (HICON) GetClassLongPtr (main_window, GCLP_HICONSM);
    }
    else if (size == ZED_ICON_LARGE)
    {
      SendMessageTimeout (main_window, WM_GETICON, ICON_BIG, 0,
          flags, timeout, (PDWORD_PTR) &icon);

      if (icon == NULL)
        icon = (HICON) GetClassLongPtr (main_window, GCLP_HICON);

      if (icon == NULL)
      {
        SendMessageTimeout (main_window, WM_QUERYDRAGICON, 0, 0,
            flags, timeout, (PDWORD_PTR) &icon);
      }
    }
    else
    {
      g_assert_not_reached ();
    }
  }

  if (icon != NULL)
    result = _zed_image_data_from_native_icon_handle (icon, size);

  return result;
}

ZedImageData *
_zed_image_data_from_file (WCHAR * filename, ZedIconSize size)
{
  ZedImageData * result = NULL;
  SHFILEINFO shfi = { 0, };
  UINT flags;

  flags = SHGFI_ICON;
  if (size == ZED_ICON_SMALL)
    flags |= SHGFI_SMALLICON;
  else if (size == ZED_ICON_LARGE)
    flags |= SHGFI_LARGEICON;
  else
    g_assert_not_reached ();

  SHGetFileInfoW (filename, 0, &shfi, sizeof (shfi), flags);
  if (shfi.hIcon != NULL)
    result = _zed_image_data_from_native_icon_handle (shfi.hIcon, size);

  return result;
}

ZedImageData *
_zed_image_data_from_resource_url (WCHAR * resource_url, ZedIconSize size)
{
  static gboolean api_initialized = FALSE;
  static Wow64DisableWow64FsRedirectionFunc Wow64DisableWow64FsRedirectionImpl = NULL;
  static Wow64RevertWow64FsRedirectionFunc Wow64RevertWow64FsRedirectionImpl = NULL;
  ZedImageData * result = NULL;
  WCHAR * resource_file = NULL;
  DWORD resource_file_length;
  WCHAR * p;
  gint resource_id;
  PVOID old_redirection_value = NULL;
  UINT ret;
  HICON icon = NULL;

  if (!api_initialized)
  {
    HMODULE kmod;

    kmod = GetModuleHandleW (L"kernel32.dll");
    g_assert (kmod != NULL);

    Wow64DisableWow64FsRedirectionImpl = (Wow64DisableWow64FsRedirectionFunc) GetProcAddress (kmod, "Wow64DisableWow64FsRedirection");
    Wow64RevertWow64FsRedirectionImpl = (Wow64RevertWow64FsRedirectionFunc) GetProcAddress (kmod, "Wow64RevertWow64FsRedirection");
    g_assert ((Wow64DisableWow64FsRedirectionImpl != NULL) == (Wow64RevertWow64FsRedirectionImpl != NULL));

    api_initialized = TRUE;
  }

  resource_file_length = ExpandEnvironmentStringsW (resource_url, NULL, 0);
  if (resource_file_length == 0)
    goto beach;
  resource_file = (WCHAR *) g_malloc ((resource_file_length + 1) * sizeof (WCHAR));
  if (ExpandEnvironmentStringsW (resource_url, resource_file, resource_file_length) == 0)
    goto beach;

  p = wcsrchr (resource_file, L',');
  if (p == NULL)
    goto beach;
  *p = L'\0';

  resource_id = wcstol (p + 1, NULL, 10);

  if (Wow64DisableWow64FsRedirectionImpl != NULL)
    Wow64DisableWow64FsRedirectionImpl (&old_redirection_value);

  ret = ExtractIconExW (resource_file, resource_id, (size == ZED_ICON_LARGE) ? &icon : NULL, (size == ZED_ICON_SMALL) ? &icon : NULL, 1);

  if (Wow64RevertWow64FsRedirectionImpl != NULL)
    Wow64RevertWow64FsRedirectionImpl (old_redirection_value);

  if (ret != 1)
    goto beach;

  result = _zed_image_data_from_native_icon_handle (icon, size);

beach:
  if (icon != NULL)
    DestroyIcon (icon);
  g_free (resource_file);

  return result;
}

ZedImageData *
_zed_image_data_from_native_icon_handle (HICON icon, ZedIconSize size)
{
  ZedImageData * result;
  GVariantBuilder * builder;
  HDC dc;
  gint width = -1, height = -1;
  BITMAPV5HEADER bi = { 0, };
  guint rowstride;
  guchar * data = NULL;
  gchar * data_base64;
  HBITMAP bm;
  guint i;

  dc = CreateCompatibleDC (NULL);

  if (size == ZED_ICON_SMALL)
  {
    width = GetSystemMetrics (SM_CXSMICON);
    height = GetSystemMetrics (SM_CYSMICON);
  }
  else if (size == ZED_ICON_LARGE)
  {
    width = GetSystemMetrics (SM_CXICON);
    height = GetSystemMetrics (SM_CYICON);
  }
  else
  {
    g_assert_not_reached ();
  }

  bi.bV5Size = sizeof (bi);
  bi.bV5Width = width;
  bi.bV5Height = -height;
  bi.bV5Planes = 1;
  bi.bV5BitCount = 32;
  bi.bV5Compression = BI_BITFIELDS;
  bi.bV5RedMask   = 0x00ff0000;
  bi.bV5GreenMask = 0x0000ff00;
  bi.bV5BlueMask  = 0x000000ff;
  bi.bV5AlphaMask = 0xff000000;

  rowstride = width * (bi.bV5BitCount / 8);

  bm = CreateDIBSection (dc, (BITMAPINFO *) &bi, DIB_RGB_COLORS, (void **) &data, NULL, 0);

  SelectObject (dc, bm);
  DrawIconEx (dc, 0, 0, icon, width, height, 0, NULL, DI_NORMAL);
  GdiFlush ();

  for (i = 0; i != rowstride * height; i += 4)
  {
    guchar hold;

    hold = data[i + 0];
    data[i + 0] = data[i + 2];
    data[i + 2] = hold;
  }

  result = g_new (ZedImageData, 1);
  data_base64 = g_base64_encode (data, rowstride * height);
  zed_image_data_init (result, width, height, width * 4, data_base64);
  g_free (data_base64);

  DeleteObject (bm);

  DeleteDC (dc);

  return result;
}

static HWND
find_main_window_of_pid (DWORD pid)
{
  FindMainWindowCtx ctx;

  ctx.pid = pid;
  ctx.main_window = NULL;

  EnumWindows (inspect_window, (LPARAM) &ctx);

  return ctx.main_window;
}

static BOOL CALLBACK
inspect_window (HWND hwnd, LPARAM lparam)
{
  if ((GetWindowLong (hwnd, GWL_STYLE) & WS_VISIBLE) != 0)
  {
    FindMainWindowCtx * ctx = (FindMainWindowCtx *) lparam;
    DWORD pid;

    GetWindowThreadProcessId (hwnd, &pid);
    if (pid == ctx->pid)
    {
      ctx->main_window = hwnd;
      return FALSE;
    }
  }

  return TRUE;
}
