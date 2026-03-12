// benchmarks/bench_structs.cpp
#include "utils.hpp"
#include <qbuem_json/qbuem_json.hpp>
#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <optional>
#include <list>
#include <deque>
#include <set>
#include <unordered_map>

#ifdef BEAST_HAS_GLAZE
#include <glaze/glaze.hpp>
#endif

#include <nlohmann/json.hpp>
#include <rapidjson/document.h>
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>

// nlohmann/json std::optional support
namespace nlohmann {
    template <typename T>
    struct adl_serializer<std::optional<T>> {
        static void to_json(json& j, const std::optional<T>& opt) {
            if (opt) j = *opt;
            else j = nullptr;
        }
        static void from_json(const json& j, std::optional<T>& opt) {
            if (j.is_null()) opt = std::nullopt;
            else opt = j.get<T>();
        }
    };
}

using namespace qbuem::json;

// ── 1. Simple Flat Struct ────────────────────────────────────────────────────
struct SimpleStruct {
    int id;
    double value;
    std::string name;
    bool active;
};
QBUEM_JSON_FIELDS(SimpleStruct, id, value, name, active)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(SimpleStruct, id, value, name, active)

#ifdef BEAST_HAS_GLAZE
template <> struct glz::meta<SimpleStruct> {
    using T = SimpleStruct;
    static constexpr auto value = object("id", &T::id, "value", &T::value, "name", &T::name, "active", &T::active);
};
#endif

void to_rapidjson(rapidjson::Value& v, const SimpleStruct& s, rapidjson::Document::AllocatorType& a) {
    v.SetObject();
    v.AddMember("id", s.id, a);
    v.AddMember("value", s.value, a);
    v.AddMember("name", rapidjson::Value(s.name.c_str(), a).Move(), a);
    v.AddMember("active", s.active, a);
}
void from_rapidjson(const rapidjson::Value& v, SimpleStruct& s) {
    s.id = v["id"].GetInt();
    s.value = v["value"].GetDouble();
    s.name = v["name"].GetString();
    s.active = v["active"].GetBool();
}

// ── 2. Nested Struct ─────────────────────────────────────────────────────────
struct Address {
    std::string street;
    std::string city;
    int zip;
};
QBUEM_JSON_FIELDS(Address, street, city, zip)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Address, street, city, zip)

struct NestedStruct {
    uint64_t user_id;
    Address address;
    std::vector<int> scores;
};
QBUEM_JSON_FIELDS(NestedStruct, user_id, address, scores)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(NestedStruct, user_id, address, scores)

#ifdef BEAST_HAS_GLAZE
template <> struct glz::meta<Address> {
    using T = Address;
    static constexpr auto value = object("street", &T::street, "city", &T::city, "zip", &T::zip);
};
template <> struct glz::meta<NestedStruct> {
    using T = NestedStruct;
    static constexpr auto value = object("user_id", &T::user_id, "address", &T::address, "scores", &T::scores);
};
#endif

void to_rapidjson(rapidjson::Value& v, const Address& s, rapidjson::Document::AllocatorType& a) {
    v.SetObject();
    v.AddMember("street", rapidjson::Value(s.street.c_str(), a).Move(), a);
    v.AddMember("city", rapidjson::Value(s.city.c_str(), a).Move(), a);
    v.AddMember("zip", s.zip, a);
}
void from_rapidjson(const rapidjson::Value& v, Address& s) {
    s.street = v["street"].GetString();
    s.city = v["city"].GetString();
    s.zip = v["zip"].GetInt();
}
void to_rapidjson(rapidjson::Value& v, const NestedStruct& s, rapidjson::Document::AllocatorType& a) {
    v.SetObject();
    v.AddMember("user_id", s.user_id, a);
    rapidjson::Value addr;
    to_rapidjson(addr, s.address, a);
    v.AddMember("address", addr, a);
    rapidjson::Value scores(rapidjson::kArrayType);
    for (int score : s.scores) scores.PushBack(score, a);
    v.AddMember("scores", scores, a);
}
void from_rapidjson(const rapidjson::Value& v, NestedStruct& s) {
    s.user_id = v["user_id"].GetUint64();
    from_rapidjson(v["address"], s.address);
    s.scores.clear();
    for (auto& score : v["scores"].GetArray()) s.scores.push_back(score.GetInt());
}

// ── 3. Complex Struct (Maps, Optionals, Depth) ────────────────────────────────
struct Metadata {
    std::optional<std::string> description;
    std::map<std::string, std::string> tags;
};
QBUEM_JSON_FIELDS(Metadata, description, tags)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Metadata, description, tags)

struct ComplexStruct {
    std::string title;
    std::vector<NestedStruct> history;
    Metadata meta_info;
};
QBUEM_JSON_FIELDS(ComplexStruct, title, history, meta_info)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ComplexStruct, title, history, meta_info)

#ifdef BEAST_HAS_GLAZE
template <> struct glz::meta<Metadata> {
    using T = Metadata;
    static constexpr auto value = object("description", &T::description, "tags", &T::tags);
};
template <> struct glz::meta<ComplexStruct> {
    using T = ComplexStruct;
    static constexpr auto value = object("title", &T::title, "history", &T::history, "meta_info", &T::meta_info);
};
#endif

void to_rapidjson(rapidjson::Value& v, const Metadata& s, rapidjson::Document::AllocatorType& a) {
    v.SetObject();
    if (s.description) v.AddMember("description", rapidjson::Value(s.description->c_str(), a).Move(), a);
    else v.AddMember("description", rapidjson::Value(rapidjson::kNullType), a);
    rapidjson::Value tags(rapidjson::kObjectType);
    for (auto const& [key, val] : s.tags) {
        tags.AddMember(rapidjson::Value(key.c_str(), a).Move(), rapidjson::Value(val.c_str(), a).Move(), a);
    }
    v.AddMember("tags", tags, a);
}
void from_rapidjson(const rapidjson::Value& v, Metadata& s) {
    if (v["description"].IsNull()) s.description = std::nullopt;
    else s.description = v["description"].GetString();
    s.tags.clear();
    for (auto const& m : v["tags"].GetObject()) {
        s.tags[m.name.GetString()] = m.value.GetString();
    }
}
void to_rapidjson(rapidjson::Value& v, const ComplexStruct& s, rapidjson::Document::AllocatorType& a) {
    v.SetObject();
    v.AddMember("title", rapidjson::Value(s.title.c_str(), a).Move(), a);
    rapidjson::Value history(rapidjson::kArrayType);
    for (auto const& h : s.history) {
        rapidjson::Value hv;
        to_rapidjson(hv, h, a);
        history.PushBack(hv, a);
    }
    v.AddMember("history", history, a);
    rapidjson::Value meta;
    to_rapidjson(meta, s.meta_info, a);
    v.AddMember("meta_info", meta, a);
}
void from_rapidjson(const rapidjson::Value& v, ComplexStruct& s) {
    s.title = v["title"].GetString();
    s.history.clear();
    for (auto& h : v["history"].GetArray()) {
        NestedStruct ns{};
        from_rapidjson(h, ns);
        s.history.push_back(std::move(ns));
    }
    from_rapidjson(v["meta_info"], s.meta_info);
}

// ── 4. Deeply Nested (Recursive) ──────────────────────────────────────────────
struct Node {
    int val;
    std::vector<Node> children;
};
QBUEM_JSON_FIELDS(Node, val, children)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Node, val, children)

#ifdef BEAST_HAS_GLAZE
template <> struct glz::meta<Node> {
    using T = Node;
    static constexpr auto value = object("val", &T::val, "children", &T::children);
};
#endif

void to_rapidjson(rapidjson::Value& v, const Node& s, rapidjson::Document::AllocatorType& a) {
    v.SetObject();
    v.AddMember("val", s.val, a);
    rapidjson::Value children(rapidjson::kArrayType);
    for (auto const& c : s.children) {
        rapidjson::Value cv;
        to_rapidjson(cv, c, a);
        children.PushBack(cv, a);
    }
    v.AddMember("children", children, a);
}
void from_rapidjson(const rapidjson::Value& v, Node& s) {
    s.val = v["val"].GetInt();
    s.children.clear();
    if (v.HasMember("children") && v["children"].IsArray()) {
        for (auto& c : v["children"].GetArray()) {
            Node cn{};
            from_rapidjson(c, cn);
            s.children.push_back(std::move(cn));
        }
    }
}

// ── 5. Extreme Harsh STL (100 levels) ────────────────────────────────────────
struct HarshNode {
    int id;
    std::string data;
    std::optional<double> score;
    std::vector<int> vec;
    std::list<int> list;
    std::deque<int> deque;
    std::set<int> set;
    std::map<std::string, int> map;
    std::unordered_map<std::string, int> umap;
    std::vector<HarshNode> children;
    std::map<std::string, HarshNode> neighbors;
};
QBUEM_JSON_FIELDS(HarshNode, id, data, score, vec, list, deque, set, map, umap, children, neighbors)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(HarshNode, id, data, score, vec, list, deque, set, map, umap, children, neighbors)

#ifdef BEAST_HAS_GLAZE
template <> struct glz::meta<HarshNode> {
    using T = HarshNode;
    static constexpr auto value = object("id", &T::id, "data", &T::data, "score", &T::score, "vec", &T::vec, "list", &T::list, "deque", &T::deque, "set", &T::set, "map", &T::map, "umap", &T::umap, "children", &T::children, "neighbors", &T::neighbors);
};
#endif

void to_rapidjson(rapidjson::Value& v, const HarshNode& s, rapidjson::Document::AllocatorType& a) {
    v.SetObject();
    v.AddMember("id", s.id, a);
    v.AddMember("data", rapidjson::Value(s.data.c_str(), a).Move(), a);
    if (s.score) v.AddMember("score", *s.score, a);
    else v.AddMember("score", rapidjson::Value(rapidjson::kNullType), a);
    
    rapidjson::Value rv(rapidjson::kArrayType);
    for (int x : s.vec) rv.PushBack(x, a);
    v.AddMember("vec", rv, a);

    rapidjson::Value lv(rapidjson::kArrayType);
    for (int x : s.list) lv.PushBack(x, a);
    v.AddMember("list", lv, a);

    rapidjson::Value dv(rapidjson::kArrayType);
    for (int x : s.deque) dv.PushBack(x, a);
    v.AddMember("deque", dv, a);

    rapidjson::Value sv(rapidjson::kArrayType);
    for (int x : s.set) sv.PushBack(x, a);
    v.AddMember("set", sv, a);

    rapidjson::Value mv(rapidjson::kObjectType);
    for (auto const& [k, val] : s.map) {
        mv.AddMember(rapidjson::Value(k.c_str(), a).Move(), val, a);
    }
    v.AddMember("map", mv, a);

    rapidjson::Value umv(rapidjson::kObjectType);
    for (auto const& [k, val] : s.umap) {
        umv.AddMember(rapidjson::Value(k.c_str(), a).Move(), val, a);
    }
    v.AddMember("umap", umv, a);
    
    rapidjson::Value children(rapidjson::kArrayType);
    for (auto const& c : s.children) {
        rapidjson::Value cv;
        to_rapidjson(cv, c, a);
        children.PushBack(cv, a);
    }
    v.AddMember("children", children, a);
    
    rapidjson::Value neighbors(rapidjson::kObjectType);
    for (auto const& [key, val] : s.neighbors) {
        rapidjson::Value nv;
        to_rapidjson(nv, val, a);
        neighbors.AddMember(rapidjson::Value(key.c_str(), a).Move(), nv, a);
    }
    v.AddMember("neighbors", neighbors, a);
}

void from_rapidjson(const rapidjson::Value& v, HarshNode& s) {
    s.id = v["id"].GetInt();
    s.data = v["data"].GetString();
    if (v["score"].IsNull()) s.score = std::nullopt;
    else s.score = v["score"].GetDouble();
    
    s.vec.clear();
    for (auto& x : v["vec"].GetArray()) s.vec.push_back(x.GetInt());
    
    s.list.clear();
    for (auto& x : v["list"].GetArray()) s.list.push_back(x.GetInt());

    s.deque.clear();
    for (auto& x : v["deque"].GetArray()) s.deque.push_back(x.GetInt());

    s.set.clear();
    for (auto& x : v["set"].GetArray()) s.set.insert(x.GetInt());

    s.map.clear();
    for (auto& m : v["map"].GetObject()) s.map[m.name.GetString()] = m.value.GetInt();

    s.umap.clear();
    for (auto& m : v["umap"].GetObject()) s.umap[m.name.GetString()] = m.value.GetInt();

    s.children.clear();
    if (v.HasMember("children") && v["children"].IsArray()) {
        for (auto& c : v["children"].GetArray()) {
            HarshNode cn{};
            from_rapidjson(c, cn);
            s.children.push_back(std::move(cn));
        }
    }
    
    s.neighbors.clear();
    if (v.HasMember("neighbors") && v["neighbors"].IsObject()) {
        for (auto const& m : v["neighbors"].GetObject()) {
            HarshNode nn{};
            from_rapidjson(m.value, nn);
            s.neighbors[m.name.GetString()] = std::move(nn);
        }
    }
}

// ── Helpers ──────────────────────────────────────────────────────────────────

template<typename T>
void run_benchmark(const std::string& name, const T& obj, size_t iterations) {
    bench::print_header("Struct: " + name);
    
    std::string json_str = qbuem::write(obj);
    std::cout << "JSON Size: " << json_str.size() << " bytes\n";
    bench::print_table_header();

    // ── qbuem-json (Standard) ──
    {
        bench::Timer pt, st;
        size_t rss0 = bench::get_current_rss_kb();
        {
            T temp_obj{};
            qbuem::Document doc;
            auto root = qbuem::parse(doc, json_str);
            qbuem::from_json(root, temp_obj);
            bench::do_not_optimize(temp_obj);
        }
        size_t rss1 = bench::get_current_rss_kb();
        size_t alloc_kb = (rss1 > rss0) ? rss1 - rss0 : 0;

        pt.start();
        for (size_t i = 0; i < iterations; ++i) {
            qbuem::Document doc;
            auto root = qbuem::parse(doc, json_str);
            T temp_obj{};
            qbuem::from_json(root, temp_obj);
            bench::do_not_optimize(temp_obj);
        }
        double p_ns = pt.elapsed_ns() / iterations;

        st.start();
        std::string out;
        out.reserve(json_str.size() + 128);
        for (size_t i = 0; i < iterations; ++i) {
            out.clear();
            qbuem::write_to(out, obj);
            bench::do_not_optimize(out);
        }
        double s_ns = st.elapsed_ns() / iterations;
        bench::Result{"qbuem-json (DOM)", p_ns, s_ns, true, alloc_kb}.print();
    }

    // ── qbuem-json (Nexus Fusion Core) ──
    {
        bench::Timer pt, st;
        size_t rss0 = bench::get_current_rss_kb();
        {
            T temp_obj = qbuem::fuse<T>(json_str);
            bench::do_not_optimize(temp_obj);
        }
        size_t rss1 = bench::get_current_rss_kb();
        size_t alloc_kb = (rss1 > rss0) ? rss1 - rss0 : 0;

        pt.start();
        for (size_t i = 0; i < iterations; ++i) {
            T temp_obj = qbuem::fuse<T>(json_str);
            bench::do_not_optimize(temp_obj);
        }
        double p_ns = pt.elapsed_ns() / iterations;

        st.start();
        std::string out_buf;
        out_buf.reserve(json_str.size() + 128);
        for (size_t i = 0; i < iterations; ++i) {
            out_buf.clear();
            qbuem::write_to(out_buf, obj);
            bench::do_not_optimize(out_buf);
        }
        double s_ns = st.elapsed_ns() / iterations;

        bench::Result{"qbuem-json (Nexus)", p_ns, s_ns, true, alloc_kb}.print();
    }

#ifdef BEAST_HAS_GLAZE
    // ── Glaze ──
    {
        size_t rss0 = bench::get_current_rss_kb();
        {
            T temp_obj{};
            auto ec = glz::read_json(temp_obj, json_str);
            (void)ec;
            bench::do_not_optimize(temp_obj);
        }
        size_t rss1 = bench::get_current_rss_kb();
        size_t alloc_kb = (rss1 > rss0) ? rss1 - rss0 : 0;

        bench::Timer pt, st;
        pt.start();
        for (size_t i = 0; i < iterations; ++i) {
            T temp_obj{};
            auto ec = glz::read_json(temp_obj, json_str);
            (void)ec;
            bench::do_not_optimize(temp_obj);
        }
        double p_ns = pt.elapsed_ns() / iterations;

        st.start();
        std::string out;
        out.reserve(json_str.size() + 128);
        for (size_t i = 0; i < iterations; ++i) {
            out.clear();
            auto ec = glz::write_json(obj, out);
            (void)ec;
            bench::do_not_optimize(out);
        }
        double s_ns = st.elapsed_ns() / iterations;
        bench::Result{"glaze-struct", p_ns, s_ns, true, alloc_kb}.print();
    }
#endif

    // ── nlohmann/json ──
    {
        size_t rss0 = bench::get_current_rss_kb();
        try {
            T temp_obj = nlohmann::json::parse(json_str).get<T>();
            bench::do_not_optimize(temp_obj);
        } catch(...) {}
        size_t rss1 = bench::get_current_rss_kb();
        size_t alloc_kb = (rss1 > rss0) ? rss1 - rss0 : 0;

        bench::Timer pt, st;
        pt.start();
        for (size_t i = 0; i < iterations; ++i) {
            try {
                auto j = nlohmann::json::parse(json_str);
                T temp_obj = j.get<T>();
                bench::do_not_optimize(temp_obj);
            } catch(...) {}
        }
        double p_ns = pt.elapsed_ns() / iterations;

        st.start();
        std::string out;
        for (size_t i = 0; i < iterations; ++i) {
            try {
                nlohmann::json j = obj;
                out = j.dump();
                bench::do_not_optimize(out);
            } catch(...) {}
        }
        double s_ns = st.elapsed_ns() / iterations;
        bench::Result{"nlohmann-struct", p_ns, s_ns, true, alloc_kb}.print();
    }

    // ── RapidJSON ──
    {
        size_t rss0 = bench::get_current_rss_kb();
        {
            rapidjson::Document d;
            d.Parse(json_str.c_str());
            T temp_obj{};
            if (!d.HasParseError()) from_rapidjson(d, temp_obj);
            bench::do_not_optimize(temp_obj);
        }
        size_t rss1 = bench::get_current_rss_kb();
        size_t alloc_kb = (rss1 > rss0) ? rss1 - rss0 : 0;

        bench::Timer pt, st;
        pt.start();
        for (size_t i = 0; i < iterations; ++i) {
            rapidjson::Document d;
            d.Parse(json_str.c_str());
            T temp_obj{};
            if (!d.HasParseError()) from_rapidjson(d, temp_obj);
            bench::do_not_optimize(temp_obj);
        }
        double p_ns = pt.elapsed_ns() / iterations;

        st.start();
        rapidjson::StringBuffer buffer;
        for (size_t i = 0; i < iterations; ++i) {
            buffer.Clear();
            rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
            rapidjson::Document d;
            rapidjson::Value v;
            to_rapidjson(v, obj, d.GetAllocator());
            v.Accept(writer);
            bench::do_not_optimize(buffer);
        }
        double s_ns = st.elapsed_ns() / iterations;
        bench::Result{"rapidjson-struct", p_ns, s_ns, true, alloc_kb}.print();
    }
    
    std::cout << "\n";
}

int main(int argc, char** argv) {
    size_t iterations = 10000;
    bool quick_mode = false;

    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--iter") == 0 && i + 1 < argc) {
            iterations = static_cast<size_t>(std::atoi(argv[++i]));
        } else if (std::strcmp(argv[i], "--quick") == 0) {
            quick_mode = true;
        }
    }

    if (quick_mode) iterations = 100;

    // 1. Simple
    SimpleStruct simple{123, 3.14159, "qbuem-json Performance", true};
    run_benchmark("Simple Object", simple, iterations);

    // 2. Nested
    NestedStruct nested{
        9876543210,
        {"Dynamic Drive", "Silicon Valley", 94025},
        {10, 20, 30, 40, 50, 60, 70, 80, 90, 100}
    };
    run_benchmark("Nested Object", nested, iterations);

    // 3. Complex
    Metadata meta{"High-performance JSON library", {{"lang", "cpp20"}, {"simd", "swar"}, {"speed", "fastest"}}};
    ComplexStruct complex{"qbuem-json vs Glaze", {nested, nested, nested}, meta};
    run_benchmark("Complex Object", complex, iterations / 5);

    // 4. Deeply Nested
    auto make_tree = [](auto self, int depth) -> Node {
        Node n{depth, {}};
        if (depth > 0) {
            n.children.push_back(self(self, depth - 1));
            n.children.push_back(self(self, depth - 1));
        }
        return n;
    };
    Node root = make_tree(make_tree, 8); // 2^9 - 1 nodes
    run_benchmark("Recursive Tree (Depth 8)", root, iterations / 20);

    // 5. Extreme Harsh STL (100 levels)
    auto make_harsh = [](auto self, int depth) -> HarshNode {
        HarshNode n{depth, "harsh data " + std::to_string(depth), 0.99, {1,2,3}, {4,5}, {6,7}, {8,9}, {{"k", 10}}, {{"u", 11}}, {}, {}};
        if (depth > 0) {
            if (depth % 2 == 0) {
                n.children.push_back(self(self, depth - 1));
            } else {
                n.neighbors["next"] = self(self, depth - 1);
            }
        }
        return n;
    };
    HarshNode harsh = make_harsh(make_harsh, 100);
    run_benchmark("Extreme Harsh STL (100 levels)", harsh, quick_mode ? 5 : 50);

    return 0;
}
