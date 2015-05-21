#pragma once

#include "py_ptr.h"
#include <type_traits>

namespace python {
    template<typename TReturn=py_ptr>
    struct py_callable : public details::py_ptrbase<py_callable<TReturn>> {
        PYPTR_CONSTRUCTORS(py_callable);

        TReturn operator()() {
            return typename details::pyptr_type<TReturn>::type(
                steal(PyObject_Call(ptr, py_tuple<>::empty(), nullptr))
            );
        }

        template<typename TArgs>
        typename ::std::enable_if<details::is_py_tuple<TArgs>::value, TReturn>::type
        operator()(TArgs args) {
            return typename details::pyptr_type<TReturn>::type(
                steal(PyObject_Call(ptr, args, nullptr))
            );
        }

        template<typename TArgs, typename TKwArgs>
        typename ::std::enable_if<details::is_py_tuple<TArgs>::value && details::is_py_dict<TKwArgs>::value, TReturn>::type
        operator()(TArgs args, TKwArgs kwArgs) {
            return typename details::pyptr_type<TReturn>::type(
                steal(PyObject_Call(ptr, args, kwArgs))
            );
        }
    };

    template<>
    struct py_callable<void> : public details::py_ptrbase<py_callable<void>> {
        PYPTR_CONSTRUCTORS(py_callable);

        void operator()() {
            py_ptr result = steal(PyObject_Call(ptr, py_tuple<>::empty(), nullptr));
        }

        template<typename TArgs>
        typename ::std::enable_if<details::is_py_tuple<TArgs>::value>::type
        operator()(TArgs args) {
            py_ptr result = steal(PyObject_Call(ptr, args, nullptr));
        }

        template<typename TArgs, typename TKwArgs>
        typename ::std::enable_if<details::is_py_tuple<TArgs>::value && details::is_py_dict<TKwArgs>::value>::type
        operator()(TArgs args, TKwArgs kwArgs) {
            py_ptr result = steal(PyObject_Call(ptr, args, kwArgs));
        }
    };

    PYPTR_TEMPLATE_CHECKPTR(py_callable<TReturn>, "callable", PyCallable_Check(ptr) != 0, typename TReturn);

    namespace details {
        template<typename T>
        struct return_type {
            typedef py_ptr type;
        };

        template<typename TReturn>
        struct return_type<py_callable<TReturn>> {
            typedef TReturn type;
        };

        template<typename T>
        struct named_arg {
            py_str name;
            T value;

            named_arg(const py_str& name, const T& value) : name(name), value(value) { }
        };

        template<typename T> struct is_named_arg : ::std::false_type { };
        template<typename T> struct is_named_arg<named_arg<T>> : ::std::true_type { };
        template<typename T> struct is_named_arg<named_arg<T>&> : ::std::true_type { };
        template<typename T> struct is_named_arg<named_arg<T>&&> : ::std::true_type { };
        template<typename T> struct is_named_arg<const named_arg<T>&> : ::std::true_type { };

        template<typename... Ts> struct positional_arg_count;
        template<> struct positional_arg_count<> : ::std::integral_constant<int, 0> { };
        template<typename T0, typename... Ts>
        struct positional_arg_count<T0, Ts...> : ::std::integral_constant<
            int,
            (is_named_arg<T0>::value ? 0 : 1) + positional_arg_count<Ts...>::value
        > { };

        template<typename... Ts> struct need_kwarg_dict;
        template<> struct need_kwarg_dict<> : ::std::false_type{};
        template<typename T0, typename... Ts>
        struct need_kwarg_dict<T0, Ts...> : ::std::integral_constant<
            bool,
            is_named_arg<T0>::value | need_kwarg_dict<Ts...>::value
        > { };
        
        template<bool need_dict> struct kwarg_type       { typedef nullptr_t type; };
        template<>               struct kwarg_type<true> { typedef PyObject *type; };

        
        template<typename... Ts>
        struct arg_set {
            PyObject *args;
            typename kwarg_type<need_kwarg_dict<Ts...>::value>::type kwArgs;

            static void init_dict(nullptr_t &dict) { dict = nullptr; }
            static void init_dict(PyObject *&dict) { dict = PyDict_New(); if (dict == nullptr) throw_pyerr(); }

            arg_set() {
                args = PyTuple_New(positional_arg_count<Ts...>::value);
                if (args == nullptr) throw_pyerr();
                init_dict(kwArgs);
            }

            static inline void free_dict(nullptr_t dict) { }
            static inline void free_dict(PyObject *dict) { Py_DECREF(dict); }

            ~arg_set() {
                Py_DECREF(args);
                free_dict(kwArgs);
            }
        };

        template<typename T, typename TKwArgs>
        void set_arg(PyObject *args, TKwArgs kwArgs, Py_ssize_t index, T arg) {
            PyTuple_SET_ITEM(args, index, detach(arg));
        }

        template<typename T>
        void set_arg(PyObject *args, PyObject *kwArgs, Py_ssize_t index, const named_arg<T>& arg) {
            if (PyDict_SetItem(kwArgs, arg.name, detach(arg.value)) != 0) throw_pyerr();
        }

        template<typename Callable, typename TArgs>
        typename ::std::enable_if<!::std::is_void<typename return_type<Callable>::type>::value, typename return_type<Callable>::type>::type
        actual_call(const Callable& callable, TArgs& arg_set) {
            return typename pyptr_type<typename return_type<Callable>::type>::type(
                steal(PyObject_Call(callable, arg_set.args, arg_set.kwArgs))
            );
        }

        template<typename Callable, typename TArgs>
        typename ::std::enable_if<::std::is_void<typename return_type<Callable>::type>::value>::type
        actual_call(const Callable& callable, TArgs& arg_set) {
            py_ptr result = steal(PyObject_Call(callable, arg_set.args, arg_set.kwArgs));
        }

        template<typename Callable, typename TArgs, typename TIndex, typename... Ts>
        typename return_type<Callable>::type call_helper(const Callable& callable, TArgs& arg_set, TIndex, Ts&&... args);

        template<typename Callable, typename TArgs, typename TIndex>
        typename return_type<Callable>::type call_helper(const Callable& callable, TArgs& arg_set, TIndex) {
            return actual_call(callable, arg_set);
        }

        template<typename Callable, typename TArgs, typename TIndex, typename T0, typename... Ts>
        typename return_type<Callable>::type call_helper(const Callable& callable, TArgs& arg_set, TIndex, T0&& arg0, Ts&&... args) {
            set_arg(arg_set.args, arg_set.kwArgs, TIndex::value, arg0);
            return call_helper(
                callable,
                arg_set,
                ::std::integral_constant<int, TIndex::value + positional_arg_count<T0>::value>(),
                args...
            );
        }
    }

    template<typename T> auto arg(T&& value) -> decltype(value) { return value; }
    template<typename T> details::named_arg<typename ::std::remove_reference<T>::type> arg(py_str&& name, T&& value) {
        return details::named_arg<typename ::std::remove_reference<T>::type>(name, value);
    }
    template<typename T> details::named_arg<typename ::std::remove_reference<T>::type> arg(const py_str& name, T&& value) {
        return details::named_arg<typename ::std::remove_reference<T>::type>(name, value);
    }

    template<typename Callable, typename... Ts>
    typename details::return_type<Callable>::type call(const Callable& callable, Ts&&... args) {
        details::arg_set<Ts...> arg_set;
        return details::call_helper(callable, arg_set, ::std::integral_constant<int, 0>(), args...);
    }
    
    template<typename Callable, typename TArgs, typename TKwArgs>
    py_ptr call_varargs(const Callable& callable, TArgs tupleArgs, TKwArgs dictArgs) {
        return py_callable<py_ptr>(borrow(callable))(::std::forward(tupleArgs), ::std::forward(dictArgs));
    }

    template<typename TReturn, typename TArgs, typename TKwArgs>
    TReturn call_varargs(py_callable<TReturn> callable, TArgs tupleArgs, TKwArgs dictArgs) {
        return callable(::std::forward(tupleArgs), ::std::forward(dictArgs));
    }

    template<typename Callable, typename TArgs>
    py_ptr call_varargs(const Callable& callable, TArgs tupleArgs) {
        return py_callable<py_ptr>(borrow(callable))(::std::forward(tupleArgs));
    }

    template<typename TReturn, typename TArgs>
    TReturn call_varargs(py_callable<TReturn> callable, TArgs tupleArgs) {
        return callable(tupleArgs);
    }
}
