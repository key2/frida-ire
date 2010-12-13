/*
 * Copyright (C) 2008-2010 Ole Andr� Vadla Ravn�s <ole.andre.ravnas@tandberg.com>
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

#ifndef __GUM_RETURN_ADDRESS_H__
#define __GUM_RETURN_ADDRESS_H__

#include <gum/gumdefs.h>

typedef struct _GumReturnAddressDetails GumReturnAddressDetails;
typedef gpointer GumReturnAddress;
typedef struct _GumReturnAddressArray GumReturnAddressArray;

struct _GumReturnAddressDetails
{
  GumReturnAddress address;
  gchar module_name[GUM_MAX_PATH + 1];
  gchar function_name[GUM_MAX_SYMBOL_NAME + 1];
  gchar file_name[GUM_MAX_PATH + 1];
  guint line_number;
};

struct _GumReturnAddressArray
{
  guint len;
  GumReturnAddress items[GUM_MAX_BACKTRACE_DEPTH];
};

G_BEGIN_DECLS

GUM_API gboolean gum_return_address_details_from_address (
    GumReturnAddress address, GumReturnAddressDetails * details);

GUM_API gboolean gum_return_address_array_is_equal (
    const GumReturnAddressArray * array1,
    const GumReturnAddressArray * array2);

G_END_DECLS

#endif
