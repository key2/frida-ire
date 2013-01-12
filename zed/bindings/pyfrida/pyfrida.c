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
};

struct _PyScript
{
  PyObject_HEAD

  Script * handle;
};

static int PySessionManager_init (PySessionManager * self, PyObject * args, PyObject * kwds);
static void PySessionManager_dealloc (PySessionManager * self);
static PyObject * PySessionManager_obtain_session_for (PySessionManager * self, PyObject * args);

static int PySession_init (PySession * self, PyObject * args, PyObject * kwds);
static void PySession_dealloc (PySession * self);
static PyObject * PySession_close (PySession * self, PyObject * args);
static PyObject * PySession_create_script (PySession * self, PyObject * args);
static PyObject * PySession_on (PySession * self, PyObject * args);
static PyObject * PySession_off (PySession * self, PyObject * args);

static int PyScript_init (PyScript * self, PyObject * args, PyObject * kwds);
static void PyScript_dealloc (PyScript * self);
static PyObject * PyScript_load (PyScript * self, PyObject * args);
static PyObject * PyScript_unload (PyScript * self, PyObject * args);
static PyObject * PyScript_post_message (PyScript * self, PyObject * args);
static PyObject * PyScript_on (PyScript * self, PyObject * args);
static PyObject * PyScript_off (PyScript * self, PyObject * args);

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
  PyObject * session, * no_args;

  if (!PyArg_ParseTuple (args, "l", &pid))
    return NULL;

  handle = session_manager_obtain_session_for (self->handle, (guint) pid, &error);
  if (error != NULL)
  {
    PyErr_SetString (PyExc_SystemError, error->message);
    g_error_free (error);
    return NULL;
  }

  no_args = PyTuple_New (0);
  session = PyObject_CallObject ((PyObject *) &PySessionType, no_args);
  Py_DECREF (no_args);
  ((PySession *) session)->handle = handle;

  return session;
}


static int
PySession_init (PySession * self, PyObject * args, PyObject * kwds)
{
  self->handle = NULL;
  return 0;
}

static void
PySession_dealloc (PySession * self)
{
  if (self->handle != NULL)
    g_object_unref (self->handle);
  self->ob_type->tp_free ((PyObject *) self);
}

static PyObject *
PySession_close (PySession * self, PyObject * args)
{
  session_close (self->handle);
  Py_RETURN_NONE;
}

static PyObject *
PySession_create_script (PySession * self, PyObject * args)
{
  const char * source;
  GError * error = NULL;
  Script * handle;
  PyObject * script, * no_args;

  if (!PyArg_ParseTuple (args, "s", &source))
    return NULL;

  handle = session_create_script (self->handle, source, &error);
  if (error != NULL)
  {
    PyErr_SetString (PyExc_SystemError, error->message);
    g_error_free (error);
    return NULL;
  }

  no_args = PyTuple_New (0);
  script = PyObject_CallObject ((PyObject *) &PyScriptType, no_args);
  Py_DECREF (no_args);
  ((PyScript *) script)->handle = handle;

  return script;
}

static PyObject *
PySession_on (PySession * self, PyObject * args)
{
  /* FIXME */
  Py_RETURN_NONE;
}

static PyObject *
PySession_off (PySession * self, PyObject * args)
{
  /* FIXME */
  Py_RETURN_NONE;
}


static int
PyScript_init (PyScript * self, PyObject * args, PyObject * kwds)
{
  self->handle = NULL;
  return 0;
}

static void
PyScript_dealloc (PyScript * self)
{
  if (self->handle != NULL)
    g_object_unref (self->handle);
  self->ob_type->tp_free ((PyObject *) self);
}

static PyObject *
PyScript_load (PyScript * self, PyObject * args)
{
  GError * error = NULL;

  script_load (self->handle, &error);
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

  script_unload (self->handle, &error);
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
  const char * msg;
  GError * error = NULL;

  if (!PyArg_ParseTuple (args, "s", &msg))
    return NULL;

  script_post_message (self->handle, msg, &error);
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
  /* FIXME */
  Py_RETURN_NONE;
}

static PyObject *
PyScript_off (PyScript * self, PyObject * args)
{
  /* FIXME */
  Py_RETURN_NONE;
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

