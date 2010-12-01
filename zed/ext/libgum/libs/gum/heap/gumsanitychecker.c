/*
 * Copyright (C) 2010 Ole Andr� Vadla Ravn�s <ole.andre.ravnas@tandberg.com>
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

#include "gumsanitychecker.h"

#include "gumallocatorprobe.h"
#include "gumallocationtracker.h"
#include "gumallocationblock.h"
#include "gumallocationgroup.h"
#include "gumboundschecker.h"
#include "guminstancetracker.h"
#include "gumhash.h"
#include "gummemory.h"

#include <string.h>

typedef struct _GumInstanceDetails GumInstanceDetails;

struct _GumSanityCheckerPrivate
{
  GumSanityOutputFunc output;
  gpointer output_user_data;

  GumInstanceTracker * instance_tracker;

  GumAllocatorProbe * alloc_probe;
  GumAllocationTracker * alloc_tracker;

  GumBoundsChecker * bounds_checker;
};

static void gum_sanity_checker_print_instance_leaks_summary (
    GumSanityChecker * self, GumList * stale);
static void gum_sanity_checker_print_instance_leaks_details (
    GumSanityChecker * self, GumList * stale);
static void gum_sanity_checker_print_block_leaks_summary (
    GumSanityChecker * self, GumList * block_groups);
static void gum_sanity_checker_print_block_leaks_details (
    GumSanityChecker * self, GumList * stale);

static GumHashTable * gum_sanity_checker_count_leaks_by_type_name (
    GumSanityChecker * self, GumList * instances);

static void gum_sanity_checker_details_from_instance (GumSanityChecker * self,
    GumInstanceDetails * details, gconstpointer instance);

static gint gum_sanity_checker_compare_type_names (gconstpointer a,
    gconstpointer b, gpointer user_data);
static gint gum_sanity_checker_compare_instances (gconstpointer a,
    gconstpointer b, gpointer user_data);
static gint gum_sanity_checker_compare_groups (gconstpointer a,
    gconstpointer b, gpointer user_data);
static gint gum_sanity_checker_compare_blocks (gconstpointer a,
    gconstpointer b, gpointer user_data);

static void gum_sanity_checker_printf (GumSanityChecker * self,
    const gchar * format, ...);
static void gum_sanity_checker_print (GumSanityChecker * self,
    const gchar * text);

GumSanityChecker *
gum_sanity_checker_new (GumSanityOutputFunc func,
                        gpointer user_data)
{
  GumSanityChecker * checker;
  GumSanityCheckerPrivate * priv;

  checker = (GumSanityChecker *) gum_malloc0 (sizeof (GumSanityChecker) +
      sizeof (GumSanityCheckerPrivate));
  checker->priv = (GumSanityCheckerPrivate *) (checker + 1);

  priv = checker->priv;
  priv->output = func;
  priv->output_user_data = user_data;

  return checker;
}

void
gum_sanity_checker_destroy (GumSanityChecker * checker)
{
  gum_free (checker);
}

gboolean
gum_sanity_checker_run (GumSanityChecker * self,
                        GumSanitySequenceFunc func,
                        gpointer user_data)
{
  gboolean no_leaks_of_any_kind;

  /*
   * First run without any instrumentation
   *
   * This also warms up any static allocations.
   */
  func (user_data);

  gum_sanity_checker_begin (self, GUM_CHECK_INSTANCE_LEAKS);
  func (user_data);
  no_leaks_of_any_kind = gum_sanity_checker_end (self);

  if (no_leaks_of_any_kind)
  {
    gum_sanity_checker_begin (self, GUM_CHECK_BLOCK_LEAKS);
    func (user_data);
    no_leaks_of_any_kind = gum_sanity_checker_end (self);
  }

  if (no_leaks_of_any_kind)
  {
    gum_sanity_checker_begin (self, GUM_CHECK_BOUNDS);
    func (user_data);
    no_leaks_of_any_kind = gum_sanity_checker_end (self);
  }

  return no_leaks_of_any_kind;
}

void
gum_sanity_checker_begin (GumSanityChecker * self,
                          guint flags)
{
  GumSanityCheckerPrivate * priv = self->priv;

  if ((flags & GUM_CHECK_INSTANCE_LEAKS) != 0)
  {
    priv->instance_tracker = gum_instance_tracker_new ();
    gum_instance_tracker_begin (priv->instance_tracker, NULL);
  }

  if ((flags & GUM_CHECK_BLOCK_LEAKS) != 0)
  {
    priv->alloc_tracker = gum_allocation_tracker_new ();
    gum_allocation_tracker_begin (priv->alloc_tracker);

    priv->alloc_probe = gum_allocator_probe_new ();
    g_object_set (priv->alloc_probe, "allocation-tracker", priv->alloc_tracker,
        NULL);
    gum_allocator_probe_attach (priv->alloc_probe);
  }

  if ((flags & GUM_CHECK_BOUNDS) != 0)
  {
    priv->bounds_checker = gum_bounds_checker_new ();
    g_object_set (priv->bounds_checker, "front-alignment", 1, NULL);
    gum_bounds_checker_attach (priv->bounds_checker);
  }
}

gboolean
gum_sanity_checker_end (GumSanityChecker * self)
{
  GumSanityCheckerPrivate * priv = self->priv;
  gboolean all_checks_passed = TRUE;

  if (priv->instance_tracker != NULL)
  {
    GumList * stale_instances;

    gum_instance_tracker_end (priv->instance_tracker);

    stale_instances =
        gum_instance_tracker_peek_instances (priv->instance_tracker);

    if (stale_instances != NULL)
    {
      all_checks_passed = FALSE;

      gum_sanity_checker_printf (self, "Instance leaks detected:\n\n");
      gum_sanity_checker_print_instance_leaks_summary (self, stale_instances);
      gum_sanity_checker_print (self, "\n");
      gum_sanity_checker_print_instance_leaks_details (self, stale_instances);

      gum_list_free (stale_instances);
    }

    g_object_unref (priv->instance_tracker);
    priv->instance_tracker = NULL;
  }

  if (priv->alloc_probe != NULL)
  {
    GumList * stale_blocks;

    gum_allocator_probe_detach (priv->alloc_probe);

    stale_blocks =
        gum_allocation_tracker_peek_block_list (priv->alloc_tracker);

    if (stale_blocks != NULL)
    {
      if (all_checks_passed)
      {
        GumList * block_groups;

        block_groups =
            gum_allocation_tracker_peek_block_groups (priv->alloc_tracker);

        gum_sanity_checker_printf (self, "Block leaks detected:\n\n");
        gum_sanity_checker_print_block_leaks_summary (self, block_groups);
        gum_sanity_checker_print (self, "\n");
        gum_sanity_checker_print_block_leaks_details (self, stale_blocks);

        gum_allocation_group_list_free (block_groups);
      }

      all_checks_passed = FALSE;

      gum_allocation_block_list_free (stale_blocks);
    }

    g_object_unref (priv->alloc_probe);
    priv->alloc_probe = NULL;

    g_object_unref (priv->alloc_tracker);
    priv->alloc_tracker = NULL;
  }

  if (priv->bounds_checker != NULL)
  {
    gum_bounds_checker_detach (priv->bounds_checker);

    g_object_unref (priv->bounds_checker);
    priv->bounds_checker = NULL;
  }

  return all_checks_passed;
}

static void
gum_sanity_checker_print_instance_leaks_summary (GumSanityChecker * self,
                                                 GumList * stale)
{
  GumHashTable * count_by_type;
  GumList * walk, * keys;

  count_by_type = gum_sanity_checker_count_leaks_by_type_name (self, stale);

  keys = gum_hash_table_get_keys (count_by_type);
  keys = gum_list_sort_with_data (keys,
      gum_sanity_checker_compare_type_names, count_by_type);

  gum_sanity_checker_print (self, "\tCount\tGType\n");
  gum_sanity_checker_print (self, "\t-----\t-----\n");

  for (walk = keys; walk != NULL; walk = walk->next)
  {
    const gchar * type_name = (const gchar *) walk->data;
    guint count;

    count = GPOINTER_TO_UINT (gum_hash_table_lookup (count_by_type,
        type_name));
    gum_sanity_checker_printf (self, "\t%u\t%s\n", count, type_name);
  }

  gum_list_free (keys);

  gum_hash_table_unref (count_by_type);
}

static void
gum_sanity_checker_print_instance_leaks_details (GumSanityChecker * self,
                                                 GumList * stale)
{
  GumList * instances, * walk;

  instances = gum_list_copy (stale);
  instances = gum_list_sort_with_data (instances,
      gum_sanity_checker_compare_instances, self);

  gum_sanity_checker_print (self, "\tAddress\t\tRefCount\tGType\n");
  gum_sanity_checker_print (self, "\t--------\t--------\t-----\n");

  for (walk = instances; walk != NULL; walk = walk->next)
  {
    GumInstanceDetails details;

    gum_sanity_checker_details_from_instance (self, &details, walk->data);

    gum_sanity_checker_printf (self, "\t%p\t%d%s\t%s\n",
        details.address,
        details.ref_count,
        details.ref_count <= 9 ? "\t" : "",
        details.type_name);
  }

  gum_list_free (instances);
}

static void
gum_sanity_checker_print_block_leaks_summary (GumSanityChecker * self,
                                              GumList * block_groups)
{
  GumList * groups, * walk;

  groups = gum_list_copy (block_groups);
  groups = gum_list_sort_with_data (groups,
      gum_sanity_checker_compare_groups, self);

  gum_sanity_checker_print (self, "\tCount\tSize\n");
  gum_sanity_checker_print (self, "\t-----\t----\n");

  for (walk = groups; walk != NULL; walk = walk->next)
  {
    GumAllocationGroup * group = (GumAllocationGroup *) walk->data;

    if (group->alive_now == 0)
      continue;

    gum_sanity_checker_printf (self, "\t%u\t%u\n",
        group->alive_now, group->size);
  }

  gum_list_free (groups);
}

static void
gum_sanity_checker_print_block_leaks_details (GumSanityChecker * self,
                                              GumList * stale)
{
  GumList * blocks, * walk;

  blocks = gum_list_copy (stale);
  blocks = gum_list_sort_with_data (blocks,
      gum_sanity_checker_compare_blocks, self);

  gum_sanity_checker_print (self, "\tAddress\t\tSize\n");
  gum_sanity_checker_print (self, "\t--------\t----\n");

  for (walk = blocks; walk != NULL; walk = walk->next)
  {
    GumAllocationBlock * block = (GumAllocationBlock *) walk->data;

    gum_sanity_checker_printf (self, "\t%p\t%u\n",
        block->address, block->size);
  }

  gum_list_free (blocks);
}

static GumHashTable *
gum_sanity_checker_count_leaks_by_type_name (GumSanityChecker * self,
                                             GumList * instances)
{
  GumHashTable * count_by_type;
  const GumInstanceVTable * vtable;
  GumList * walk;

  count_by_type =
      gum_hash_table_new_full (g_str_hash, g_str_equal, NULL, NULL);

  vtable =
      gum_instance_tracker_get_current_vtable (self->priv->instance_tracker);

  for (walk = instances; walk != NULL; walk = walk->next)
  {
    const gchar * type_name;
    guint count;

    type_name = vtable->type_id_to_name (G_TYPE_FROM_INSTANCE (walk->data));
    count = GPOINTER_TO_UINT (gum_hash_table_lookup (count_by_type,
        type_name));
    count++;
    gum_hash_table_insert (count_by_type, (gpointer) type_name,
        GUINT_TO_POINTER (count));
  }

  return count_by_type;
}

static void
gum_sanity_checker_details_from_instance (GumSanityChecker * self,
                                          GumInstanceDetails * details,
                                          gconstpointer instance)
{
  const GumInstanceVTable * vtable;
  GType type;

  vtable =
      gum_instance_tracker_get_current_vtable (self->priv->instance_tracker);

  details->address = instance;
  type = G_TYPE_FROM_INSTANCE (instance);
  details->type_name = vtable->type_id_to_name (type);
  if (g_type_is_a (type, G_TYPE_OBJECT))
    details->ref_count = ((GObject *) instance)->ref_count;
  else
    details->ref_count = 1;
}

static gint
gum_sanity_checker_compare_type_names (gconstpointer a,
                                       gconstpointer b,
                                       gpointer user_data)
{
  const gchar * name_a = (const gchar *) a;
  const gchar * name_b = (const gchar *) b;
  GumHashTable * count_by_type = (GumHashTable *) user_data;
  guint count_a, count_b;

  count_a = GPOINTER_TO_UINT (gum_hash_table_lookup (count_by_type, name_a));
  count_b = GPOINTER_TO_UINT (gum_hash_table_lookup (count_by_type, name_b));
  if (count_a > count_b)
    return -1;
  else if (count_a < count_b)
    return 1;
  else
    return strcmp (name_a, name_b);
}

static gint
gum_sanity_checker_compare_instances (gconstpointer a,
                                      gconstpointer b,
                                      gpointer user_data)
{
  GumSanityChecker * self = (GumSanityChecker *) user_data;
  GumInstanceDetails da, db;
  gint name_equality;

  gum_sanity_checker_details_from_instance (self, &da, a);
  gum_sanity_checker_details_from_instance (self, &db, b);

  name_equality = strcmp (da.type_name, db.type_name);
  if (name_equality != 0)
    return name_equality;

  if (da.ref_count > db.ref_count)
    return -1;
  else if (da.ref_count < db.ref_count)
    return 1;

  if (da.address > db.address)
    return -1;
  else if (da.address < db.address)
    return 1;
  else
    return 0;
}

static gint
gum_sanity_checker_compare_groups (gconstpointer a,
                                   gconstpointer b,
                                   gpointer user_data)
{
  GumAllocationGroup * group_a = (GumAllocationGroup *) a;
  GumAllocationGroup * group_b = (GumAllocationGroup *) b;

  (void) user_data;

  if (group_a->alive_now > group_b->alive_now)
    return -1;
  else if (group_a->alive_now < group_b->alive_now)
    return 1;

  if (group_a->size > group_b->size)
    return -1;
  else if (group_a->size < group_b->size)
    return 1;
  else
    return 0;
}

static gint
gum_sanity_checker_compare_blocks (gconstpointer a,
                                   gconstpointer b,
                                   gpointer user_data)
{
  GumAllocationBlock * block_a = (GumAllocationBlock *) a;
  GumAllocationBlock * block_b = (GumAllocationBlock *) b;
  gsize addr_a, addr_b;

  (void) user_data;

  if (block_a->size > block_b->size)
    return -1;
  else if (block_a->size < block_b->size)
    return 1;

  addr_a = GPOINTER_TO_SIZE (block_a->address);
  addr_b = GPOINTER_TO_SIZE (block_b->address);
  if (addr_a > addr_b)
    return -1;
  else if (addr_a < addr_b)
    return 1;
  else
    return 0;
}

static void
gum_sanity_checker_printf (GumSanityChecker * self,
                           const gchar * format,
                           ...)
{
  va_list args;
  gchar * text;

  va_start (args, format);

  text = g_strdup_vprintf (format, args);
  gum_sanity_checker_print (self, text);
  g_free (text);

  va_end (args);
}

static void
gum_sanity_checker_print (GumSanityChecker * self,
                          const gchar * text)
{
  self->priv->output (text, self->priv->output_user_data);
}
