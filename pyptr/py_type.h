#pragma once

#include "py_ptr.h"
#include "tuple.h"
#include "strings.h"
#include "dict.h"

namespace python {
    template<typename TInstance>
    struct py_type : public details::py_ptrbase<py_type<TInstance>> {
        PYPTR_CONSTRUCTORS(py_type);

        explicit py_type(py_str name)
            : Base(steal(PyObject_Call(reinterpret_cast<PyObject*>(&PyType_Type), make_py_tuple(name, py_tuple<>::empty(), py_dict<>::empty()), nullptr))) { }

        py_type(py_str name, py_dict<py_str, py_ptr> dict) 
            : Base(steal(PyObject_Call(reinterpret_cast<PyObject*>(&PyType_Type), make_py_tuple(name, py_tuple<>::empty(), dict), nullptr))) { }

        py_type(py_str name, py_tuple<py_type> bases, py_dict<py_str, py_ptr> dict)
            : Base(steal(PyObject_Call(reinterpret_cast<PyObject*>(&PyType_Type), make_py_tuple(name, bases, dict), nullptr))) { }
    };

    PYPTR_TEMPLATE_CHECKPTR(py_type<T>, PyType_Type, PyType_Check(ptr), typename T);

    namespace details {
        template<typename TInstance>
        struct return_type<py_type<TInstance>> {
            typedef py_object<TInstance> type;
        };

        template<> struct return_type<py_type<py_ptr>> { typedef py_ptr type; };
        template<> struct return_type<py_type<void>> { typedef py_ptr type; };

        template<typename TInstance, typename TDict>
        typename return_type<py_type<TInstance>>::type
        call(const py_type<TInstance>& callable, PyObject *args, TDict kwArgs) {
            auto inst = PyObject_Call(callable, args, kwArgs);
            if (inst != nullptr) {
                auto dict = PyObject_GetAttrString(inst, "__dict__");
                if (dict == nullptr) {
                    if (PyErr_ExceptionMatches(PyExc_AttributeError)) {
                        PyErr_Clear();
                    }
                } else {
                    Py_DECREF(dict);
                }
            }
            return steal(inst);
        }
    }
}
