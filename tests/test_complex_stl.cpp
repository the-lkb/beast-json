/**
 * @file test_complex_stl.cpp
 *
 * Deeply nested + complex composite STL/구조체 직렬화/역직렬화 테스트.
 *
 * Coverage:
 *   - 100-depth nested JSON object/array parsing + dump roundtrip
 *   - 3-level struct: World → Region → City
 *   - map<string, map<string, vector<Point2D>>>
 *   - vector<map<string, vector<int>>>
 *   - optional<vector<map<string, int>>>
 *   - 1000-element vector, 500-key map, 10×100 map-of-vector-of-struct
 *   - MegaStruct: sub-structs composing all STL types (within 16-field limit)
 *   - Complex mutation on deeply nested values
 *   - Recursive traversal (DFS int counter, depth counter)
 *   - Multi-Value from same Document (memory safety)
 *   - Document reuse across 10 iterations
 *   - JSON Pointer deep access
 *   - Special key names + empty containers nested
 */

#include <qbuem_json/qbuem_json.hpp>
#include <gtest/gtest.h>

#include <array>
#include <deque>
#include <list>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

using namespace qbuem;

// ─── Struct definitions
// ────────────────────────────────────────────────────────

struct Point2D {
  double x{}, y{};
};
QBUEM_JSON_FIELDS(Point2D, x, y)

struct City {
  std::string name;
  int population{};
  double lat{}, lng{};
  std::vector<std::string> districts;
};
QBUEM_JSON_FIELDS(City, name, population, lat, lng, districts)

struct Region {
  std::string name;
  std::vector<City> cities;
  std::map<std::string, int> stats;
  std::optional<std::string> capital;
};
QBUEM_JSON_FIELDS(Region, name, cities, stats, capital)

struct World {
  std::string version;
  std::vector<Region> regions;
  std::map<std::string, std::vector<std::string>> tags;
  std::optional<int> year;
  int total_cities{};
};
QBUEM_JSON_FIELDS(World, version, regions, tags, year, total_cities)

// ─── MegaStruct: split into sub-structs (QBUEM_FOR_EACH max = 16 fields) ─────

struct MegaPrimitivePart {
  bool flag{};
  int32_t i32{};
  int64_t i64{};
  double dbl{};
  std::string str;
  std::optional<int> opt_int;
  std::optional<std::string> opt_str;
  std::optional<std::vector<int>> opt_vec;
};
QBUEM_JSON_FIELDS(MegaPrimitivePart, flag, i32, i64, dbl, str, opt_int, opt_str,
                  opt_vec)

struct MegaSequencePart {
  std::vector<int> vec_int;
  std::vector<double> vec_dbl;
  std::vector<std::string> vec_str;
  std::list<int> lst;
  std::deque<double> deq;
  std::set<int> s_int;
  std::set<std::string> s_str;
  std::unordered_set<int> us_int;
};
QBUEM_JSON_FIELDS(MegaSequencePart, vec_int, vec_dbl, vec_str, lst, deq, s_int,
                  s_str, us_int)

struct MegaMapPart {
  std::map<std::string, int> m_int;
  std::map<std::string, std::string> m_str;
  std::map<std::string, std::vector<int>> m_vec;
  std::unordered_map<std::string, double> um_dbl;
  std::array<int, 4> arr4;
  std::pair<int, std::string> pair_is;
  std::vector<Point2D> points;
  std::map<std::string, Point2D> named_pts;
};
QBUEM_JSON_FIELDS(MegaMapPart, m_int, m_str, m_vec, um_dbl, arr4, pair_is,
                  points, named_pts)

struct MegaStruct {
  MegaPrimitivePart prims;
  MegaSequencePart seqs;
  MegaMapPart maps;
  Point2D pt;
};
QBUEM_JSON_FIELDS(MegaStruct, prims, seqs, maps, pt)

// ─── Helper
// ───────────────────────────────────────────────────────────────────

template <typename T> static T roundtrip(const T &v) {
  return qbuem::read<T>(qbuem::write(v));
}

// ─── 100-depth nested JSON
// ────────────────────────────────────────────────────

TEST(DeepNested, Object100Depth) {
  std::string json;
  const int depth = 100;
  for (int i = 0; i < depth; ++i)
    json += "{\"k\":";
  json += "1";
  for (int i = 0; i < depth; ++i)
    json += "}";

  Document doc;
  auto root = parse(doc, json);
  EXPECT_TRUE(root.is_object());
  EXPECT_EQ(root.dump(), json);
}

TEST(DeepNested, Array100Depth) {
  std::string json;
  const int depth = 100;
  for (int i = 0; i < depth; ++i)
    json += "[";
  json += "1";
  for (int i = 0; i < depth; ++i)
    json += "]";

  Document doc;
  auto root = parse(doc, json);
  EXPECT_TRUE(root.is_array());
  EXPECT_EQ(root.dump(), json);
}

TEST(DeepNested, Mixed50DepthObjArr) {
  // {"a":[{"a":[{"a":...}]}]}  50 levels of obj→arr→obj
  std::string json;
  for (int i = 0; i < 50; ++i)
    json += "{\"a\":[";
  json += "42";
  for (int i = 0; i < 50; ++i)
    json += "]}";

  Document doc;
  auto root = parse(doc, json);
  EXPECT_TRUE(root.is_object());
  EXPECT_EQ(root.dump(), json);
}

// ─── DFS depth + int counting ────────────────────────────────────────────────

static int depth_of(const json::Value &v) {
  if (!v.is_object() && !v.is_array())
    return 0;
  int max_child = 0;
  if (v.is_object()) {
    for (auto [k, child] : v.items())
      max_child = std::max(max_child, depth_of(child));
  } else {
    for (auto child : v.elements())
      max_child = std::max(max_child, depth_of(child));
  }
  return 1 + max_child;
}

static int count_all_ints(const json::Value &v) {
  if (v.is_int())
    return 1;
  int total = 0;
  if (v.is_object()) {
    for (auto [k, child] : v.items())
      total += count_all_ints(child);
  } else if (v.is_array()) {
    for (auto child : v.elements())
      total += count_all_ints(child);
  }
  return total;
}

TEST(DeepNested, DepthTraversal10) {
  std::string json;
  const int d = 10;
  for (int i = 0; i < d; ++i)
    json += "{\"k\":";
  json += "null";
  for (int i = 0; i < d; ++i)
    json += "}";

  Document doc;
  auto root = parse(doc, json);
  EXPECT_EQ(depth_of(root), d);
}

TEST(ComplexTraversal, RecursiveIntCount) {
  std::string json = R"({
        "a": 1,
        "b": [2, 3, {"c": 4}],
        "d": {"e": 5, "f": [6, 7, 8]},
        "g": "skip",
        "h": null,
        "i": true
    })";
  Document doc;
  auto root = parse(doc, json);
  EXPECT_EQ(count_all_ints(root), 8);
}

// ─── 3-level struct: World → Region → City
// ────────────────────────────────────

TEST(DeepStruct, WorldRegionCity) {
  World w;
  w.version = "2.0";
  w.year = 2025;

  Region r1;
  r1.name = "Asia";
  r1.capital = "Tokyo";
  r1.stats = {{"gdp", 50000}, {"pop", 1400000}};

  City c1;
  c1.name = "Seoul";
  c1.population = 9700000;
  c1.lat = 37.5665;
  c1.lng = 126.9779;
  c1.districts = {"Gangnam", "Mapo", "Jongno"};

  City c2;
  c2.name = "Tokyo";
  c2.population = 13960000;
  c2.districts = {"Shinjuku", "Shibuya"};
  r1.cities = {c1, c2};

  Region r2;
  r2.name = "Europe";
  r2.capital = std::nullopt;
  r2.stats = {{"pop", 750000000}};
  City c3;
  c3.name = "Paris";
  c3.population = 2161000;
  c3.districts = {"Montmartre", "Le Marais"};
  r2.cities = {c3};

  w.regions = {r1, r2};
  w.tags = {{"languages", {"Korean", "Japanese", "French"}},
            {"continents", {"Asia", "Europe"}}};
  w.total_cities = 3;

  auto w2 = roundtrip(w);
  EXPECT_EQ(w2.version, "2.0");
  ASSERT_TRUE(w2.year.has_value());
  EXPECT_EQ(*w2.year, 2025);
  ASSERT_EQ(w2.regions.size(), 2u);
  EXPECT_EQ(w2.regions[0].name, "Asia");
  ASSERT_TRUE(w2.regions[0].capital.has_value());
  EXPECT_EQ(*w2.regions[0].capital, "Tokyo");
  EXPECT_EQ(w2.regions[0].stats["gdp"], 50000);
  ASSERT_EQ(w2.regions[0].cities.size(), 2u);
  EXPECT_EQ(w2.regions[0].cities[0].name, "Seoul");
  ASSERT_EQ(w2.regions[0].cities[0].districts.size(), 3u);
  EXPECT_EQ(w2.regions[0].cities[0].districts[0], "Gangnam");
  EXPECT_FALSE(w2.regions[1].capital.has_value());
  ASSERT_EQ(w2.tags["languages"].size(), 3u);
  EXPECT_EQ(w2.total_cities, 3);
}

// ─── map<str, map<str, vector<Point2D>>>
// ───────────────────────────────────────

TEST(ComplexTypes, MapOfMapOfVector) {
  std::map<std::string, std::map<std::string, std::vector<Point2D>>> data;
  data["level1"]["level2"] = {{1.0, 2.0}, {3.0, 4.0}, {5.0, 6.0}};
  data["level1"]["other"] = {{0.0, 0.0}};
  data["another"]["sub"] = {{7.0, 8.0}, {9.0, 10.0}};

  auto data2 = roundtrip(data);
  ASSERT_EQ(data2["level1"]["level2"].size(), 3u);
  EXPECT_DOUBLE_EQ(data2["level1"]["level2"][0].x, 1.0);
  EXPECT_DOUBLE_EQ(data2["level1"]["level2"][2].y, 6.0);
  ASSERT_EQ(data2["another"]["sub"].size(), 2u);
  EXPECT_DOUBLE_EQ(data2["another"]["sub"][1].x, 9.0);
}

// ─── vector<map<string, vector<int>>>
// ─────────────────────────────────────────

TEST(ComplexTypes, VectorOfMapOfVector) {
  std::vector<std::map<std::string, std::vector<int>>> v;
  v.push_back({{"a", {1, 2, 3}}, {"b", {4, 5}}});
  v.push_back({{"c", {6, 7, 8, 9}}});
  v.push_back({});

  auto v2 = roundtrip(v);
  ASSERT_EQ(v2.size(), 3u);
  EXPECT_EQ(v2[0]["a"][2], 3);
  EXPECT_EQ(v2[0]["b"][0], 4);
  EXPECT_EQ(v2[1]["c"][3], 9);
  EXPECT_EQ(v2[2].size(), 0u);
}

// ─── optional<vector<map<string, int>>>
// ───────────────────────────────────────

TEST(ComplexTypes, OptionalVectorMapRoundtrip) {
  std::optional<std::vector<std::map<std::string, int>>> opt;
  opt = std::vector<std::map<std::string, int>>{
      {{"x", 1}, {"y", 2}},
      {{"z", 3}},
  };
  auto opt2 = roundtrip(opt);
  ASSERT_TRUE(opt2.has_value());
  ASSERT_EQ(opt2->size(), 2u);
  EXPECT_EQ((*opt2)[0]["x"], 1);
  EXPECT_EQ((*opt2)[1]["z"], 3);

  std::optional<std::vector<std::map<std::string, int>>> none;
  EXPECT_FALSE(roundtrip(none).has_value());
}

// ─── 대량 데이터
// ───────────────────────────────────────────────────────────────

TEST(LargeData, Vector1000Ints) {
  std::vector<int> v;
  v.reserve(1000);
  for (int i = 0; i < 1000; ++i)
    v.push_back(i * 7);
  auto v2 = roundtrip(v);
  ASSERT_EQ(v2.size(), 1000u);
  for (int i = 0; i < 1000; ++i)
    EXPECT_EQ(v2[i], i * 7) << "idx " << i;
}

TEST(LargeData, Map500Keys) {
  std::map<std::string, int> m;
  for (int i = 0; i < 500; ++i)
    m["key_" + std::to_string(i)] = i * 3;
  auto m2 = roundtrip(m);
  ASSERT_EQ(m2.size(), 500u);
  EXPECT_EQ(m2["key_0"], 0);
  EXPECT_EQ(m2["key_499"], 499 * 3);
}

TEST(LargeData, MapOfVectorOf100Points) {
  std::map<std::string, std::vector<Point2D>> m;
  for (int g = 0; g < 10; ++g) {
    std::string key = "group_" + std::to_string(g);
    for (int p = 0; p < 100; ++p)
      m[key].push_back({static_cast<double>(g), static_cast<double>(p)});
  }
  auto m2 = roundtrip(m);
  ASSERT_EQ(m2.size(), 10u);
  for (int g = 0; g < 10; ++g) {
    std::string key = "group_" + std::to_string(g);
    ASSERT_EQ(m2[key].size(), 100u);
    EXPECT_DOUBLE_EQ(m2[key][50].x, static_cast<double>(g));
  }
}

// ─── 200개 Region 대량 직렬화
// ─────────────────────────────────────────────────

TEST(LargeData, Vector200Regions) {
  std::vector<Region> regions;
  regions.reserve(200);
  for (int i = 0; i < 200; ++i) {
    Region r;
    r.name = "Region_" + std::to_string(i);
    r.capital = (i % 3 == 0)
                    ? std::optional<std::string>("Cap_" + std::to_string(i))
                    : std::nullopt;
    r.stats = {{"id", i}, {"pop", i * 1000}};
    for (int j = 0; j < 5; ++j) {
      City c;
      c.name = "City_" + std::to_string(i) + "_" + std::to_string(j);
      c.population = (i + 1) * 10000 + j;
      c.districts = {"D1", "D2"};
      r.cities.push_back(c);
    }
    regions.push_back(r);
  }
  auto r2 = roundtrip(regions);
  ASSERT_EQ(r2.size(), 200u);
  EXPECT_EQ(r2[0].name, "Region_0");
  ASSERT_TRUE(r2[0].capital.has_value());
  EXPECT_EQ(r2[100].stats["pop"], 100000);
  ASSERT_EQ(r2[100].cities.size(), 5u);
  EXPECT_FALSE(r2[1].capital.has_value());
  ASSERT_TRUE(r2[3].capital.has_value());
}

// ─── MegaStruct: 모든 STL 타입 조합 ──────────────────────────────────────────

TEST(MegaStructTest, FullRoundTrip) {
  MegaStruct ms;

  ms.prims.flag = true;
  ms.prims.i32 = -2147483000;
  ms.prims.i64 = -9000000000LL;
  ms.prims.dbl = 3.141592653589793;
  ms.prims.str = "hello beast";
  ms.prims.opt_int = 42;
  ms.prims.opt_str = "optional";
  ms.prims.opt_vec = {100, 200, 300};

  ms.seqs.vec_int = {1, 2, 3, 4, 5};
  ms.seqs.vec_dbl = {0.1, 0.2, 0.3};
  ms.seqs.vec_str = {"a", "b", "c"};
  ms.seqs.lst = {10, 20, 30};
  ms.seqs.deq = {1.5, 2.5};
  ms.seqs.s_int = {1, 2, 3};
  ms.seqs.s_str = {"x", "y"};
  ms.seqs.us_int = {100, 200};

  ms.maps.m_int = {{"one", 1}, {"two", 2}};
  ms.maps.m_str = {{"key", "val"}};
  ms.maps.m_vec = {{"nums", {10, 20, 30}}};
  ms.maps.um_dbl = {{"pi", 3.14}};
  ms.maps.arr4 = {10, 20, 30, 40};
  ms.maps.pair_is = {99, "pair"};
  ms.maps.points = {{0, 0}, {1, 1}};
  ms.maps.named_pts = {{"origin", {0, 0}}, {"unit", {1, 1}}};
  ms.pt = {7.5, 8.5};

  auto ms2 = roundtrip(ms);

  // Primitives
  EXPECT_EQ(ms2.prims.flag, true);
  EXPECT_EQ(ms2.prims.i32, -2147483000);
  EXPECT_EQ(ms2.prims.i64, int64_t{-9000000000LL});
  EXPECT_NEAR(ms2.prims.dbl, 3.141592653589793, 1e-13);
  EXPECT_EQ(ms2.prims.str, "hello beast");
  ASSERT_TRUE(ms2.prims.opt_int.has_value());
  EXPECT_EQ(*ms2.prims.opt_int, 42);
  ASSERT_TRUE(ms2.prims.opt_str.has_value());
  EXPECT_EQ(*ms2.prims.opt_str, "optional");
  ASSERT_TRUE(ms2.prims.opt_vec.has_value());
  EXPECT_EQ((*ms2.prims.opt_vec)[1], 200);

  // Sequences
  ASSERT_EQ(ms2.seqs.vec_int.size(), 5u);
  EXPECT_EQ(ms2.seqs.vec_int[4], 5);
  EXPECT_EQ(ms2.seqs.vec_str[0], "a");
  ASSERT_EQ(ms2.seqs.lst.size(), 3u);
  EXPECT_EQ(*ms2.seqs.lst.begin(), 10);
  ASSERT_EQ(ms2.seqs.deq.size(), 2u);
  EXPECT_NEAR(ms2.seqs.deq[1], 2.5, 1e-9);
  ASSERT_EQ(ms2.seqs.s_int.size(), 3u);
  EXPECT_TRUE(ms2.seqs.s_int.count(2));
  ASSERT_EQ(ms2.seqs.s_str.size(), 2u);
  EXPECT_EQ(ms2.seqs.us_int.size(), 2u);

  // Maps
  EXPECT_EQ(ms2.maps.m_int["two"], 2);
  EXPECT_EQ(ms2.maps.m_str["key"], "val");
  ASSERT_EQ(ms2.maps.m_vec["nums"].size(), 3u);
  EXPECT_EQ(ms2.maps.m_vec["nums"][2], 30);
  EXPECT_NEAR(ms2.maps.um_dbl["pi"], 3.14, 1e-9);
  EXPECT_EQ(ms2.maps.arr4[3], 40);
  EXPECT_EQ(ms2.maps.pair_is.first, 99);
  EXPECT_EQ(ms2.maps.pair_is.second, "pair");
  ASSERT_EQ(ms2.maps.points.size(), 2u);
  EXPECT_DOUBLE_EQ(ms2.maps.points[1].x, 1.0);
  EXPECT_DOUBLE_EQ(ms2.maps.named_pts["unit"].y, 1.0);
  EXPECT_DOUBLE_EQ(ms2.pt.x, 7.5);
}

// ─── 빈 컬렉션들의 nested 조합
// ────────────────────────────────────────────────

TEST(ComplexTypes, EmptyContainersNested) {
  std::map<std::string, std::vector<std::map<std::string, std::set<int>>>> data;
  data["key1"] = {};
  data["key2"] = {{}, {}};
  data["key3"] = {{{"set", {}}}};

  auto data2 = roundtrip(data);
  EXPECT_EQ(data2["key1"].size(), 0u);
  EXPECT_EQ(data2["key2"].size(), 2u);
  EXPECT_EQ(data2["key2"][0].size(), 0u);
  EXPECT_EQ(data2["key3"][0]["set"].size(), 0u);
}

// ─── nullable all levels
// ──────────────────────────────────────────────────────

TEST(ComplexTypes, NullsAtAllLevels) {
  World w;
  w.version = "null-test";
  w.year = std::nullopt;
  Region r;
  r.name = "Void";
  r.capital = std::nullopt;
  City c;
  c.name = "Ghost";
  c.districts = {};
  r.cities = {c};
  w.regions = {r};
  w.total_cities = 0;

  auto w2 = roundtrip(w);
  EXPECT_FALSE(w2.year.has_value());
  EXPECT_FALSE(w2.regions[0].capital.has_value());
  EXPECT_EQ(w2.regions[0].cities[0].districts.size(), 0u);
}

// ─── 복잡한 mixed JSON structure (parse only) ────────────────────────────────

TEST(DeepNested, ComplexMixedStructure) {
  std::string json = R"({
        "meta": {"version": 3, "flags": [true, false, null]},
        "data": [
            {"id": 1, "tags": ["a","b"], "nested": {"x": 1.5, "y": [1,2,3]}},
            {"id": 2, "tags": [], "nested": {"x": 2.5, "y": []}},
            {"id": 3, "tags": ["c"], "nested": null}
        ],
        "stats": {"counts": {"total": 3, "active": 2}, "ratios": [0.33, 0.67]}
    })";
  Document doc;
  auto root = parse(doc, json);

  EXPECT_EQ(root["meta"]["version"].as<int>(), 3);
  EXPECT_TRUE(root["meta"]["flags"][0].as<bool>());
  EXPECT_TRUE(root["meta"]["flags"][2].is_null());
  EXPECT_EQ(root["data"].size(), 3u);
  EXPECT_EQ(root["data"][0]["id"].as<int>(), 1);
  EXPECT_EQ(root["data"][0]["tags"][0].as<std::string>(), "a");
  EXPECT_NEAR(root["data"][0]["nested"]["x"].as<double>(), 1.5, 1e-9);
  EXPECT_EQ(root["data"][0]["nested"]["y"][2].as<int>(), 3);
  EXPECT_EQ(root["data"][1]["tags"].size(), 0u);
  EXPECT_TRUE(root["data"][2]["nested"].is_null());
  EXPECT_EQ(root["stats"]["counts"]["total"].as<int>(), 3);
  EXPECT_NEAR(root["stats"]["ratios"][1].as<double>(), 0.67, 1e-9);

  // SafeValue chain on present path
  auto active = root.get("stats")["counts"]["active"].as<int>();
  ASSERT_TRUE(active.has_value());
  EXPECT_EQ(*active, 2);

  // SafeValue chain on absent path — must not throw
  EXPECT_FALSE(root.get("nonexistent")["level"]["x"].as<int>().has_value());
}

// ─── Deep mutation chain ─────────────────────────────────────────────────────

TEST(ComplexMutation, DeepObjectMutation) {
  std::string json =
      R"({"level1":{"level2":{"level3":{"value":42,"items":[1,2,3]}}}})";
  Document doc;
  auto root = parse(doc, json);

  root["level1"]["level2"]["level3"]["value"].set(9999);
  EXPECT_EQ(root["level1"]["level2"]["level3"]["value"].as<int>(), 9999);

  root["level1"]["level2"]["level3"]["items"][1].set(222);
  EXPECT_EQ(root["level1"]["level2"]["level3"]["items"][1].as<int>(), 222);

  // unset restores original
  root["level1"]["level2"]["level3"]["value"].unset();
  EXPECT_EQ(root["level1"]["level2"]["level3"]["value"].as<int>(), 42);

  std::string out = root.dump();
  EXPECT_NE(out.find("222"), std::string::npos);
  EXPECT_NE(out.find("42"), std::string::npos);
}

// ─── Document reuse memory safety ────────────────────────────────────────────

TEST(MemorySafety, DocumentReuse10Iterations) {
  Document doc;
  for (int iter = 0; iter < 10; ++iter) {
    std::string json = "[";
    for (int i = 0; i < 200; ++i) {
      if (i > 0)
        json += ",";
      json += "{\"id\":" + std::to_string(i) + ",\"scores\":[" +
              std::to_string(i) + "," + std::to_string(i * 2) + "]}";
    }
    json += "]";
    auto root = parse(doc, json);
    ASSERT_TRUE(root.is_array());
    ASSERT_EQ(root.size(), 200u);
    EXPECT_EQ(root[0]["id"].as<int>(), 0);
    EXPECT_EQ(root[199]["id"].as<int>(), 199);
    EXPECT_EQ(root[100]["scores"][1].as<int>(), 200);
  }
}

// ─── Multi-Value from same Document ──────────────────────────────────────────

TEST(MemorySafety, MultipleValuesFromSameDocument) {
  Document doc;
  auto root = parse(doc, R"({"a":{"x":1},"b":{"y":2},"c":[10,20,30]})");

  json::Value va = root["a"];
  json::Value vb = root["b"];
  json::Value vc = root["c"];

  EXPECT_EQ(va["x"].as<int>(), 1);
  EXPECT_EQ(vb["y"].as<int>(), 2);
  EXPECT_EQ(vc[2].as<int>(), 30);

  // Mutate through one view
  va["x"].set(999);
  EXPECT_EQ(va["x"].as<int>(), 999);
  EXPECT_EQ(root["a"]["x"].as<int>(), 999);
  EXPECT_EQ(vb["y"].as<int>(), 2); // unaffected
}

// ─── JSON Pointer deep access
// ─────────────────────────────────────────────────

TEST(DeepNested, JsonPointerDeepAccess) {
  std::string json = R"({"a":{"b":{"c":{"d":{"e":42}}}}})";
  Document doc;
  auto root = parse(doc, json);

  EXPECT_EQ(root.at("/a/b/c/d/e").as<int>(), 42);
  EXPECT_EQ(root.at<"/a/b/c/d/e">().as<int>(), 42);
  EXPECT_FALSE(root.at("/a/b/x/d/e").is_valid());
}

// ─── Special key names
// ────────────────────────────────────────────────────────

TEST(StringEdge, SpecialKeyNames) {
  std::string json = R"({"key with spaces":1,"123":2,"":3})";
  Document doc;
  auto root = parse(doc, json);
  EXPECT_EQ(root["key with spaces"].as<int>(), 1);
  EXPECT_EQ(root["123"].as<int>(), 2);
  EXPECT_EQ(root[""].as<int>(), 3);
  EXPECT_EQ(root.dump(), json);
}
