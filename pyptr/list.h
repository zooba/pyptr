#pragma once
#include "py_ptr.h"
#include <list>
#include <type_traits>
#include <vector>

namespace python {
    namespace details {
        template<typename T>
        struct list_item_proxy {
        private:
            PyObject *list;
            Py_ssize_t index;
        public:
            list_item_proxy(PyObject *list, Py_ssize_t index) : list(list), index(index) { }

            list_item_proxy& operator=(T&& value) {
                if (PyList_SetItem(list, index, typename pyptr_type<T>::type(value).detach()) != 0) {
                    details::throw_pyerr();
                }
                return *this;
            }

            operator T() const {
                return typename pyptr_type<T>::type(borrow(PyList_GetItem(list, index)));
            }
        };
    }

    template<typename T>
    struct py_list : public details::py_ptrbase<py_list<T>> {
        PYPTR_CONSTRUCTORS(py_list);
        PYPTR_ITERABLE(T);

        template<typename Container>
        py_list(const Container& cont) {
            static_assert(::std::is_same<typename details::pyptr_type<Container>::type, py_list<typename details::pyptr_type<T>::type>>::value, "invalid container type");
            ptr = PyList_New(0);
            for (auto it = ::std::begin(cont); it != ::std::end(cont); ++it) {
                if (PyList_Append(ptr, details::detach<decltype(*it), T>(*it)) != 0) {
                    details::throw_pyerr();
                }
            }
        }

        template<typename Iter>
        py_list(Iter begin, Iter end) {
            ptr = PyList_New(0);
            if (ptr == nullptr) details::throw_pyerr();
            for(auto it = begin; it != end; ++it) {
                if (PyList_Append(ptr, details::detach<decltype(*it), T>(*it)) != 0) {
                    details::throw_pyerr();
                }
            }
        }

        details::list_item_proxy<T> operator[](ssize_t index) {
            return details::list_item_proxy<T>(*this, index);
        }

        void append(const T& value) {
            if (PyList_Append(ptr, details::detach(value)) != 0) {
                details::throw_pyerr();
            }
        }

        void del(ssize_t index) {
            if (PyObject_DelItem(ptr, py_int(index)) != 0) {
                details::throw_pyerr();
            }
        }

        size_t size() const {
            Py_ssize_t res = PyObject_Size(ptr);
            if (res < 0) {
                details::throw_pyerr();
                return 0;
            }
            return static_cast<size_t>(res);
        }

        static inline py_list<T> empty() {
            return steal(PyList_New(0));
        }
    };

    PYPTR_TEMPLATE_CHECKPTR(py_list<T>, PyList_Type, PyList_Check(ptr), typename T);

    namespace details {
        template<typename T>
        struct pyptr_type<::std::vector<T>> { typedef py_list<typename pyptr_type<T>::type> type; };

        template<typename T>
        struct pyptr_type<::std::list<T>> { typedef py_list<typename pyptr_type<T>::type> type; };
    }

    inline py_list<py_ptr> make_py_list() {
        return steal(PyList_New(0));
    }

    template<typename T>
    py_list<T> make_py_list(T item0) {
        py_list<T> result = steal(PyList_New(1));
        PyList_SET_ITEM((PyObject*)result, 0, item0.detach());
        return result;
    }

    template<typename T>
    py_list<T> make_py_list(T item0, T item1) {
        py_list<T> result = steal(PyList_New(2));
        PyList_SET_ITEM((PyObject*)result, 0, item0.detach());
        PyList_SET_ITEM((PyObject*)result, 1, item1.detach());
        return result;
    }

    template<typename T>
    py_list<T> make_py_list(T item0, T item1, T item2) {
        py_list<T> result = steal(PyList_New(3));
        PyList_SET_ITEM((PyObject*)result, 0, item0.detach());
        PyList_SET_ITEM((PyObject*)result, 1, item1.detach());
        PyList_SET_ITEM((PyObject*)result, 2, item2.detach());
        return result;
    }
}

