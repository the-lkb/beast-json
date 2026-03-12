#include <qbuem_json/qbuem_json.hpp>
#include <gtest/gtest.h>
#include <string>

using namespace qbuem;

TEST(Serializer, BasicTypes) {
  std::string json = "[null,true,false,123,-456,3.14,\"hello\"]";
    Document parser(json);
  Value root = parse(parser, json);

  std::string out;
  
  out = root.dump();

  EXPECT_EQ(out, json);
}

TEST(Serializer, Nested) {
  std::string json = "{\"a\":[1,2],\"b\":{\"c\":3}}";
    Document parser(json);
  Value root = parse(parser, json);

  std::string out;
  
  out = root.dump();

  EXPECT_EQ(out, json);
}

TEST(Serializer, DeepNesting) {
  std::string json;
  int depth = 100; // Exceeds 64
  for (int i = 0; i < depth; i++)
    json += "{\"a\":";
  json += "1";
  for (int i = 0; i < depth; i++)
    json += "}";

  Document parser(json);
  Value root = parse(parser, json);

  std::string out;
  
  out = root.dump();

  EXPECT_EQ(out, json);
}
