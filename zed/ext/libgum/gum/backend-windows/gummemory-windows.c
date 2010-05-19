/*
 * Copyright (C) 2008 Ole Andr� Vadla Ravn�s <ole.andre.ravnas@tandberg.com>
 * Copyright (C) 2008 Christian Berentsen <christian.berentsen@tandberg.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "gummemory.h"
#define _WIN32_LEAN_AND_MEAN
#include <windows.h>

static DWORD gum_page_protection_to_windows (GumPageProtection page_prot);

static HANDLE _gum_memory_heap = INVALID_HANDLE_VALUE;

void
gum_memory_init (void)
{
  ULONG heap_frag_value = 2;

  _gum_memory_heap = HeapCreate (HEAP_GENERATE_EXCEPTIONS, 0, 0);

  HeapSetInformation (_gum_memory_heap, HeapCompatibilityInformation,
      &heap_frag_value, sizeof (heap_frag_value));
}

guint
gum_query_page_size (void)
{
  SYSTEM_INFO si;
  GetSystemInfo (&si);
  return si.dwPageSize;
}

gboolean
gum_memory_is_readable (gpointer address,
                        guint len)
{
  MEMORY_BASIC_INFORMATION mbi;
  SIZE_T ret;

  ret = VirtualQuery (address, &mbi, sizeof (mbi));
  g_assert (ret != 0);

  /* FIXME: this will do for now: */
  g_assert ((guint8 *) address + len <=
      (guint8 *) mbi.BaseAddress + mbi.RegionSize);

  return (mbi.Protect == PAGE_READWRITE
      || mbi.Protect == PAGE_READONLY
      || mbi.Protect == PAGE_EXECUTE_READ
      || mbi.Protect == PAGE_EXECUTE_READWRITE);
}

void
gum_mprotect (gpointer address,
              guint size,
              GumPageProtection page_prot)
{
  DWORD win_page_prot, old_protect;
  BOOL success;

  win_page_prot = gum_page_protection_to_windows (page_prot);
  success = VirtualProtect (address, size, win_page_prot, &old_protect);
  g_assert (success);
}

gpointer
gum_malloc (gsize size)
{
  return HeapAlloc (_gum_memory_heap, 0, size);
}

gpointer
gum_malloc0 (gsize size)
{
  return HeapAlloc (_gum_memory_heap, HEAP_ZERO_MEMORY, size);
}

gpointer
gum_realloc (gpointer mem,
             gsize size)
{
  if (mem != NULL)
    return HeapReAlloc (_gum_memory_heap, 0, mem, size);
  else
    return gum_malloc (size);
}

gpointer
gum_memdup (gconstpointer mem,
            gsize byte_size)
{
  gpointer result;

  result = gum_malloc (byte_size);
  memcpy (result, mem, byte_size);

  return result;
}

void
gum_free (gpointer mem)
{
  BOOL success;

  success = HeapFree (_gum_memory_heap, 0, mem);
  g_assert (success);
}

gpointer
gum_alloc_n_pages (guint n_pages,
                   GumPageProtection page_prot)
{
  guint size;
  DWORD win_page_prot;
  gpointer result;

  size = n_pages * gum_query_page_size ();
  win_page_prot = gum_page_protection_to_windows (page_prot);
  result = VirtualAlloc (NULL, size, MEM_COMMIT | MEM_RESERVE, win_page_prot);

  return result;
}

void
gum_free_pages (gpointer mem)
{
  BOOL success;

  success = VirtualFree (mem, 0, MEM_RELEASE);
  g_assert (success);
}

static DWORD
gum_page_protection_to_windows (GumPageProtection page_prot)
{
  switch (page_prot)
  {
    case GUM_PAGE_NO_ACCESS:
      return PAGE_NOACCESS;
    case GUM_PAGE_READ:
      return PAGE_READONLY;
    case GUM_PAGE_READ | GUM_PAGE_WRITE:
      return PAGE_READWRITE;
    case GUM_PAGE_EXECUTE | GUM_PAGE_READ | GUM_PAGE_WRITE:
      return PAGE_EXECUTE_READWRITE;
  }

  g_assert_not_reached ();
  return PAGE_NOACCESS;
}