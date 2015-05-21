#pragma once

#include "py_ptr.h"
#include "strings.h"

namespace python {
    struct py_bool;
    struct py_int;
    struct py_float;

    PYPTR_SIMPLE_CHECKPTR(py_bool, PyBool_Type, PyBool_Check(ptr));
    PYPTR_SIMPLE_CHECKPTR(py_int, PyLong_Type, PyLong_Check(ptr));
    PYPTR_SIMPLE_CHECKPTR(py_float, PyFloat_Type, PyFloat_Check(ptr));

    namespace details {
        template<> struct pyptr_type<bool> { typedef py_bool type; };
        template<> struct pyptr_type<int> { typedef py_int type; };
        template<> struct pyptr_type<unsigned int> { typedef py_int type; };
        template<> struct pyptr_type<long> { typedef py_int type; };
        template<> struct pyptr_type<unsigned long> { typedef py_int type; };
        template<> struct pyptr_type<long long> { typedef py_int type; };
        template<> struct pyptr_type<unsigned long long> { typedef py_int type; };
        template<> struct pyptr_type<float> { typedef py_float type; };
        template<> struct pyptr_type<double> { typedef py_float type; };
    }

    struct py_bool : public details::py_ptrbase<py_bool> {
        PYPTR_CONSTRUCTORS(py_bool);
        py_bool(bool value) : Base(PyBool_FromLong(value ? 1 : 0)) { }

        inline explicit operator bool() const {
            return is_true();
        }

        inline bool operator!() const {
            return !is_true();
        }
    };

    struct py_int : public details::py_ptrbase<py_int> {
        PYPTR_CONSTRUCTORS(py_int);

        py_int(int value) : Base(PyLong_FromLong(value)) { }
        py_int(unsigned int value) : Base(PyLong_FromUnsignedLong(value)) { }
        py_int(long value) : Base(PyLong_FromLong(value)) { }
        py_int(unsigned long value) : Base(PyLong_FromUnsignedLong(value)) { }
        py_int(long long value) : Base(PyLong_FromLongLong(value)) { }
        py_int(unsigned long long value) : Base(PyLong_FromUnsignedLongLong(value)) { }

        inline py_int(const py_str& value) : Base(PyNumber_Long(value)) { details::check_ptr<py_int>::check(ptr); }
        inline py_int(const py_ptr& value) : Base(PyNumber_Long(value)) { details::check_ptr<py_int>::check(ptr); }

#if PY_MAJOR_VERSION == 3
#if PY_MINOR_VERSION >= 3
        inline py_int(const py_str& value, int base) : Base(PyLong_FromUnicodeObject(value, base)) { details::check_ptr<py_int>::check(ptr); }
#else
        inline py_int(const py_str& value, int base) : Base(PyLong_FromUnicode(PyUnicode_DATA(value), PyObject_Size(value), base)) { details::check_ptr<py_int>::check(ptr); }
#endif

#elif PY_MAJOR_VERSION == 2
        inline py_int(const py_str& value, int base) : Base(PyLong_FromString(PyString_AS_STRING(static_cast<PyObject*>(value)), nullptr, base)) { details::check_ptr<py_int>::check(ptr); }

#endif

        inline operator int() const {
            auto result = PyLong_AsLong(ptr);
            if (PyErr_Occurred()) details::throw_pyerr();
            return (int)result;
        }

        inline operator unsigned int() const {
            auto result = PyLong_AsUnsignedLong(ptr);
            if (PyErr_Occurred()) details::throw_pyerr();
            return (unsigned int)result;
        }

        inline operator long() const {
            auto result = PyLong_AsLong(ptr);
            if (PyErr_Occurred()) details::throw_pyerr();
            return result;
        }

        inline operator unsigned long() const {
            auto result = PyLong_AsUnsignedLong(ptr);
            if (PyErr_Occurred()) details::throw_pyerr();
            return result;
        }

        inline operator long long() const {
            auto result = PyLong_AsLongLong(ptr);
            if (PyErr_Occurred()) details::throw_pyerr();
            return result;
        }

        inline operator unsigned long long() const {
            auto result = PyLong_AsUnsignedLongLong(ptr);
            if (PyErr_Occurred()) details::throw_pyerr();
            return result;
        }
    };

    struct py_float : public details::py_ptrbase<py_float> {
        PYPTR_CONSTRUCTORS(py_float);

        py_float(float value) : Base(PyFloat_FromDouble((double)value)) { }
        py_float(double value) : Base(PyFloat_FromDouble(value)) { }

        py_float(const py_str& value) : Base(PyNumber_Float(value)) { details::check_ptr<py_float>::check(ptr); }
        py_float(const py_ptr& value) : Base(PyNumber_Float(value)) { details::check_ptr<py_float>::check(ptr); }

        operator float() const {
            auto result = PyFloat_AsDouble(ptr);
            if (PyErr_Occurred()) details::throw_pyerr();
            return (float)result;
        }

        operator double() const {
            auto result = PyFloat_AsDouble(ptr);
            if (PyErr_Occurred()) details::throw_pyerr();
            return result;
        }
    };
}
