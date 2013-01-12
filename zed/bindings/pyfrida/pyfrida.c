#include <Python.h>
#include <zed-core.h>
#include <gum/gum.h>

static GMainLoop * main_loop;
static GMainContext * main_context;

typedef struct {
  PyObject_HEAD
  FridaSessionManager * handle;
} SessionManager;


static int SessionManager_init (SessionManager * self, PyObject * args, PyObject * kwds);
static PyObject * SessionManager_obtain_session_for (SessionManager * self, PyObject * args);
static void SessionManager_dealloc (SessionManager * self);

static PyMethodDef SessionManager_methods[] = {
  { "obtain_session_for", (PyCFunction) SessionManager_obtain_session_for, 1, "Obtain session for a PID." },
  { NULL }
};

static PyTypeObject SessionManagerType = {
  PyObject_HEAD_INIT (NULL)
  0,                             /* ob_size        */
  "frida.SessionManager",        /* tp_name        */
  sizeof (SessionManager),       /* tp_basicsize   */
  0,                             /* tp_itemsize    */
  (destructor) SessionManager_dealloc, /* tp_dealloc     */
  0,                             /* tp_print       */
  0,                             /* tp_getattr     */
  0,                             /* tp_setattr     */
  0,                             /* tp_compare     */
  0,                             /* tp_repr        */
  0,                             /* tp_as_number   */
  0,                             /* tp_as_sequence */
  0,                             /* tp_as_mapping  */
  0,                             /* tp_hash        */
  0,                             /* tp_call        */
  0,                             /* tp_str         */
  0,                             /* tp_getattro    */
  0,                             /* tp_setattro    */
  0,                             /* tp_as_buffer   */
  Py_TPFLAGS_DEFAULT,            /* tp_flags       */
  "Frida Session Manager",       /* tp_doc         */
  0,                             /* tp_traverse       */
  0,                             /* tp_clear          */
  0,                             /* tp_richcompare    */
  0,                             /* tp_weaklistoffset */
  0,                             /* tp_iter           */
  0,                             /* tp_iternext       */
  SessionManager_methods,        /* tp_methods        */
  0,                             /* tp_members        */
  0,                             /* tp_getset         */
  0,                             /* tp_base           */
  0,                             /* tp_dict           */
  0,                             /* tp_descr_get      */
  0,                             /* tp_descr_set      */
  0,                             /* tp_dictoffset     */
  (initproc) SessionManager_init,/* tp_init           */
};


static int
SessionManager_init (SessionManager * self, PyObject * args, PyObject * kwds)
{
  self->handle = frida_session_manager_new (main_context);
  return 0;
}

static void
SessionManager_dealloc (SessionManager * self)
{
  g_object_unref (self->handle);
  self->ob_type->tp_free ((PyObject *) self);
}

static PyObject *
SessionManager_obtain_session_for (SessionManager * self, PyObject * args)
{
  long pid;
  GError * error = NULL;

  if (!PyArg_ParseTuple (args, "l", &pid))
    return NULL;

  void * p = frida_session_manager_obtain_session_for (self->handle, (guint) pid, &error);
  if (error != NULL)
  {
    PyErr_SetString (PyExc_SystemError, error->message);
    g_error_free (error);
    return NULL;
  }
  return Py_BuildValue("l", p);
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

  SessionManagerType.tp_new = PyType_GenericNew;
  if (PyType_Ready (&SessionManagerType) < 0)
    return;

  module = Py_InitModule ("frida", NULL);

  Py_INCREF (&SessionManagerType);
  PyModule_AddObject (module, "SessionManager", (PyObject *) &SessionManagerType);
}

