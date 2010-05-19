/*
 * Copyright (C) 2008 Ole Andr� Vadla Ravn�s <ole.andre.ravnas@tandberg.com>
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

#include "gumboundschecker.h"
#include "guminterceptor.h"
#include "gumpagepool.h"

#include <stdlib.h>
#include <string.h>

#define DEFAULT_POOL_SIZE       4096
#define DEFAULT_FRONT_ALIGNMENT   16

enum
{
  PROP_0,
  PROP_POOL_SIZE,
  PROP_FRONT_ALIGNMENT
};

G_DEFINE_TYPE (GumBoundsChecker, gum_bounds_checker, G_TYPE_OBJECT);

struct _GumBoundsCheckerPrivate
{
  gboolean disposed;

  GumInterceptor * interceptor;
  gboolean attached;
  volatile gboolean detaching;

  guint pool_size;
  guint front_alignment;
  GumPagePool * page_pool;
};

#define GUM_BOUNDS_CHECKER_GET_PRIVATE(o) ((o)->priv)

static void gum_bounds_checker_dispose (GObject * object);

static void gum_bounds_checker_get_property (GObject * object,
    guint property_id, GValue * value, GParamSpec * pspec);
static void gum_bounds_checker_set_property (GObject * object,
    guint property_id, const GValue * value, GParamSpec * pspec);

typedef gpointer (* MallocFunc) (gsize size);
typedef gpointer (* CallocFunc) (gsize num, gsize size);
typedef gpointer (* ReallocFunc) (gpointer old_address, gsize new_size);
typedef void (* FreeFunc) (gpointer mem);

static gpointer replacement_malloc (MallocFunc malloc_impl,
    GumBoundsChecker * self, gpointer caller_ret_addr, gsize size);
static gpointer replacement_calloc (CallocFunc calloc_impl,
    GumBoundsChecker * self, gpointer caller_ret_addr, gsize num, gsize size);
static gpointer replacement_realloc (ReallocFunc realloc_impl,
    GumBoundsChecker * self, gpointer caller_ret_addr, gpointer old_address,
    gsize new_size);
static void replacement_free (FreeFunc free_impl, GumBoundsChecker * self,
    gpointer caller_ret_addr, gpointer address);

static void
gum_bounds_checker_class_init (GumBoundsCheckerClass * klass)
{
  GObjectClass * object_class = G_OBJECT_CLASS (klass);

  g_type_class_add_private (klass, sizeof (GumBoundsCheckerPrivate));

  object_class->dispose = gum_bounds_checker_dispose;
  object_class->get_property = gum_bounds_checker_get_property;
  object_class->set_property = gum_bounds_checker_set_property;

  g_object_class_install_property (object_class, PROP_POOL_SIZE,
      g_param_spec_uint ("pool-size", "Pool Size",
      "Pool size in number of pages",
      2, G_MAXUINT, DEFAULT_POOL_SIZE,
      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (object_class, PROP_FRONT_ALIGNMENT,
      g_param_spec_uint ("front-alignment", "Front Alignment",
      "Front alignment requirement",
      1, 64, DEFAULT_FRONT_ALIGNMENT,
      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
}

static void
gum_bounds_checker_init (GumBoundsChecker * self)
{
  GumBoundsCheckerPrivate * priv;

  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, GUM_TYPE_BOUNDS_CHECKER,
      GumBoundsCheckerPrivate);

  priv = GUM_BOUNDS_CHECKER_GET_PRIVATE (self);
  priv->interceptor = gum_interceptor_obtain ();
  priv->pool_size = DEFAULT_POOL_SIZE;
  priv->front_alignment = DEFAULT_FRONT_ALIGNMENT;
}

static void
gum_bounds_checker_dispose (GObject * object)
{
  GumBoundsChecker * self = GUM_BOUNDS_CHECKER (object);
  GumBoundsCheckerPrivate * priv = GUM_BOUNDS_CHECKER_GET_PRIVATE (self);

  if (!priv->disposed)
  {
    priv->disposed = TRUE;

    gum_bounds_checker_detach (self);

    g_object_unref (priv->interceptor);
  }

  G_OBJECT_CLASS (gum_bounds_checker_parent_class)->dispose (object);
}

static void
gum_bounds_checker_get_property (GObject * object,
                                 guint property_id,
                                 GValue * value,
                                 GParamSpec * pspec)
{
  GumBoundsChecker * self = GUM_BOUNDS_CHECKER (object);
  GumBoundsCheckerPrivate * priv = GUM_BOUNDS_CHECKER_GET_PRIVATE (self);

  switch (property_id)
  {
    case PROP_POOL_SIZE:
      g_value_set_uint (value, priv->pool_size);
      break;
    case PROP_FRONT_ALIGNMENT:
      g_value_set_uint (value, priv->front_alignment);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

static void
gum_bounds_checker_set_property (GObject * object,
                                 guint property_id,
                                 const GValue * value,
                                 GParamSpec * pspec)
{
  GumBoundsChecker * self = GUM_BOUNDS_CHECKER (object);
  GumBoundsCheckerPrivate * priv = GUM_BOUNDS_CHECKER_GET_PRIVATE (self);

  switch (property_id)
  {
    case PROP_POOL_SIZE:
      g_assert (priv->page_pool == NULL);
      priv->pool_size = g_value_get_uint (value);
      break;
    case PROP_FRONT_ALIGNMENT:
      g_assert (priv->page_pool == NULL);
      priv->front_alignment = g_value_get_uint (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

GumBoundsChecker *
gum_bounds_checker_new (void)
{
  return g_object_new (GUM_TYPE_BOUNDS_CHECKER, NULL);
}

void
gum_bounds_checker_attach (GumBoundsChecker * self)
{
  GumBoundsCheckerPrivate * priv = GUM_BOUNDS_CHECKER_GET_PRIVATE (self);

  g_assert (priv->page_pool == NULL);
  priv->page_pool = gum_page_pool_new (GUM_PROTECT_MODE_ABOVE,
      priv->pool_size);
  g_object_set (priv->page_pool, "front-alignment", priv->front_alignment,
      NULL);

  gum_interceptor_replace_function (priv->interceptor, malloc,
      replacement_malloc, self);
  gum_interceptor_replace_function (priv->interceptor, calloc,
      replacement_calloc, self);
  gum_interceptor_replace_function (priv->interceptor, realloc,
      replacement_realloc, self);
  gum_interceptor_replace_function (priv->interceptor, free,
      replacement_free, self);

  priv->attached = TRUE;
}

void
gum_bounds_checker_detach (GumBoundsChecker * self)
{
  GumBoundsCheckerPrivate * priv = GUM_BOUNDS_CHECKER_GET_PRIVATE (self);

  if (priv->attached)
  {
    priv->attached = FALSE;
    priv->detaching = TRUE;

    while (gum_page_pool_peek_used (priv->page_pool) > 0)
    {
      g_usleep (G_USEC_PER_SEC / 20);
    }

    gum_interceptor_revert_function (priv->interceptor, malloc);
    gum_interceptor_revert_function (priv->interceptor, calloc);
    gum_interceptor_revert_function (priv->interceptor, realloc);
    gum_interceptor_revert_function (priv->interceptor, free);

    g_object_unref (priv->page_pool);
    priv->page_pool = NULL;
  }
}

static gpointer
replacement_malloc (MallocFunc malloc_impl,
                    GumBoundsChecker * self,
                    gpointer caller_ret_addr,
                    gsize size)
{
  GumBoundsCheckerPrivate * priv = GUM_BOUNDS_CHECKER_GET_PRIVATE (self);
  gpointer result;

  if (priv->detaching)
    goto fallback;

  result = gum_page_pool_try_alloc (priv->page_pool, size);
  if (result == NULL)
    goto fallback;

  return result;

fallback:
  return malloc_impl (size);
}

static gpointer
replacement_calloc (CallocFunc calloc_impl,
                    GumBoundsChecker * self,
                    gpointer caller_ret_addr,
                    gsize num,
                    gsize size)
{
  GumBoundsCheckerPrivate * priv = GUM_BOUNDS_CHECKER_GET_PRIVATE (self);
  gpointer result;

  if (priv->detaching)
    goto fallback;

  result = gum_page_pool_try_alloc (priv->page_pool, num * size);
  if (result != NULL)
    memset (result, 0, num * size);
  else
    goto fallback;

  return result;

fallback:
  return calloc_impl (num, size);
}

static gpointer
replacement_realloc (ReallocFunc realloc_impl,
                     GumBoundsChecker * self,
                     gpointer caller_ret_addr,
                     gpointer old_address,
                     gsize new_size)
{
  GumBoundsCheckerPrivate * priv = GUM_BOUNDS_CHECKER_GET_PRIVATE (self);
  gpointer result = NULL;
  guint old_size;
  gboolean success;

  if (old_address == NULL)
    return malloc (new_size);

  old_size = gum_page_pool_query_block_size (priv->page_pool, old_address);
  if (old_size == 0)
    goto fallback;

  result = gum_page_pool_try_alloc (priv->page_pool, new_size);
  if (result == NULL)
    result = malloc (new_size);

  if (result != NULL)
    memcpy (result, old_address, MIN (old_size, new_size));

  success = gum_page_pool_try_free (priv->page_pool, old_address);
  g_assert (success);

  return result;

fallback:
  return realloc_impl (old_address, new_size);
}

static void
replacement_free (FreeFunc free_impl,
                  GumBoundsChecker * self,
                  gpointer caller_ret_addr,
                  gpointer address)
{
  GumBoundsCheckerPrivate * priv = GUM_BOUNDS_CHECKER_GET_PRIVATE (self);

  if (!gum_page_pool_try_free (priv->page_pool, address))
    free_impl (address);
}