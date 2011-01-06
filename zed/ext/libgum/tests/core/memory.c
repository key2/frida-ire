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

#include "testutil.h"

#include "gummemory-priv.h"

#define MEMORY_TESTCASE(NAME) \
    void test_memory_ ## NAME (void)
#define MEMORY_TESTENTRY(NAME) \
    TEST_ENTRY_SIMPLE ("Core/Memory", test_memory, NAME)

TEST_LIST_BEGIN (memory)
  MEMORY_TESTENTRY (read_from_valid_address_should_succeed)
  MEMORY_TESTENTRY (read_from_invalid_address_should_fail)
  MEMORY_TESTENTRY (write_to_valid_address_should_succeed)
  MEMORY_TESTENTRY (write_to_invalid_address_should_fail)
  MEMORY_TESTENTRY (match_pattern_from_string_does_proper_validation)
  MEMORY_TESTENTRY (scan_range_finds_three_exact_matches)
  MEMORY_TESTENTRY (scan_range_finds_three_wildcarded_matches)
  MEMORY_TESTENTRY (alloc_n_pages_returns_aligned_address)
  MEMORY_TESTENTRY (alloc_n_pages_near_returns_aligned_address_within_range)
TEST_LIST_END ()

typedef struct _TestForEachContext {
  gboolean value_to_return;
  guint number_of_calls;

  gpointer expected_address[3];
  guint expected_size;
} TestForEachContext;

static gboolean match_found_cb (gpointer address, guint size,
    gpointer user_data);

MEMORY_TESTCASE (read_from_valid_address_should_succeed)
{
  guint8 magic[2] = { 0x13, 0x37 };
  gint n_bytes_read;
  guint8 * result;

  result = gum_memory_read (magic, sizeof (magic), &n_bytes_read);
  g_assert (result != NULL);

  g_assert_cmpint (n_bytes_read, ==, sizeof (magic));

  g_assert_cmphex (result[0], ==, magic[0]);
  g_assert_cmphex (result[1], ==, magic[1]);

  g_free (result);
}

MEMORY_TESTCASE (read_from_invalid_address_should_fail)
{
  gpointer invalid_address = GSIZE_TO_POINTER (0x42);
  g_assert (gum_memory_read (invalid_address, 1, NULL) == NULL);
}

MEMORY_TESTCASE (write_to_valid_address_should_succeed)
{
  guint8 bytes[3] = { 0x00, 0x00, 0x12 };
  guint8 magic[2] = { 0x13, 0x37 };
  gboolean success;

  success = gum_memory_write (bytes, magic, sizeof (magic));
  g_assert (success);

  g_assert_cmphex (bytes[0], ==, 0x13);
  g_assert_cmphex (bytes[1], ==, 0x37);
  g_assert_cmphex (bytes[2], ==, 0x12);
}

MEMORY_TESTCASE (write_to_invalid_address_should_fail)
{
  guint8 bytes[3] = { 0x00, 0x00, 0x12 };
  gpointer invalid_address = GSIZE_TO_POINTER (0x42);
  g_assert (gum_memory_write (invalid_address, bytes, sizeof (bytes)) == FALSE);
}

#define GUM_PATTERN_NTH_TOKEN(p, n) \
    ((GumMatchToken *) g_ptr_array_index (p->tokens, n))
#define GUM_PATTERN_NTH_TOKEN_NTH_BYTE(p, n, b) \
    (g_array_index (((GumMatchToken *) g_ptr_array_index (p->tokens, \
        n))->bytes, guint8, b))

MEMORY_TESTCASE (match_pattern_from_string_does_proper_validation)
{
  GumMatchPattern * pattern;

  pattern = gum_match_pattern_new_from_string ("1337");
  g_assert (pattern != NULL);
  g_assert_cmpuint (pattern->size, ==, 2);
  g_assert_cmpuint (pattern->tokens->len, ==, 1);
  g_assert_cmpuint (GUM_PATTERN_NTH_TOKEN (pattern, 0)->bytes->len, ==, 2);
  g_assert_cmphex (GUM_PATTERN_NTH_TOKEN_NTH_BYTE (pattern, 0, 0), ==, 0x13);
  g_assert_cmphex (GUM_PATTERN_NTH_TOKEN_NTH_BYTE (pattern, 0, 1), ==, 0x37);
  gum_match_pattern_free (pattern);

  pattern = gum_match_pattern_new_from_string ("13 37");
  g_assert (pattern != NULL);
  g_assert_cmpuint (pattern->size, ==, 2);
  g_assert_cmpuint (pattern->tokens->len, ==, 1);
  g_assert_cmpuint (GUM_PATTERN_NTH_TOKEN (pattern, 0)->bytes->len, ==, 2);
  g_assert_cmphex (GUM_PATTERN_NTH_TOKEN_NTH_BYTE (pattern, 0, 0), ==, 0x13);
  g_assert_cmphex (GUM_PATTERN_NTH_TOKEN_NTH_BYTE (pattern, 0, 1), ==, 0x37);
  gum_match_pattern_free (pattern);

  pattern = gum_match_pattern_new_from_string ("1 37");
  g_assert (pattern == NULL);

  pattern = gum_match_pattern_new_from_string ("13 3");
  g_assert (pattern == NULL);

  pattern = gum_match_pattern_new_from_string ("13+37");
  g_assert (pattern == NULL);

  pattern = gum_match_pattern_new_from_string ("13 ?? 37");
  g_assert (pattern != NULL);
  g_assert_cmpuint (pattern->size, ==, 3);
  g_assert_cmpuint (pattern->tokens->len, ==, 3);
  g_assert_cmpuint (GUM_PATTERN_NTH_TOKEN (pattern, 0)->bytes->len, ==, 1);
  g_assert_cmphex (GUM_PATTERN_NTH_TOKEN_NTH_BYTE (pattern, 0, 0), ==, 0x13);
  g_assert_cmpuint (GUM_PATTERN_NTH_TOKEN (pattern, 1)->bytes->len, ==, 1);
  g_assert_cmphex (GUM_PATTERN_NTH_TOKEN_NTH_BYTE (pattern, 1, 0), ==, 0x42);
  g_assert_cmpuint (GUM_PATTERN_NTH_TOKEN (pattern, 2)->bytes->len, ==, 1);
  g_assert_cmphex (GUM_PATTERN_NTH_TOKEN_NTH_BYTE (pattern, 2, 0), ==, 0x37);
  gum_match_pattern_free (pattern);

  pattern = gum_match_pattern_new_from_string ("13 ? 37");
  g_assert (pattern == NULL);

  pattern = gum_match_pattern_new_from_string ("??");
  g_assert (pattern == NULL);

  pattern = gum_match_pattern_new_from_string ("?? 13");
  g_assert (pattern == NULL);

  pattern = gum_match_pattern_new_from_string ("13 ??");
  g_assert (pattern == NULL);

  pattern = gum_match_pattern_new_from_string (" ");
  g_assert (pattern == NULL);

  pattern = gum_match_pattern_new_from_string ("");
  g_assert (pattern == NULL);
}

MEMORY_TESTCASE (scan_range_finds_three_exact_matches)
{
  guint8 buf[] = {
    0x13, 0x37,
    0x12,
    0x13, 0x37,
    0x13, 0x37
  };
  GumMemoryRange range;
  GumMatchPattern * pattern;
  TestForEachContext ctx;

  range.base_address = buf;
  range.size = sizeof (buf);

  pattern = gum_match_pattern_new_from_string ("13 37");
  g_assert (pattern != NULL);

  ctx.expected_address[0] = buf + 0;
  ctx.expected_address[1] = buf + 2 + 1;
  ctx.expected_address[2] = buf + 2 + 1 + 2;
  ctx.expected_size = 2;

  ctx.number_of_calls = 0;
  ctx.value_to_return = TRUE;
  gum_memory_scan (&range, pattern, match_found_cb, &ctx);
  g_assert_cmpuint (ctx.number_of_calls, ==, 3);

  ctx.number_of_calls = 0;
  ctx.value_to_return = FALSE;
  gum_memory_scan (&range, pattern, match_found_cb, &ctx);
  g_assert_cmpuint (ctx.number_of_calls, ==, 1);

  gum_match_pattern_free (pattern);
}

MEMORY_TESTCASE (scan_range_finds_three_wildcarded_matches)
{
  guint8 buf[] = {
    0x12, 0x11, 0x13, 0x37,
    0x12, 0x00,
    0x12, 0xc0, 0x13, 0x37,
    0x12, 0x44, 0x13, 0x37
  };
  GumMemoryRange range;
  GumMatchPattern * pattern;
  TestForEachContext ctx;

  range.base_address = buf;
  range.size = sizeof (buf);

  pattern = gum_match_pattern_new_from_string ("12 ?? 13 37");
  g_assert (pattern != NULL);

  ctx.number_of_calls = 0;
  ctx.value_to_return = TRUE;

  ctx.expected_address[0] = buf + 0;
  ctx.expected_address[1] = buf + 4 + 2;
  ctx.expected_address[2] = buf + 4 + 2 + 4;
  ctx.expected_size = 4;

  gum_memory_scan (&range, pattern, match_found_cb, &ctx);

  g_assert_cmpuint (ctx.number_of_calls, ==, 3);

  gum_match_pattern_free (pattern);
}

MEMORY_TESTCASE (alloc_n_pages_returns_aligned_address)
{
  gpointer page;

  page = gum_alloc_n_pages (1, GUM_PAGE_RW);
  g_assert (GPOINTER_TO_SIZE (page) % gum_query_page_size () == 0);
  gum_free_pages (page);
}

MEMORY_TESTCASE (alloc_n_pages_near_returns_aligned_address_within_range)
{
  GumAddressSpec as;
  guint variable_on_stack;
  gpointer page;
  gsize actual_distance;

  as.near_address = &variable_on_stack;
  as.max_distance = G_MAXINT32;

  page = gum_alloc_n_pages_near (1, GUM_PAGE_RW, &as);
  g_assert (page != NULL);

  g_assert (GPOINTER_TO_SIZE (page) % gum_query_page_size () == 0);

  actual_distance = ABS (page - as.near_address);
  g_assert_cmpuint (actual_distance, <=, as.max_distance);

  gum_free_pages (page);
}

static gboolean
match_found_cb (gpointer address,
                guint size,
                gpointer user_data)
{
  TestForEachContext * ctx = (TestForEachContext *) user_data;

  g_assert_cmpuint (ctx->number_of_calls, <, 3);

  g_assert (address == ctx->expected_address[ctx->number_of_calls]);
  g_assert_cmpuint (size, ==, ctx->expected_size);

  ctx->number_of_calls++;

  return ctx->value_to_return;
}
