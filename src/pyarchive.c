// Copyright: 2009 Brian Harring <ferringb@gmail.com>
// License: GPL2/BSD

#include <Python.h>
#include <archive.h>
#include <archive_entry.h>

#define DEFAULT_BUFSIZE 4096

typedef struct {
    PyObject_HEAD
    struct archive *archive;
    Py_ssize_t header_position;
} PyArchive;

typedef struct {
    PyObject_HEAD
    PyArchive *archive;
    struct archive_entry *archive_entry;
} PyArchiveEntry;


static void
PyArchiveEntry_dealloc(PyArchiveEntry *pae)
{
    PyObject_GC_UnTrack(pae);
    if(pae->archive_entry) {
        archive_entry_free(pae->archive_entry);
    }
    Py_XDECREF(pae->archive);
    pae->ob_type->tp_free((PyObject *)pae);
}

static PyObject *
PyArchiveEntry_str(PyArchiveEntry *self)
{
    return PyString_FromString(archive_entry_pathname(self->archive_entry));
}

static PyTypeObject PyArchiveEntryType = {
    PyObject_HEAD_INIT(NULL)
    0,                                /* ob_size */
    "archive_entry",                  /* tp_name */
    sizeof(PyArchiveEntry),   /* tp_basicsize */
    0,                                /* tp_itemsize */
    (destructor)PyArchiveEntry_dealloc,
                                      /* tp_dealloc */
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
    (reprfunc)PyArchiveEntry_str,     /* tp_str */
    0,                                /* tp_getattro */
    0,                                /* tp_setattro */
    0,                                /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE, /* tp_flags */
    0,                                /* tp_doc */
    0,                                /* tp_traverse */
    0,                                /* tp_clear */
    0,                                /* tp_richcompare */
    0,                                /* tp_weaklistoffset */
    0,                                /* tp_iter */
    0,                                /* tp_iternext */
    0,                                /* tp_methods */
    0,                                /* tp_members */
    0,                                /* tp_getset */
    0,                                /* tp_base */
    0,                                /* tp_dict */
    0,                                /* tp_descr_get */
    0,                                /* tp_descr_set */
    0,                                /* tp_dictoffset */
    0,                                /* tp_init */
    PyType_GenericAlloc,              /* tp_alloc */
    PyType_GenericNew,                /* tp_new */
    PyObject_GC_Del,                  /* tp_free */
};
    

static PyArchiveEntry *
mk_PyArchiveEntry(PyArchive *archive, struct archive_entry *entry)
{
    PyArchiveEntry *pae = PyObject_GC_New(PyArchiveEntry,
        &PyArchiveEntryType);
    if(!pae)
        return NULL;
    pae->archive_entry = archive_entry_clone(entry);
    if(!pae) {
        Py_DECREF(pae);
        return NULL;
    }
    Py_INCREF(archive);
    pae->archive = archive;
    return pae;
}


PyObject *
PyArchive_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    PyArchive *self = (PyArchive *)type->tp_alloc(type, 0);
    if(self)
        self->archive = NULL;
    return (PyObject *)self;
}


PyObject *
PyArchive_init(PyArchive *self, PyObject *args, PyObject *kwds)
{
    PyObject *filepath = NULL;
    if(!PyArg_ParseTuple(args, "S", &filepath))
        return NULL;
    if(NULL == (self->archive = archive_read_new()))
        return NULL;
    // doesn't set an exception.
    // error checking in general.
    if(ARCHIVE_OK != archive_read_support_compression_all(self->archive))
        return NULL;
    if(ARCHIVE_OK != archive_read_support_format_all(self->archive))
        return NULL;
    int ret;
    if(ARCHIVE_OK != (ret = archive_read_open_file(self->archive,
        PyString_AsString(filepath), DEFAULT_BUFSIZE))) {
        printf("bailing w/ %i\n", ret);
        printf("%s\n", archive_error_string(self->archive));
        archive_read_finish(self->archive);
        self->archive = NULL;
        return NULL;
    }
    self->header_position = 0;
    return NULL;
}


static void
PyArchive_dealloc(PyArchive *self)
{
    if(self->archive) {
        archive_read_finish(self->archive);
        self->archive = NULL;
    }
    self->ob_type->tp_free((PyObject *)self);
}


static PyObject *
PyArchive_iternext(PyArchive *self)
{
    struct archive_entry *entry = NULL;
    int ret = archive_read_next_header(self->archive, &entry);
    if(ret != ARCHIVE_OK) {
        // do something appropriate...
        return NULL;
    }
    self->header_position++;
    return mk_PyArchiveEntry(self, entry);
}

static PyTypeObject PyArchiveType = {
    PyObject_HEAD_INIT(NULL)
    0,                                /* ob_size */
    "archive",                        /* tp_name */
    sizeof(PyArchive),       /* tp_basicsize */
    0,                                /* tp_itemsize */
    (destructor)PyArchive_dealloc,     /* tp_dealloc */
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
    (iternextfunc)PyArchive_iternext, /* tp_iternext */
    0,                                /* tp_methods */
    0,                                /* tp_members */
    0,                                /* tp_getset */
    0,                                /* tp_base */
    0,                                /* tp_dict */
    0,                                /* tp_descr_get */
    0,                                /* tp_descr_set */
    0,                                /* tp_dictoffset */
    (initproc)PyArchive_init,          /* tp_init */
    0,                                /* tp_alloc */
    PyArchive_new,                     /* tp_new */
};

PyDoc_STRVAR(
    Pydocumentation,
    "fill this out");

PyMODINIT_FUNC
initpyarchive()
{
    PyObject *m = Py_InitModule3("pyarchive", NULL,
        Pydocumentation);
    if(!m)
        return;

    if(PyType_Ready(&PyArchiveType) < 0)
        return;

    if(PyModule_AddObject(m, "archive",
        (PyObject *)&PyArchiveType) == -1)
        return;

    if(PyModule_AddObject(m, "open",
        (PyObject *)&PyArchiveType) == -1)
        return;
}

