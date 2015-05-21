#pragma once

#include "py_ptr.h"
#include "py_capsule.h"
#include "py_object.h"
#include <functional>

namespace python {
    namespace details {
        template<typename TResult, typename TInstance, typename... Ts>
        struct function_type {
            typedef typename pyptr_type<TResult>::type return_type;
            typedef TResult (TInstance::*type)(Ts...);
            typedef typename make_indices<sizeof...(Ts)>::type arg_indices;
            typedef typename py_tuple<py_object<TInstance>, Ts...> arg_tuple;
            
            static inline return_type call(type func, const arg_tuple& args) {
                return call_helper(func, args, arg_indices());
            }

            template<size_t... Indices>
            static inline return_type call_helper(type func, const arg_tuple& args, indices<Indices...>) {
                return call_and_rethrow([&]() -> return_type {
                    auto obj = args.get<0>();
                    return ((*obj)->*func)(args.get<Indices + 1>()...);
                });
            }
        };

        template<typename TInstance, typename... Ts>
        struct function_type<void, TInstance, Ts...> {
            typedef typename py_ptr return_type;
            typedef void(TInstance::*type)(Ts...);
            typedef typename make_indices<sizeof...(Ts)>::type arg_indices;
            typedef typename py_tuple<py_object<TInstance>, Ts...> arg_tuple;

            static inline return_type call(type func, const arg_tuple& args) {
                return call_helper(func, args, arg_indices());
            }

            template<size_t... Indices>
            static inline return_type call_helper(type func, const arg_tuple& args, indices<Indices...>) {
                return call_and_rethrow([&]() -> return_type {
                    auto obj = args.get<0>();
                    ((*obj)->*func)(args.get<Indices + 1>()...);
                    return borrow(Py_None);
                });
            }
        };

        template<typename TResult, typename... Ts>
        struct function_type<TResult, void, Ts...> {
            typedef typename pyptr_type<TResult>::type return_type;
            typedef TResult(*type)(Ts...);
            typedef typename make_indices<sizeof...(Ts)>::type arg_indices;
            typedef typename py_tuple<Ts...> arg_tuple;
            static inline return_type call(type func, const arg_tuple& args) {
                return call_helper(func, args, arg_indices());
            }
            template<size_t... Indices>
            static inline return_type call_helper(type func, const arg_tuple& args, indices<Indices...>) {
                return call_and_rethrow([&]() -> return_type {
                    return func(args.get<Indices>()...);
                });
            }
        };

        template<typename... Ts>
        struct function_type<void, void, Ts...> {
            typedef void return_type;
            typedef void(*type)(Ts...);
            typedef typename make_indices<sizeof...(Ts)>::type arg_indices;
            typedef typename py_tuple<Ts...> arg_tuple;
            static inline py_ptr call(type func, const arg_tuple& args) {
                return call_helper(func, args, arg_indices());
            }

            template<size_t... Indices>
            static inline py_ptr call_helper(type func, const arg_tuple& args, indices<Indices...>) {
                return call_and_rethrow([&func, &args]() {
                    func(args.get<Indices>()...);
                    return borrow(Py_None);
                });
            }
        };

        template<typename TFunc>
        auto call_and_rethrow(TFunc fn) -> typename pyptr_type<decltype(fn())>::type {
            try {
                return fn();
            } catch(const ::std::runtime_error& e) {
                PyErr_SetString(PyExc_RuntimeError, e.what());
                return nullptr;
            } catch(const ::std::exception& e) {
                PyErr_SetString(PyExc_Exception, e.what());
                return nullptr;
            }
        }
    }
    
    template<typename TResult, typename TInstance, typename... Ts>
    struct py_callback : public details::py_ptrbase<py_callback<TResult, TInstance, Ts...>> {
        typedef details::function_type<TResult, TInstance, Ts...> Function;
        PYPTR_CONSTRUCTORS(py_callback);

    private:
        struct capsule_contents {
            PyMethodDef md;
            typename Function::type func;
        };

        static PyObject *called(PyObject *self, PyObject *args, PyObject *kwargs) {
            py_capsule<capsule_contents> contents(borrow(self));
            if (contents == nullptr) {
                return nullptr;
            }
            if (contents->md.ml_meth != reinterpret_cast<PyCFunction>(called)) {
                PyErr_BadInternalCall();
                return nullptr;
            }

            typename Function::arg_tuple argTuple = borrow(args);
            return details::detach(Function::call(contents->func, argTuple));
        }

        static PyObject *make_cfunction(typename Function::type func, const char *doc) {
            if (func == nullptr) {
                return nullptr;
            }
            py_capsule<capsule_contents> caps(new capsule_contents());
            caps->md.ml_name = nullptr;
            caps->md.ml_meth = reinterpret_cast<PyCFunction>(called);
            caps->md.ml_flags = METH_VARARGS;
            caps->md.ml_doc = doc;
            caps->func = func;

            return PyCFunction_New(&caps->md, caps);
        }

    public:
        py_callback(typename Function::type func)
            : Base(steal(make_cfunction(func, nullptr))) { }
    };

    namespace details {
        template<typename TResult, typename TInstance, typename... Ts>
        struct check_ptr<py_callback<TResult, TInstance, Ts...>> {
            static bool check(PyObject *ptr) { return PyCFunction_Check(ptr) != 0; }
            static const char *expected() { return "C function"; }
        };
    }

    template<typename TResult, typename TInstance, typename... Ts>
    py_callback<TResult, TInstance, Ts...> make_callback(TResult (TInstance::*fn)(Ts...)) {
        return fn;
    }

    template<typename TResult, typename... Ts>
    py_callback<TResult, void, Ts...> make_callback(TResult (*fn)(Ts...)) {
        return fn;
    }

    template<typename TResult, typename... Ts>
    py_callback<TResult, void, Ts...> make_callback(TResult (&fn)(Ts...)) {
        return &fn;
    }
}