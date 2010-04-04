// Copyright: 2009 Brian Harring <ferringb@gmail.com>
// License: GPL2/BSD

#include <Python.h>
#include <archive.h>
#include <archive_entry.h>
#include <unistd.h> /* gid_t, uid_t, time_t */

/*
todo

wide char:
  gname/uname support
  pathname
  symlink
  hardlink
*/


#define GETSET_HELPER(type, doc, attr)          \
    {doc, (getter)type##_get_##attr ,           \
            (setter)type##_set_##attr , NULL}
            
#if ARCHIVE_VERSION_NUMBER > 2006001
#define HAS_ARCHIVE_READ_NEXT_HEADER2 1
#endif

#define DEFAULT_BUFSIZE 4096
#define PYARCHIVE_EOF       0x1
#define PYARCHIVE_FAILURE   0x2

typedef struct {
    PyObject_HEAD
    struct archive *archive;
    Py_ssize_t      header_position;
    unsigned int    flags;
} PyArchive;

typedef struct {
    PyObject_HEAD
    PyArchive *archive;
    struct archive_entry *archive_entry;
} PyArchiveEntry;

#ifdef HAS_ARCHIVE_READ_NEXT_HEADER2
#define MAX_FREE_ARCHIVE_ENTRY 5
static PyArchiveEntry *archive_entry_freelist[MAX_FREE_ARCHIVE_ENTRY];
static int archive_entry_free_count = 0;
#endif

static PyObject *PyArchiveError = NULL;

/* utility funcs */

static void
_convert_and_set_archive_error(PyObject *err_kls, struct archive *a)
{
    PyObject *tmp = NULL, *err_pstr = NULL;
    const char *err_cstr = archive_error_string(a);
    if(!err_cstr) {
        PyErr_NoMemory();
        return;
    }
    if(!err_kls)
        err_kls = (PyObject *)PyArchiveError;

    if(err_pstr = PyString_FromString(err_cstr)) {
        tmp = PyObject_CallFunction(err_kls, "O", err_pstr);
        if(tmp) {
            PyErr_SetObject(err_kls, tmp);
            Py_DECREF(tmp);
        }
        Py_DECREF(err_pstr);
    }
}


/* class funcs */

static void
PyArchiveEntry_dealloc(PyArchiveEntry *pae)
{
    PyObject_GC_UnTrack(pae);
    PyObject *archive = (PyObject *)pae->archive;

#ifdef HAS_ARCHIVE_READ_NEXT_HEADER2
    if(archive_entry_free_count < MAX_FREE_ARCHIVE_ENTRY) {
        archive_entry_freelist[archive_entry_free_count++] = pae;
        pae->archive = NULL;
    } else {
        if(pae->archive_entry) {
            archive_entry_free(pae->archive_entry);
        }
        pae->ob_type->tp_free((PyObject *)pae);
    }
#else
    if(pae->archive_entry) {
        archive_entry_free(pae->archive_entry);
    }
    pae->ob_type->tp_free((PyObject *)pae);
#endif
    Py_XDECREF(archive);
}


static PyObject *
PyArchiveEntry_str(PyArchiveEntry *self)
{
    return PyObject_GetAttrString((PyObject*)self, "name");
}

/* get/setters */

static PyObject *
PyArchiveEntry_get_name(PyArchiveEntry *self, void *closure)
{
    return PyString_FromString(archive_entry_pathname(self->archive_entry));
}

static int
PyArchiveEntry_set_name(PyArchiveEntry *self, void *closure)
{
    PyErr_SetString(PyExc_AttributeError, "name is currently immutable");
    return -1;
}

static PyObject *
PyArchive_Entry_generic_immutable(PyArchiveEntry *self, void *closure)
{
    PyErr_SetString(PyExc_AttributeError, "immutable attribute");
}

/* XXX
 yes this is redundant code... avoiding gcc macro tricks since it may 
 not work under windows... check w/ tim re: that 

---time funcs---
*/

static PyObject *
PyArchive_Entry_get_ctime_is_set(PyArchiveEntry *self, void *closure)
{
    if(archive_entry_ctime_is_set(self->archive_entry)) {
        Py_RETURN_TRUE;
    }
    Py_RETURN_FALSE;
}

static PyObject *
PyArchive_Entry_get_mtime_is_set(PyArchiveEntry *self, void *closure)
{
    if(archive_entry_mtime_is_set(self->archive_entry)) {
        Py_RETURN_TRUE;
    }
    Py_RETURN_FALSE;
}

static PyObject *
PyArchive_Entry_get_atime_is_set(PyArchiveEntry *self, void *closure)
{
    if(archive_entry_atime_is_set(self->archive_entry)) {
        Py_RETURN_TRUE;
    }
    Py_RETURN_FALSE;
}

static PyObject *
PyArchive_Entry_get_birthtime_is_set(PyArchiveEntry *self, void *closure)
{
    if(archive_entry_birthtime_is_set(self->archive_entry)) {
        Py_RETURN_TRUE;
    }
    Py_RETURN_FALSE;
}

static PyObject *
PyArchive_Entry_get_ctime(PyArchiveEntry *self, void *closure)
{
    return PyInt_FromLong((long)archive_entry_ctime(self->archive_entry));
}

static PyObject *
PyArchive_Entry_get_mtime(PyArchiveEntry *self, void *closure)
{
    return PyInt_FromLong((long)archive_entry_mtime(self->archive_entry));
}

static PyObject *
PyArchive_Entry_get_atime(PyArchiveEntry *self, void *closure)
{
    return PyInt_FromLong((long)archive_entry_atime(self->archive_entry));
}

static PyObject *
PyArchive_Entry_get_birthtime(PyArchiveEntry *self, void *closure)
{
    return PyInt_FromLong((long)archive_entry_birthtime(self->archive_entry));
}

static PyObject *
PyArchive_Entry_get_atime_nsec(PyArchiveEntry *self, void *closure)
{
    return PyInt_FromLong((long)archive_entry_atime_nsec(self->archive_entry));
}

static PyObject *
PyArchive_Entry_get_mtime_nsec(PyArchiveEntry *self, void *closure)
{
    return PyInt_FromLong((long)archive_entry_mtime_nsec(self->archive_entry));
}

static PyObject *
PyArchive_Entry_get_ctime_nsec(PyArchiveEntry *self, void *closure)
{
    return PyInt_FromLong((long)archive_entry_ctime_nsec(self->archive_entry));
}

static PyObject *
PyArchive_Entry_get_birthtime_nsec(PyArchiveEntry *self, void *closure)
{
    return PyInt_FromLong((long)archive_entry_birthtime_nsec(self->archive_entry));
}

/* 
---gid/uid/groups/user funcs--
wide char support was skipped; revisit
 */

static PyObject *
PyArchive_Entry_get_gid(PyArchiveEntry *self, void *closure)
{
    return PyInt_FromLong((long)archive_entry_gid(self->archive_entry));
}

static PyObject *
PyArchive_Entry_get_uid(PyArchiveEntry *self, void *closure)
{
    return PyInt_FromLong((long)archive_entry_uid(self->archive_entry));
}

static PyObject *
PyArchive_Entry_get_gname(PyArchiveEntry *self, void *closure)
{
    return PyString_FromString(archive_entry_gname(self->archive_entry));
}

static PyObject *
PyArchive_Entry_get_uname(PyArchiveEntry *self, void *closure)
{
    return PyString_FromString(archive_entry_uname(self->archive_entry));
}

/*
-- misc --
*/
static PyObject *
PyArchive_Entry_get_size_is_set(PyArchiveEntry *self, void *closure)
{
    if(archive_entry_size_is_set(self->archive_entry)) {
        Py_RETURN_TRUE;
    }
    Py_RETURN_FALSE;
}

static PyObject *
PyArchive_Entry_get_size(PyArchiveEntry *self, void *closure)
{
    return PyLong_FromLongLong(archive_entry_size(self->archive_entry));
}

static PyObject *
PyArchive_Entry_get_dev(PyArchiveEntry *self, void *closure)
{
    return PyInt_FromLong(archive_entry_dev(self->archive_entry));
}

static PyObject *
PyArchive_Entry_get_rdev(PyArchiveEntry *self, void *closure)
{
    return PyInt_FromLong(archive_entry_rdev(self->archive_entry));
}

static PyObject *
PyArchive_Entry_get_devmajor(PyArchiveEntry *self, void *closure)
{
    return PyInt_FromLong(archive_entry_devmajor(self->archive_entry));
}

static PyObject *
PyArchive_Entry_get_devminor(PyArchiveEntry *self, void *closure)
{
    return PyInt_FromLong(archive_entry_devminor(self->archive_entry));
}

static PyObject *
PyArchive_Entry_get_rdevmajor(PyArchiveEntry *self, void *closure)
{
    return PyInt_FromLong(archive_entry_rdevmajor(self->archive_entry));
}

static PyObject *
PyArchive_Entry_get_rdevminor(PyArchiveEntry *self, void *closure)
{
    return PyInt_FromLong(archive_entry_rdevminor(self->archive_entry));
}

static PyObject *
PyArchive_Entry_get_inode(PyArchiveEntry *self, void *closure)
{
    return PyInt_FromLong(archive_entry_ino(self->archive_entry));
}

static PyObject *
PyArchive_Entry_get_mode(PyArchiveEntry *self, void *closure)
{
    return PyInt_FromLong(archive_entry_mode(self->archive_entry));
}

static PyObject *
PyArchive_Entry_get_symlink(PyArchiveEntry *self, void *closure)
{
    const char *sym = archive_entry_symlink(self->archive_entry);
    if(sym)
        return PyString_FromString(sym);
    return PyString_FromString("");
}

static PyGetSetDef PyArchiveEntry_getsetters[] = {
    GETSET_HELPER(PyArchiveEntry, "name", name),
    {"ctime_is_set", (getter)PyArchive_Entry_get_ctime_is_set,
        (setter)PyArchive_Entry_generic_immutable, NULL},
    {"atime_is_set", (getter)PyArchive_Entry_get_atime_is_set,
        (setter)PyArchive_Entry_generic_immutable, NULL},
    {"mtime_is_set", (getter)PyArchive_Entry_get_mtime_is_set,
        (setter)PyArchive_Entry_generic_immutable, NULL},
    {"birthtime_is_set", (getter)PyArchive_Entry_get_mtime_is_set,
        (setter)PyArchive_Entry_generic_immutable, NULL},
    {"ctime", (getter)PyArchive_Entry_get_ctime,
        (setter)PyArchive_Entry_generic_immutable, NULL},
    {"mtime", (getter)PyArchive_Entry_get_mtime,
        (setter)PyArchive_Entry_generic_immutable, NULL},
    {"atime", (getter)PyArchive_Entry_get_atime,
        (setter)PyArchive_Entry_generic_immutable, NULL},
    {"birthtime", (getter)PyArchive_Entry_get_birthtime,
        (setter)PyArchive_Entry_generic_immutable, NULL},
    {"atime_nsec", (getter)PyArchive_Entry_get_atime_nsec,
        (setter)PyArchive_Entry_generic_immutable, NULL},
    {"mtime_nsec", (getter)PyArchive_Entry_get_mtime_nsec,
        (setter)PyArchive_Entry_generic_immutable, NULL},
    {"ctime_nsec", (getter)PyArchive_Entry_get_ctime_nsec,
        (setter)PyArchive_Entry_generic_immutable, NULL},
    {"birthtime_nsec", (getter)PyArchive_Entry_get_birthtime_nsec,
        (setter)PyArchive_Entry_generic_immutable, NULL},
    {"gid", (getter)PyArchive_Entry_get_gid,
        (setter)PyArchive_Entry_generic_immutable, NULL},
    {"uid", (getter)PyArchive_Entry_get_uid,
        (setter)PyArchive_Entry_generic_immutable, NULL},
    {"uname", (getter)PyArchive_Entry_get_uname,
        (setter)PyArchive_Entry_generic_immutable, NULL},
    {"gname", (getter)PyArchive_Entry_get_gname,
        (setter)PyArchive_Entry_generic_immutable, NULL},
    {"size_is_set", (getter)PyArchive_Entry_get_size_is_set,
        (setter)PyArchive_Entry_generic_immutable, NULL},
    {"size", (getter)PyArchive_Entry_get_size,
        (setter)PyArchive_Entry_generic_immutable, NULL},
    {"dev", (getter)PyArchive_Entry_get_dev,
        (setter)PyArchive_Entry_generic_immutable, NULL},
    {"rdev", (getter)PyArchive_Entry_get_rdev,
        (setter)PyArchive_Entry_generic_immutable, NULL},
    {"devmajor", (getter)PyArchive_Entry_get_devmajor,
        (setter)PyArchive_Entry_generic_immutable, NULL},
    {"devminor", (getter)PyArchive_Entry_get_devminor,
        (setter)PyArchive_Entry_generic_immutable, NULL},
    {"rdevmajor", (getter)PyArchive_Entry_get_rdevmajor,
        (setter)PyArchive_Entry_generic_immutable, NULL},
    {"rdevminor", (getter)PyArchive_Entry_get_rdevminor,
        (setter)PyArchive_Entry_generic_immutable, NULL},
    {"inode", (getter)PyArchive_Entry_get_inode,
        (setter)PyArchive_Entry_generic_immutable, NULL},
    {"mode", (getter)PyArchive_Entry_get_mode,
        (setter)PyArchive_Entry_generic_immutable, NULL},
    {"symlink", (getter)PyArchive_Entry_get_symlink,
        (setter)PyArchive_Entry_generic_immutable, NULL},
    {NULL}
};



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
    PyArchiveEntry_getsetters,        /* tp_getset */
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
mk_PyArchiveEntry(PyArchive *archive)
{
    PyArchiveEntry *pae = NULL;
#ifdef HAS_ARCHIVE_READ_NEXT_HEADER2
    if(archive_entry_free_count) {
        archive_entry_free_count--;
        pae = archive_entry_freelist[archive_entry_free_count];
        _Py_NewReference((PyObject *)pae);
    } else {
        pae = PyObject_GC_New(PyArchiveEntry,
            &PyArchiveEntryType);
        if(!pae)
            return NULL;
        pae->archive_entry = archive_entry_new();
        if(!pae->archive_entry) {
            pae->archive = NULL;
            Py_DECREF(pae);
            return NULL;
        }
    }
#else
    pae = PyObject_GC_New(PyArchiveEntry, &PyArchiveEntryType);
    if(!pae)
        return NULL;
    pae->archive_entry = NULL;
#endif
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


static int
PyArchive_init(PyArchive *self, PyObject *args, PyObject *kwds)
{
    PyObject *filepath = NULL;
    int ret;
    if(!PyArg_ParseTuple(args, "S", &filepath))
        return -1;
    if(NULL == (self->archive = archive_read_new())) {
        // memory...
        PyErr_NoMemory();
        return -1;
    }
    self->header_position = 0;

    /* XXX: rework this so that it allows controllable compression/format support
       from python side, and so that it handles warnings
    */
    if(ARCHIVE_OK != (ret = archive_read_support_compression_all(self->archive))) {
        goto pyarchive_init_err;
    }
    if(ARCHIVE_OK != (ret = archive_read_support_format_all(self->archive))) {
        goto pyarchive_init_err;
    }
    self->flags = 0;
    if(ARCHIVE_OK != (ret = archive_read_open_file(self->archive,
        PyString_AsString(filepath), DEFAULT_BUFSIZE))) {
        goto pyarchive_init_err;
    }
    self->header_position = 0;
    return 0;

pyarchive_init_err:
    _convert_and_set_archive_error(NULL, self->archive);
    archive_read_finish(self->archive);
    self->archive = NULL;
    self->flags |= PYARCHIVE_FAILURE;
    return -1;
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
    int ret;
    PyArchiveEntry *pae = NULL;
    if(self->flags) {
        /* something is wrong... */
        return NULL;
    }
    pae = mk_PyArchiveEntry(self);
    if(!pae)
        return NULL;

#ifdef HAS_ARCHIVE_READ_NEXT_HEADER2
    ret = archive_read_next_header2(self->archive, pae->archive_entry);
#else
    struct archive_entry *entry = NULL;
    ret = archive_read_next_header(self->archive, &entry);
    if(entry) {
        // clone the sucker into pae.
        pae->archive_entry =archive_entry_clone(entry);
        if(!pae->archive_entry) {
            // exception setting is missing her.
            Py_DECREF(pae);
            return NULL;
        }
    }
#endif
    if(ret == ARCHIVE_EOF) {
        self->flags |= PYARCHIVE_EOF;
    } else if(ret != ARCHIVE_OK) {
        // XXX yes this converts warnings into failures.  later.
        self->flags |= PYARCHIVE_FAILURE;
    } else {
        self->header_position++;
        return (PyObject *)pae;
    }
    Py_DECREF(pae);
    return NULL;
    self->header_position++;
    return (PyObject *)pae;
}

static PyTypeObject PyArchiveType = {
    PyObject_HEAD_INIT(NULL)
    0,                                /* ob_size */
    "archive",                        /* tp_name */
    sizeof(PyArchive),                /* tp_basicsize */
    0,                                /* tp_itemsize */
    (destructor)PyArchive_dealloc,    /* tp_dealloc */
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
    (initproc)PyArchive_init,         /* tp_init */
    0,                                /* tp_alloc */
    PyArchive_new,                    /* tp_new */
};

PyDoc_STRVAR(
    Pydocumentation,
    "fill this out");

PyMODINIT_FUNC
init_extension()
{
    PyObject *new_m = NULL;
    PyObject *err_m = NULL;
    new_m = Py_InitModule3("pyarchive._extension", NULL,
        Pydocumentation);
    if(!new_m)
        return;

    if(!(err_m = PyImport_ImportModule("pyarchive.errors")))
        return; 

    if(!(PyArchiveError = PyObject_GetAttrString(err_m, "PyArchiveError"))) {
        Py_DECREF(err_m);
        return;
    }

    Py_CLEAR(err_m);

    if(PyType_Ready(&PyArchiveType) < 0)
        return;

    if(PyModule_AddObject(new_m, "archive",
        (PyObject *)&PyArchiveType) == -1)
        return;

    if(PyType_Ready(&PyArchiveEntryType) < 0)
        return;
    if(PyModule_AddObject(new_m, "archive_entry",
        (PyObject *)&PyArchiveEntryType) == -1)
        return;

    if(PyModule_AddObject(new_m, "open",
        (PyObject *)&PyArchiveType) == -1)
        return;
}

