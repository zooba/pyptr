#pragma once

#include "py_ptr.h"
#include "initialization.h"
#include "strings.h"
#include "dict.h"

#include "py_type.h"
#include "py_object.h"

#include <array>

namespace python {
    namespace details {
        struct method_descriptor_maker {
            struct data {
                PyObject_HEAD;
                PyObject *name;
                PyObject *function;
            };

            struct getset_maker {
                struct holder {
                    PyGetSetDef *data;
                    operator PyGetSetDef *() { return data; }
                    holder(size_t count) : data(new PyGetSetDef[count]) { }
                    ~holder() { delete[] data; }
                };

                static inline PyObject *get_func(data *obj, void*) {
                    Py_XINCREF(obj->function);
                    return obj->function;
                }

                static inline PyObject *get_name(data *obj, void*) {
                    Py_XINCREF(obj->name);
                    return obj->name;
                }

                inline py_capsule<holder> operator()() {
                    py_capsule<holder> caps(new holder(4));
                    PyGetSetDef *defs = **caps;
                    memset(defs, 0, sizeof(PyGetSetDef) * 4);
                    defs[0].name = "im_func";
                    defs[0].get = reinterpret_cast<getter>(get_func);
                    defs[1].name = "__func__";
                    defs[1].get = reinterpret_cast<getter>(get_func);
                    defs[2].name = "__name__";
                    defs[2].get = reinterpret_cast<getter>(get_name);

                    return caps;
                }
            };

            inline static int init(PyObject *self, PyObject *args, PyObject*) {
                PyObject *name, *function;
                if (!PyArg_UnpackTuple(args, Py_TYPE(self)->tp_name, 2, 2, &name, &function)) return -1;

                auto ptr = reinterpret_cast<data*>(self);
                Py_XDECREF(ptr->name);
                Py_XINCREF(name);
                ptr->name = name;
                Py_XDECREF(ptr->function);
                Py_XINCREF(function);
                ptr->function = function;
                return 0;
            }

            inline static void dealloc(PyObject *self) {
                auto ptr = reinterpret_cast<data*>(self);
                Py_CLEAR(ptr->name);
                Py_CLEAR(ptr->function);
            }

            inline static PyTypeObject *begin_type(const char *name) {
                gil _gil;
                auto type = reinterpret_cast<PyHeapTypeObject*>(PyType_GenericAlloc(&PyType_Type, 0));
                if (type == nullptr) {
                    throw_pyerr();
                    return nullptr;
                }
                type->ht_type.tp_name = name;
                type->ht_type.tp_basicsize = sizeof(data);
                type->ht_type.tp_alloc = PyType_GenericAlloc;
                type->ht_type.tp_init = init;
                type->ht_type.tp_dealloc = dealloc;
                type->ht_type.tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HEAPTYPE;
                auto lastNamePart = strrchr(name, '.');
                if (!lastNamePart) {
                    lastNamePart = name;
                }
                py_str nameobj(lastNamePart);
                type->ht_name = static_cast<PyObject*>(borrow(nameobj));
#if PY_MAJOR_VERSION == 3 && PY_MINOR_VERSION >= 3
                type->ht_qualname = static_cast<PyObject*>(borrow(nameobj));
#endif

                auto getsets = _gil.current_interpreter().get_or_make_static_ptr<getset_maker>();
                type->ht_type.tp_getset = **getsets;

                return reinterpret_cast<PyTypeObject*>(type);
            }

            inline static py_type<py_ptr> end_type(PyTypeObject *type) {
                if (type == nullptr || PyType_Ready(type) < 0) {
                    Py_XDECREF(type);
                    throw_pyerr();
                    return nullptr;
                }
                return steal(reinterpret_cast<PyObject*>(type));
            }
        };

        struct instance_method_descriptor_maker : method_descriptor_maker {
            inline static PyObject *descr_get(PyObject *self, PyObject *obj, PyObject *type) {
                auto ptr = reinterpret_cast<data*>(self);
#if PY_MAJOR_VERSION == 3
                if (obj == nullptr) {
                    return self;
                } else {
                    return PyMethod_New(ptr->function, obj);
                }
#elif PY_MAJOR_VERSION == 2
                return PyMethod_New(ptr->function, obj, type);
#endif
            }

            inline py_type<py_ptr> operator()() {
                auto type = begin_type("pyptr.instance_method");
                if (type != nullptr) {
                    type->tp_descr_get = descr_get;
                }
                return end_type(type);
            }
        };

        struct class_method_descriptor_maker : method_descriptor_maker {
            inline static PyObject *descr_get(PyObject *self, PyObject *obj, PyObject *type) {
                auto ptr = reinterpret_cast<data*>(self);
#if PY_MAJOR_VERSION == 3
                if (type == nullptr) {
                    return self;
                } else {
                    return PyMethod_New(ptr->function, type);
                }
#elif PY_MAJOR_VERSION == 2
                if (type == nullptr) {
                    return self;
                } else {
                    return PyMethod_New(ptr->function, type, reinterpret_cast<PyObject*>(Py_TYPE(type)));
                }
#endif
            }

            inline py_type<py_ptr> operator()() {
                auto type = begin_type("pyptr.class_method");
                if (type != nullptr) {
                    type->tp_descr_get = descr_get;
                }
                return end_type(type);
            }
        };

        struct property_descriptor_maker {
            struct data {
                PyObject_HEAD;
                PyObject *name;
                PyObject *getter;
                PyObject *setter;
            };

            inline static int init(PyObject *self, PyObject *args, PyObject*) {
                PyObject *name, *getter, *setter = nullptr;
                if (!PyArg_UnpackTuple(args, Py_TYPE(self)->tp_name, 2, 3, &name, &getter, &setter)) return -1;

                auto ptr = reinterpret_cast<data*>(self);
                Py_XDECREF(ptr->name);
                Py_XINCREF(name);
                ptr->name = name;
                Py_XDECREF(ptr->getter);
                Py_XINCREF(getter);
                ptr->getter = getter;
                Py_XDECREF(ptr->setter);
                Py_XINCREF(setter);
                ptr->setter = setter;
                return 0;
            }

            inline static void dealloc(PyObject *self) {
                auto ptr = reinterpret_cast<data*>(self);
                Py_CLEAR(ptr->name);
                Py_CLEAR(ptr->getter);
                Py_CLEAR(ptr->setter);
            }

            inline static PyObject *get(PyObject *self, PyObject *obj, PyObject *type) {
                if (self == nullptr || obj == nullptr) {
                    Py_XINCREF(self);
                    return self;
                }
                auto ptr = reinterpret_cast<data*>(self);
                return PyObject_CallFunctionObjArgs(ptr->getter, obj, nullptr);
            }

            inline static int set(PyObject *self, PyObject *obj, PyObject *value) {
                if (self == nullptr || obj == nullptr || value == nullptr) {
                    return -1;
                }
                auto ptr = reinterpret_cast<data*>(self);
                if (ptr->setter == nullptr) {
                    PyErr_SetString(PyExc_AttributeError, "can't set attribute");
                    return -1;
                }
                auto res = PyObject_CallFunctionObjArgs(ptr->setter, obj, value, nullptr);
                if (res == nullptr) {
                    return -1;
                }
                Py_DECREF(res);
                return 0;
            }

            inline py_type<py_ptr> operator()() {
                gil _gil;
                auto type = reinterpret_cast<PyHeapTypeObject*>(PyType_GenericAlloc(&PyType_Type, 0));
                if (type == nullptr) {
                    throw_pyerr();
                    return nullptr;
                }
                type->ht_type.tp_name = "pyptr.property";
                type->ht_type.tp_basicsize = sizeof(data);
                type->ht_type.tp_alloc = PyType_GenericAlloc;
                type->ht_type.tp_init = init;
                type->ht_type.tp_dealloc = dealloc;
                type->ht_type.tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HEAPTYPE;
                type->ht_type.tp_descr_get = get;
                type->ht_type.tp_descr_set = set;
                auto lastNamePart = strrchr(type->ht_type.tp_name, '.');
                if (!lastNamePart) {
                    lastNamePart = type->ht_type.tp_name;
                }
                py_str nameobj(lastNamePart);
                type->ht_name = static_cast<PyObject*>(borrow(nameobj));
#if PY_MAJOR_VERSION == 3 && PY_MINOR_VERSION >= 3
                type->ht_qualname = static_cast<PyObject*>(borrow(nameobj));
#endif

                if (type == nullptr || PyType_Ready(&type->ht_type) < 0) {
                    Py_XDECREF(type);
                    throw_pyerr();
                    return nullptr;
                }
                return steal(reinterpret_cast<PyObject*>(type));
            }
        };

        template <typename TGetter, typename TSetter>
        struct property_proxy {
            TGetter get;
            TSetter set;

            property_proxy(TGetter get, TSetter set)
                : get(get), set(set) { }
        };

        template<typename TInner>
        class class_member_proxy {
            typedef typename class_factory<TInner>::Type Type;
            typedef typename class_factory<TInner>::Instance Instance;

            class_factory<TInner>& owner;
            const char *name;

        public:
            class_member_proxy(const char *name, class_factory<TInner>& owner)
                : name(name), owner(owner) { }

            // Add data member
            class_member_proxy<TInner>& operator =(const py_ptr& value) {
                owner._members[name] = value;
                return *this;
            }

            // Add instance method
            template<typename TResult, typename... Ts>
            class_member_proxy<TInner>& operator =(TResult (*value)(Instance, Ts...)) {
                gil _gil;
                auto descr = _gil.current_interpreter().get_or_make_static_ptr<details::instance_method_descriptor_maker>();
                owner._members[name] = python::call(descr, py_str(name), make_callback(value));
                return *this;
            }

            // Add implicit instance method
            template<typename TResult, typename... Ts>
            class_member_proxy<TInner>& operator =(TResult (TInner::*value)(Ts...)) {
                gil _gil;
                auto descr = _gil.current_interpreter().get_or_make_static_ptr<details::instance_method_descriptor_maker>();
                owner._members[name] = python::call(descr, py_str(name), make_callback(value));
                return *this;
            }

            // Add class method
            template<typename TResult, typename... Ts>
            class_member_proxy<TInner>& operator =(TResult(*value)(Type, Ts...)) {
                gil _gil;
                auto descr = _gil.current_interpreter().get_or_make_static_ptr<details::class_method_descriptor_maker>();
                owner._members[name] = python::call(descr, py_str(name), make_callback(value));
                return *this;
            }

            // Add static method
            template<typename TResult, typename... Ts>
            class_member_proxy<TInner>& operator =(const py_callback<TResult, void, Ts...>& value) {
                owner._members[name] = value;
                return *this;
            }

            template<typename TFunction>
            class_member_proxy<TInner>& operator =(TFunction *fn) {
                return operator =(make_callback(fn));
            }

            template<typename TGetter, typename TSetter>
            class_member_proxy<TInner>& operator =(const property_proxy<TGetter, TSetter>& value) {
                gil _gil;
                auto descr = _gil.current_interpreter().get_or_make_static_ptr<details::property_descriptor_maker>();
                owner._members[name] = python::call(descr, py_str(name), value.get, value.set);
                return *this;
            }

            template<typename TGetter>
            class_member_proxy<TInner>& operator =(const property_proxy<TGetter, nullptr_t>& value) {
                gil _gil;
                auto descr = _gil.current_interpreter().get_or_make_static_ptr<details::property_descriptor_maker>();
                owner._members[name] = python::call(descr, py_str(name), value.get);
                return *this;
            }
        };
    }

    template<typename TResult, typename... Ts>
    py_ptr static_method(const py_callback<TResult, void, Ts...>& value) {
        return value;
    }

    template<typename TFunction>
    py_ptr static_method(TFunction *fn) {
        return static_method(make_callback(fn));
    }

    template<typename TSelf, typename TResult1, typename TResult2>
    details::property_proxy<py_callback<TResult1,void,py_object<TSelf>>, py_callback<void,void,py_object<TSelf>,TResult1>>
    property(TResult1 (*get)(py_object<TSelf>), void (*set)(py_object<TSelf>, TResult2)) {
        static_assert(::std::is_same<TResult1, TResult2>::value, "getter and setter must have same value type");
        return{ get, set };
    }

    template<typename TSelf, typename TResult1, typename TResult2>
    details::property_proxy<py_callback<TResult1,TSelf>, py_callback<void,TSelf,TResult1>>
    property(TResult1(TSelf::*get)(), void(TSelf::*set)(TResult2)) {
        static_assert(::std::is_same<TResult1, TResult2>::value, "getter and setter must have same value type");
        return{ get, set };
    }

    template<typename TSelf, typename TResult>
    details::property_proxy<py_callback<TResult,void,py_object<TSelf>>, nullptr_t>
    property(TResult (*get)(py_object<TSelf>)) {
        return{ get, nullptr };
    }

    template<typename TSelf, typename TResult>
    details::property_proxy<py_callback<TResult, TSelf>, nullptr_t>
    property(TResult(TSelf::*get)()) {
        return{ get, nullptr };
    }

    template<typename TInner>
    class class_factory {
        friend details::class_member_proxy<TInner>;
    public:
        typedef py_object<TInner> Instance;
        typedef py_type<TInner> Type;

    private:
        Type _type;
        py_str _name;
        py_dict<py_str, py_ptr> _members;

        static int init(PyObject *self, PyObject *args, PyObject *kwargs) {
            py_object<TInner> obj(borrow(self), new TInner());
            auto initObj = getattr(obj, "__pyptr_init__", (py_callable<py_ptr>)nullptr);
            if (initObj) {
                py_ptr res(steal(PyObject_Call(initObj, args, kwargs)));
                return res ? 0 : 1;
            }
            return 0;
        }

        inline void construct_type() {
            if (_type) {
                return;
            }

            auto nameParts = _name.rpartition(".");
            _members.setdefault("__module__", nameParts.get<0>());
            auto initObj = _members.get("__init__");
            if (initObj) {
                _members["__pyptr_init__"] = initObj;
                _members.del("__init__");
            }
            _type = Type(nameParts.get<2>(), ::std::move(_members));
            auto tp = reinterpret_cast<PyTypeObject*>(static_cast<PyObject*>(_type));
            tp->tp_init = init;
            _name = nullptr;
            _members = nullptr;
        }
    public:
        explicit class_factory(py_str name) : _name(name), _members(py_dict<py_str, py_ptr>::empty()) { }

        inline Type get_type() {
            construct_type();
            return _type;
        }

        inline Instance create_instance() {
            return call(get_type());
        }

        inline details::class_member_proxy<TInner> operator[](const char *name) {
            return details::class_member_proxy<TInner>(name, *this);
        }
    };
}
