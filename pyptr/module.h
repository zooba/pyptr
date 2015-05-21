#pragma once

#include "py_ptr.h"
#include "object_methods.h"
#include "initialization.h"

namespace python {
    namespace details {
        template<typename T>
        class module_member_proxy {
            T& owner;
            const char *name;
        public:
            module_member_proxy(const char *name, T& owner)
                : name(name), owner(owner) { }

            module_member_proxy& operator =(const py_ptr& value) {
                owner.members.emplace_back(name, value);
                return *this;
            }

            template<typename TResult, typename... TArgs>
            module_member_proxy& operator =(TResult(*fn)(TArgs...)) {
                owner.members.emplace_back(name, make_callback(fn));
                return *this;
            }
        };
    }

    class module_base {
        friend details::module_member_proxy<module_base>;

        ::std::string moduleName;
        ::std::vector<::std::tuple<::std::string, py_ptr>> members;
        py_ptr _instance;

#if PY_MAJOR_VERSION == 3
        static void free(void *obj) {
            auto state = reinterpret_cast<module_base**>(PyModule_GetState(reinterpret_cast<PyObject*>(obj)));
            if (state && state[0] && state[0]->deleteThisOnFree) {
                delete state[0];
            }
        }

    protected:
        PyModuleDef def;
#endif

    public:
        bool deleteThisOnFree;

    protected:
        module_base(const char *name)
            : moduleName(name), _instance(nullptr), deleteThisOnFree(false) {
#if PY_MAJOR_VERSION == 3
            PyModuleDef cleanDef = PyModuleDef_HEAD_INIT;
            def = cleanDef;
            def.m_size = sizeof(module_base*);
            def.m_free = free;
#endif
        }
    public:
        virtual ~module_base() {
        }

    private:
        void make_instance() {
#if PY_MAJOR_VERSION == 3
            def.m_name = moduleName.c_str();

            _instance = steal(PyModule_Create(&def));
            auto state = reinterpret_cast<module_base**>(PyModule_GetState(_instance));
            state[0] = this;
#elif PY_MAJOR_VERSION == 2
            _instance = steal(Py_InitModule(moduleName.c_str(), nullptr));
#endif
            for (auto& m : members) {
                if (PyModule_AddObject(
                    _instance,
                    ::std::get<0>(m).c_str(),
                    ::std::get<1>(m).detach()
                ) != 0) {
                    _instance = nullptr;
                    return;
                }
            }
        }

    public:
        py_ptr instance() {
            if (!_instance) {
                make_instance();
            }
            return _instance;
        }

        py_str get_name() {
            return moduleName.c_str();
        }

        details::module_member_proxy<module_base> operator[](const char *name) {
            return details::module_member_proxy<module_base>(name, *this);
        }

    };

    namespace details {
        template<typename T>
        PyObject *make_module() {
            gil _gil;
            module_base *modPtr = new T();
            auto mod = modPtr->instance();
            modPtr->deleteThisOnFree = true;
            return mod; // .detach();
        }
    }
}

#if PY_MAJOR_VERSION == 2
#define INIT_MODULE_NAME(TModule) init##TModule
#define EXPORT_PYTHON_MODULE(TModule) \
    PyMODINIT_FUNC INIT_MODULE_NAME(TModule) () { ::python::details::make_module<TModule>(); }
#else
#define INIT_MODULE_NAME(TModule) PyInit_##TModule
#define EXPORT_PYTHON_MODULE(TModule) \
    PyMODINIT_FUNC INIT_MODULE_NAME(TModule) () { return ::python::details::make_module<TModule>(); }
#endif
