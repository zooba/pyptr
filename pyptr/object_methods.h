#pragma once

#include "py_ptr.h"
#include "strings.h"

namespace python {
    inline py_ptr getattr(const details::_py_ptrbase& object, const py_str& attr) {
        return steal(PyObject_GetAttr(object, attr));
    }

    template<typename TValue>
    TValue getattr(const details::_py_ptrbase& object, const py_str& attr) {
        return typename details::pyptr_type<TValue>::type(steal(PyObject_GetAttr(object, attr)));
    }

    inline py_ptr getattr(const details::_py_ptrbase& object, const py_str& attr, py_ptr defaultValue) {
        auto ptr = PyObject_GetAttr(object, attr);
        if (ptr == nullptr && PyErr_ExceptionMatches(PyExc_AttributeError)) {
            PyErr_Clear();
            return defaultValue;
        }
        return steal(ptr);
    }

    template<typename TValue>
    TValue getattr(const details::_py_ptrbase& object, const py_str& attr, TValue defaultValue) {
        auto ptr = PyObject_GetAttr(object, attr);
        if (ptr == nullptr && PyErr_ExceptionMatches(PyExc_AttributeError)) {
            PyErr_Clear();
            return typename details::pyptr_type<TValue>::type(defaultValue);
        }
        return typename details::pyptr_type<TValue>::type(steal(ptr));
    }

    template<typename TValue>
    void setattr(const details::_py_ptrbase& object, const py_str& attr, const TValue& value) {
        if (PyObject_SetAttr(object, attr, typename details::pyptr_type<TValue>::type(value)) != 0) {
            details::throw_pyerr();
        }
    }

    inline void delattr(const details::_py_ptrbase& object, const py_str& attr) {
        if (PyObject_DelAttr(object, attr) != 0) {
            details::throw_pyerr();
        }
    }
}
