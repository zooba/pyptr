#pragma once

#include "py_ptr.h"
#include "py_capsule.h"

namespace python {
    template<typename TInner> class class_factory;
    template<typename TInner> struct py_object;
    namespace details {
        template<typename TInner> struct check_ptr<py_object<TInner>>;

        template<typename TInner>
        struct update_inner<py_object<TInner>> {
            static void update(PyObject *ptr, TInner*& inner) {
                if (ptr != nullptr) {
                    py_capsule<TInner> caps(steal(PyObject_GetAttrString(ptr, "__pyptr_ptr__")));
                    if (caps) {
                        inner = *caps;
                    } else {
                        inner = nullptr;
                        details::throw_pyerr();
                    }
                } else {
                    inner = nullptr;
                }
            }
        };
    }

    template<typename TInner>
    struct py_object : public details::py_ptrbase<py_object<TInner>> {
    private:
        friend class_factory<TInner>;
        friend details::check_ptr<py_object<TInner>>;
    private:
        TInner *_inner;

        py_object(details::owned_ptr obj, TInner *inner) : _inner(inner) {
            py_capsule<TInner> caps(inner, true);
            if (PyObject_SetAttrString(static_cast<PyObject*>(obj), "__pyptr_ptr__", caps) < 0) {
                details::throw_pyerr();
            }
            ptr = static_cast<PyObject*>(obj);
        }
    public:
        py_object() : _inner(nullptr) { }
        py_object(nullptr_t) : _inner(nullptr) { }
        
        py_object(py_object&& other) : _inner(nullptr) {
            details::set_ptr<Type>::set(ptr, other.ptr, true);
            _inner = other._inner;
        }

        py_object(const py_object& other) {
            details::set_ptr<Type>::set_clone(ptr, other.ptr, true);
            _inner = other._inner;
        }

        py_object(details::owned_ptr other) : Base(other.ptr) {
            details::set_ptr<Type>::check(ptr, true);
            details::update_inner<Type>::update(ptr, _inner);
        }

        py_object(details::owned_ptr other, bool throwOnTypeError) : Base(other.ptr) {
            details::set_ptr<Type>::check(ptr, throwOnTypeError);
            details::update_inner<Type>::update(ptr, _inner);
        }

        py_object& operator =(const py_object& other) {
            details::set_ptr<Type>::replace_clone(ptr, other.ptr);
            details::update_inner<Type>::update(ptr, _inner);
            return *this;
        }

        py_object& operator =(py_object&& other) {
            details::set_ptr<Type>::replace(ptr, other.ptr);
            details::update_inner<Type>::update(ptr, _inner);
            return *this;
        }

        py_object& operator =(nullptr_t) {
            Py_CLEAR(ptr);
            _inner = nullptr;
            return *this;
        }

        py_object& operator =(details::owned_ptr other) {
            details::set_ptr<Type>::replace(ptr, other.ptr);
            details::update_inner<Type>::update(ptr, _inner);
            return *this;
        }

        TInner *operator *()  { return _inner; }
        TInner *operator ->() { return _inner; }
        const TInner *operator *()  const { return _inner; }
        const TInner *operator ->() const { return _inner; }
    };

    namespace details {
        template<typename TInner>
        struct check_ptr<py_object<TInner>> {
            static inline bool check(PyObject *ptr) {
                py_capsule<TInner> caps(steal(PyObject_GetAttrString(ptr, "__pyptr_ptr__")));
                if (!caps) {
                    PyErr_Clear();
                    return false;
                }
                return *caps != nullptr;
            }

            static inline const char *expected() {
                return typeid(TInner).name();
            }
        };
    }
}
