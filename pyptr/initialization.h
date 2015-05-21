#pragma once

#include "py_ptr.h"
#include "py_capsule.h"
#include "strings.h"
#include "dict.h"

#include <map>

namespace python {
    class interpreter;

    class gil {
        friend class allow_threads;
        PyGILState_STATE state;
        bool held;
        ::std::unique_ptr<interpreter> current;

    public:
        gil() {
            state = PyGILState_Ensure();
            held = true;
        }

        ~gil() {
            if (held) {
                PyGILState_Release(state);
            }
        }

        interpreter& current_interpreter() {
            if (!current) {
                current = ::std::make_unique<interpreter>();
            }
            return *current;
        }
    };

    class allow_threads {
        gil& _current;
    public:

        allow_threads(gil& current) : _current(current) {
            _current.held = false;
            PyGILState_Release(_current.state);
        }

        ~allow_threads() {
            _current.state = PyGILState_Ensure();
            _current.held = true;
        }
    };
    
    class trace_callbacks {
    public:
        virtual ~trace_callbacks() { }

        virtual void on_call(py_ptr frame) { }
        virtual void on_exception(py_ptr frame, py_ptr exc_type, py_ptr exc_value, py_ptr traceback) { }
        virtual void on_line(py_ptr frame, int lineno) { }
        virtual void on_return(py_ptr rame, py_ptr return_value) { }
        virtual void on_c_call(py_ptr frame, py_ptr function) { }
        virtual void on_c_return(py_ptr frame, py_ptr function) { }
        virtual void on_c_exception(py_ptr frame, py_ptr function) { }
    };

    class interpreter {
        bool needFinalize;

        void initialize() {
            if (!Py_IsInitialized()) {
                needFinalize = false;

                try {
                    Py_Initialize();
                }
                catch (...) {
                    throw ::std::runtime_error("unable to initialize Python");
                }
                needFinalize = true;
            }

#if PY_MAJOR_VERSION == 3 && PY_MINOR_VERSION >= 4
            PyEval_InitThreads();
#endif

            if (PySys_GetObject("__pyptr__") == nullptr) {
                PySys_SetObject("__pyptr__", details::detach(py_dict<py_str, py_ptr>::empty()));
            }
        }

    public:
        inline interpreter() : needFinalize(false) {
            initialize();
        }
        
        inline interpreter(const std::wstring &home) : needFinalize(false) {
            Py_SetPythonHome(const_cast<wchar_t*>(home.c_str()));
            initialize();
        }
        
        inline interpreter(const std::wstring &home, const std::wstring &path) : needFinalize(false) {
            Py_SetPythonHome(const_cast<wchar_t*>(home.c_str()));
            Py_SetPath(const_cast<wchar_t*>(path.c_str()));
            initialize();
        }

        inline ~interpreter() {
            if (needFinalize) {
                Py_Finalize();
            }
        }

        inline void set_trace(trace_callbacks *callbacks) {
            if (callbacks != nullptr) {
                py_capsule<trace_callbacks> ptr(callbacks, false);
                PyEval_SetTrace(reinterpret_cast<Py_tracefunc>(trace_callback), ptr);
            } else {
                PyEval_SetTrace(nullptr, nullptr);
            }
        }

        inline void set_profile(trace_callbacks *callbacks) {
            if (callbacks != nullptr) {
                py_capsule<trace_callbacks> ptr(callbacks, false);
                PyEval_SetProfile(reinterpret_cast<Py_tracefunc>(trace_callback), ptr);
            } else {
                PyEval_SetProfile(nullptr, nullptr);
            }
        }

        template<typename Maker>
        auto get_or_make_static_ptr() -> decltype((Maker())()) {
            py_str key(typeid(Maker).name());
            py_dict<py_str, py_ptr> statics(borrow(PySys_GetObject("__pyptr__")));
            auto res = statics.get(key);
            if (res == nullptr) {
                statics[key] = res = Maker()();
            }
            return res.detach();
        }

        inline py_list<py_str> path() {
            return borrow(PySys_GetObject("path"));
        }

        inline py_dict<py_str, py_ptr> modules() {
            return borrow(PyImport_GetModuleDict());
        }

        inline py_ptr import(py_str name) {
            return steal(PyImport_Import(name));
        }

    private:
        // interpreter is immovable
        interpreter(const interpreter&);
        // interpreter is immovable
        interpreter(interpreter&&);

        static int trace_callback(PyObject *capsule, PyObject *frame, int what, PyObject *arg) {
            auto callbacks = *py_capsule<trace_callbacks>(capsule);
            if (callbacks == nullptr) {
                return -1;
            }
            try {
                py_ptr f = borrow(frame);
                switch (what) {
                case PyTrace_CALL:
                    callbacks->on_call(f);
                    break;
                case PyTrace_EXCEPTION: {
                    py_tuple<py_ptr, py_ptr, py_ptr> exc_info = borrow(arg);
                    callbacks->on_exception(f, exc_info.get<0>(), exc_info.get<1>(), exc_info.get<2>());
                    break;
                }
                case PyTrace_LINE:
                    callbacks->on_line(f, getattr<py_int>(f, py_str("f_lineno")));
                    break;
                case PyTrace_RETURN:
                    callbacks->on_return(f, borrow(arg));
                    break;
                case PyTrace_C_CALL:
                    callbacks->on_c_call(f, borrow(arg));
                    break;
                case PyTrace_C_RETURN:
                    callbacks->on_c_return(f, borrow(arg));
                    break;
                case PyTrace_C_EXCEPTION:
                    callbacks->on_c_exception(f, borrow(arg));
                    break;
                }
            } catch(std::exception exc) {
                PyErr_SetString(PyExc_RuntimeError, exc.what());
                return -1;
            }
            return 0;
        }
    };
}
