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

#include "gumpagepool.h"
#include "gummemory.h"

#define DEFAULT_PROTECT_MODE    GUM_PROTECT_MODE_ABOVE
#define MIN_POOL_SIZE           2
#define MAX_POOL_SIZE           G_MAXUINT32
#define DEFAULT_POOL_SIZE       G_MAXUINT16
#define DEFAULT_FRONT_ALIGNMENT 16

#define GUM_PAGE_POOL_LOCK()   (g_mutex_lock (priv->mutex))
#define GUM_PAGE_POOL_UNLOCK() (g_mutex_unlock (priv->mutex))

enum
{
  PROP_0,
  PROP_PAGE_SIZE,
  PROP_PROTECT_MODE,
  PROP_SIZE,
  PROP_FRONT_ALIGNMENT
};

G_DEFINE_TYPE (GumPagePool, gum_page_pool, G_TYPE_OBJECT);

typedef struct _PageState PageState;

typedef struct _AlignmentCriteria AlignmentCriteria;
typedef struct _TailAlignResult   TailAlignResult;

struct _GumPagePoolPrivate
{
  gboolean disposed;

  GMutex * mutex;

  /*< properties */
  guint page_size;
  GumProtectMode protect_mode;
  guint size;
  guint front_alignment;

  /*< state */
  guint available;
  guint cur_offset;
  guint8 * pool;
  guint8 * pool_end;
  PageState * page_state;
};

struct _PageState
{
  guint num_pages; /* obviously redundant */
  guint size;
};

struct _AlignmentCriteria
{
  gsize front;
  gsize tail;
};

struct _TailAlignResult
{
  gpointer aligned_ptr;
  gpointer next_tail_ptr;
  gsize gap_size;
};

#define GUM_PAGE_POOL_GET_PRIVATE(o) ((o)->priv)

static void gum_page_pool_constructed (GObject * object);
static void gum_page_pool_finalize (GObject * object);

static void gum_page_pool_get_property (GObject * object,
    guint property_id, GValue * value, GParamSpec * pspec);
static void gum_page_pool_set_property (GObject * object,
    guint property_id, const GValue * value, GParamSpec * pspec);

static gint find_start_index_with_n_free_pages (GumPagePool * self,
    guint n_pages);
static gint find_start_index_for_address (GumPagePool * self, guint8 * p);

static guint num_pages_needed_for (GumPagePool * self, guint size);

static gpointer claim_n_pages_for_size_at (GumPagePool * self, guint n_pages,
    guint size, guint start_index);
static gpointer release_n_pages_at (GumPagePool * self, guint n_pages,
    guint start_index);

static void tail_align (gpointer ptr, gsize size,
    const AlignmentCriteria * criteria, TailAlignResult * result);

static void
gum_page_pool_class_init (GumPagePoolClass * klass)
{
  GObjectClass * object_class = G_OBJECT_CLASS (klass);

  g_type_class_add_private (klass, sizeof (GumPagePoolPrivate));

  object_class->constructed = gum_page_pool_constructed;
  object_class->finalize = gum_page_pool_finalize;
  object_class->get_property = gum_page_pool_get_property;
  object_class->set_property = gum_page_pool_set_property;

  g_object_class_install_property (object_class, PROP_PAGE_SIZE,
      g_param_spec_uint ("page-size", "Page Size", "System Page Size",
      4096, G_MAXUINT, 4096,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (object_class, PROP_PROTECT_MODE,
      g_param_spec_uint ("protect-mode", "Protect Mode", "Protect Mode",
      0, G_MAXUINT, DEFAULT_PROTECT_MODE,
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (object_class, PROP_SIZE,
      g_param_spec_uint ("size", "Size", "Size in number of pages",
      MIN_POOL_SIZE, MAX_POOL_SIZE, DEFAULT_POOL_SIZE,
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (object_class, PROP_FRONT_ALIGNMENT,
      g_param_spec_uint ("front-alignment", "Front Alignment",
      "Front alignment requirement",
      1, 64, DEFAULT_FRONT_ALIGNMENT,
      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
}

static void
gum_page_pool_init (GumPagePool * self)
{
  GumPagePoolPrivate * priv;

  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, GUM_TYPE_PAGE_POOL,
      GumPagePoolPrivate);

  priv = GUM_PAGE_POOL_GET_PRIVATE (self);

  priv->mutex = g_mutex_new ();

  priv->page_size = gum_query_page_size ();
  priv->protect_mode = DEFAULT_PROTECT_MODE;
  priv->size = DEFAULT_POOL_SIZE;
  priv->front_alignment = DEFAULT_FRONT_ALIGNMENT;
}

static void
gum_page_pool_constructed (GObject * object)
{
  GumPagePool * self = GUM_PAGE_POOL (object);
  GumPagePoolPrivate * priv = GUM_PAGE_POOL_GET_PRIVATE (self);

  priv->available = priv->size;
  priv->pool = gum_alloc_n_pages (priv->size, GUM_PAGE_NO_ACCESS);
  priv->pool_end = priv->pool + (priv->size * priv->page_size);
  priv->page_state = gum_malloc0 (priv->size * sizeof (PageState));
}

static void
gum_page_pool_finalize (GObject * object)
{
  GumPagePool * self = GUM_PAGE_POOL (object);
  GumPagePoolPrivate * priv = GUM_PAGE_POOL_GET_PRIVATE (self);

  gum_free (priv->page_state);
  gum_free_pages (priv->pool);

  g_mutex_free (priv->mutex);

  G_OBJECT_CLASS (gum_page_pool_parent_class)->finalize (object);
}

static void
gum_page_pool_get_property (GObject * object,
                            guint property_id,
                            GValue * value,
                            GParamSpec * pspec)
{
  GumPagePool * self = GUM_PAGE_POOL (object);
  GumPagePoolPrivate * priv = GUM_PAGE_POOL_GET_PRIVATE (self);

  switch (property_id)
  {
    case PROP_PAGE_SIZE:
      g_value_set_uint (value, priv->page_size);
      break;
    case PROP_PROTECT_MODE:
      g_value_set_uint (value, priv->protect_mode);
      break;
    case PROP_SIZE:
      g_value_set_uint (value, priv->size);
      break;
    case PROP_FRONT_ALIGNMENT:
      g_value_set_uint (value, priv->front_alignment);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

static void
gum_page_pool_set_property (GObject * object,
                            guint property_id,
                            const GValue * value,
                            GParamSpec * pspec)
{
  GumPagePool * self = GUM_PAGE_POOL (object);
  GumPagePoolPrivate * priv = GUM_PAGE_POOL_GET_PRIVATE (self);

  switch (property_id)
  {
    case PROP_PROTECT_MODE:
      priv->protect_mode = g_value_get_uint (value);
      break;
    case PROP_SIZE:
      priv->size = g_value_get_uint (value);
      break;
    case PROP_FRONT_ALIGNMENT:
      priv->front_alignment = g_value_get_uint (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

GumPagePool *
gum_page_pool_new (GumProtectMode protect_mode,
                   guint n_pages)
{
  return g_object_new (GUM_TYPE_PAGE_POOL,
      "protect-mode", protect_mode,
      "size", n_pages,
      NULL);
}

gpointer
gum_page_pool_try_alloc (GumPagePool * self,
                         guint size)
{
  GumPagePoolPrivate * priv = GUM_PAGE_POOL_GET_PRIVATE (self);
  gpointer result = NULL;
  guint n_pages;

  g_assert (size != 0);

  n_pages = num_pages_needed_for (self, size);

  GUM_PAGE_POOL_LOCK ();
  if (n_pages <= priv->available)
  {
    gint start_index;

    start_index = find_start_index_with_n_free_pages (self, n_pages);
    if (start_index >= 0)
    {
      guint8 * page_start;
      AlignmentCriteria align_criteria;
      TailAlignResult align_result;

      page_start =
          claim_n_pages_for_size_at (self, n_pages, size, start_index);

      align_criteria.front = priv->front_alignment;
      align_criteria.tail = priv->page_size;
      tail_align (page_start, size, &align_criteria, &align_result);

      result = align_result.aligned_ptr;
    }
  }
  GUM_PAGE_POOL_UNLOCK ();

  return result;
}

gboolean
gum_page_pool_try_free (GumPagePool * self,
                        gpointer mem)
{
  GumPagePoolPrivate * priv = GUM_PAGE_POOL_GET_PRIVATE (self);
  gint start_index;
  guint n_pages;

  start_index = find_start_index_for_address (self, mem);
  if (start_index < 0)
    return FALSE;

  GUM_PAGE_POOL_LOCK ();
  n_pages = priv->page_state[start_index].num_pages;
  release_n_pages_at (self, n_pages, start_index);
  GUM_PAGE_POOL_UNLOCK ();

  return TRUE;
}

guint
gum_page_pool_peek_available (GumPagePool * self)
{
  GumPagePoolPrivate * priv = GUM_PAGE_POOL_GET_PRIVATE (self);
  guint result;

  GUM_PAGE_POOL_LOCK ();
  result = priv->available;
  GUM_PAGE_POOL_UNLOCK ();

  return result;
}

guint
gum_page_pool_peek_used (GumPagePool * self)
{
  GumPagePoolPrivate * priv = GUM_PAGE_POOL_GET_PRIVATE (self);
  guint result;

  GUM_PAGE_POOL_LOCK ();
  result = priv->size - priv->available;
  GUM_PAGE_POOL_UNLOCK ();

  return result;
}

void
gum_page_pool_get_bounds (GumPagePool * self,
                          guint8 ** lower,
                          guint8 ** upper)
{
  GumPagePoolPrivate * priv = GUM_PAGE_POOL_GET_PRIVATE (self);

  *lower = priv->pool;
  *upper = priv->pool_end;
}

guint
gum_page_pool_query_block_size (GumPagePool * self,
                                gpointer mem)
{
  GumPagePoolPrivate * priv = GUM_PAGE_POOL_GET_PRIVATE (self);
  guint result;
  gint start_index;

  start_index = find_start_index_for_address (self, mem);
  if (start_index < 0)
    return 0;

  GUM_PAGE_POOL_LOCK ();
  result = priv->page_state[start_index].size;
  GUM_PAGE_POOL_UNLOCK ();

  return result;
}

static gint
find_start_index_with_n_free_pages (GumPagePool * self,
                                    guint n_pages)
{
  GumPagePoolPrivate * priv = GUM_PAGE_POOL_GET_PRIVATE (self);
  gint result = -1;
  guint first_index;
  guint i, n;

  first_index = priv->cur_offset;

start_over:

  for (i = first_index, n = 0; i < priv->size && n < n_pages; i++)
  {
    if (priv->page_state[i].num_pages == 0)
      n++;
    else
      n = 0;
  }

  if (n == n_pages)
  {
    result = i - n_pages;
  }
  else if (first_index != 0)
  {
    first_index = 0;
    goto start_over;
  }

  return result;
}

static gint
find_start_index_for_address (GumPagePool * self,
                              guint8 * p)
{
  GumPagePoolPrivate * priv = GUM_PAGE_POOL_GET_PRIVATE (self);

  if (p < priv->pool || p > priv->pool_end)
    return -1;

  return (p - priv->pool) / priv->page_size;
}

static guint
num_pages_needed_for (GumPagePool * self,
                      guint size)
{
  GumPagePoolPrivate * priv = GUM_PAGE_POOL_GET_PRIVATE (self);
  guint n_pages;

  n_pages = (size / priv->page_size) + 1;
  if (size % priv->page_size != 0)
    n_pages++;

  return n_pages;
}

#define POOL_ADDRESS_FROM_PAGE_INDEX(n) (priv->pool + (n * priv->page_size))

static gpointer
claim_n_pages_for_size_at (GumPagePool * self,
                           guint n_pages,
                           guint size,
                           guint start_index)
{
  GumPagePoolPrivate * priv = GUM_PAGE_POOL_GET_PRIVATE (self);
  gpointer start_address;
  guint i;

  priv->cur_offset = start_index + n_pages;
  priv->available -= n_pages;

  for (i = start_index; i < start_index + n_pages; i++)
  {
    PageState * state = &priv->page_state[i];

    state->num_pages = n_pages;
    state->size = size;
  }

  start_address = POOL_ADDRESS_FROM_PAGE_INDEX (start_index);
  gum_mprotect (start_address, (n_pages - 1) * priv->page_size,
      GUM_PAGE_READ | GUM_PAGE_WRITE);
  return start_address;
}

static gpointer
release_n_pages_at (GumPagePool * self,
                    guint n_pages,
                    guint start_index)
{
  GumPagePoolPrivate * priv = GUM_PAGE_POOL_GET_PRIVATE (self);
  gpointer start_address;
  guint i;

  priv->available += n_pages;

  for (i = start_index; i < start_index + n_pages; i++)
  {
    PageState * state = &priv->page_state[i];

    state->num_pages = 0;
    state->size = 0;
  }

  start_address = POOL_ADDRESS_FROM_PAGE_INDEX (start_index);
  gum_mprotect (start_address, n_pages - 1, GUM_PAGE_NO_ACCESS);
  return start_address;
}

static void
tail_align (gpointer ptr,
            gsize size,
            const AlignmentCriteria * criteria,
            TailAlignResult * result)
{
  gsize unaligned_start_address, unaligned_end_address;
  gsize next_tail_boundary;
  gsize aligned_start_address, aligned_end_address;

  unaligned_start_address = GPOINTER_TO_SIZE (ptr);
  unaligned_end_address = unaligned_start_address + size - 1;
  next_tail_boundary = ((unaligned_end_address / criteria->tail) + 1) * criteria->tail;

  aligned_start_address = ((next_tail_boundary - size) / criteria->front) * criteria->front;
  if (aligned_start_address < unaligned_start_address)
  {
    aligned_start_address += criteria->tail;
    next_tail_boundary += criteria->tail;
  }
  aligned_end_address = aligned_start_address + size - 1;

  result->aligned_ptr = GSIZE_TO_POINTER (aligned_start_address);
  result->next_tail_ptr = GSIZE_TO_POINTER (next_tail_boundary);
  result->gap_size = next_tail_boundary - (aligned_end_address + 1);
}
