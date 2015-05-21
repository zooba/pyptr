#pragma once

#include "py_ptr.h"

namespace python {
    namespace details {
        inline void throw_pyerr() {
            if (PyErr_Occurred()) {
                PyObject *exc_type, *exc_value, *trace;
                PyErr_Fetch(&exc_type, &exc_value, &trace);
                PyErr_NormalizeException(&exc_type, &exc_value, &trace);
                auto what = static_cast<::std::string>(format("%s: %s", make_py_tuple(getattr<py_str>(py_ptr(borrow(exc_type)), "__name__"), borrow(exc_value))));
                Py_XDECREF(exc_value);
                Py_XDECREF(trace);
                PyErr_Clear();
                // TODO: Throw correct error
                if (exc_type == PyExc_RuntimeError) {
                    Py_DECREF(exc_type);
                    throw ::std::runtime_error(what.c_str());
                }
                Py_XDECREF(exc_type);
                throw ::std::exception(what.c_str());
            }
        }
    }
}

