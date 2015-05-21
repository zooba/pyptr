#pragma once
#include "py_ptr.h"
#include "iterable.h"
#include <type_traits>

namespace python {
    template<typename TValue = py_ptr>
    struct py_set: public details::py_ptrbase<py_set<TValue>> {
        PYPTR_CONSTRUCTORS(py_set);
        PYPTR_ITERABLE(TValue);

        template <typename T>
        py_set(const details::py_ptrbase<T>& iterable) : Base(PySet_New(iterable)) {
            static_assert(::std::is_same<decltype(*T().begin()), TValue>::value, "invalid iterable");
        }

        void clear() {
            PySet_Clear(ptr);
        }

        bool contains(const typename details::pyptr_type<TValue>::type& value) {
            int res = PySet_Contains(ptr, value);
            if (res > 0) {
                return true;
            } else if (res < 0) {
                details::throw_pyerr();
            }
            return false;
        }

        bool add(const typename details::pyptr_type<TValue>::type& value) {
            int res = PySet_Contains(ptr, value);
            if (res > 0) {
                return false;
            } else if (res == 0) {
                if (PySet_Add(ptr, value) == 0) {
                    return true;
                }
            }
            details::throw_pyerr();
            return false;
        }

        TValue pop() const {
            return typename details::pyptr_type<TValue>::type(steal(PySet_Pop(ptr)));
        }

        inline size_t size() const {
            Py_ssize_t res = PyObject_Size(ptr);
            if (res < 0) {
                details::throw_pyerr();
                return 0;
            }
            return static_cast<size_t>(res);
        }

        static inline py_set<TValue> empty() {
            return steal(PySet_New(nullptr));
        }
    };

    PYPTR_TEMPLATE_CHECKPTR(py_set<TValue>, PySet_Type, PyAnySet_Check(ptr) != 0, typename TValue);
}
