#include <Python.h>

PyObject *
frida_sum (PyObject * a, PyObject * b)
{
	return Py_BuildValue ("i", 10);
}

static PyMethodDef
methods[] = 
{
  { "sum", frida_sum, 2, NULL },
  { NULL, NULL, 0, NULL }
};

PyMODINIT_FUNC
initfrida (void)
{
  Py_InitModule ("frida", methods);
}
