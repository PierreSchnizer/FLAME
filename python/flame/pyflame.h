#include <stdexcept>

#ifndef PYFLAME_H
#define PYFLAME_H
#define PY_SSIZE_T_CLEAN
#include <Python.h>

struct Config;
struct StateBase;

Config* list2conf(PyObject *dict);
void List2Config(Config& ret, PyObject *dict, unsigned depth=0);
PyObject* conf2dict(const Config *conf);

PyObject* wrapstate(StateBase*); // takes ownership of argument from caller
StateBase* unwrapstate(PyObject*); // ownership of returned pointer remains with argument

PyObject* PyGLPSPrint(PyObject *, PyObject *args);
Config* PyGLPSParse2Config(PyObject *, PyObject *args, PyObject *kws);
PyObject* PyGLPSParse(PyObject *, PyObject *args, PyObject *kws);

int registerModMachine(PyObject *mod);
int registerModState(PyObject *mod);

#define CATCH2V(CXX, PYEXC) catch(CXX& e) { if(!PyErr_Occurred()) PyErr_SetString(PyExc_##PYEXC, e.what()); return; }
#define CATCH3(CXX, PYEXC, RET) catch(CXX& e) { if(!PyErr_Occurred()) PyErr_SetString(PyExc_##PYEXC, e.what()); return RET; }
#define CATCH2(CXX, PYEXC) CATCH3(CXX, PYEXC, NULL)
#define CATCH1(RET) CATCH3(std::exception, RuntimeError, RET)
#define CATCH() CATCH2(std::exception, RuntimeError)

#if PY_MAJOR_VERSION >= 3
#define PyInt_Type PyLong_Type
#define PyInt_Check PyLong_Check
#define PyInt_FromLong PyLong_FromLong
#define PyInt_FromSize_t PyLong_FromSize_t
#define PyInt_AsLong PyLong_AsLong
#define PyString_Check PyUnicode_Check
#define PyString_FromString PyUnicode_FromString
#define MODINIT_RET(VAL) return (VAL)

#else
#define MODINIT_RET(VAL) return

#endif

struct borrow {};

template<typename T = PyObject>
struct PyRef {
    T* _ptr;
    PyRef() :_ptr(NULL) {}
    //! copy an existing reference (calls Py_XINCREF)
    PyRef(const PyRef& o)
        :_ptr(o._ptr)
    {
        Py_XINCREF(_ptr);
    }
    //! Construct around an existing reference (does not call Py_XINCREF).
    //! Explicitly take ownership of a reference which must already
    //! be implicitly owned by the caller.
    //! @throws std::bad_allo if p==NULL (assumed to be a python exception)
    explicit PyRef(PyObject* p) : _ptr((T*)p) {
        if(!p)
            throw std::bad_alloc(); // TODO: probably already a python exception
    }
    //! Construct around a borrowed reference (calls Py_XINCREF)
    PyRef(T* p, borrow) : _ptr(p) {
        if(!p)
            throw std::bad_alloc();
        Py_INCREF(p);
    }
    ~PyRef() {Py_CLEAR(_ptr);}
    PyRef& operator =(const PyRef& o)
    {
        if(&o!=this) {
            PyObject *tmp = _ptr;
            _ptr = o._ptr;
            Py_XINCREF(_ptr);
            Py_XDECREF(tmp);
        }
        return *this;
    }
    T* release() {
        T* ret = _ptr;
        assert(ret);
        _ptr = NULL;
        return ret;
    }
    void clear() {
        Py_CLEAR(_ptr);
    }
    void reset(T* p) {
        if(!p)
            throw std::bad_alloc(); // TODO: probably already a python exception
        Py_CLEAR(_ptr);
        _ptr = p;
    }
    void reset(T* p, borrow) {
        if(!p)
            throw std::bad_alloc(); // TODO: probably already a python exception
        PyObject *tmp = _ptr;
        _ptr = p;
        Py_INCREF(p);
        Py_XDECREF(tmp);
    }
    struct allow_null {};
    T* reset(T* p, allow_null) {
        Py_CLEAR(_ptr);
        _ptr = p;
        return p;
    }
    T* get() const { return _ptr; }
    PyObject* releasePy() {
        return (PyObject*)release();
    }
    T* py() const {
        return (T*)_ptr;
    }
    template<typename E>
    E* as() const {
        return (E*)_ptr;
    }
    T& operator*() const {
        return *_ptr;
    }
    T* operator->() const {
        return _ptr;
    }
};

// Extract C string from python object.  py2 str or unicode, py3 str only (aka. unicode)
struct PyCString
{
    PyRef<> pystr; // py2 str or py3 bytes
    PyCString() {}
    PyCString(const PyRef<>& o) { *this = o; }
    PyCString& operator=(const PyRef<>& o) { reset(o.py()); return *this; }
    void reset(PyObject *obj)
    {
        if(PY_MAJOR_VERSION>=3 || PyUnicode_Check(obj)) {
            pystr.reset(PyUnicode_AsUTF8String(obj));
        } else if(PyBytes_Check(obj)) {
            pystr.reset(obj, borrow());
        }
    }
    const char *c_str(PyObject *obj) {
        if(!obj)
            throw std::bad_alloc();
        reset(obj);
        return c_str();
    }
    const char *c_str() const {
        const char *ret = PyBytes_AsString(pystr.py());
        if(!ret)
            throw std::invalid_argument("Can't extract string from object");
        return ret;

    }
private:
    PyCString(const PyCString&);
    PyCString& operator=(const PyCString&);
};

struct PyGetBuf
{
    Py_buffer buf;
    bool havebuf;
    PyGetBuf() : havebuf(false) {}
    ~PyGetBuf() {
        if(havebuf) PyBuffer_Release(&buf);
    }
    bool get(PyObject *obj) {
        if(havebuf) PyBuffer_Release(&buf);
        havebuf = PyObject_GetBuffer(obj, &buf, PyBUF_SIMPLE)==0;
        if(!havebuf) PyErr_Clear();
        return havebuf;
    }
    size_t size() const { return havebuf ? buf.len : 0; }
    void * data() const { return buf.buf; }
};

#endif // PYFLAME_H
