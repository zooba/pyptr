#pragma once

#include "py_ptr.h"
#include <string>

namespace python {
    struct py_code : public details::py_ptrbase<py_code> {
        PYPTR_CONSTRUCTORS(py_code);

        inline py_ptr to_module(py_str name) const {
            ::std::string n(name);
#if PY_MAJOR_VERSION == 3
            return steal(PyImport_ExecCodeModule(const_cast<char*>(n.c_str()), ptr));
#elif PY_MAJOR_VERSION == 2
            return steal(PyImport_ExecCodeModule(const_cast<char*>(n.c_str()), ptr));
#else
#error Unable to determine Python version
#endif
        }

        void execute() const {
            py_ptr res(eval<py_ptr>());
        }

        template<typename TResult>
        TResult eval() const {
            return eval<TResult>(borrow(PyEval_GetGlobals()), borrow(PyEval_GetLocals()));
        }

        template<typename TResult>
        TResult eval(py_dict<py_str, py_ptr> globals, py_ptr locals) const {
            if (!globals) {
                globals = py_dict<py_str, py_ptr>::empty();
            }
            globals.setdefault("__builtins__", borrow(PyEval_GetBuiltins()));
            
            if (!locals) {
                locals = py_dict<>::empty();
            }

#if PY_MAJOR_VERSION == 3
            auto res = PyEval_EvalCode(ptr, globals, locals);
#elif PY_MAJOR_VERSION == 2
            auto res = PyEval_EvalCode(reinterpret_cast<PyCodeObject*>(ptr), globals, locals);
#else
#error Unable to determine Python version
#endif
            return typename details::pyptr_type<TResult>::type(steal(res));
        }

        static inline py_code compile(const char *code, const char *filename) {
            return steal(Py_CompileString(code, filename, Py_file_input));
        }

        static inline py_code compile_eval(const char *code, const char *filename) {
            return steal(Py_CompileString(code, filename, Py_eval_input));
        }

        static inline py_code compile_single(const char *code, const char *filename) {
            return steal(Py_CompileString(code, filename, Py_single_input));
        }

        static inline py_code compile(py_str code, py_str filename) {
            return compile(static_cast<::std::string>(code).c_str(), static_cast<::std::string>(filename).c_str());
        }

        static inline py_code compile_eval(py_str code, py_str filename) {
            return compile_eval(static_cast<::std::string>(code).c_str(), static_cast<::std::string>(filename).c_str());
        }

        static inline py_code compile_single(py_str code, py_str filename) {
            return compile_single(static_cast<::std::string>(code).c_str(), static_cast<::std::string>(filename).c_str());
        }

    };

    PYPTR_SIMPLE_CHECKPTR(py_code, PyCode_Type, PyCode_Check(ptr) != 0);
}
