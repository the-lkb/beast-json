/**
 * qbuem_json_py.cpp — High-performance Python bindings for qbuem-json using nanobind.
 *
 * This extension provides near-native parsing and access performance by
 * exposing the C++ object model directly to Python.
 */

#include <nanobind/nanobind.h>
#include <nanobind/make_iterator.h>
#include <nanobind/stl/unique_ptr.h>
#include <nanobind/stl/string_view.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/pair.h>
#include <nanobind/stl/vector.h>
#include <qbuem_json/qbuem_json.hpp>

namespace nb = nanobind;
using namespace nb::literals;

// ── Python Wrapper for Document ──────────────────────────────────────────────

class PyDocument {
public:
    qbuem::Document doc;
    qbuem::Value    root;
    std::string     source; // Keep the source buffer alive

    PyDocument() = default;

    void parse(const std::string& json_str) {
        source = json_str;
        root = qbuem::parse(doc, source);
        if (!root.is_valid()) {
            throw std::runtime_error("qbuem-json: parse error");
        }
    }

    qbuem::Value get_root() const { return root; }
};

// ── Module Definition ────────────────────────────────────────────────────────

NB_MODULE(qbuem_json_native, m) {
    nb::class_<qbuem::Value>(m, "Value")
        .def("is_valid", &qbuem::Value::is_valid)
        .def("is_null", &qbuem::Value::is_null)
        .def("is_bool", &qbuem::Value::is_bool)
        .def("is_int", &qbuem::Value::is_int)
        .def("is_double", &qbuem::Value::is_double)
        .def("is_string", &qbuem::Value::is_string)
        .def("is_array", &qbuem::Value::is_array)
        .def("is_object", &qbuem::Value::is_object)
        .def("type_name", &qbuem::Value::type_name)
        .def("size", &qbuem::Value::size)
        .def("__len__", &qbuem::Value::size)
        .def("__getitem__", [](const qbuem::Value &v, const std::string &key) {
            auto child = v[key];
            if (!child.is_valid()) throw nb::key_error(key.c_str());
            return child;
        })
        .def("__getitem__", [](const qbuem::Value &v, size_t idx) {
            auto child = v[idx];
            if (!child.is_valid()) throw nb::index_error();
            return child;
        })
        .def("as_bool", [](const qbuem::Value &v) { return v.as<bool>(); })
        .def("as_int", [](const qbuem::Value &v) { return v.as<int64_t>(); })
        .def("as_double", [](const qbuem::Value &v) { return v.as<double>(); })
        .def("as_string", [](const qbuem::Value &v) { return std::string(v.as<std::string_view>()); })
        .def("dump", [](const qbuem::Value &v, int indent) { return v.dump(indent); }, "indent"_a = 0)
        .def("__iter__", [](const qbuem::Value &v) -> nb::object {
            if (v.is_object()) {
                auto range = v.items();
                return nb::make_iterator(nb::type<qbuem::Value>(), "ObjectIterator", 
                                         range.begin(), range.end());
            } else if (v.is_array()) {
                auto range = v.elements();
                return nb::make_iterator(nb::type<qbuem::Value>(), "ArrayIterator",
                                         range.begin(), range.end());
            }
            throw nb::type_error("Value is not iterable");
        }, nb::keep_alive<0, 1>());

    nb::class_<PyDocument>(m, "Document")
        .def(nb::init<>())
        .def("parse", &PyDocument::parse)
        .def("root", &PyDocument::get_root);

    m.def("loads", [](const std::string &s) {
        auto d = std::make_unique<PyDocument>();
        d->parse(s);
        return d; // nanobind handles unique_ptr
    });
}
