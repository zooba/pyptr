#pragma once
#include "py_ptr.h"
#include <tuple>
#include <type_traits>

namespace python {
    namespace details {
        template<typename TKey, typename TValue>
        struct dict_item_proxy {
        private:
            py_ptr dict;
            TKey key;
        public:
            dict_item_proxy(const py_ptr& dict, const TKey& key) : dict(dict), key(key) { }

            dict_item_proxy& operator=(const TValue& value) {
                if (PyDict_SetItem(dict, key, value) != 0) {
                    details::throw_pyerr();
                }
                return *this;
            }

            operator TValue() const {
                return get();
            }

            TValue get() const {
                typename details::pyptr_type<TKey>::type k(key);
#if PY_MAJOR_VERSION == 3
                return typename details::pyptr_type<TValue>::type(borrow(PyDict_GetItemWithError(dict, k)));
#elif PY_MAJOR_VERSION == 2
                auto result = PyDict_GetItem(dict, k);
                if (result == nullptr && !PyErr_Occurred()) {
                    PyErr_SetObject(PyExc_KeyError, k);
                    details::throw_pyerr();
                }
                return typename details::pyptr_type<TValue>::type(borrow(result));
#else
#error Unable to determine Python version
#endif
            }
        };
    }

    template<typename TKey = py_ptr, typename TValue = py_ptr>
    struct py_dict: public details::py_ptrbase<py_dict<TKey, TValue>> {
        PYPTR_CONSTRUCTORS(py_dict);

        void clear() {
            PyDict_Clear(ptr);
        }

        TValue get(TKey key, TValue defaultValue = nullptr) const {
            typename details::pyptr_type<TKey>::type k(key);
            auto result = PyDict_GetItem(ptr, k);
            if (result == nullptr) {
                if (PyErr_Occurred() == nullptr) {
                    return defaultValue;
                }
                details::throw_pyerr();
            }
            return typename details::pyptr_type<TValue>::type(borrow(result));
        }

        TValue setdefault(TKey key, TValue defaultValue) {
            typename details::pyptr_type<TKey>::type k(key);
            auto result = PyDict_GetItem(ptr, k);
            if (result == nullptr) {
                if (PyErr_Occurred() == nullptr) {
                    if (defaultValue) {
                        if (PyDict_SetItem(ptr, k, defaultValue) != 0) {
                            details::throw_pyerr();
                        }
                        return defaultValue;
                    }
                    PyErr_SetObject(PyExc_KeyError, k);
                }
                details::throw_pyerr();
            }
            return typename details::pyptr_type<TValue>::type(borrow(result));
        }

        void del(TKey key) {
            typename details::pyptr_type<TKey>::type k(key);
            auto result = PyDict_DelItem(ptr, k);
            if (result != 0) {
                details::throw_pyerr();
            }
        }

        details::dict_item_proxy<TKey, TValue> operator[](const TKey& key) {
            return details::dict_item_proxy<TKey, TValue>(*this, key);
        }

        details::dict_item_proxy<TKey, TValue> operator[](const char *key) {
            return (*this)[py_str(key)];
        }

        details::dict_item_proxy<TKey, TValue> operator[](const wchar_t *key) {
            return (*this)[py_str(key)];
        }

        inline size_t size() const {
            Py_ssize_t res = PyObject_Size(ptr);
            if (res < 0) {
                details::throw_pyerr();
                return 0;
            }
            return static_cast<size_t>(res);
        }

        static inline py_dict<TKey, TValue> empty() {
            return steal(PyDict_New());
        }
    };

    namespace details {
        template<typename TKey, typename TValue>
        struct check_ptr<py_dict<TKey, TValue>> {
            static bool check(PyObject *ptr) { return ptr == nullptr || PyDict_Check(ptr); }
            static PyTypeObject& expected() { return PyDict_Type; }
        };

        template<typename NotADict>
        struct is_py_dict : ::std::false_type { };

        template<typename TKey, typename TValue>
        struct is_py_dict<py_dict<TKey, TValue>> : ::std::true_type { };

        template<typename NotADict>
        struct dict_key_type { };

        template<typename TKey, typename TValue>
        struct dict_key_type<py_dict<TKey, TValue>> {
            typedef TKey type;
        };

        template<typename NotADict>
        struct dict_value_type { };

        template<typename TKey, typename TValue>
        struct dict_value_type<py_dict<TKey, TValue>> {
            typedef TValue type;
        };

    }
}
