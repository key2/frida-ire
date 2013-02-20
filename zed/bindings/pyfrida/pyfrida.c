#include "pyfrida.h"

#include <gum/gum.h>
#ifdef _MSC_VER
# pragma warning (push)
# pragma warning (disable: 4211)
#endif
#include <Python.h>
#ifdef _MSC_VER
# pragma warning (pop)
#endif

#define FRIDA_FUNCPTR_TO_POINTER(f) (GSIZE_TO_POINTER (f))

static PyObject * json_loads;
static PyObject * json_dumps;

static GMainLoop * main_loop;
static GMainContext * main_context;
static SessionManager * session_manager;


typedef struct _PySession        PySession;
typedef struct _PyScript         PyScript;

struct _PySession
{
  PyObject_HEAD

  Session * handle;
  GList * on_close;
};

struct _PyScript
{
  PyObject_HEAD

  Script * handle;
  GList * on_message;
};

static PyObject * PyFrida_attach (PyObject * self, PyObject * args);

static PyObject * PySession_from_handle (Session * handle);
static int PySession_init (PySession * self, PyObject * args, PyObject * kwds);
static void PySession_dealloc (PySession * self);
static PyObject * PySession_close (PySession * self, PyObject * args);
static PyObject * PySession_create_script (PySession * self, PyObject * args);
static PyObject * PySession_on (PySession * self, PyObject * args);
static PyObject * PySession_off (PySession * self, PyObject * args);
static void PySession_on_close (PySession * self, Session * handle);

static PyObject * PyScript_from_handle (Script * handle);
static int PyScript_init (PyScript * self, PyObject * args, PyObject * kwds);
static void PyScript_dealloc (PyScript * self);
static PyObject * PyScript_load (PyScript * self, PyObject * args);
static PyObject * PyScript_unload (PyScript * self, PyObject * args);
static PyObject * PyScript_post_message (PyScript * self, PyObject * args);
static PyObject * PyScript_on (PyScript * self, PyObject * args);
static PyObject * PyScript_off (PyScript * self, PyObject * args);
static void PyScript_on_message (PyScript * self, const gchar * message, const gchar * data, gint data_size, Script * handle);

static PyMethodDef PyFrida_methods[] =
{
  { "attach", (PyCFunction) PyFrida_attach, 1, "Attach to a PID." },
  { NULL }
};

static PyMethodDef PySession_methods[] =
{
  { "close", (PyCFunction) PySession_close, METH_NOARGS, "Close the session." },
  { "create_script", (PyCFunction) PySession_create_script, 1, "Create a new script." },
  { "on", (PyCFunction) PySession_on, 2, "Add an event handler." },
  { "off", (PyCFunction) PySession_off, 2, "Remove an event handler." },
  { NULL }
};

static PyMethodDef PyScript_methods[] =
{
  { "load", (PyCFunction) PyScript_load, METH_NOARGS, "Load the script." },
  { "unload", (PyCFunction) PyScript_unload, METH_NOARGS, "Unload the script." },
  { "post_message", (PyCFunction) PyScript_post_message, 1, "Post a JSON-formatted message to the script." },
  { "on", (PyCFunction) PyScript_on, 2, "Add an event handler." },
  { "off", (PyCFunction) PyScript_off, 2, "Remove an event handler." },
  { NULL }
};

static PyTypeObject PySessionType =
{
  PyObject_HEAD_INIT (NULL)
  0,                                            /* ob_size           */
  "_frida.Session",                             /* tp_name           */
  sizeof (PySession),                           /* tp_basicsize      */
  0,                                            /* tp_itemsize       */
  (destructor) PySession_dealloc,               /* tp_dealloc        */
  NULL,                                         /* tp_print          */
  NULL,                                         /* tp_getattr        */
  NULL,                                         /* tp_setattr        */
  NULL,                                         /* tp_compare        */
  NULL,                                         /* tp_repr           */
  NULL,                                         /* tp_as_number      */
  NULL,                                         /* tp_as_sequence    */
  NULL,                                         /* tp_as_mapping     */
  NULL,                                         /* tp_hash           */
  NULL,                                         /* tp_call           */
  NULL,                                         /* tp_str            */
  NULL,                                         /* tp_getattro       */
  NULL,                                         /* tp_setattro       */
  NULL,                                         /* tp_as_buffer      */
  Py_TPFLAGS_DEFAULT,                           /* tp_flags          */
  "Frida Session",                              /* tp_doc            */
  NULL,                                         /* tp_traverse       */
  NULL,                                         /* tp_clear          */
  NULL,                                         /* tp_richcompare    */
  0,                                            /* tp_weaklistoffset */
  NULL,                                         /* tp_iter           */
  NULL,                                         /* tp_iternext       */
  PySession_methods,                            /* tp_methods        */
  NULL,                                         /* tp_members        */
  NULL,                                         /* tp_getset         */
  NULL,                                         /* tp_base           */
  NULL,                                         /* tp_dict           */
  NULL,                                         /* tp_descr_get      */
  NULL,                                         /* tp_descr_set      */
  0,                                            /* tp_dictoffset     */
  (initproc) PySession_init,                    /* tp_init           */
};

static PyTypeObject PyScriptType =
{
  PyObject_HEAD_INIT (NULL)
  0,                                            /* ob_size           */
  "_frida.Script",                              /* tp_name           */
  sizeof (PyScript),                            /* tp_basicsize      */
  0,                                            /* tp_itemsize       */
  (destructor) PyScript_dealloc,                /* tp_dealloc        */
  NULL,                                         /* tp_print          */
  NULL,                                         /* tp_getattr        */
  NULL,                                         /* tp_setattr        */
  NULL,                                         /* tp_compare        */
  NULL,                                         /* tp_repr           */
  NULL,                                         /* tp_as_number      */
  NULL,                                         /* tp_as_sequence    */
  NULL,                                         /* tp_as_mapping     */
  NULL,                                         /* tp_hash           */
  NULL,                                         /* tp_call           */
  NULL,                                         /* tp_str            */
  NULL,                                         /* tp_getattro       */
  NULL,                                         /* tp_setattro       */
  NULL,                                         /* tp_as_buffer      */
  Py_TPFLAGS_DEFAULT,                           /* tp_flags          */
  "Frida Script",                               /* tp_doc            */
  NULL,                                         /* tp_traverse       */
  NULL,                                         /* tp_clear          */
  NULL,                                         /* tp_richcompare    */
  0,                                            /* tp_weaklistoffset */
  NULL,                                         /* tp_iter           */
  NULL,                                         /* tp_iternext       */
  PyScript_methods,                             /* tp_methods        */
  NULL,                                         /* tp_members        */
  NULL,                                         /* tp_getset         */
  NULL,                                         /* tp_base           */
  NULL,                                         /* tp_dict           */
  NULL,                                         /* tp_descr_get      */
  NULL,                                         /* tp_descr_set      */
  0,                                            /* tp_dictoffset     */
  (initproc) PyScript_init,                     /* tp_init           */
};


static PyObject *
PyFrida_attach (PyObject * self, PyObject * args)
{
  long pid;
  GError * error = NULL;
  Session * handle;

  (void) self;

  if (!PyArg_ParseTuple (args, "l", &pid))
    return NULL;

  Py_BEGIN_ALLOW_THREADS
  handle = session_manager_obtain_session_for (session_manager, (guint) pid, &error);
  Py_END_ALLOW_THREADS
  if (error != NULL)
  {
    PyErr_SetString (PyExc_SystemError, error->message);
    g_error_free (error);
    return NULL;
  }

  return PySession_from_handle (handle);
}

static gboolean
PyFrida_parse_signal_method_args (PyObject * args, const char ** signal, PyObject ** callback)
{
  if (!PyArg_ParseTuple (args, "sO", signal, callback))
    return FALSE;

  if (!PyCallable_Check (*callback))
  {
    PyErr_SetString (PyExc_TypeError, "second argument must be callable");
    return FALSE;
  }

  return TRUE;
}


static PyObject *
PySession_from_handle (Session * handle)
{
  PyObject * session;

  session = g_object_get_data (G_OBJECT (handle), "pyobject");
  if (session == NULL)
  {
    session = PyObject_CallFunction ((PyObject *) &PySessionType, NULL);
    ((PySession *) session)->handle = handle;
    g_object_set_data (G_OBJECT (handle), "pyobject", session);
  }
  else
  {
    g_object_unref (handle);
    Py_INCREF (session);
  }

  return session;
}

static int
PySession_init (PySession * self, PyObject * args, PyObject * kwds)
{
  (void) args;
  (void) kwds;

  self->handle = NULL;
  self->on_close = NULL;

  return 0;
}

static void
PySession_dealloc (PySession * self)
{
  if (self->on_close != NULL)
  {
    g_signal_handlers_disconnect_by_func (self->handle, FRIDA_FUNCPTR_TO_POINTER (PySession_on_close), self);
    g_list_free_full (self->on_close, (GDestroyNotify) Py_DecRef);
  }

  if (self->handle != NULL)
  {
    g_object_set_data (G_OBJECT (self->handle), "pyobject", NULL);
    Py_BEGIN_ALLOW_THREADS
    g_object_unref (self->handle);
    Py_END_ALLOW_THREADS
  }

  self->ob_type->tp_free ((PyObject *) self);
}

static PyObject *
PySession_close (PySession * self, PyObject * args)
{
  (void) args;

  Py_BEGIN_ALLOW_THREADS
  session_close (self->handle);
  Py_END_ALLOW_THREADS

  Py_RETURN_NONE;
}

static PyObject *
PySession_create_script (PySession * self, PyObject * args)
{
  const char * source;
  GError * error = NULL;
  Script * handle;

  if (!PyArg_ParseTuple (args, "s", &source))
    return NULL;

  Py_BEGIN_ALLOW_THREADS
  handle = session_create_script (self->handle, source, &error);
  Py_END_ALLOW_THREADS
  if (error != NULL)
  {
    PyErr_SetString (PyExc_SystemError, error->message);
    g_error_free (error);
    return NULL;
  }

  return PyScript_from_handle (handle);
}

static PyObject *
PySession_on (PySession * self, PyObject * args)
{
  const char * signal;
  PyObject * callback;

  if (!PyFrida_parse_signal_method_args (args, &signal, &callback))
    return NULL;

  if (strcmp (signal, "close") == 0)
  {
    if (self->on_close == NULL)
    {
      g_signal_connect_swapped (self->handle, "closed", G_CALLBACK (PySession_on_close), self);
    }

    Py_INCREF (callback);
    self->on_close = g_list_append (self->on_close, callback);
  }
  else
  {
    PyErr_SetString (PyExc_NotImplementedError, "unsupported signal");
    return NULL;
  }

  Py_RETURN_NONE;
}

static PyObject *
PySession_off (PySession * self, PyObject * args)
{
  const char * signal;
  PyObject * callback;

  if (!PyFrida_parse_signal_method_args (args, &signal, &callback))
    return NULL;

  if (strcmp (signal, "close") == 0)
  {
    GList * entry;

    entry = g_list_find (self->on_close, callback);
    if (entry != NULL)
    {
      self->on_close = g_list_delete_link (self->on_close, entry);
      Py_DECREF (callback);
    }
    else
    {
      PyErr_SetString (PyExc_ValueError, "unknown callback");
      return NULL;
    }

    if (self->on_close == NULL)
    {
      g_signal_handlers_disconnect_by_func (self->handle, FRIDA_FUNCPTR_TO_POINTER (PySession_on_close), self);
    }
  }
  else
  {
    PyErr_SetString (PyExc_NotImplementedError, "unsupported signal");
    return NULL;
  }

  Py_RETURN_NONE;
}

static void
PySession_on_close (PySession * self, Session * handle)
{
  PyGILState_STATE gstate;

  gstate = PyGILState_Ensure ();

  if (g_object_get_data (G_OBJECT (handle), "pyobject") == self)
  {
    GList * callbacks, * cur;

    g_list_foreach (self->on_close, (GFunc) Py_IncRef, NULL);
    callbacks = g_list_copy (self->on_close);

    for (cur = callbacks; cur != NULL; cur = cur->next)
    {
      PyObject * result = PyObject_CallFunction ((PyObject *) cur->data, NULL);
      if (result == NULL)
        PyErr_Clear ();
      else
        Py_DECREF (result);
    }

    g_list_free_full (callbacks, (GDestroyNotify) Py_DecRef);
  }

  PyGILState_Release (gstate);
}


static PyObject *
PyScript_from_handle (Script * handle)
{
  PyObject * script;

  script = g_object_get_data (G_OBJECT (handle), "pyobject");
  if (script == NULL)
  {
    script = PyObject_CallFunction ((PyObject *) &PyScriptType, NULL);
    ((PyScript *) script)->handle = handle;
    g_object_set_data (G_OBJECT (handle), "pyobject", script);
  }
  else
  {
    g_object_unref (handle);
    Py_INCREF (script);
  }

  return script;
}

static int
PyScript_init (PyScript * self, PyObject * args, PyObject * kwds)
{
  (void) args;
  (void) kwds;

  self->handle = NULL;
  self->on_message = NULL;

  return 0;
}

static void
PyScript_dealloc (PyScript * self)
{
  if (self->on_message != NULL)
  {
    g_signal_handlers_disconnect_by_func (self->handle, FRIDA_FUNCPTR_TO_POINTER (PyScript_on_message), self);
    g_list_free_full (self->on_message, (GDestroyNotify) Py_DecRef);
  }

  if (self->handle != NULL)
  {
    g_object_set_data (G_OBJECT (self->handle), "pyobject", NULL);
    Py_BEGIN_ALLOW_THREADS
    g_object_unref (self->handle);
    Py_END_ALLOW_THREADS
  }

  self->ob_type->tp_free ((PyObject *) self);
}

static PyObject *
PyScript_load (PyScript * self, PyObject * args)
{
  GError * error = NULL;

  (void) args;

  Py_BEGIN_ALLOW_THREADS
  script_load (self->handle, &error);
  Py_END_ALLOW_THREADS
  if (error != NULL)
  {
    PyErr_SetString (PyExc_SystemError, error->message);
    g_error_free (error);
    return NULL;
  }

  Py_RETURN_NONE;
}

static PyObject *
PyScript_unload (PyScript * self, PyObject * args)
{
  GError * error = NULL;

  (void) args;

  Py_BEGIN_ALLOW_THREADS
  script_unload (self->handle, &error);
  Py_END_ALLOW_THREADS
  if (error != NULL)
  {
    PyErr_SetString (PyExc_SystemError, error->message);
    g_error_free (error);
    return NULL;
  }

  Py_RETURN_NONE;
}

static PyObject *
PyScript_post_message (PyScript * self, PyObject * args)
{
  PyObject * message_object, * message;
  GError * error = NULL;

  if (!PyArg_ParseTuple (args, "O", &message_object))
    return NULL;

  message = PyObject_CallFunction (json_dumps, "O", message_object);
  if (message == NULL)
    return NULL;

  Py_BEGIN_ALLOW_THREADS
  script_post_message (self->handle, PyString_AsString (message), &error);
  Py_END_ALLOW_THREADS

  Py_DECREF (message);

  if (error != NULL)
  {
    PyErr_SetString (PyExc_SystemError, error->message);
    g_error_free (error);
    return NULL;
  }

  Py_RETURN_NONE;
}

static PyObject *
PyScript_on (PyScript * self, PyObject * args)
{
  const char * signal;
  PyObject * callback;

  if (!PyFrida_parse_signal_method_args (args, &signal, &callback))
    return NULL;

  if (strcmp (signal, "message") == 0)
  {
    if (self->on_message == NULL)
    {
      g_signal_connect_swapped (self->handle, "message", G_CALLBACK (PyScript_on_message), self);
    }

    Py_INCREF (callback);
    self->on_message = g_list_append (self->on_message, callback);
  }
  else
  {
    PyErr_SetString (PyExc_NotImplementedError, "unsupported signal");
    return NULL;
  }

  Py_RETURN_NONE;
}

static PyObject *
PyScript_off (PyScript * self, PyObject * args)
{
  const char * signal;
  PyObject * callback;

  if (!PyFrida_parse_signal_method_args (args, &signal, &callback))
    return NULL;

  if (strcmp (signal, "message") == 0)
  {
    GList * entry;

    entry = g_list_find (self->on_message, callback);
    if (entry != NULL)
    {
      self->on_message = g_list_delete_link (self->on_message, entry);
      Py_DECREF (callback);
    }
    else
    {
      PyErr_SetString (PyExc_ValueError, "unknown callback");
      return NULL;
    }

    if (self->on_message == NULL)
    {
      g_signal_handlers_disconnect_by_func (self->handle, FRIDA_FUNCPTR_TO_POINTER (PyScript_on_message), self);
    }
  }
  else
  {
    PyErr_SetString (PyExc_NotImplementedError, "unsupported signal");
    return NULL;
  }

  Py_RETURN_NONE;
}

static void
PyScript_on_message (PyScript * self, const gchar * message, const gchar * data, gint data_size, Script * handle)
{
  PyGILState_STATE gstate;

  gstate = PyGILState_Ensure ();

  if (g_object_get_data (G_OBJECT (handle), "pyobject") == self)
  {
    GList * callbacks, * cur;
    PyObject * args, * message_object;

    g_list_foreach (self->on_message, (GFunc) Py_IncRef, NULL);
    callbacks = g_list_copy (self->on_message);

    message_object = PyObject_CallFunction (json_loads, "s", message);
    g_assert (message_object != NULL);
    args = Py_BuildValue ("Os#", message_object, data, data_size);
    Py_DECREF (message_object);

    for (cur = callbacks; cur != NULL; cur = cur->next)
    {
      PyObject * result = PyObject_CallObject ((PyObject *) cur->data, args);
      if (result == NULL)
        PyErr_Clear ();
      else
        Py_DECREF (result);
    }

    Py_DECREF (args);

    g_list_free_full (callbacks, (GDestroyNotify) Py_DecRef);
  }

  PyGILState_Release (gstate);
}


static gpointer
run_main_loop (gpointer data)
{
  (void) data;

  g_main_context_push_thread_default (main_context);
  g_main_loop_run (main_loop);
  g_main_context_pop_thread_default (main_context);

  return NULL;
}

PyMODINIT_FUNC
init_frida (void)
{
  PyObject * json;

  PyEval_InitThreads ();

  json = PyImport_ImportModule ("json");
  json_loads = PyObject_GetAttrString (json, "loads");
  json_dumps = PyObject_GetAttrString (json, "dumps");
  Py_DECREF (json);

  g_type_init ();
  gum_init_with_features ((GumFeatureFlags) (GUM_FEATURE_ALL & ~GUM_FEATURE_SYMBOL_LOOKUP));

  main_context = g_main_context_new ();
  main_loop = g_main_loop_new (main_context, FALSE);
  g_thread_create (run_main_loop, NULL, FALSE, NULL);
  session_manager = session_manager_new (main_context);

  PySessionType.tp_new = PyType_GenericNew;
  if (PyType_Ready (&PySessionType) < 0)
    return;

  PyScriptType.tp_new = PyType_GenericNew;
  if (PyType_Ready (&PyScriptType) < 0)
    return;

  Py_InitModule ("_frida", PyFrida_methods);
}
