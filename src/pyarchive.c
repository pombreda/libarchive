// Copyright: 2009 Brian Harring <ferringb@gmail.com>
// License: GPL2/BSD

#include <Python.h>
#include <archive.h>
#include <archive_entry.h>
#include <unistd.h> /* gid_t, uid_t, time_t */
#include <sys/stat.h> /* S_IS* crap */

/*
todo

wide char:
  gname/uname support
  pathname
  symlink
  hardlink

optimizations:
  ArchiveEntry continual StringFromString;
   store/reuse the string if it was accessed to avoid repeat
   PyString initializations (could use intern alternatively)
  convert to archive_read_data_block perhaps?  Has some 'fun'
   issues when dealing w/ the vm's lifespan of objects vs
   libarchive's vm (mem obj management wise)
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
} PyArchiveStream;

typedef struct {
    PyObject_HEAD
    PyArchiveStream         *archive;
    Py_ssize_t              archive_header_position;
    struct archive_entry    *archive_entry;
} PyArchiveEntry;

#ifdef HAS_ARCHIVE_READ_NEXT_HEADER2
#define MAX_FREE_ARCHIVE_ENTRY 5
static PyArchiveEntry *archive_entry_freelist[MAX_FREE_ARCHIVE_ENTRY];
static int archive_entry_free_count = 0;
#endif

static PyObject *PyArchiveError = NULL;
static PyObject *UsageError = NULL;
// XXX: this won't survive long; hack
static PyObject *StringIO_kls = NULL;

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


static void
_usage_error(PyObject *err_kls, PyArchiveStream *archive, const char *msg)
{
    PyObject *tmp = NULL, *err_pstr = NULL;
    if(!err_kls)
        err_kls = (PyObject *)UsageError;

    if(err_pstr = PyString_FromString(msg)) {
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

/*
--methods--
*/

/*
--compatibility crap for TarInfo behaviour --
*/

static PyObject *
PyArchive_Entry_isblk(PyArchiveEntry *self)
{
    if(S_ISBLK(archive_entry_mode(self->archive_entry))) {
        Py_RETURN_TRUE;
    }
    Py_RETURN_FALSE;
}

static PyObject *
PyArchive_Entry_ischr(PyArchiveEntry *self)
{
    if(S_ISCHR(archive_entry_mode(self->archive_entry))) {
        Py_RETURN_TRUE;
    }
    Py_RETURN_FALSE;
}

static PyObject *
PyArchive_Entry_isdev(PyArchiveEntry *self)
{
    long mode = archive_entry_mode(self->archive_entry);
    if(S_ISBLK(mode) || S_ISCHR(mode)) {
        Py_RETURN_TRUE;
    }
    Py_RETURN_FALSE;
}

static PyObject *
PyArchive_Entry_isdir(PyArchiveEntry *self)
{
    if(S_ISDIR(archive_entry_mode(self->archive_entry))) {
        Py_RETURN_TRUE;
    }
    Py_RETURN_FALSE;
}

static PyObject *
PyArchive_Entry_isfifo(PyArchiveEntry *self)
{
    if(S_ISFIFO(archive_entry_mode(self->archive_entry))) {
        Py_RETURN_TRUE;
    }
    Py_RETURN_FALSE;
}

// broken out into a macro since it's used a few other places, and having
// to refcount true/false (and handle it at the invoker) is overkill
#define PyArchive_Entry_raw_isreg(self)    \
    (S_ISREG(archive_entry_mode(((PyArchiveEntry *)self)->archive_entry)))

// note isreg serves double duty as isfile
static PyObject *
PyArchive_Entry_isreg(PyArchiveEntry *self)
{
    if(PyArchive_Entry_raw_isreg(self)) {
        Py_RETURN_TRUE;
    }
    Py_RETURN_FALSE;
}

static PyObject *
PyArchive_Entry_issym(PyArchiveEntry *self)
{
    if(S_ISLNK(archive_entry_mode(self->archive_entry))) {
        Py_RETURN_TRUE;
    }
    Py_RETURN_FALSE;
}

/* XXX bloody hack */
static PyObject *
PyArchive_Entry_data(PyArchiveEntry *self)
{
    Py_ssize_t len = 0;
    PyObject *str = NULL, *obj = NULL;
    if(self->archive_header_position != self->archive->header_position) {
        _usage_error(NULL, self->archive, "stream isn't positioned to allow for data access");
        return NULL;
    }
    if(!(PyArchive_Entry_raw_isreg(self))) {
        PyErr_SetString(PyExc_TypeError, "data() is only usable on files");
        return NULL;
    }
    len = archive_entry_size(self->archive_entry);
    if(!(str = PyString_FromStringAndSize(NULL, len))) {
        return NULL;
    }
    if(len != archive_read_data(self->archive->archive, 
        PyString_AS_STRING(str), len)) {
        _convert_and_set_archive_error(NULL, self->archive->archive);
    } else {
        obj = PyObject_CallFunction(StringIO_kls, "O", str);
    }
    Py_DECREF(str);
    return obj;
}


static PyMethodDef PyArchiveEntry_methods[] = {
    {"isblk", (PyCFunction)PyArchive_Entry_isblk, METH_NOARGS},
    {"ischr", (PyCFunction)PyArchive_Entry_ischr, METH_NOARGS},
    {"isdev", (PyCFunction)PyArchive_Entry_isdev, METH_NOARGS},
    {"isdir", (PyCFunction)PyArchive_Entry_isdir, METH_NOARGS},
    {"isfifo", (PyCFunction)PyArchive_Entry_isfifo, METH_NOARGS},
    {"isreg", (PyCFunction)PyArchive_Entry_isreg, METH_NOARGS},
    {"isfile", (PyCFunction)PyArchive_Entry_isreg, METH_NOARGS},
    {"issym", (PyCFunction)PyArchive_Entry_issym, METH_NOARGS},
    {"data", (PyCFunction)PyArchive_Entry_data, METH_NOARGS},
    {NULL},
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
    PyArchiveEntry_methods,           /* tp_methods */
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
mk_PyArchiveEntry(PyArchiveStream *archive)
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
        pae->archive_header_position = archive->header_position;
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
PyArchiveStream_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    PyArchiveStream *self = (PyArchiveStream *)type->tp_alloc(type, 0);
    if(self)
        self->archive = NULL;
    return (PyObject *)self;
}


static int
PyArchiveStream_init(PyArchiveStream *self, PyObject *args, PyObject *kwds)
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
PyArchiveStream_dealloc(PyArchiveStream *self)
{
    if(self->archive) {
        archive_read_finish(self->archive);
        self->archive = NULL;
    }
    self->ob_type->tp_free((PyObject *)self);
}


static PyObject *
PyArchiveStream_iternext(PyArchiveStream *self)
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

static PyTypeObject PyArchiveStreamType = {
    PyObject_HEAD_INIT(NULL)
    0,                                /* ob_size */
    "archive",                        /* tp_name */
    sizeof(PyArchiveStream),                /* tp_basicsize */
    0,                                /* tp_itemsize */
    (destructor)PyArchiveStream_dealloc,    /* tp_dealloc */
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
    (iternextfunc)PyArchiveStream_iternext, /* tp_iternext */
    0,                                /* tp_methods */
    0,                                /* tp_members */
    0,                                /* tp_getset */
    0,                                /* tp_base */
    0,                                /* tp_dict */
    0,                                /* tp_descr_get */
    0,                                /* tp_descr_set */
    0,                                /* tp_dictoffset */
    (initproc)PyArchiveStream_init,         /* tp_init */
    0,                                /* tp_alloc */
    PyArchiveStream_new,                    /* tp_new */
};

PyDoc_STRVAR(
    Pydocumentation,
    "fill this out");

PyMODINIT_FUNC
init_extension()
{
    PyObject *new_m = NULL;
    PyObject *tmp = NULL;

    if(NULL == (tmp = PyImport_ImportModule("cStringIO"))) {
        PyErr_Clear();
        if(NULL == (tmp = PyImport_ImportModule("StringIO"))) {
            return;
        }
    }

    if(NULL == (StringIO_kls = PyObject_GetAttrString(tmp, "StringIO"))) {
        Py_DECREF(tmp);
        return;
    }

    Py_DECREF(tmp);

    if(NULL == (tmp = PyImport_ImportModule("pyarchive.errors")))
        return; 

    if(!(PyArchiveError = PyObject_GetAttrString(tmp, "PyArchiveError"))) {
        Py_DECREF(tmp);
        return;
    }

    if(!(UsageError = PyObject_GetAttrString(tmp, "PyArchiveError"))) {
        Py_DECREF(tmp);
        return;
    }

    Py_DECREF(tmp);

    new_m = Py_InitModule3("pyarchive._extension", NULL,
        Pydocumentation);
    if(!new_m)
        return;

    if(PyType_Ready(&PyArchiveStreamType) < 0)
        return;

    if(PyModule_AddObject(new_m, "archive_stream",
        (PyObject *)&PyArchiveStreamType) == -1)
        return;

    if(PyType_Ready(&PyArchiveEntryType) < 0)
        return;

    if(PyModule_AddObject(new_m, "archive_entry",
        (PyObject *)&PyArchiveEntryType) == -1)
        return;

}

