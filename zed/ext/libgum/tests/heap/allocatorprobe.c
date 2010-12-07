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

#include "allocatorprobe-fixture.c"

TEST_LIST_BEGIN (allocator_probe)
  ALLOCPROBE_TESTENTRY (basics)
  ALLOCPROBE_TESTENTRY (ignore_gquark)
  ALLOCPROBE_TESTENTRY (nonstandard_basics)
  ALLOCPROBE_TESTENTRY (nonstandard_ignored)
  ALLOCPROBE_TESTENTRY (full_cycle)
  ALLOCPROBE_TESTENTRY (gtype_interop)
TEST_LIST_END ()

ALLOCPROBE_TESTCASE (basics)
{
  guint malloc_count, realloc_count, free_count;
  gpointer a, b;

  g_object_set (fixture->ap, "enable-counters", TRUE, NULL);

  READ_PROBE_COUNTERS ();
  g_assert_cmpuint (malloc_count, ==, 0);
  g_assert_cmpuint (realloc_count, ==, 0);
  g_assert_cmpuint (free_count, ==, 0);

  a = malloc (314);
  READ_PROBE_COUNTERS ();
  g_assert_cmpuint (malloc_count, ==, 0);
  g_assert_cmpuint (realloc_count, ==, 0);
  g_assert_cmpuint (free_count, ==, 0);
  free (a);

  ATTACH_PROBE ();

  a = malloc (42);
  READ_PROBE_COUNTERS ();
  g_assert_cmpuint (malloc_count, ==, 1);
  g_assert_cmpuint (realloc_count, ==, 0);
  g_assert_cmpuint (free_count, ==, 0);

  b = calloc (1, 48);
  READ_PROBE_COUNTERS ();
  g_assert_cmpuint (malloc_count, ==, 2);
  g_assert_cmpuint (realloc_count, ==, 0);
  g_assert_cmpuint (free_count, ==, 0);

  a = realloc (a, 84);
  READ_PROBE_COUNTERS ();
  g_assert_cmpuint (malloc_count, ==, 2);
  g_assert_cmpuint (realloc_count, ==, 1);
  g_assert_cmpuint (free_count, ==, 0);

  free (b);
  free (a);
  READ_PROBE_COUNTERS ();
  g_assert_cmpuint (malloc_count, ==, 2);
  g_assert_cmpuint (realloc_count, ==, 1);
  g_assert_cmpuint (free_count, ==, 2);

  DETACH_PROBE ();

  READ_PROBE_COUNTERS ();
  g_assert_cmpuint (malloc_count, ==, 0);
  g_assert_cmpuint (realloc_count, ==, 0);
  g_assert_cmpuint (free_count, ==, 0);
}

ALLOCPROBE_TESTCASE (ignore_gquark)
{
  guint malloc_count, realloc_count, free_count;

  g_object_set (fixture->ap, "enable-counters", TRUE, NULL);
  ATTACH_PROBE ();

  g_quark_from_static_string ("gumtestquark1");
  g_quark_from_string ("gumtestquark2");

  READ_PROBE_COUNTERS ();
  g_assert_cmpuint (malloc_count, ==, 0);
  g_assert_cmpuint (realloc_count, ==, 0);
  g_assert_cmpuint (free_count, ==, 0);

  DETACH_PROBE ();
}

#if defined (G_OS_WIN32) && defined (_DEBUG)

#include <crtdbg.h>

ALLOCPROBE_TESTCASE (nonstandard_basics)
{
  g_object_set (fixture->ap, "enable-counters", TRUE, NULL);

  ATTACH_PROBE ();
  do_nonstandard_heap_calls (fixture, _NORMAL_BLOCK, 1);
  DETACH_PROBE ();
}

ALLOCPROBE_TESTCASE (nonstandard_ignored)
{
  g_object_set (fixture->ap, "enable-counters", TRUE, NULL);
  ATTACH_PROBE ();

  do_nonstandard_heap_calls (fixture, _CRT_BLOCK, 0);

  DETACH_PROBE ();
}

#endif

ALLOCPROBE_TESTCASE (full_cycle)
{
  GumAllocationTracker * t;
  gpointer a, b;

  t = gum_allocation_tracker_new ();
  gum_allocation_tracker_begin (t);

  g_object_set (fixture->ap, "allocation-tracker", t, NULL);

  ATTACH_PROBE ();

  g_assert_cmpuint (gum_allocation_tracker_peek_block_count (t), ==, 0);

  a = malloc (24);
  g_assert_cmpuint (gum_allocation_tracker_peek_block_count (t), ==, 1);
  g_assert_cmpuint (gum_allocation_tracker_peek_block_total_size (t), ==, 24);

  b = calloc (2, 42);
  g_assert_cmpuint (gum_allocation_tracker_peek_block_count (t), ==, 2);
  g_assert_cmpuint (gum_allocation_tracker_peek_block_total_size (t), ==, 108);

  a = realloc (a, 40);
  g_assert_cmpuint (gum_allocation_tracker_peek_block_count (t), ==, 2);
  g_assert_cmpuint (gum_allocation_tracker_peek_block_total_size (t), ==, 124);

  free (a);
  g_assert_cmpuint (gum_allocation_tracker_peek_block_count (t), ==, 1);
  g_assert_cmpuint (gum_allocation_tracker_peek_block_total_size (t), ==, 84);

  free (b);
  g_assert_cmpuint (gum_allocation_tracker_peek_block_count (t), ==, 0);
  g_assert_cmpuint (gum_allocation_tracker_peek_block_total_size (t), ==, 0);

  g_object_unref (t);
}

/*
 * Turns out that doing any GType lookups from within the context where
 * malloc() or similar is being called can be dangerous, as the caller
 * might be from within GType itself. The caller could hold a lock that
 * we try to reacquire by re-entering into GType, which is bad.
 *
 * We circumvent such issues by storing away as much as possible, which
 * also improves performance.
 *
 * FIXME: This test covers both AllocatorProbe and Interceptor, so the
 *        latter should obviously also have a test covering its own layer.
 */
ALLOCPROBE_TESTCASE (gtype_interop)
{
  MyPony * pony;

  ATTACH_PROBE ();

  pony = MY_PONY (g_object_new (MY_TYPE_PONY, NULL));
  g_object_unref (pony);
}

#if defined (G_OS_WIN32) && defined (_DEBUG)

static void
do_nonstandard_heap_calls (TestAllocatorProbeFixture * fixture,
                           gint block_type,
                           gint factor)
{
  guint malloc_count, realloc_count, free_count;
  gpointer a, b;

  a = _malloc_dbg (42, block_type, __FILE__, __LINE__);
  READ_PROBE_COUNTERS ();
  g_assert_cmpuint (malloc_count,  ==, 1 * factor);
  g_assert_cmpuint (realloc_count, ==, 0);
  g_assert_cmpuint (free_count,    ==, 0);

  b = _calloc_dbg (1, 48, block_type, __FILE__, __LINE__);
  READ_PROBE_COUNTERS ();
  g_assert_cmpuint (malloc_count,  ==, 2 * factor);
  g_assert_cmpuint (realloc_count, ==, 0);
  g_assert_cmpuint (free_count,    ==, 0);

  a = _realloc_dbg (a, 84, block_type, __FILE__, __LINE__);
  READ_PROBE_COUNTERS ();
  g_assert_cmpuint (malloc_count,  ==, 2 * factor);
  g_assert_cmpuint (realloc_count, ==, 1 * factor);
  g_assert_cmpuint (free_count,    ==, 0);

  b = _recalloc_dbg (b, 2, 48, block_type, __FILE__, __LINE__);
  READ_PROBE_COUNTERS ();
  g_assert_cmpuint (malloc_count,  ==, 2 * factor);
  g_assert_cmpuint (realloc_count, ==, 2 * factor);
  g_assert_cmpuint (free_count,    ==, 0);

  _free_dbg (b, block_type);
  _free_dbg (a, block_type);
  READ_PROBE_COUNTERS ();
  g_assert_cmpuint (malloc_count,  ==, 2 * factor);
  g_assert_cmpuint (realloc_count, ==, 2 * factor);
  g_assert_cmpuint (free_count,    ==, 2 * factor);
}

#endif
