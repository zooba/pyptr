#pragma once

#include "py_ptr.h"

namespace python {
    template<typename T>
    struct py_iterator : public details::py_ptrbase<py_iterator<T>> {
        PYPTR_CONSTRUCTORS(py_iterator)

        typename details::pyptr_type<T>::type next() {
            if (ptr == nullptr) {
                return T();
            }

            auto nextPtr = PyIter_Next(ptr);
            if (nextPtr == nullptr && (PyErr_Occurred() == nullptr || PyErr_ExceptionMatches(PyExc_StopIteration))) {
                PyErr_Clear();
                details::set_ptr<Type>::set_clone(ptr, nullptr, true);
                return nullptr;
            }
            return steal(nextPtr);
        }
    };

    PYPTR_TEMPLATE_CHECKPTR(py_iterator<T>, "iterator", PyIter_Check(ptr), typename T)

    template<typename T>
    struct py_generator : public details::py_ptrbase<py_generator<T>> {
        PYPTR_CONSTRUCTORS(py_generator);

    private:
        T send_internal(PyObject *value) {
            if (ptr == nullptr) {
                return nullptr;
            }

            auto nextPtr = _PyGen_Send(ptr, value);
            if (nextPtr == nullptr) {
                if (PyErr_Occurred() == nullptr || PyErr_ExceptionMatches(PyExc_StopIteration) || PyErr_ExceptionMatches(PyExc_GeneratorExit)) {
                    PyErr_Clear();
                } else {
                    details::throw_pyerr();
                }
                details::clear_ptr(ptr, nullptr);
                return T();
            }
            return typename details::pyptr_type<T>::type(steal(nextPtr));
        }
    public:
        template<typename TArg>
        T send(const TArg& value) {
            return send_internal(value);
        }

        T next() {
            return send_internal(nullptr);
        }

#if PY_MAJOR_VERSION == 3 && PY_MINOR_VERSION >= 3
    private:
        template<typename TReturn>
        T send_internal(PyObject *value, TReturn& returnValue) const {
            if (ptr == nullptr) {
                returnValue = TReturn();
                return T();
            }

            auto nextPtr = _PyGen_Send(ptr, value);
            if (nextPtr == nullptr) {
                PyObject *returned;
                if (_PyGen_FetchStopIterationValue(&returned) != 0) {
                    details::throw_pyerr();
                }
                returnValue = typename details::pyptr_type<TReturn>::type(borrow(returned));
                details::clear_ptr(ptr, nullptr);
                return nullptr;
            }
            return typename details::pyptr_type<T>::type(steal(nextPtr));
        }
    public:
        template<typename TArg, typename TReturn>
        T send(const TArg& value, TReturn& returnValue) const {
            return send_internal(value, returnValue);
        }

        template<typename TReturn>
        T next(TReturn& returnValue) {
            return send_internal(nullptr, returnValue);
        }
#endif
    };

    PYPTR_TEMPLATE_CHECKPTR(py_generator<T>, PyGen_Type, PyGen_Check(ptr), typename T);

    namespace details {
        template<typename T>
        struct iterator {
            typedef typename ::std::forward_iterator_tag iterator_category;
            typedef typename T value_type;
            typedef typename void difference_type;
            typedef difference_type distance_type;
            typedef typename T *pointer;
            typedef typename T& reference;

            py_iterator<T> iterObj;
            typename pyptr_type<T>::type current;

            iterator(py_iterator<T> iterObj) : iterObj(iterObj) { }

            iterator<T> operator++(int) {
                auto result = *this;
                current = iterObj.next();
                return result;
            }

            iterator<T>& operator++() {
                current = iterObj.next();
                return *this;
            }

            T operator*() const {
                return current;
            }

            bool operator==(const iterator<T>& other) const {
                return !iterObj && !other.iterObj;
            }

            bool operator!=(const iterator<T>& other) const {
                return iterObj || other.iterObj;
            }
        };

        #define PYPTR_ITERABLE(T) \
            py_iterator<T> iter() { return steal(PyObject_GetIter(ptr)); } \
            details::iterator<T> begin() { return ++details::iterator<T>(iter()); } \
            details::iterator<T> end() { return details::iterator<T>(nullptr); }

    }
}

