#pragma once

#include "py_ptr.h"

namespace python {
    template<typename TInner> struct py_capsule;
    namespace details {
        template<typename T>
        struct update_inner { };

        template<typename TInner>
        struct update_inner<py_capsule<TInner>> {
            static void update(PyObject *ptr, TInner*& inner) {
                if (ptr != nullptr) {
#if PY_MAJOR_VERSION == 2 && PY_MINOR_VERSION <= 6
                    auto id = PyCObject_GetDesc(ptr);
                    if (id != nullptr && ::std::strcmp(reinterpret_cast<char*>(id), typeid(py_capsule<TInner>).name()) == 0) {
                        inner = reinterpret_cast<TInner*>(PyCObject_AsVoidPtr(ptr));
                    } else {
                        inner = nullptr;
                    }
#else
                    inner = reinterpret_cast<TInner*>(PyCapsule_GetPointer(ptr, typeid(py_capsule<TInner>).name()));
#endif
                } else {
                    inner = nullptr;
                }
            }
        };
    }

    template<typename TInner>
    struct py_capsule : public details::py_ptrbase<py_capsule<TInner>> {
    private:
        TInner *_inner;

#if PY_MAJOR_VERSION == 2 && PY_MINOR_VERSION <= 6
        inline static void dealloc(void *ptr, void *id) {
            if (ptr != nullptr && id != nullptr && ::std::strcmp(reinterpret_cast<const char*>(id), typeid(Type).name()) == 0) {
                delete reinterpret_cast<TInner*>(ptr);
            }
        }
#else
        inline static void dealloc(PyObject *caps) {
            if (caps != nullptr && PyCapsule_CheckExact(caps)) {
                auto ptr = reinterpret_cast<TInner*>(PyCapsule_GetPointer(caps, typeid(Type).name()));
                delete ptr;
            }
        }
#endif
    public:
        py_capsule() : _inner(nullptr) { }
        py_capsule(nullptr_t) : _inner(nullptr) { }

#if PY_MAJOR_VERSION == 2 && PY_MINOR_VERSION <= 6
        py_capsule(TInner *inner)
            : Base(PyCObject_FromVoidPtrAndDesc(inner, const_cast<void*>(reinterpret_cast<const void*>(typeid(Type).name())), dealloc)), _inner(inner) { }
        py_capsule(TInner *inner, bool deleteInnerOnFree)
            : Base(PyCObject_FromVoidPtrAndDesc(inner, const_cast<void*>(reinterpret_cast<const void*>(typeid(Type).name())), deleteInnerOnFree ? dealloc : nullptr)), _inner(inner) { }
#else
        py_capsule(TInner *inner)
            : Base(PyCapsule_New(inner, typeid(Type).name(), dealloc)), _inner(inner) { }
        py_capsule(TInner *inner, bool deleteInnerOnFree)
            : Base(PyCapsule_New(inner, typeid(Type).name(), deleteInnerOnFree ? dealloc : nullptr)), _inner(inner) { }
#endif

        py_capsule(py_capsule&& other) : _inner(nullptr) {
            details::set_ptr<Type>::set(ptr, other.ptr, true);
            _inner = other._inner;
        }

        py_capsule(const py_capsule& other) {
            details::set_ptr<Type>::set_clone(ptr, other.ptr, true);
            _inner = other._inner;
        }

        py_capsule(details::owned_ptr other) : Base(other.ptr) {
            details::set_ptr<Type>::check(ptr, true);
            details::update_inner<Type>::update(ptr, _inner);
        }

        py_capsule(details::owned_ptr other, bool throwOnTypeError) : Base(other.ptr) {
            details::set_ptr<Type>::check(ptr, throwOnTypeError);
            details::update_inner<Type>::update(ptr, _inner, throwOnTypeError);
        }

        py_capsule& operator =(const py_capsule& other) {
            details::set_ptr<Type>::replace_clone(ptr, other.ptr, true);
            details::update_inner<Type>::update(ptr, _inner);
            return *this;
        }

        py_capsule& operator =(py_capsule&& other) {
            details::set_ptr<Type>::replace(ptr, other.ptr, true);
            details::update_inner<Type>::update(ptr, _inner);
            return *this;
        }

        py_capsule& operator =(nullptr_t) {
            Py_CLEAR(ptr);
            _inner = nullptr;
            return *this;
        }

        py_capsule& operator =(details::owned_ptr other) {
            details::set_ptr<Type>::replace(ptr, other.ptr, true);
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
        struct check_ptr<py_capsule<TInner>> {
            static inline bool check(PyObject *ptr) {
#if PY_MAJOR_VERSION == 2 && PY_MINOR_VERSION <= 6
                if (!PyCObject_Check(ptr)) return false;
                auto id = reinterpret_cast<char*>(PyCObject_GetDesc(ptr));
                return id != nullptr && ::std::strcmp(id, typeid(py_capsule<TInner>).name()) == 0;
#else
                return PyCapsule_CheckExact(ptr) && PyCapsule_GetPointer(ptr, typeid(py_capsule<TInner>).name()) != nullptr;
#endif
            }

            static inline const char *expected() {
                return typeid(TInner).name();
            }
        };
    }
}
