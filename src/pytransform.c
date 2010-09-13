/*
 * Copyright: 2010 Brian Harring <ferringb@gmail.com>
 * License: GPL v2 (only) or BSD-3 clause
 */

#include <Python.h>
#include <structmember.h>
#include <transform.h>

static PyObject *PyTransformError;

/*
rough thoughts
transform object
add transforms
transform_stack.open()
*/

#define T_STATE_NEW    0
#define T_STATE_DATA   1
#define T_STATE_CLOSE  2

#define Py_RETURN_NULL return (PyObject *)NULL

#define MAXIMUM(x, y) ((x) > (y) ? (x) : (y))

static void
_convert_and_set_transform_error(PyObject *err_kls, struct transform *t)
{
	PyObject *tmp = NULL, *err_pstr = NULL;
	const char *err_cstr = transform_error_string(t);
	if(!err_cstr) {
		PyErr_NoMemory();
		return;
	}
	if(!err_kls)
		err_kls = (PyObject *)PyTransformError;

	if(NULL != (err_pstr = PyString_FromString(err_cstr))) {
		tmp = PyObject_CallFunction(err_kls, "O", err_pstr);
		if(tmp) {
			PyErr_SetObject(err_kls, tmp);
			Py_DECREF(tmp);
		}
		Py_DECREF(err_pstr);
	}
}

static int
_transform_int_call(struct transform *t, int ret)
{
	if (TRANSFORM_OK == ret || TRANSFORM_WARN == ret) {
		return 1;
	}

	_convert_and_set_transform_error(NULL, t);
	return 0;
}

typedef struct {
	PyObject_HEAD
	struct transform *transform;
	int is_read;
	int state;
} PyTransform;

static PyObject *
PyTransform_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
	PyTransform *self = (PyTransform *)type->tp_alloc(type, 0);
	if (self) {
		self->transform = NULL;
		self->is_read = 0;
		self->state = T_STATE_NEW;
	}
	return (PyObject *)self;
}

static int
PyTransform_init(PyTransform *self, PyObject *args, PyObject *kwds)
{
	PyObject *is_read;

	if(!PyArg_ParseTuple(args, "O", &is_read))
		return -1;

	if (-1 == (self->is_read = PyObject_IsTrue(is_read))) {
		return -1;
	}

	if (self->is_read) {
		self->transform = transform_read_new();
	} else {
		self->transform = transform_write_new();
	}

	if (!self->transform)
		return -1;

	return 0;
}

static void
PyTransform_dealloc(PyTransform *self)
{
	int ret;
	if (self->transform) {
		if (self->is_read) {
			ret = transform_read_free(self->transform);
		} else {
			ret = transform_write_free(self->transform);
		}
		if (TRANSFORM_FATAL == ret) {
			_convert_and_set_transform_error(NULL, self->transform);
		}
	}
	self->ob_type->tp_free((PyObject *)self);
}

static int
_transform_sanity_check(PyTransform *self, int allowed_state, int is_read)
{
	if (self->state == T_STATE_CLOSE) {
		PyErr_SetString(PyExc_TypeError, "transform is already closed");
		return 0;
	}
	if (self->state != allowed_state) {
		if (allowed_state == T_STATE_DATA) {
			PyErr_SetString(PyExc_TypeError, "transform must be in a data reading/writing state");
		} else {
			PyErr_SetString(PyExc_TypeError, "transform must be in a new/unopened state");
		}
		return 0;
	}

	if (is_read != -1) {
		if (is_read) {
			if (!self->is_read) {
				PyErr_SetString(PyExc_TypeError, "transform not opened for reading");
				return 0;
			}
		} else if (self->is_read) {
			PyErr_SetString(PyExc_TypeError, "transform not opened for writing");
			return 0;
		}
	}
	return 1;
}



/* add filters */
static PyObject *
PyTransform_add_simple_read_filter(PyTransform *self, PyObject *int_code)
{
	unsigned long code = 0;
	int ret = TRANSFORM_FATAL;
	if (!_transform_sanity_check(self, T_STATE_NEW, -1)) {
		Py_RETURN_NULL;
	}

	if (PyLong_CheckExact(int_code)) {
		code = PyLong_AsUnsignedLong(int_code);
	} else {
		if (!PyInt_CheckExact(int_code)) {
			PyErr_SetString(PyExc_TypeError, "add_simple_read_filter takes one arg- an integer");
			Py_RETURN_NULL;
		}
		code = PyInt_AsUnsignedLongMask(int_code);
	}

	if (code == (unsigned long)-1) {
		Py_RETURN_NULL;
	}

	switch(code) {

#define f(type, func)	case (type): ret = (func)(self->transform); break;

	f(TRANSFORM_FILTER_GZIP, transform_read_add_gzip);
	f(TRANSFORM_FILTER_BZIP2, transform_read_add_bzip2);
	f(TRANSFORM_FILTER_COMPRESS, transform_read_add_compress);
	f(TRANSFORM_FILTER_LZMA, transform_read_add_lzma);
	f(TRANSFORM_FILTER_XZ, transform_read_add_xz);
	f(TRANSFORM_FILTER_UU, transform_read_add_uu);
	f(TRANSFORM_FILTER_RPM, transform_read_add_rpm);
	f(TRANSFORM_FILTER_LZIP, transform_read_add_lzip);

	default:
		PyErr_Format(PyExc_ValueError, "unknown read transform code: %lu", code);
		Py_RETURN_NULL;
	}

	if (!_transform_int_call(self->transform, ret))
		Py_RETURN_NULL;
	Py_RETURN_NONE;
}


static PyObject *
PyTransform_open_filename(PyTransform *self, PyObject *filename)
{
	PyObject *str = NULL;

	if (_transform_sanity_check(self, T_STATE_NEW, -1)) {
		str = PyObject_Str(filename);
		if (str) {
			int ret;
			if (self->is_read) {
				ret = transform_read_open_filename(self->transform,
						PyString_AS_STRING(filename), 0);
			} else {
				ret = transform_write_open_filename(self->transform,
						PyString_AS_STRING(filename));
			}
			if (_transform_int_call(self->transform, ret)) {
				self->state = T_STATE_DATA;
				Py_RETURN_NONE;
			}
		}
	}
	Py_RETURN_NULL;
}	

static PyObject *
PyTransform_read(PyTransform *self, PyObject *args)
{
	long requested = -1;
	ssize_t avail;
	PyObject *result = NULL;
	void *t_buff;
	PyObject *new_string = NULL;
	Py_ssize_t position = 0;
	int t_ret = TRANSFORM_OK;

	if (!_transform_sanity_check(self, T_STATE_DATA, 1))
		Py_RETURN_NULL;

	if (!PyArg_ParseTuple(args, "|l:read", &requested))
		Py_RETURN_NULL;

	if (requested != -1) {
		new_string = PyString_FromStringAndSize(NULL, requested);
		if (!new_string) {
			Py_RETURN_NULL;
		}
	}

	while (requested != 0) {
		t_buff = (void *)transform_read_ahead(self->transform, 1, &avail);
		if (0 == avail) {
			/* note at some point transform should be returning TRANSFORM_EOF... */
			break;
		} else if (avail <  0) {
			/* error of some sort... */
			Py_XDECREF(new_string);
			_transform_int_call(self->transform, avail);
			Py_RETURN_NULL;
		}
		if (requested != -1) {
			/* simple case; just memcpy it in */

			int chunk = requested > avail ? avail : requested;
			assert(PyString_GET_SIZE(new_string) >= position + chunk);
			memcpy(PyString_AS_STRING(new_string) + position,
				t_buff, chunk);

			t_ret = transform_read_consume(self->transform, chunk);
			requested -= chunk;
			position += chunk;

		} else {
			/* annoying case; may have to do expansion here */
			if (!new_string) {
				if(!(new_string = PyString_FromStringAndSize(NULL, avail * 2))) {
					Py_RETURN_NULL;
				}
			} else if (PyString_GET_SIZE(new_string) < position + avail) {
				if (!_PyString_Resize(&new_string, MAXIMUM(PyString_GET_SIZE(new_string) * 2, avail)) < 0) {
					Py_RETURN_NULL;
				}

			}
			memcpy(PyString_AS_STRING(new_string) + position, t_buff, avail);
			t_ret = transform_read_consume(self->transform, avail);
			position += avail;
		}

		if (t_ret < 0) {
			/* note it is impossible due to transform internals to consume less
			 * than a read_ahead returned; so we don't care about how much was
			 * skipped.
			 * we do care about errors however, which are <= -1
			 */
			_convert_and_set_transform_error(NULL, self->transform);
			Py_XDECREF(new_string);
			Py_RETURN_NULL;
		}
	}

	if (-1 == requested) {
		if (new_string) {
			_PyString_Resize(&new_string, position);
		} else {
			new_string = PyString_FromString("");
		}
	} else if (requested != position) {
		_PyString_Resize(&new_string, position);
	}		
	return new_string;
}

static PyObject *
PyTransform_iternext(PyTransform *self)
{
	ssize_t avail = 0, seen = 0;
	Py_ssize_t length;
	PyObject *str;
	void *buff, *end = NULL;

	if (!_transform_sanity_check(self, T_STATE_DATA, 1))
		Py_RETURN_NULL;


	do {
		seen += avail;
		buff = (void *)transform_read_ahead(self->transform, seen + 1, &avail);
		if (!buff) {
			if (avail < 0) {
				/* error... */
				_convert_and_set_transform_error(NULL, self->transform);
				Py_RETURN_NULL;
			}
			/* EOF */
			if (avail == 0) {
				/* no more to return */
				Py_RETURN_NULL;
			}
			break;
		}

	} while (NULL == (end = memchr(buff + seen, '\n', avail)));

	if (end) {
		/* increment to consume the newline. */
		end++;
	} else {
		/* else consume all */
		end = buff + avail;
	}

	if (end) {
		str = PyString_FromStringAndSize(buff, end - buff);
		if (str) {
			/* error checking. */
			transform_read_consume(self->transform, end - buff);
		}
		return str;
	}
	Py_RETURN_NULL;
}


static PyObject *
PyTransform_tell(PyTransform *self)
{
	Py_ssize_t count;
	if (!_transform_sanity_check(self, T_STATE_DATA, -1))
		Py_RETURN_NULL;

	count = transform_filter_bytes(self->transform, 0);
	if (count == -1)
		count = 0;
	return PyLong_FromSsize_t(count);
}

static PyObject *
PyTransform_write(PyTransform *self, PyObject *data)
{
	const void *buf;
	Py_ssize_t len;
	int ret;

	if (!_transform_sanity_check(self, T_STATE_DATA, 0))
		Py_RETURN_NULL;

	if (PyObject_CheckReadBuffer(data)) {
		if (-1 == PyObject_AsReadBuffer(data, &buf, &len)) {
			Py_RETURN_NULL;
		}
	} else {
		if (!PyString_Check(data)) {
			PyErr_SetString(PyExc_TypeError, "write arguement must be a string");
			Py_RETURN_NULL;
		}
		buf = PyString_AS_STRING(data);
		len = PyString_GET_SIZE(data);
	}

	if (_transform_int_call(self->transform,
		transform_write_output(self->transform, buf, len))) {
		Py_RETURN_NONE;
	}
	Py_RETURN_NULL;
}


static PyMethodDef PyTransform_methods[] = {
	{"open_filename", (PyCFunction)PyTransform_open_filename, METH_O},
	{"read", (PyCFunction)PyTransform_read, METH_VARARGS},
	{"write", (PyCFunction)PyTransform_write, METH_O},
	{"tell", (PyCFunction)PyTransform_tell, METH_NOARGS},
	{"_add_simple_read_filter",
		(PyCFunction)PyTransform_add_simple_read_filter, METH_O},
	{NULL}
};

static PyMemberDef PyTransform_members[] = {
	{"is_read", T_INT, offsetof(PyTransform, is_read), READONLY},
	{NULL}
};


static PyTypeObject PyTransformType = {
	PyObject_HEAD_INIT(NULL)
	0,                                /* ob_size */
	"transform_stack",                        /* tp_name */
	sizeof(PyTransform),                /* tp_basicsize */
	0,                                /* tp_itemsize */
	(destructor)PyTransform_dealloc,    /* tp_dealloc */
	0,                                /* tp_print */
	0,                                /* tp_getattr */
	0,                                /* tp_setattr */
	0,                                /* tp_compare */
	0,                                /* tp_repr */
	0,                                /* tp_as_number */
	0,                                /* tp_as_sequence */
	0,                                /* tp_as_mapping */
	0,                                /* tp_hash */
	0,                                /* tp_call */
	0,                                /* tp_str */
	0,                                /* tp_getattro */
	0,                                /* tp_setattro */
	0,                                /* tp_as_buffer */
	Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE, /* tp_flags */
	0,                                /* tp_doc */
	0,                                /* tp_traverse */
	0,                                /* tp_clear */
	0,                                /* tp_richcompare */
	0,                                /* tp_weaklistoffset */
	(getiterfunc)PyObject_SelfIter,   /* tp_iter */
	(iternextfunc)PyTransform_iternext, /* tp_iternext */
	PyTransform_methods,          /* tp_methods */
	PyTransform_members,          /* tp_members */
	0,                                /* tp_getset */
	0,                                /* tp_base */
	0,                                /* tp_dict */
	0,                                /* tp_descr_get */
	0,                                /* tp_descr_set */
	0,                                /* tp_dictoffset */
	(initproc)PyTransform_init,         /* tp_init */
	0,                                /* tp_alloc */
	PyTransform_new,                    /* tp_new */
};

PyDoc_STRVAR(
	PyTransform_docs,
		"fill this out");
	    
PyMODINIT_FUNC
init_extension()
{
	PyObject *tmp, *tmp2, *mod;

	if (!(tmp = PyImport_ImportModule("pytransform.errors")))
		return;

	PyTransformError = PyObject_GetAttrString(tmp, "PyTransformError");
	Py_DECREF(tmp);
	if (!PyTransformError)
		return;

	if (PyType_Ready(&PyTransformType) < 0)
		return;

	mod = Py_InitModule3("pytransform._extension", NULL, PyTransform_docs);
	if (!mod)
		return;

	if (PyModule_AddObject(mod, "transform",
		(PyObject *)&PyTransformType) < 0) {
		Py_DECREF(mod);
		return;
	}
}
