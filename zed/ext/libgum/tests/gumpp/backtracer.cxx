/*
 * Copyright (C) 2011 Ole Andr� Vadla Ravn�s <ole.andre.ravnas@tandberg.com>
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

#include "gumpp.hpp"

#include "testutil.h"

G_BEGIN_DECLS

#define GUMPP_TESTCASE(NAME) \
  void test_gumpp_backtracer_ ## NAME (void)
#define GUMPP_TESTENTRY(NAME) \
  TEST_ENTRY_SIMPLE ("Gum++/Backtracer", test_gumpp_backtracer, NAME)

TEST_LIST_BEGIN (gumpp_backtracer)
  GUMPP_TESTENTRY (can_get_stack_trace_from_invocation_context)
TEST_LIST_END ()

gpointer gumpp_test_target_function (GString * str);

class BacktraceTestListener : public Gum::InvocationListener
{
public:
  BacktraceTestListener ()
    : backtracer (Gum::Backtracer_make_default ())
  {
  }

  virtual void on_enter (Gum::InvocationContext * context)
  {
    g_string_append_c (static_cast<GString *> (context->get_listener_function_data_ptr ()), '>');

    Gum::ReturnAddressArray return_addresses;
    backtracer->generate (context->get_cpu_context (), return_addresses);
    g_assert_cmpuint (return_addresses.len, >=, 1);

    Gum::ReturnAddress first_address = return_addresses.items[0];
    Gum::ReturnAddressDetails rad;
    g_assert (Gum::ReturnAddressDetails_from_address (first_address, rad));
    g_assert (g_str_has_suffix (rad.function_name, "_can_get_stack_trace_from_invocation_context"));
    gchar * file_basename = g_path_get_basename (rad.file_name);
    g_assert_cmpstr (file_basename, ==, "backtracer.cxx");
    g_free (file_basename);
  }

  virtual void on_leave (Gum::InvocationContext * context)
  {
    g_string_append_c (static_cast<GString *> (context->get_listener_function_data_ptr ()), '<');
  }

  Gum::RefPtr<Gum::Backtracer> backtracer;
};

GUMPP_TESTCASE (can_get_stack_trace_from_invocation_context)
{
  Gum::RefPtr<Gum::Interceptor> interceptor (Gum::Interceptor_obtain ());

  BacktraceTestListener listener;

  GString * output = g_string_new ("");
  interceptor->attach_listener (reinterpret_cast<void *> (gumpp_test_target_function), &listener, output);

  gumpp_test_target_function (output);
  g_assert_cmpstr (output->str, ==, ">|<");

  g_string_free (output, TRUE);

  interceptor->detach_listener (&listener);
}

gpointer GUM_NOINLINE
gumpp_test_target_function (GString * str)
{
  g_string_append_c (str, '|');

  return NULL;
}

G_END_DECLS
