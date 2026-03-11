// benchmarks/bench.cpp
// Unified beast-json benchmark — merges bench_all, bench_structs, bench_hft
//
// Two measurement sections:
//   general — DOM parse+serialize on real JSON files (twitter, canada, …)
//   structs — Struct binding: C++ structs ↔ JSON (simple → complex → HFT msgs)
//
// Usage:
//   ./bench --all --quick          # CI default: both sections, quick iters
//   ./bench --section general --all # general section only
//   ./bench --section structs       # structs section only
//   ./bench twitter.json            # one file, general section
//   ./bench twitter.json --iter 300 --lib "beast::lazy"  # subprocess dispatch

#include "utils.hpp"
#include <beast_json/beast_json.hpp>
#include <fstream>

#ifdef BEAST_HAS_GLAZE
#include <glaze/glaze.hpp>
#endif

#include <nlohmann/json.hpp>
#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>
#include <simdjson.h>
#include <yyjson.h>

#include <algorithm>
#include <array>
#include <cstring>
#include <deque>
#include <iostream>
#include <list>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

// ── nlohmann optional<T> support ────────────────────────────────────────────
namespace nlohmann {
    template <typename T>
    struct adl_serializer<std::optional<T>> {
        static void to_json(json& j, const std::optional<T>& opt) {
            if (opt) j = *opt; else j = nullptr;
        }
        static void from_json(const json& j, std::optional<T>& opt) {
            if (j.is_null()) opt = std::nullopt; else opt = j.get<T>();
        }
    };
}

using namespace beast::json;

// ═══════════════════════════════════════════════════════════════════════════
// A.  General C++ struct definitions
// ═══════════════════════════════════════════════════════════════════════════

// ── A1. Simple flat struct ──────────────────────────────────────────────────
struct SimpleStruct { int id; double value; std::string name; bool active; };
BEAST_JSON_FIELDS(SimpleStruct, id, value, name, active)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(SimpleStruct, id, value, name, active)

#ifdef BEAST_HAS_GLAZE
template <> struct glz::meta<SimpleStruct> {
    using T = SimpleStruct;
    static constexpr auto value = object("id",&T::id,"value",&T::value,"name",&T::name,"active",&T::active);
};
#endif

static void to_rapidjson(rapidjson::Value& v, const SimpleStruct& s, rapidjson::Document::AllocatorType& a) {
    v.SetObject();
    v.AddMember("id", s.id, a);
    v.AddMember("value", s.value, a);
    v.AddMember("name", rapidjson::Value(s.name.c_str(), a).Move(), a);
    v.AddMember("active", s.active, a);
}
static void from_rapidjson(const rapidjson::Value& v, SimpleStruct& s) {
    s.id = v["id"].GetInt(); s.value = v["value"].GetDouble();
    s.name = v["name"].GetString(); s.active = v["active"].GetBool();
}

// ── A2. Nested struct ───────────────────────────────────────────────────────
struct Address { std::string street; std::string city; int zip; };
BEAST_JSON_FIELDS(Address, street, city, zip)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Address, street, city, zip)

struct NestedStruct { uint64_t user_id; Address address; std::vector<int> scores; };
BEAST_JSON_FIELDS(NestedStruct, user_id, address, scores)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(NestedStruct, user_id, address, scores)

#ifdef BEAST_HAS_GLAZE
template <> struct glz::meta<Address> {
    using T = Address;
    static constexpr auto value = object("street",&T::street,"city",&T::city,"zip",&T::zip);
};
template <> struct glz::meta<NestedStruct> {
    using T = NestedStruct;
    static constexpr auto value = object("user_id",&T::user_id,"address",&T::address,"scores",&T::scores);
};
#endif

static void to_rapidjson(rapidjson::Value& v, const Address& s, rapidjson::Document::AllocatorType& a) {
    v.SetObject();
    v.AddMember("street", rapidjson::Value(s.street.c_str(), a).Move(), a);
    v.AddMember("city",   rapidjson::Value(s.city.c_str(),   a).Move(), a);
    v.AddMember("zip", s.zip, a);
}
static void from_rapidjson(const rapidjson::Value& v, Address& s) {
    s.street = v["street"].GetString(); s.city = v["city"].GetString(); s.zip = v["zip"].GetInt();
}
static void to_rapidjson(rapidjson::Value& v, const NestedStruct& s, rapidjson::Document::AllocatorType& a) {
    v.SetObject();
    v.AddMember("user_id", s.user_id, a);
    rapidjson::Value addr; to_rapidjson(addr, s.address, a); v.AddMember("address", addr, a);
    rapidjson::Value scores(rapidjson::kArrayType);
    for (int x : s.scores) scores.PushBack(x, a);
    v.AddMember("scores", scores, a);
}
static void from_rapidjson(const rapidjson::Value& v, NestedStruct& s) {
    s.user_id = v["user_id"].GetUint64(); from_rapidjson(v["address"], s.address);
    s.scores.clear();
    for (auto& x : v["scores"].GetArray()) s.scores.push_back(x.GetInt());
}

// ── A3. Complex struct (map, optional, depth) ───────────────────────────────
struct Metadata { std::optional<std::string> description; std::map<std::string,std::string> tags; };
BEAST_JSON_FIELDS(Metadata, description, tags)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Metadata, description, tags)

struct ComplexStruct { std::string title; std::vector<NestedStruct> history; Metadata meta_info; };
BEAST_JSON_FIELDS(ComplexStruct, title, history, meta_info)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ComplexStruct, title, history, meta_info)

#ifdef BEAST_HAS_GLAZE
template <> struct glz::meta<Metadata> {
    using T = Metadata;
    static constexpr auto value = object("description",&T::description,"tags",&T::tags);
};
template <> struct glz::meta<ComplexStruct> {
    using T = ComplexStruct;
    static constexpr auto value = object("title",&T::title,"history",&T::history,"meta_info",&T::meta_info);
};
#endif

static void to_rapidjson(rapidjson::Value& v, const Metadata& s, rapidjson::Document::AllocatorType& a) {
    v.SetObject();
    if (s.description) v.AddMember("description", rapidjson::Value(s.description->c_str(), a).Move(), a);
    else v.AddMember("description", rapidjson::Value(rapidjson::kNullType), a);
    rapidjson::Value tags(rapidjson::kObjectType);
    for (auto const& [k,val] : s.tags)
        tags.AddMember(rapidjson::Value(k.c_str(),a).Move(), rapidjson::Value(val.c_str(),a).Move(), a);
    v.AddMember("tags", tags, a);
}
static void from_rapidjson(const rapidjson::Value& v, Metadata& s) {
    if (v["description"].IsNull()) s.description = std::nullopt;
    else s.description = v["description"].GetString();
    s.tags.clear();
    for (auto const& m : v["tags"].GetObject()) s.tags[m.name.GetString()] = m.value.GetString();
}
static void to_rapidjson(rapidjson::Value& v, const ComplexStruct& s, rapidjson::Document::AllocatorType& a) {
    v.SetObject();
    v.AddMember("title", rapidjson::Value(s.title.c_str(), a).Move(), a);
    rapidjson::Value history(rapidjson::kArrayType);
    for (auto const& h : s.history) {
        rapidjson::Value hv; to_rapidjson(hv, h, a); history.PushBack(hv, a);
    }
    v.AddMember("history", history, a);
    rapidjson::Value meta; to_rapidjson(meta, s.meta_info, a); v.AddMember("meta_info", meta, a);
}
static void from_rapidjson(const rapidjson::Value& v, ComplexStruct& s) {
    s.title = v["title"].GetString(); s.history.clear();
    for (auto& h : v["history"].GetArray()) {
        NestedStruct ns{}; from_rapidjson(h, ns); s.history.push_back(std::move(ns));
    }
    from_rapidjson(v["meta_info"], s.meta_info);
}

// ── A4. Recursive tree ──────────────────────────────────────────────────────
struct Node { int val; std::vector<Node> children; };
BEAST_JSON_FIELDS(Node, val, children)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Node, val, children)

#ifdef BEAST_HAS_GLAZE
template <> struct glz::meta<Node> {
    using T = Node;
    static constexpr auto value = object("val",&T::val,"children",&T::children);
};
#endif

static void to_rapidjson(rapidjson::Value& v, const Node& s, rapidjson::Document::AllocatorType& a) {
    v.SetObject(); v.AddMember("val", s.val, a);
    rapidjson::Value ch(rapidjson::kArrayType);
    for (auto const& c : s.children) { rapidjson::Value cv; to_rapidjson(cv,c,a); ch.PushBack(cv,a); }
    v.AddMember("children", ch, a);
}
static void from_rapidjson(const rapidjson::Value& v, Node& s) {
    s.val = v["val"].GetInt(); s.children.clear();
    if (v.HasMember("children") && v["children"].IsArray())
        for (auto& c : v["children"].GetArray()) { Node cn{}; from_rapidjson(c,cn); s.children.push_back(std::move(cn)); }
}

// ── A5. Extreme harsh STL (100 levels) ─────────────────────────────────────
struct HarshNode {
    int id; std::string data; std::optional<double> score;
    std::vector<int> vec; std::list<int> list; std::deque<int> deque;
    std::set<int> set; std::map<std::string,int> map; std::unordered_map<std::string,int> umap;
    std::vector<HarshNode> children; std::map<std::string,HarshNode> neighbors;
};
BEAST_JSON_FIELDS(HarshNode, id, data, score, vec, list, deque, set, map, umap, children, neighbors)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(HarshNode, id, data, score, vec, list, deque, set, map, umap, children, neighbors)

#ifdef BEAST_HAS_GLAZE
template <> struct glz::meta<HarshNode> {
    using T = HarshNode;
    static constexpr auto value = object("id",&T::id,"data",&T::data,"score",&T::score,
        "vec",&T::vec,"list",&T::list,"deque",&T::deque,"set",&T::set,
        "map",&T::map,"umap",&T::umap,"children",&T::children,"neighbors",&T::neighbors);
};
#endif

static void to_rapidjson(rapidjson::Value& v, const HarshNode& s, rapidjson::Document::AllocatorType& a) {
    v.SetObject();
    v.AddMember("id", s.id, a);
    v.AddMember("data", rapidjson::Value(s.data.c_str(), a).Move(), a);
    if (s.score) v.AddMember("score", *s.score, a);
    else v.AddMember("score", rapidjson::Value(rapidjson::kNullType), a);
    auto push_arr = [&](const char* key, auto& c) {
        rapidjson::Value arr(rapidjson::kArrayType);
        for (int x : c) arr.PushBack(x, a);
        v.AddMember(rapidjson::Value(key, a), arr, a);
    };
    push_arr("vec", s.vec); push_arr("list", s.list);
    push_arr("deque", s.deque); push_arr("set", s.set);
    rapidjson::Value mv(rapidjson::kObjectType);
    for (auto const& [k,val] : s.map) mv.AddMember(rapidjson::Value(k.c_str(),a).Move(), val, a);
    v.AddMember("map", mv, a);
    rapidjson::Value umv(rapidjson::kObjectType);
    for (auto const& [k,val] : s.umap) umv.AddMember(rapidjson::Value(k.c_str(),a).Move(), val, a);
    v.AddMember("umap", umv, a);
    rapidjson::Value ch(rapidjson::kArrayType);
    for (auto const& c : s.children) { rapidjson::Value cv; to_rapidjson(cv,c,a); ch.PushBack(cv,a); }
    v.AddMember("children", ch, a);
    rapidjson::Value nb(rapidjson::kObjectType);
    for (auto const& [k,val] : s.neighbors) {
        rapidjson::Value nv; to_rapidjson(nv,val,a);
        nb.AddMember(rapidjson::Value(k.c_str(),a).Move(), nv, a);
    }
    v.AddMember("neighbors", nb, a);
}
static void from_rapidjson(const rapidjson::Value& v, HarshNode& s) {
    s.id = v["id"].GetInt(); s.data = v["data"].GetString();
    if (v["score"].IsNull()) s.score = std::nullopt; else s.score = v["score"].GetDouble();
    s.vec.clear(); for (auto& x : v["vec"].GetArray()) s.vec.push_back(x.GetInt());
    s.list.clear(); for (auto& x : v["list"].GetArray()) s.list.push_back(x.GetInt());
    s.deque.clear(); for (auto& x : v["deque"].GetArray()) s.deque.push_back(x.GetInt());
    s.set.clear(); for (auto& x : v["set"].GetArray()) s.set.insert(x.GetInt());
    s.map.clear(); for (auto& m : v["map"].GetObject()) s.map[m.name.GetString()] = m.value.GetInt();
    s.umap.clear(); for (auto& m : v["umap"].GetObject()) s.umap[m.name.GetString()] = m.value.GetInt();
    s.children.clear();
    if (v.HasMember("children") && v["children"].IsArray())
        for (auto& c : v["children"].GetArray()) { HarshNode cn{}; from_rapidjson(c,cn); s.children.push_back(std::move(cn)); }
    s.neighbors.clear();
    if (v.HasMember("neighbors") && v["neighbors"].IsObject())
        for (auto const& m : v["neighbors"].GetObject()) {
            HarshNode nn{}; from_rapidjson(m.value, nn); s.neighbors[m.name.GetString()] = std::move(nn);
        }
}

// ═══════════════════════════════════════════════════════════════════════════
// B.  HFT message struct definitions
// ═══════════════════════════════════════════════════════════════════════════

// L1 Market Tick — highest-frequency message in any market data feed
struct MarketTick {
    std::string sym;           // SSO on GCC/Clang for ≤15-char symbols → 0 heap alloc
    double bid = 0.0, ask = 0.0;
    int64_t bid_sz = 0, ask_sz = 0, ts_ns = 0;
};
BEAST_JSON_FIELDS(MarketTick, sym, bid, ask, bid_sz, ask_sz, ts_ns)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(MarketTick, sym, bid, ask, bid_sz, ask_sz, ts_ns)

// New/cancel/modify order to matching engine
struct Order {
    std::string id, sym, side; double px = 0.0;
    int64_t qty = 0; std::string type; int64_t ts_ns = 0;
};
BEAST_JSON_FIELDS(Order, id, sym, side, px, qty, type, ts_ns)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Order, id, sym, side, px, qty, type, ts_ns)

// Trade / Execution report
struct Trade { std::string sym; double px = 0.0; int64_t qty = 0; std::string aggressor; int64_t ts_ns = 0; };
BEAST_JSON_FIELDS(Trade, sym, px, qty, aggressor, ts_ns)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Trade, sym, px, qty, aggressor, ts_ns)

// Single order book level
struct BookLevel { double px = 0.0; int64_t sz = 0, cnt = 0; };
BEAST_JSON_FIELDS(BookLevel, px, sz, cnt)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(BookLevel, px, sz, cnt)

// L2 top-5 snapshot — std::array: fixed-size, no heap allocation
struct BookSnapshot {
    std::string sym;
    std::array<BookLevel,5> bids = {}, asks = {};
    int64_t ts_ns = 0;
};
BEAST_JSON_FIELDS(BookSnapshot, sym, bids, asks, ts_ns)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(BookSnapshot, sym, bids, asks, ts_ns)

#ifdef BEAST_HAS_GLAZE
template <> struct glz::meta<MarketTick> {
    using T = MarketTick;
    static constexpr auto value = object("sym",&T::sym,"bid",&T::bid,"ask",&T::ask,
        "bid_sz",&T::bid_sz,"ask_sz",&T::ask_sz,"ts_ns",&T::ts_ns);
};
template <> struct glz::meta<Order> {
    using T = Order;
    static constexpr auto value = object("id",&T::id,"sym",&T::sym,"side",&T::side,
        "px",&T::px,"qty",&T::qty,"type",&T::type,"ts_ns",&T::ts_ns);
};
template <> struct glz::meta<Trade> {
    using T = Trade;
    static constexpr auto value = object("sym",&T::sym,"px",&T::px,"qty",&T::qty,
        "aggressor",&T::aggressor,"ts_ns",&T::ts_ns);
};
template <> struct glz::meta<BookLevel> {
    using T = BookLevel;
    static constexpr auto value = object("px",&T::px,"sz",&T::sz,"cnt",&T::cnt);
};
template <> struct glz::meta<BookSnapshot> {
    using T = BookSnapshot;
    static constexpr auto value = object("sym",&T::sym,"bids",&T::bids,"asks",&T::asks,"ts_ns",&T::ts_ns);
};
#endif

// RapidJSON bindings for HFT types
static void to_rapidjson(rapidjson::Value& v, const BookLevel& b, rapidjson::Document::AllocatorType& a) {
    v.SetObject(); v.AddMember("px", b.px, a); v.AddMember("sz", b.sz, a); v.AddMember("cnt", b.cnt, a);
}
static void from_rapidjson(const rapidjson::Value& v, BookLevel& b) {
    b.px = v["px"].GetDouble(); b.sz = v["sz"].GetInt64(); b.cnt = v["cnt"].GetInt64();
}

static void to_rapidjson(rapidjson::Value& v, const MarketTick& t, rapidjson::Document::AllocatorType& a) {
    v.SetObject();
    v.AddMember("sym", rapidjson::Value(t.sym.c_str(), a).Move(), a);
    v.AddMember("bid", t.bid, a); v.AddMember("ask", t.ask, a);
    v.AddMember("bid_sz", t.bid_sz, a); v.AddMember("ask_sz", t.ask_sz, a);
    v.AddMember("ts_ns", t.ts_ns, a);
}
static void from_rapidjson(const rapidjson::Value& v, MarketTick& t) {
    t.sym = v["sym"].GetString(); t.bid = v["bid"].GetDouble(); t.ask = v["ask"].GetDouble();
    t.bid_sz = v["bid_sz"].GetInt64(); t.ask_sz = v["ask_sz"].GetInt64(); t.ts_ns = v["ts_ns"].GetInt64();
}

static void to_rapidjson(rapidjson::Value& v, const Order& o, rapidjson::Document::AllocatorType& a) {
    v.SetObject();
    v.AddMember("id",   rapidjson::Value(o.id.c_str(),   a).Move(), a);
    v.AddMember("sym",  rapidjson::Value(o.sym.c_str(),  a).Move(), a);
    v.AddMember("side", rapidjson::Value(o.side.c_str(), a).Move(), a);
    v.AddMember("px", o.px, a); v.AddMember("qty", o.qty, a);
    v.AddMember("type", rapidjson::Value(o.type.c_str(), a).Move(), a);
    v.AddMember("ts_ns", o.ts_ns, a);
}
static void from_rapidjson(const rapidjson::Value& v, Order& o) {
    o.id = v["id"].GetString(); o.sym = v["sym"].GetString(); o.side = v["side"].GetString();
    o.px = v["px"].GetDouble(); o.qty = v["qty"].GetInt64();
    o.type = v["type"].GetString(); o.ts_ns = v["ts_ns"].GetInt64();
}

static void to_rapidjson(rapidjson::Value& v, const Trade& t, rapidjson::Document::AllocatorType& a) {
    v.SetObject();
    v.AddMember("sym", rapidjson::Value(t.sym.c_str(), a).Move(), a);
    v.AddMember("px", t.px, a); v.AddMember("qty", t.qty, a);
    v.AddMember("aggressor", rapidjson::Value(t.aggressor.c_str(), a).Move(), a);
    v.AddMember("ts_ns", t.ts_ns, a);
}
static void from_rapidjson(const rapidjson::Value& v, Trade& t) {
    t.sym = v["sym"].GetString(); t.px = v["px"].GetDouble(); t.qty = v["qty"].GetInt64();
    t.aggressor = v["aggressor"].GetString(); t.ts_ns = v["ts_ns"].GetInt64();
}

static void to_rapidjson(rapidjson::Value& v, const BookSnapshot& bs, rapidjson::Document::AllocatorType& a) {
    v.SetObject();
    v.AddMember("sym", rapidjson::Value(bs.sym.c_str(), a).Move(), a);
    auto push_levels = [&](const char* key, const std::array<BookLevel,5>& arr) {
        rapidjson::Value lvls(rapidjson::kArrayType);
        for (const auto& b : arr) { rapidjson::Value bv; to_rapidjson(bv,b,a); lvls.PushBack(bv,a); }
        v.AddMember(rapidjson::Value(key,a), lvls, a);
    };
    push_levels("bids", bs.bids); push_levels("asks", bs.asks);
    v.AddMember("ts_ns", bs.ts_ns, a);
}
static void from_rapidjson(const rapidjson::Value& v, BookSnapshot& bs) {
    bs.sym = v["sym"].GetString(); bs.ts_ns = v["ts_ns"].GetInt64();
    auto& bids = v["bids"]; for (unsigned i=0; i<5 && i<bids.Size(); ++i) from_rapidjson(bids[i], bs.bids[i]);
    auto& asks = v["asks"]; for (unsigned i=0; i<5 && i<asks.Size(); ++i) from_rapidjson(asks[i], bs.asks[i]);
}

// ═══════════════════════════════════════════════════════════════════════════
// C.  General section — DOM file benchmark (subprocess-per-library model)
// ═══════════════════════════════════════════════════════════════════════════

static void run_file(const std::string& exe_path, const std::string& lib_filter,
                     const std::string& filename, size_t N, bool parse_only) {
    if (lib_filter.empty()) {
        std::string content;
        try { content = bench::read_file(filename.c_str()); }
        catch (const std::exception& e) { std::cerr << "Skip " << filename << ": " << e.what() << "\n"; return; }
        if (content.empty()) { std::cerr << "Skip " << filename << ": empty\n"; return; }

        bench::print_header("bench_all \xe2\x80\x94 " + filename);   // em-dash UTF-8
        std::cout << "Size: " << (content.size() / 1024.0) << " KB  Iterations: " << N << "\n";
        bench::print_table_header();

        std::vector<std::string> libs = {"beast::lazy","simdjson","yyjson","RapidJSON","Glaze DOM","nlohmann"};
        for (const auto& lib : libs) {
#ifndef BEAST_HAS_GLAZE
            if (lib == "Glaze DOM") continue;
#endif
            std::string cmd = exe_path + " " + filename + " --iter " + std::to_string(N) +
                              " --lib \"" + lib + "\"";
            if (parse_only) cmd += " --parse-only";
            std::cout << std::flush;
            int ret = system(cmd.c_str()); (void)ret;
        }
        std::cout << "\n";
        return;
    }

    std::string content;
    try { content = bench::read_file(filename.c_str()); } catch (...) { return; }

    if (lib_filter == "beast::lazy") {
        size_t rss0 = bench::get_current_rss_kb();
        beast::Document ctx; beast::parse(ctx, content);
        size_t alloc_kb = bench::get_current_rss_kb() - rss0;

        bench::Timer pt, st;
        pt.start();
        for (size_t i = 0; i < N; ++i) beast::parse(ctx, content);
        double p_ns = pt.elapsed_ns() / N;

        double s_ns = 0.0; bool ok = true;
        auto doc = beast::parse(ctx, content);
        if (!parse_only) {
            std::string buf; st.start();
            for (size_t i = 0; i < N; ++i) doc.dump(buf);
            s_ns = st.elapsed_ns() / N;
            ok = !doc.dump().empty();
        }
        bench::Result{"beast::lazy", p_ns, s_ns, ok, alloc_kb}.print();
    }

    if (lib_filter == "simdjson") {
        size_t rss0 = bench::get_current_rss_kb();
        simdjson::padded_string padded(content);
        simdjson::dom::parser parser;
        auto doc_mem = parser.parse(padded); (void)doc_mem;
        size_t alloc_kb = bench::get_current_rss_kb() - rss0;

        bench::Timer pt, st;
        pt.start();
        for (size_t i = 0; i < N; ++i) { auto d = parser.parse(padded); (void)d; }
        double p_ns = pt.elapsed_ns() / N;

        double s_ns = 0.0;
        if (!parse_only) {
            auto doc = parser.parse(padded); std::string out;
            st.start();
            for (size_t i = 0; i < N; ++i) out = simdjson::minify(doc);
            s_ns = st.elapsed_ns() / N;
        }
        bench::Result{"simdjson", p_ns, s_ns, true, alloc_kb}.print();
    }

    if (lib_filter == "yyjson") {
        size_t rss0 = bench::get_current_rss_kb();
        yyjson_doc* d_mem = yyjson_read(content.c_str(), content.size(), 0);
        size_t alloc_kb = bench::get_current_rss_kb() - rss0;
        yyjson_doc_free(d_mem);

        bench::Timer pt, st;
        pt.start();
        for (size_t i = 0; i < N; ++i) { yyjson_doc* d = yyjson_read(content.c_str(), content.size(), 0); yyjson_doc_free(d); }
        double p_ns = pt.elapsed_ns() / N;

        double s_ns = 0.0;
        if (!parse_only) {
            yyjson_doc* d = yyjson_read(content.c_str(), content.size(), 0);
            st.start();
            for (size_t i = 0; i < N; ++i) { size_t l; char* s = yyjson_write(d,0,&l); free(s); }
            s_ns = st.elapsed_ns() / N;
            yyjson_doc_free(d);
        }
        bench::Result{"yyjson", p_ns, s_ns, true, alloc_kb}.print();
    }

    if (lib_filter == "RapidJSON") {
        size_t rss0 = bench::get_current_rss_kb();
        rapidjson::Document d_mem; d_mem.Parse(content.c_str());
        size_t alloc_kb = bench::get_current_rss_kb() - rss0;

        bench::Timer pt, st;
        pt.start();
        for (size_t i = 0; i < N; ++i) { rapidjson::Document d; d.Parse(content.c_str()); }
        double p_ns = pt.elapsed_ns() / N;

        double s_ns = 0.0;
        if (!parse_only) {
            rapidjson::Document d; d.Parse(content.c_str());
            rapidjson::StringBuffer buf; st.start();
            for (size_t i = 0; i < N; ++i) {
                buf.Clear(); rapidjson::Writer<rapidjson::StringBuffer> w(buf); d.Accept(w);
            }
            s_ns = st.elapsed_ns() / N;
        }
        bench::Result{"RapidJSON", p_ns, s_ns, true, alloc_kb}.print();
    }

#ifdef BEAST_HAS_GLAZE
    if (lib_filter == "Glaze DOM") {
        size_t rss0 = bench::get_current_rss_kb();
        glz::json_t d_mem; (void)glz::read_json(d_mem, content);
        size_t alloc_kb = bench::get_current_rss_kb() - rss0;

        bench::Timer pt, st;
        pt.start();
        for (size_t i = 0; i < N; ++i) { glz::json_t d; auto ec=glz::read_json(d,content); (void)ec; }
        double p_ns = pt.elapsed_ns() / N;

        double s_ns = 0.0;
        if (!parse_only) {
            glz::json_t d; (void)glz::read_json(d, content); std::string out;
            st.start();
            for (size_t i = 0; i < N; ++i) { out.clear(); (void)glz::write_json(d,out); }
            s_ns = st.elapsed_ns() / N;
        }
        bench::Result{"Glaze DOM", p_ns, s_ns, true, alloc_kb}.print();
    }
#endif

    if (lib_filter == "nlohmann") {
        size_t rss0 = bench::get_current_rss_kb();
        [[maybe_unused]] auto j_mem = nlohmann::json::parse(content);
        size_t alloc_kb = bench::get_current_rss_kb() - rss0;

        bench::Timer pt, st;
        pt.start();
        for (size_t i = 0; i < N; ++i) { [[maybe_unused]] auto j = nlohmann::json::parse(content); }
        double p_ns = pt.elapsed_ns() / N;

        double s_ns = 0.0;
        if (!parse_only) {
            nlohmann::json j = nlohmann::json::parse(content);
            st.start();
            for (size_t i = 0; i < N; ++i) (void)j.dump();
            s_ns = st.elapsed_ns() / N;
        }
        bench::Result{"nlohmann", p_ns, s_ns, true, alloc_kb}.print();
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// D.  Structs section — struct binding benchmark template
// ═══════════════════════════════════════════════════════════════════════════

template<typename T>
static void run_benchmark(const std::string& name, const T& obj, size_t iterations) {
    bench::print_header("Struct: " + name);
    std::string json_str = beast::write(obj);
    std::cout << "JSON Size: " << json_str.size() << " bytes\n";
    bench::print_table_header();

    // ── Beast DOM ──
    {
        bench::Timer pt, st;
        size_t rss0 = bench::get_current_rss_kb();
        { T tmp{}; beast::Document doc; auto root = beast::parse(doc,json_str); beast::from_json(root,tmp); bench::do_not_optimize(tmp); }
        size_t alloc_kb = bench::get_current_rss_kb() - rss0;

        pt.start();
        for (size_t i = 0; i < iterations; ++i) {
            beast::Document doc; auto root = beast::parse(doc,json_str); T tmp{}; beast::from_json(root,tmp); bench::do_not_optimize(tmp);
        }
        double p_ns = pt.elapsed_ns() / iterations;

        std::string out; out.reserve(json_str.size()+128);
        st.start();
        for (size_t i = 0; i < iterations; ++i) { out.clear(); beast::write_to(out,obj); bench::do_not_optimize(out); }
        double s_ns = st.elapsed_ns() / iterations;
        bench::Result{"Beast (DOM)", p_ns, s_ns, true, alloc_kb}.print();
    }

    // ── Beast Nexus ──
    {
        bench::Timer pt, st;
        size_t rss0 = bench::get_current_rss_kb();
        { T tmp = beast::fuse<T>(json_str); bench::do_not_optimize(tmp); }
        size_t alloc_kb = bench::get_current_rss_kb() - rss0;

        pt.start();
        for (size_t i = 0; i < iterations; ++i) { T tmp = beast::fuse<T>(json_str); bench::do_not_optimize(tmp); }
        double p_ns = pt.elapsed_ns() / iterations;

        std::string out; out.reserve(json_str.size()+128);
        st.start();
        for (size_t i = 0; i < iterations; ++i) { out.clear(); beast::write_to(out,obj); bench::do_not_optimize(out); }
        double s_ns = st.elapsed_ns() / iterations;
        bench::Result{"Beast (Nexus)", p_ns, s_ns, true, alloc_kb}.print();
    }

#ifdef BEAST_HAS_GLAZE
    // ── Glaze ──
    {
        size_t rss0 = bench::get_current_rss_kb();
        { T tmp{}; auto ec=glz::read_json(tmp,json_str); (void)ec; bench::do_not_optimize(tmp); }
        size_t alloc_kb = bench::get_current_rss_kb() - rss0;

        bench::Timer pt, st;
        pt.start();
        for (size_t i = 0; i < iterations; ++i) { T tmp{}; auto ec=glz::read_json(tmp,json_str); (void)ec; bench::do_not_optimize(tmp); }
        double p_ns = pt.elapsed_ns() / iterations;

        std::string out; out.reserve(json_str.size()+128);
        st.start();
        for (size_t i = 0; i < iterations; ++i) { out.clear(); auto ec=glz::write_json(obj,out); (void)ec; bench::do_not_optimize(out); }
        double s_ns = st.elapsed_ns() / iterations;
        bench::Result{"glaze-struct", p_ns, s_ns, true, alloc_kb}.print();
    }
#endif

    // ── nlohmann ──
    {
        size_t rss0 = bench::get_current_rss_kb();
        try { T tmp = nlohmann::json::parse(json_str).get<T>(); bench::do_not_optimize(tmp); } catch(...) {}
        size_t alloc_kb = bench::get_current_rss_kb() - rss0;

        bench::Timer pt, st;
        pt.start();
        for (size_t i = 0; i < iterations; ++i) {
            try { auto j = nlohmann::json::parse(json_str); T tmp = j.get<T>(); bench::do_not_optimize(tmp); } catch(...) {}
        }
        double p_ns = pt.elapsed_ns() / iterations;

        std::string out;
        st.start();
        for (size_t i = 0; i < iterations; ++i) {
            try { nlohmann::json j = obj; out = j.dump(); bench::do_not_optimize(out); } catch(...) {}
        }
        double s_ns = st.elapsed_ns() / iterations;
        bench::Result{"nlohmann-struct", p_ns, s_ns, true, alloc_kb}.print();
    }

    // ── RapidJSON ──
    {
        size_t rss0 = bench::get_current_rss_kb();
        { rapidjson::Document d; d.Parse(json_str.c_str()); T tmp{}; if(!d.HasParseError()) from_rapidjson(d,tmp); bench::do_not_optimize(tmp); }
        size_t alloc_kb = bench::get_current_rss_kb() - rss0;

        bench::Timer pt, st;
        pt.start();
        for (size_t i = 0; i < iterations; ++i) {
            rapidjson::Document d; d.Parse(json_str.c_str()); T tmp{}; if(!d.HasParseError()) from_rapidjson(d,tmp); bench::do_not_optimize(tmp);
        }
        double p_ns = pt.elapsed_ns() / iterations;

        rapidjson::StringBuffer buf;
        st.start();
        for (size_t i = 0; i < iterations; ++i) {
            buf.Clear(); rapidjson::Writer<rapidjson::StringBuffer> w(buf);
            rapidjson::Document d; rapidjson::Value v;
            to_rapidjson(v, obj, d.GetAllocator()); v.Accept(w); bench::do_not_optimize(buf);
        }
        double s_ns = st.elapsed_ns() / iterations;
        bench::Result{"rapidjson-struct", p_ns, s_ns, true, alloc_kb}.print();
    }

    std::cout << "\n";
}

// ═══════════════════════════════════════════════════════════════════════════
// E.  main
// ═══════════════════════════════════════════════════════════════════════════

int main(int argc, char** argv) {
    size_t N = 300;
    int file_arg = -1;
    bool run_all   = false;
    bool parse_only = false;
    bool quick_mode = false;
    std::string lib_filter;
    std::string section_filter;   // "general" | "structs" | "" = both

    for (int i = 1; i < argc; ++i) {
        if      (!strcmp(argv[i],"--iter")    && i+1<argc) N = (size_t)atoi(argv[++i]);
        else if (!strcmp(argv[i],"--lib")     && i+1<argc) lib_filter = argv[++i];
        else if (!strcmp(argv[i],"--section") && i+1<argc) section_filter = argv[++i];
        else if (!strcmp(argv[i],"--all"))       run_all    = true;
        else if (!strcmp(argv[i],"--parse-only"))parse_only = true;
        else if (!strcmp(argv[i],"--quick"))     quick_mode = true;
        else file_arg = i;
    }

    // ── Subprocess dispatch mode (invoked by run_file with --lib) ──────────
    // When called with --lib, just run that one library and exit.
    if (!lib_filter.empty()) {
        std::string filename = (file_arg >= 0) ? argv[file_arg] : "twitter.json";
        run_file(argv[0], lib_filter, filename, N, parse_only);
        return 0;
    }

    bool do_general = (section_filter.empty() || section_filter == "general");
    bool do_structs = (section_filter.empty() || section_filter == "structs");

    // ── General section ────────────────────────────────────────────────────
    if (do_general) {
        std::cout << "##section general\n";

        if (run_all) {
            // Generate harsh.json if absent
            {
                std::ifstream ifs("harsh.json");
                if (!ifs.is_open()) {
                    std::ofstream ofs("harsh.json");
                    std::string s = "{\n";
                    for (int i = 0; i < 50000; ++i) {
                        s += "\"harsh_\\\"escaped\\\\_" + std::to_string(i) + "\": ";
                        s += "[ 12345.6789e-10, true, null, false, \"nested\\n\\t\\rstring\", { \"deep\": [1,2,3,4,5] } ]";
                        if (i < 49999) s += ",\n";
                    }
                    s += "\n}"; ofs << s;
                }
            }
            const std::vector<std::string> files = {
                "twitter.json","canada.json","citm_catalog.json","gsoc-2018.json","harsh.json"
            };
            size_t scale = quick_mode ? 20 : 1;
            for (const auto& f : files)
                run_file(argv[0], "", f, std::max<size_t>(1, N/scale), parse_only);
        } else {
            std::string filename = (file_arg >= 0) ? argv[file_arg] : "twitter.json";
            size_t scale = quick_mode ? 20 : 1;
            run_file(argv[0], "", filename, std::max<size_t>(1, N/scale), parse_only);
        }
    }

    // ── Structs section ────────────────────────────────────────────────────
    if (do_structs) {
        std::cout << "##section structs\n";
        size_t iter = quick_mode ? 100 : 10000;

        // General C++ structs
        run_benchmark("Simple Object",
            SimpleStruct{123, 3.14159, "Beast Performance", true}, iter);

        run_benchmark("Nested Object",
            NestedStruct{9876543210ULL, {"Dynamic Drive","Silicon Valley",94025},
                         {10,20,30,40,50,60,70,80,90,100}}, iter);

        Metadata meta{"High-performance JSON library", {{"lang","cpp20"},{"simd","swar"},{"speed","fastest"}}};
        NestedStruct ns{9876543210ULL,{"Main St","New York",10001},{1,2,3}};
        run_benchmark("Complex Object",
            ComplexStruct{"Beast vs Glaze", {ns,ns,ns}, meta}, iter/5);

        auto make_tree = [](auto self, int depth) -> Node {
            Node n{depth,{}}; if (depth>0) { n.children.push_back(self(self,depth-1)); n.children.push_back(self(self,depth-1)); } return n;
        };
        run_benchmark("Recursive Tree (Depth 8)", make_tree(make_tree,8), iter/20);

        auto make_harsh = [](auto self, int depth) -> HarshNode {
            HarshNode n{depth,"harsh data "+std::to_string(depth),0.99,{1,2,3},{4,5},{6,7},{8,9},{{"k",10}},{{"u",11}},{},{}};
            if (depth>0) { if (depth%2==0) n.children.push_back(self(self,depth-1)); else n.neighbors["next"]=self(self,depth-1); }
            return n;
        };
        run_benchmark("Extreme Harsh STL (100 levels)", make_harsh(make_harsh,100), quick_mode ? 5 : 50);

        // HFT message types
        run_benchmark("HFT: L1 Market Tick",
            MarketTick{"AAPL", 150.25, 150.27, 1000, 800, 1708012345678901234LL}, iter);

        run_benchmark("HFT: Order",
            Order{"ORD-20240001","AAPL","B",150.25,500,"LMT",1708012345678901234LL}, iter);

        run_benchmark("HFT: Trade",
            Trade{"AAPL",150.26,300,"B",1708012345678901234LL}, iter);

        BookSnapshot snap;
        snap.sym = "AAPL"; snap.ts_ns = 1708012345678901234LL;
        for (int i=0;i<5;++i) { snap.bids[i]={150.25-i*0.01, 1000-i*100, 5}; snap.asks[i]={150.27+i*0.01, 800+i*50, 3}; }
        run_benchmark("HFT: L2 BookSnapshot 5x5", snap, iter/5);
    }

    return 0;
}
