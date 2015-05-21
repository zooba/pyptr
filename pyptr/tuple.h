#pragma once
#include "py_ptr.h"
#include <tuple>
#include <type_traits>

namespace python {
    namespace details {
        template<size_t index, typename... Ts> struct tuple_item_type;

        template<typename NotATuple>
        struct is_py_tuple : ::std::false_type { };
    }

    template<typename... Ts>
    struct py_tuple : public details::py_ptrbase<py_tuple<typename details::pyptr_type<Ts>::type...>> {
        PYPTR_CONSTRUCTORS(py_tuple);
        PYPTR_ITERABLE(py_ptr);

        py_ptr operator[](size_t index) {
            return borrow(PyTuple_GetItem(ptr, index));
        }

        template<size_t index>
        typename details::tuple_item_type<index, py_tuple<typename details::pyptr_type<Ts>::type...>>::type get() const {
            return borrow(PyTuple_GetItem(ptr, index));
        }

        inline size_t size() const {
            Py_ssize_t res = PyObject_Size(ptr);
            if (res < 0) {
                details::throw_pyerr();
                return 0;
            }
            return static_cast<size_t>(res);
        }

        static inline py_tuple<typename details::pyptr_type<Ts>::type...> empty() {
            return steal(PyTuple_New(0));
        }
    };

    namespace details {
        template<typename... Ts>
        struct check_ptr<py_tuple<Ts...>> {
            static bool check(PyObject *ptr) { return ptr == nullptr || PyTuple_Check(ptr); }
            static PyTypeObject& expected() { return PyTuple_Type; }
        };

        template<typename T0>
        struct tuple_item_type<0, py_tuple<T0>> {
            typedef T0 type;
        };

        template<typename T0, typename... Ts>
        struct tuple_item_type<0, py_tuple<T0, Ts...>> {
            typedef T0 type;
        };

        template<size_t index, typename T0>
        struct tuple_item_type<index, py_tuple<T0>> {
            typedef T0 type;
        };

        template<size_t index, typename T0, typename... Ts>
        struct tuple_item_type<index, py_tuple<T0, Ts...>> {
            typedef typename tuple_item_type<index - 1, py_tuple<Ts...>>::type type;
        };

        template<typename... Ts>
        struct is_py_tuple<py_tuple<Ts...>> : ::std::true_type { };

        template<typename... Ts>
        struct pyptr_type<::std::tuple<Ts...>> { typedef py_tuple<typename pyptr_type<Ts>::type...> type; };
    }

    template<typename... Ts>
    inline py_tuple<typename details::pyptr_type<Ts>::type...> make_py_tuple() {
        return py_tuple<Ts...>::empty();
    }

    template<typename... Ts>
    py_tuple<typename details::pyptr_type<Ts>::type...> make_py_tuple(Ts... items) {
        return steal(PyTuple_Pack(sizeof...(Ts), details::detach(items)...));
    }
}
