/*
 * Copyright (C) 2008-2010 Ole Andr� Vadla Ravn�s <ole.andre.ravnas@tandberg.com>
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

typedef struct {
  gboolean initialized;
  gchar name[8];
} TestFunctionThreadState;

typedef struct {
  gchar arg[16];
} TestFunctionInvocationState;

typedef struct {
  gpointer function_data;
  TestFunctionThreadState thread_data;
  TestFunctionInvocationState invocation_data;
} TestFunctionInvocationData;

typedef struct {
  GObject parent;
  guint on_enter_call_count;
  guint on_leave_call_count;
  guint init_thread_state_count;
  TestFunctionInvocationData last_on_enter_data;
  TestFunctionInvocationData last_on_leave_data;
  GSList * a_threads_seen;
  guint a_thread_index;
  GSList * b_threads_seen;
  guint b_thread_index;
} TestFunctionDataListener;

typedef struct {
  GObjectClass parent_class;
} TestFunctionDataListenerClass;

#define TEST_TYPE_FUNCTION_DATA_LISTENER \
    (test_function_data_listener_get_type ())
#define TEST_FUNCTION_DATA_LISTENER(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj),\
    TEST_TYPE_FUNCTION_DATA_LISTENER, TestFunctionDataListener))

static void test_function_data_listener_iface_init (gpointer g_iface,
    gpointer iface_data);
static void test_function_data_listener_finalize (GObject * object);

G_DEFINE_TYPE_EXTENDED (TestFunctionDataListener,
                        test_function_data_listener,
                        G_TYPE_OBJECT,
                        0,
                        G_IMPLEMENT_INTERFACE (GUM_TYPE_INVOCATION_LISTENER,
                            test_function_data_listener_iface_init));

static void
test_function_data_listener_init_thread_state (TestFunctionDataListener * self,
                                               TestFunctionThreadState * state,
                                               gpointer function_data)
{
  GSList ** threads_seen = NULL;
  guint * thread_index = 0;
  GThread * cur_thread;

  self->init_thread_state_count++;

  if (strcmp ((gchar *) function_data, "a") == 0)
  {
    threads_seen = &self->a_threads_seen;
    thread_index = &self->a_thread_index;
  }
  else if (strcmp ((gchar *) function_data, "b") == 0)
  {
    threads_seen = &self->b_threads_seen;
    thread_index = &self->b_thread_index;
  }
  else
    g_assert_not_reached ();

  cur_thread = g_thread_self ();
  if (g_slist_find (*threads_seen, cur_thread) == NULL)
  {
    *threads_seen = g_slist_prepend (*threads_seen, cur_thread);
    (*thread_index)++;
  }

  g_snprintf (state->name, sizeof (state->name), "%s%d",
      (gchar *) function_data, *thread_index);

  state->initialized = TRUE;
}

static void
test_function_data_listener_on_enter (GumInvocationListener * listener,
                                      GumInvocationContext * context)
{
  TestFunctionDataListener * self = TEST_FUNCTION_DATA_LISTENER (listener);
  gpointer function_data;
  TestFunctionThreadState * thread_state;
  TestFunctionInvocationState * invocation_state;

  function_data = gum_invocation_context_get_listener_function_data (context);

  thread_state = (TestFunctionThreadState *)
      gum_invocation_context_get_listener_thread_data (context,
          sizeof (TestFunctionThreadState));
  if (!thread_state->initialized)
  {
    test_function_data_listener_init_thread_state (self, thread_state,
        function_data);
  }

  invocation_state = (TestFunctionInvocationState *)
      gum_invocation_context_get_listener_function_invocation_data (context,
          sizeof (TestFunctionInvocationState));
  g_strlcpy (invocation_state->arg,
      (const gchar *) gum_invocation_context_get_nth_argument (context, 0),
      sizeof (invocation_state->arg));

  self->on_enter_call_count++;

  self->last_on_enter_data.function_data = function_data;
  self->last_on_enter_data.thread_data = *thread_state;
  self->last_on_enter_data.invocation_data = *invocation_state;
}

static void
test_function_data_listener_on_leave (GumInvocationListener * listener,
                                      GumInvocationContext * context)
{
  TestFunctionDataListener * self = TEST_FUNCTION_DATA_LISTENER (listener);
  TestFunctionThreadState * thread_state;
  TestFunctionInvocationState * invocation_state;

  thread_state = (TestFunctionThreadState *)
      gum_invocation_context_get_listener_thread_data (context,
          sizeof (TestFunctionThreadState));
  invocation_state = (TestFunctionInvocationState *)
      gum_invocation_context_get_listener_function_invocation_data (context,
          sizeof (TestFunctionInvocationState));

  self->on_leave_call_count++;
  self->last_on_leave_data.function_data =
      gum_invocation_context_get_listener_function_data (context);
  self->last_on_leave_data.thread_data = *thread_state;
  self->last_on_leave_data.invocation_data = *invocation_state;
}

static void
test_function_data_listener_iface_init (gpointer g_iface,
                                        gpointer iface_data)
{
  GumInvocationListenerIface * iface = (GumInvocationListenerIface *) g_iface;

  iface->on_enter = test_function_data_listener_on_enter;
  iface->on_leave = test_function_data_listener_on_leave;
}

static void
test_function_data_listener_class_init (TestFunctionDataListenerClass * klass)
{
  GObjectClass * gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->finalize = test_function_data_listener_finalize;
}

static void
test_function_data_listener_init (TestFunctionDataListener * self)
{
}

static void
test_function_data_listener_finalize (GObject * object)
{
  TestFunctionDataListener * self = TEST_FUNCTION_DATA_LISTENER (object);

  g_slist_free (self->a_threads_seen);
  g_slist_free (self->b_threads_seen);

  G_OBJECT_CLASS (test_function_data_listener_parent_class)->finalize (object);
}

static void
test_function_data_listener_reset (TestFunctionDataListener * self)
{
  self->on_enter_call_count = 0;
  self->on_leave_call_count = 0;
  self->init_thread_state_count = 0;
  memset (&self->last_on_enter_data, 0, sizeof (TestFunctionInvocationData));
  memset (&self->last_on_leave_data, 0, sizeof (TestFunctionInvocationData));
}
