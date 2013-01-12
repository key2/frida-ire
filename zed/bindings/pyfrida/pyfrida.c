#include "pyfrida.h"

#include <gum/gum.h>
#include <Python.h>

static GMainLoop * main_loop;
static GMainContext * main_context;


typedef struct _PySessionManager PySessionManager;
typedef struct _PySession        PySession;
typedef struct _PyScript         PyScript;

struct _PySessionManager
{
  PyObject_HEAD

  SessionManager * handle;
};

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

static int PySessionManager_init (PySessionManager * self, PyObject * args, PyObject * kwds);
static void PySessionManager_dealloc (PySessionManager * self);
static PyObject * PySessionManager_obtain_session_for (PySessionManager * self, PyObject * args);

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

static PyMethodDef PySessionManager_methods[] =
{
  { "obtain_session_for", (PyCFunction) PySessionManager_obtain_session_for, 1, "Obtain session for a PID." },
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

static PyTypeObject PySessionManagerType =
{
  PyObject_HEAD_INIT (NULL)
  0,                                            /* ob_size           */
  "frida.SessionManager",                       /* tp_name           */
  sizeof (PySessionManager),                    /* tp_basicsize      */
  0,                                            /* tp_itemsize       */
  (destructor) PySessionManager_dealloc,        /* tp_dealloc        */
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
  "Frida Session Manager",                      /* tp_doc            */
  NULL,                                         /* tp_traverse       */
  NULL,                                         /* tp_clear          */
  NULL,                                         /* tp_richcompare    */
  0,                                            /* tp_weaklistoffset */
  NULL,                                         /* tp_iter           */
  NULL,                                         /* tp_iternext       */
  PySessionManager_methods,                     /* tp_methods        */
  NULL,                                         /* tp_members        */
  NULL,                                         /* tp_getset         */
  NULL,                                         /* tp_base           */
  NULL,                                         /* tp_dict           */
  NULL,                                         /* tp_descr_get      */
  NULL,                                         /* tp_descr_set      */
  0,                                            /* tp_dictoffset     */
  (initproc) PySessionManager_init,             /* tp_init           */
};

static PyTypeObject PySessionType =
{
  PyObject_HEAD_INIT (NULL)
  0,                                            /* ob_size           */
  "frida.Session",                              /* tp_name           */
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
  "frida.Script",                               /* tp_name           */
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


static int
PySessionManager_init (PySessionManager * self, PyObject * args, PyObject * kwds)
{
  self->handle = session_manager_new (main_context);
  return 0;
}

static void
PySessionManager_dealloc (PySessionManager * self)
{
  g_object_unref (self->handle);
  self->ob_type->tp_free ((PyObject *) self);
}

static PyObject *
PySessionManager_obtain_session_for (PySessionManager * self, PyObject * args)
{
  long pid;
  GError * error = NULL;
  Session * handle;

  if (!PyArg_ParseTuple (args, "l", &pid))
    return NULL;

  Py_BEGIN_ALLOW_THREADS
  handle = session_manager_obtain_session_for (self->handle, (guint) pid, &error);
  Py_END_ALLOW_THREADS
  if (error != NULL)
  {
    PyErr_SetString (PyExc_SystemError, error->message);
    g_error_free (error);
    return NULL;
  }

  return PySession_from_handle (handle);
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
  self->handle = NULL;
  self->on_close = NULL;
  return 0;
}

static void
PySession_dealloc (PySession * self)
{
  if (self->on_close != NULL)
  {
    g_signal_handlers_disconnect_by_func (self->handle, PySession_on_close, self);
    g_list_free_full (self->on_close, (GDestroyNotify) Py_DecRef);
  }

  if (self->handle != NULL)
  {
    g_object_set_data (G_OBJECT (self->handle), "pyobject", NULL);
    g_object_unref (self->handle);
  }

  self->ob_type->tp_free ((PyObject *) self);
}

static PyObject *
PySession_close (PySession * self, PyObject * args)
{
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
      g_signal_handlers_disconnect_by_func (self->handle, PySession_on_close, self);
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
  self->handle = NULL;
  self->on_message = NULL;
  return 0;
}

static void
PyScript_dealloc (PyScript * self)
{
  if (self->on_message != NULL)
  {
    g_signal_handlers_disconnect_by_func (self->handle, PyScript_on_message, self);
    g_list_free_full (self->on_message, (GDestroyNotify) Py_DecRef);
  }

  if (self->handle != NULL)
  {
    g_object_set_data (G_OBJECT (self->handle), "pyobject", NULL);
    g_object_unref (self->handle);
  }

  self->ob_type->tp_free ((PyObject *) self);
}

static PyObject *
PyScript_load (PyScript * self, PyObject * args)
{
  GError * error = NULL;

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
  const char * message;
  GError * error = NULL;

  if (!PyArg_ParseTuple (args, "s", &message))
    return NULL;

  Py_BEGIN_ALLOW_THREADS
  script_post_message (self->handle, message, &error);
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
      g_signal_handlers_disconnect_by_func (self->handle, PyScript_on_message, self);
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

    g_list_foreach (self->on_message, (GFunc) Py_IncRef, NULL);
    callbacks = g_list_copy (self->on_message);

    for (cur = callbacks; cur != NULL; cur = cur->next)
    {
      PyObject * result = PyObject_CallFunction ((PyObject *) cur->data, "ss#", message, data, data_size);
      if (result == NULL)
        PyErr_Clear ();
      else
        Py_DECREF (result);
    }

    g_list_free_full (callbacks, (GDestroyNotify) Py_DecRef);
  }

  PyGILState_Release (gstate);
}


static gpointer
run_main_loop (gpointer data)
{
  g_main_context_push_thread_default (main_context);
  g_main_loop_run (main_loop);
  g_main_context_pop_thread_default (main_context);

  return NULL;
}

PyMODINIT_FUNC
initfrida (void)
{
  PyObject * module;

  PyEval_InitThreads ();

  g_type_init ();
  gum_init_with_features ((GumFeatureFlags) (GUM_FEATURE_ALL & ~GUM_FEATURE_SYMBOL_LOOKUP));

  main_context = g_main_context_new ();
  main_loop = g_main_loop_new (main_context, FALSE);
  g_thread_create (run_main_loop, NULL, FALSE, NULL);

  PySessionManagerType.tp_new = PyType_GenericNew;
  if (PyType_Ready (&PySessionManagerType) < 0)
    return;

  PySessionType.tp_new = PyType_GenericNew;
  if (PyType_Ready (&PySessionType) < 0)
    return;

  PyScriptType.tp_new = PyType_GenericNew;
  if (PyType_Ready (&PyScriptType) < 0)
    return;

  module = Py_InitModule ("frida", NULL);

  Py_INCREF (&PySessionManagerType);
  PyModule_AddObject (module, "SessionManager", (PyObject *) &PySessionManagerType);

  Py_INCREF (&PySessionType);
  PyModule_AddObject (module, "Session", (PyObject *) &PySessionType);

  Py_INCREF (&PyScriptType);
  PyModule_AddObject (module, "Script", (PyObject *) &PyScriptType);
}

