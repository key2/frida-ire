#include "pyfrida.h"

#include <gum/gum.h>
#include <Python.h>

static GMainLoop * main_loop;
static GMainContext * main_context;


typedef struct {
  PyObject_HEAD
  SessionManager * handle;
} PySessionManager;

static int PySessionManager_init (PySessionManager * self, PyObject * args, PyObject * kwds);
static void PySessionManager_dealloc (PySessionManager * self);
static PyObject * PySessionManager_obtain_session_for (PySessionManager * self, PyObject * args);

static PyMethodDef PySessionManager_methods[] = {
  { "obtain_session_for", (PyCFunction) PySessionManager_obtain_session_for, 1, "Obtain session for a PID." },
  { NULL }
};

static PyTypeObject PySessionManagerType = {
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

  handle = session_manager_obtain_session_for (self->handle, (guint) pid, &error);
  if (error != NULL)
  {
    PyErr_SetString (PyExc_SystemError, error->message);
    g_error_free (error);
    return NULL;
  }

  return Py_BuildValue ("l", handle);
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

  module = Py_InitModule ("frida", NULL);

  Py_INCREF (&PySessionManagerType);
  PyModule_AddObject (module, "SessionManager", (PyObject *) &PySessionManagerType);
}

