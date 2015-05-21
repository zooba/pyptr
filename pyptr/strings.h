#pragma once
#include <codecvt>
#include <locale>
#include <ostream>
#include <sstream>
#include "py_ptr.h"
#include "iterable.h"
#include "tuple.h"
#include "dict.h"

namespace python {
    struct py_str;
    namespace details {
        template<> struct pyptr_type<::std::string> { typedef py_str type; };
        template<> struct pyptr_type<::std::wstring> { typedef py_str type; };
        template<> struct pyptr_type<const char*> { typedef py_str type; };
        template<> struct pyptr_type<const wchar_t*> { typedef py_str type; };
    }
    
    struct py_str : public details::py_ptrbase<py_str> {
        PYPTR_CONSTRUCTORS(py_str);
        PYPTR_ITERABLE(py_str);

#if PY_MAJOR_VERSION == 3
#if PY_MINOR_VERSION >= 3
        operator ::std::string() const {
            if (PyUnicode_READY(ptr) == 0) {
                switch (PyUnicode_KIND(ptr)) {
                case PyUnicode_1BYTE_KIND:
                    return ::std::string(reinterpret_cast<const char*>(PyUnicode_DATA(ptr)), size());
                case PyUnicode_2BYTE_KIND: {
                    ::std::wstring_convert<::std::codecvt_utf8<Py_UCS2>, Py_UCS2> converter;
                    return converter.to_bytes(
                        reinterpret_cast<const Py_UCS2*>(PyUnicode_DATA(ptr)),
                        reinterpret_cast<const Py_UCS2*>(PyUnicode_DATA(ptr)) + size());
                }
                case PyUnicode_4BYTE_KIND:
                    ::std::wstring_convert<::std::codecvt_utf8<Py_UCS4>, Py_UCS4> converter;
                    return converter.to_bytes(
                        reinterpret_cast<const Py_UCS4*>(PyUnicode_DATA(ptr)),
                        reinterpret_cast<const Py_UCS4*>(PyUnicode_DATA(ptr)) + size());
                }
            }

            return ::std::string();
        }

        operator ::std::wstring() const {
            if (PyUnicode_READY(ptr) == 0) {
                auto k = PyUnicode_KIND(ptr);
                switch (PyUnicode_KIND(ptr)) {
                case PyUnicode_1BYTE_KIND: {
                    ::std::wstring_convert<::std::codecvt_utf8<wchar_t>, wchar_t> converter;
                    return converter.from_bytes(
                        reinterpret_cast<const char*>(PyUnicode_DATA(ptr)),
                        reinterpret_cast<const char*>(PyUnicode_DATA(ptr)) + size());
                }
                case PyUnicode_2BYTE_KIND: {
#if SIZEOF_WCHAR_T == 2
                    return ::std::wstring(reinterpret_cast<const wchar_t*>(PyUnicode_DATA(ptr)), size());
#else
                    ::std::wstring_convert<::std::codecvt_utf8<wchar_t>, wchar_t> converter;
                    return converter.from_bytes(*this);
#endif
                }
                case PyUnicode_4BYTE_KIND: {
#if SIZEOF_WCHAR_T == 4
                    return ::std::wstring(reinterpret_cast<const wchar_t*>(PyUnicode_DATA(ptr)), size());
#else
                    ::std::wstring_convert<::std::codecvt_utf8_utf16<wchar_t>, wchar_t> converter;
                    return converter.from_bytes(*this);
#endif
                }
                }
            }

            return ::std::wstring();
        }

#else

        operator ::std::string() const {
            auto buff = PyUnicode_AsUnicode(ptr);
            auto len = size();
            ::std::wstring_convert<::std::codecvt_utf8<Py_UNICODE>, Py_UNICODE> converter;
            return converter.to_bytes(buff, buff + len);
        }

        operator ::std::wstring() const {
            auto buff = PyUnicode_AsUnicode(ptr);
            auto len = size();
            return ::std::wstring(buff, buff + len);
        }
#endif

        py_str(const char *data) {
            ptr = PyUnicode_FromStringAndSize(data, ::std::strlen(data));
            if (ptr == nullptr) details::throw_pyerr();
        }
        
        py_str(const wchar_t *data) {
            ptr = PyUnicode_FromUnicode(data, ::std::wcslen(data));
            if (ptr == nullptr) details::throw_pyerr();
        }
        
        py_str(const ::std::string& data) {
            ptr = PyUnicode_FromStringAndSize(data.data(), data.size());
            if (ptr == nullptr) details::throw_pyerr();
        }

        py_str(const ::std::wstring& data) {
            ptr = PyUnicode_FromUnicode(data.data(), data.size());
            if (ptr == nullptr) details::throw_pyerr();
        }

        bool is_valid() const {
            return PyUnicode_Check(ptr);
        }
#elif PY_MAJOR_VERSION == 2
        operator ::std::string() const {
            char *buff;
            Py_ssize_t len;
            if (PyString_AsStringAndSize(ptr, &buff, &len) != 0) details::throw_pyerr();
            return ::std::string(buff, len);
        }

        operator ::std::wstring() const {
            char *buff;
            Py_ssize_t len;
            if (PyString_AsStringAndSize(ptr, &buff, &len) != 0) details::throw_pyerr();

            ::std::wstring_convert<::std::codecvt_utf8_utf16<wchar_t>> converter;
            return converter.from_bytes(buff, buff + len);
        }

        py_str(const char *data) {
            ptr = PyString_FromStringAndSize(data, ::std::strlen(data));
            if (ptr == nullptr) details::throw_pyerr();
        }
        
        py_str(const wchar_t *data) {
            auto wstr = ::std::wstring(data);
            ::std::wstring_convert<::std::codecvt_utf8_utf16<wchar_t>> converter;
            auto str = converter.to_bytes(wstr);
            auto d = str.data();
            auto l = str.size();
            ptr = PyString_FromStringAndSize(str.data(), str.size());
            if (ptr == nullptr) details::throw_pyerr();
        }
        
        py_str(const ::std::string& data) {
            ptr = PyString_FromStringAndSize(data.data(), data.size());
            if (ptr == nullptr) details::throw_pyerr();
        }

        py_str(const ::std::wstring& data) {
            ::std::wstring_convert<::std::codecvt_utf8_utf16<wchar_t>> converter;
            auto str = converter.to_bytes(data);
            ptr = PyString_FromStringAndSize(str.data(), str.size());
            if (ptr == nullptr) details::throw_pyerr();
        }

        bool is_valid() const {
            return PyString_Check(ptr);
        }
#endif

        ssize_t size() const {
            return PyObject_Size(ptr);
        }

        template<typename... Ts>
        py_str format(const py_tuple<Ts...>& args) {
#if PY_MAJOR_VERSION == 3
            return steal(PyUnicode_Format(ptr, args));
#elif PY_MAJOR_VERSION == 2
            return steal(PyString_Format(ptr, args));
#endif
        }

        template<typename TValue>
        py_str format(const py_dict<py_str, TValue>& args) {
#if PY_MAJOR_VERSION == 3
            return steal(PyUnicode_Format(ptr, args));
#elif PY_MAJOR_VERSION == 2
            return steal(PyString_Format(ptr, args));
#endif
        }

        py_tuple<py_str, py_str, py_str> partition(py_str sep) {
#if PY_MAJOR_VERSION == 3
            return steal(PyUnicode_Partition(ptr, sep));
#elif PY_MAJOR_VERSION == 2
            return steal(PyObject_CallMethod(ptr, "partition", "O", static_cast<PyObject*>(sep)));
#endif
        }

        py_tuple<py_str, py_str, py_str> rpartition(py_str sep) {
#if PY_MAJOR_VERSION == 3
            return steal(PyUnicode_RPartition(ptr, sep));
#elif PY_MAJOR_VERSION == 2
            return steal(PyObject_CallMethod(ptr, "rpartition", "O", static_cast<PyObject*>(sep)));
#endif
        }
    };

#if PY_MAJOR_VERSION == 3
    PYPTR_SIMPLE_CHECKPTR(py_str, PyUnicode_Type, PyUnicode_Check(ptr));
#elif PY_MAJOR_VERSION == 2
    PYPTR_SIMPLE_CHECKPTR(py_str, PyString_Type, PyString_Check(ptr));
#endif

    template<typename T>
    inline py_str str(const details::py_ptrbase<T>& object) {
        if ((PyObject*)object) {
            return PyObject_Str(object);
        } else {
            return nullptr;
        }
    }

    template<typename T>
    inline py_str repr(const details::py_ptrbase<T>& object) {
        if ((PyObject*)object) {
            return PyObject_Str(object);
        } else {
            return nullptr;
        }
    }

    template<typename TFmt, typename... Ts>
    typename ::std::enable_if<::std::is_same<py_str, typename details::pyptr_type<TFmt>::type>::value, py_str>::type
    format(TFmt fmt, const py_tuple<Ts...>& args) {
        return py_str(fmt).format(args);
    }

    template<typename TFmt, typename TValue>
    typename ::std::enable_if<::std::is_same<py_str, typename details::pyptr_type<TFmt>::type>::value, py_str>::type
        format(TFmt fmt, const py_dict<py_str, TValue>& args) {
            return py_str(fmt).format(args);
        }
}

inline ::std::ostream& operator<<(::std::ostream& stream, const python::py_str& obj) {
    stream << static_cast<::std::string>(obj);
    return stream;
}

inline ::std::wostream& operator<<(::std::wostream& stream, const python::py_str& obj) {
    stream << static_cast<::std::wstring>(obj);
    return stream;
}

template<typename T>
inline ::std::ostream& operator<<(::std::ostream& stream, const python::details::py_ptrbase<T>& obj) {
    stream << python::str(obj);
    return stream;
}

template<typename T>
inline ::std::wostream& operator<<(::std::wostream& stream, const python::details::py_ptrbase<T>& obj) {
    stream << python::str(obj);
    return stream;
}


