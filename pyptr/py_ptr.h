#pragma once

#ifdef INCLUDE_PYTHON_H
// If you define INCLUDE_PYTHON_H, python.h must be in your search path.
#include <python.h>
#endif

#include <exception>
#include <functional>
#include <memory>
#include <string>

#ifdef _DEBUG
#define pyptr_typeCHECK
#define pyptr_typeCHECK_EXCEPTION
#endif

namespace python {
    namespace details {
        template<size_t... Ts> struct indices {
            template<size_t i> using append = indices<Ts..., i>;
        };
        template<size_t Size> struct make_indices {
            typedef typename make_indices<Size - 1>::type::template append<Size - 1> type;
        };
        template<> struct make_indices<(size_t)0> {
            typedef indices<> type;
        };

        struct owned_ptr {
            PyObject *ptr;
            owned_ptr(PyObject *ptr) : ptr(ptr) { }
            owned_ptr(const owned_ptr& other) : ptr(other.ptr) { }
            owned_ptr(owned_ptr&& other) : ptr(other.ptr) { other.ptr = nullptr; }
            explicit operator PyObject *() { return ptr; }
        };

        template<typename T>
        struct pyptr_type;
    }

    inline details::owned_ptr steal(PyObject *ptr) {
        return ptr;
    }

    inline details::owned_ptr borrow(PyObject *ptr) {
        Py_XINCREF(ptr);
        return ptr;
    }

    namespace details {
        inline void throw_pyerr();

#ifdef pyptr_typeCHECK_EXCEPTION
        inline void invalid_type(PyObject*& ptr, const char *expected) {
            Py_CLEAR(ptr);
            PyErr_SetString(PyExc_TypeError, expected);
            throw_pyerr();
        }

        inline void invalid_type(PyObject*& ptr, PyTypeObject *expected) {
            Py_CLEAR(ptr);
            if (expected != nullptr) {
                PyErr_SetObject(PyExc_TypeError, reinterpret_cast<PyObject*>(expected));
            } else {
                PyErr_SetNone(PyExc_TypeError);
            }
            throw_pyerr();
        }
#else
        inline void invalid_type(PyObject*& ptr, const char *expected) { Py_CLEAR(ptr); }
        inline void invalid_type(PyObject*& ptr, PyTypeObject *expected) { Py_CLEAR(ptr); }
#endif
        inline void invalid_type(PyObject*& ptr, PyTypeObject& expected) { invalid_type(ptr, &expected); }
        inline void invalid_type(PyObject*& ptr, nullptr_t) { Py_CLEAR(ptr); }

        template<typename Maker>
        struct lazy_pyptr {
            mutable PyObject *ptr;
            
            lazy_pyptr() : ptr(nullptr) { }
            ~lazy_pyptr() { Py_XDECREF(ptr); }

            PyObject *operator()() const {
                if (ptr == nullptr) {
                    ptr = Maker()();
                }
                return ptr;
            }
        };

        struct tuple_maker {
            PyObject *operator()() { return PyTuple_New(0); }
        };

        struct _py_ptrbase {
        protected:
            mutable PyObject *ptr;
            _py_ptrbase() : ptr(nullptr) { }
        public:
            ~_py_ptrbase() {
                Py_CLEAR(ptr);
            }

            operator PyObject *() const {
                return ptr;
            }
        };

        template<typename T>
        struct check_ptr {
            static bool check(PyObject *ptr) { return true; }
            static nullptr_t expected() { return nullptr; }
        };

#define PYPTR_TEMPLATE_CHECKPTR(CXXTYPE, PYTYPE, TYPECHECK, TYPENAMES) \
        template<TYPENAMES> struct details::check_ptr<CXXTYPE> { \
            static inline bool check(PyObject *ptr) { return (TYPECHECK); } \
            static inline decltype(PYTYPE)& expected() { return (PYTYPE); } \
        };

#define PYPTR_SIMPLE_CHECKPTR(CXXTYPE, PYTYPE, TYPECHECK) PYPTR_TEMPLATE_CHECKPTR(CXXTYPE, PYTYPE, TYPECHECK,)


        template<typename T>
        struct set_ptr {
            static void check(PyObject*& ptr, bool throwOnTypeError) {
                if (ptr == nullptr) {
#ifdef pyptr_typeCHECK
                    throw_pyerr();
#endif
                } else if (!check_ptr<T>::check(ptr)) {
                    if (!throwOnTypeError) {
                        ptr = nullptr;
                        return;
                    }
#ifdef pyptr_typeCHECK
                    invalid_type(ptr, check_ptr<T>::expected());
#endif
                }
            }

            static void replace(PyObject*& ptr, PyObject*& other, bool throwOnTypeError) {
                std::swap(ptr, other);
                check(ptr, throwOnTypeError);
            }

            static void replace_clone(PyObject*& ptr, PyObject *other, bool throwOnTypeError) {
                Py_XINCREF(other);
                Py_XDECREF(ptr);
                ptr = other;
                check(ptr, throwOnTypeError);
            }

            static void set(PyObject*& ptr, PyObject*& other, bool throwOnTypeError) {
                ptr = other;
                other = nullptr;
                check(ptr, throwOnTypeError);
            }

            static void set_clone(PyObject*& ptr, PyObject *other, bool throwOnTypeError) {
                Py_XINCREF(other);
                ptr = other;
                check(ptr, throwOnTypeError);
            }
        };

#define PYPTR_CONSTRUCTORS_NO_DEFAULT(CLS) \
    CLS(nullptr_t) { } \
    CLS(CLS&& other) { details::set_ptr<Type>::set(ptr, other.ptr, true); } \
    CLS(const CLS& other) { details::set_ptr<Type>::set_clone(ptr, other.ptr, true); } \
    CLS(details::owned_ptr other) : Base(other.ptr) { details::set_ptr<Type>::check(ptr, true); } \
    CLS(details::owned_ptr other, bool throwOnTypeError) : Base(other.ptr) { details::set_ptr<Type>::check(ptr, throwOnTypeError); } \
    CLS& operator =(const CLS& other) { details::set_ptr<Type>::replace_clone(ptr, other.ptr, true); return *this; } \
    CLS& operator =(CLS&& other) { details::set_ptr<Type>::replace(ptr, other.ptr, true); return *this; } \
    CLS& operator =(nullptr_t) { Py_CLEAR(ptr); return *this; } \
    CLS& operator =(details::owned_ptr other) { details::set_ptr<Type>::replace(ptr, other.ptr, true); return *this; }

#define PYPTR_CONSTRUCTORS(CLS) \
    CLS() { } \
    PYPTR_CONSTRUCTORS_NO_DEFAULT(CLS)

        template<typename MyT>
        struct py_ptrbase : public _py_ptrbase {
            typedef py_ptrbase<MyT> Base;
            typedef MyT Type;

        protected:
            py_ptrbase() { ptr = nullptr; }
            py_ptrbase(PyObject *other) { ptr = other; }
            py_ptrbase(details::owned_ptr other) { details::set_ptr<Type>::set(ptr, other.ptr, true); }
            py_ptrbase(details::owned_ptr other, bool throwOnTypeError) { details::set_ptr<Type>::set(ptr, other.ptr, throwOnTypeError); }

        public:
            PyObject *detach() {
                auto res = ptr;
                ptr = nullptr;
                return res;
            }

            template<typename T>
            typename pyptr_type<T>::type cast() const {
                return typename pyptr_type<T>::type(borrow(ptr), false);
            }

            operator bool() const {
                return ptr != nullptr;
            }

            bool operator!() const {
                return ptr == nullptr;
            }

            bool is_true() const {
                return ptr != nullptr && PyObject_IsTrue(ptr) != 0;
            }

            template<typename T>
            bool is(const T& other) {
                return ptr == (PyObject*)other;
            }

            template<typename T>
            bool is_not(const T& other) {
                return ptr != (PyObject*)other;
            }

            template<typename T>
            bool isinstance(const T& type) {
                switch (PyObject_IsInstance(ptr, type)) {
                case 1:
                    return true;
                case 0:
                    return false;
                default:
                    throw_pyerr();
                    return false;
                }
            }
        };
    }

    struct py_ptr : public details::py_ptrbase<py_ptr> {
        PYPTR_CONSTRUCTORS(py_ptr);
        
        template<typename T>
        py_ptr(const details::py_ptrbase<T>& other) : Base(borrow(other)) { }
    };

    PYPTR_SIMPLE_CHECKPTR(py_ptr, "object", ptr != nullptr)

    namespace details {
        template<typename T1, typename T2, bool UseFirst>
        struct pyptr_type_helper { typedef T1 type; };
        template<typename T1, typename T2>
        struct pyptr_type_helper<T1, T2, false> { typedef T2 type; };
        
        template<typename T>
        struct pyptr_type {
            typedef typename pyptr_type_helper<T, py_ptr, ::std::is_base_of<_py_ptrbase, T>::value>::type type;
        };

        template<>
        struct pyptr_type<owned_ptr> { typedef py_ptr type; };

        template<>
        struct pyptr_type<void> { typedef py_ptr type; };

        template<typename T, typename T2 = T>
        PyObject *detach(T item) {
            return typename pyptr_type<T2>::type(item).detach();
        }
    }
}

#include "py_capsule.h"
#include "iterable.h"
#include "strings.h"
#include "primitives.h"
#include "tuple.h"
#include "list.h"
#include "dict.h"
#include "set.h"
#include "callable.h"
#include "callback.h"

#include "object_methods.h"
#include "py_code.h"
#include "module.h"
#include "class_factory.h"

#include "initialization.h"
#include "errors.h"

