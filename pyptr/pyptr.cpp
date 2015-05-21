//
// This file is intended to test build and code-gen, but will not execute
// correctly because it includes invalid Python operations.
//

#include "py_ptr.h"

using namespace python;

#include <iostream>

int main() {
    interpreter py_interpreter;

    gil _g;
    py_int x = 123;
    py_str y = repr(x);
    
    auto tup = make_py_tuple(x, y);
    auto lst = make_py_list<py_int>(1, 2, 3);

    auto i0 = tup.get<0>();
    auto i1 = tup.get<1>();
    auto i2 = py_ptr(); // tup.get<2>();
    i2 = i0;
    i1 = L"hello";
    i1 = "world";

    {
        allow_threads _at(_g);
        i0 = i2.cast<py_int>();
    }

    setattr(tup, "attr", i1);
    i0 = getattr<py_int>(tup, "attr");
    i2 = getattr(tup, "attr");
    delattr(tup, "attr");

    py_callable<py_str> callable;
    i2 = callable(tup);

    auto res = call(callable, i0);
    auto res2 = call_varargs(callable, make_py_tuple(i2, i0));
    auto res3 = call(callable, arg("name", i0), i2);

    for (auto i : tup) {
        static_assert(std::is_same<decltype(i), py_ptr>::value, "expected py_ptr");
    }
    for (auto i : lst) {
        static_assert(std::is_same<decltype(i), py_int>::value, "expected py_int");
    }

    auto lst2 = py_list<py_bool>(std::begin(tup), std::end(tup));
    lst2.append(true);
    lst2.append(false);
    lst2.del(-1);
    lst2.del(-1);
    static_assert(std::is_convertible<decltype(lst2[0]), py_bool>::value, "expected bool");
}