/**
 * @file JSONParser_tests.cpp
 * @created 2024-09-29
 * @updated 2026-02-15
 * @author Urs Behrmann
 * @brief Unit tests for the JSONParser class using Google Test
 */

#include <gtest/gtest.h>
#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <algorithm>
#include "../../parser/JSONParser.hpp"

using namespace geruest;

// Helper function to get file content
inline std::string getFile(const std::string &filePath) {
    std::ifstream file(filePath);
    if (!file.is_open()) {
        throw std::runtime_error("Could not open file: " + filePath);
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

TEST(JSONParserTest, SimpleKeyStrings) {
    std::string input = R"({"key": "value"})";
    JSONParser json(input);
    EXPECT_EQ(json.getString("key"), "value");
}

TEST(JSONParserTest, SimpleKeyIntegersShort) {
    std::string input = R"({"key": 42})";
    JSONParser json(input);
    EXPECT_EQ(json.getShort("key"), 42);
}

TEST(JSONParserTest, SimpleKeyIntegersInt) {
    std::string input = R"({"key": 42})";
    JSONParser json(input);
    EXPECT_EQ(json.getInt("key"), 42);
}

TEST(JSONParserTest, SimpleKeyIntegersLong) {
    std::string input = R"({"key": 42})";
    JSONParser json(input);
    EXPECT_EQ(json.getLong("key"), 42);
}

TEST(JSONParserTest, SimpleKeyIntegersLongLong) {
    std::string input = R"({"key": 42})";
    JSONParser json(input);
    EXPECT_EQ(json.getLongLong("key"), 42);
}

TEST(JSONParserTest, SimpleKeyFloatsFloat) {
    std::string input = R"({"key": 42.42})";
    JSONParser json(input);
    EXPECT_FLOAT_EQ(json.getFloat("key"), 42.42f);
}

TEST(JSONParserTest, SimpleKeyFloatsDouble) {
    std::string input = R"({"key": 42.42})";
    JSONParser json(input);
    const float f = 42.42f;
    const double d = f;
    const double parsed = json.getDouble("key");
    EXPECT_NEAR(parsed, d, 0.0001);
}

TEST(JSONParserTest, SimpleKeyFloatsLongDouble) {
    std::string input = R"({"key": 42.42})";
    JSONParser json(input);
    const float f = 42.42f;
    const long double ld = f;
    const long double parsed = json.getLongDouble("key");
    EXPECT_NEAR(static_cast<double>(parsed), static_cast<double>(ld), 0.0001);
}

TEST(JSONParserTest, SimpleKeyBooleans) {
    std::string input = R"({"key": true})";
    JSONParser json(input);
    EXPECT_TRUE(json.getBool("key"));
}

TEST(JSONParserTest, SimpleKeyNull) {
    std::string input = R"({"key": null})";
    JSONParser json(input);
    EXPECT_EQ(json.getString("key"), "null");
}

TEST(JSONParserTest, MultipleKeyValuePairs) {
    std::string input = R"({"key1": "value1", "key2": "value2"})";
    JSONParser json(input);
    EXPECT_EQ(json.getString("key1"), "value1");
    EXPECT_EQ(json.getString("key2"), "value2");
}

TEST(JSONParserTest, NestedObjects) {
    std::string input = R"({"key": {"nestedKey": "nestedValue"}})";
    JSONParser json(input);
    EXPECT_EQ(json.getObject("key").getString("nestedKey"), "nestedValue");
}

TEST(JSONParserTest, ArrayOfStrings) {
    std::string input = R"({"key": ["value1", "value2"]})";
    JSONParser json(input);
    std::vector<std::string> expected = {"value1", "value2"};
    EXPECT_EQ(json.getStringArray("key"), expected);
}

TEST(JSONParserTest, ArrayOfShorts) {
    std::string input = R"({"key": [1, 2]})";
    JSONParser json(input);
    std::vector<short> expected = {1, 2};
    EXPECT_EQ(json.getShortArray("key"), expected);
}

TEST(JSONParserTest, ArrayOfIntegers) {
    std::string input = R"({"key": [1, 2]})";
    JSONParser json(input);
    std::vector<int> expected = {1, 2};
    EXPECT_EQ(json.getIntArray("key"), expected);
}

TEST(JSONParserTest, ArrayOfLongs) {
    std::string input = R"({"key": [1, 2]})";
    JSONParser json(input);
    std::vector<long> expected = {1, 2};
    EXPECT_EQ(json.getLongArray("key"), expected);
}

TEST(JSONParserTest, ArrayOfLongLongs) {
    std::string input = R"({"key": [1, 2]})";
    JSONParser json(input);
    std::vector<long long> expected = {1, 2};
    EXPECT_EQ(json.getLongLongArray("key"), expected);
}

TEST(JSONParserTest, ArrayOfFloats) {
    std::string input = R"({"key": [1.1, 2.2]})";
    JSONParser json(input);
    std::vector<float> expected = {1.1f, 2.2f};
    EXPECT_EQ(json.getFloatArray("key"), expected);
}

TEST(JSONParserTest, ArrayOfDoubles) {
    std::string input = R"({"key": [1.1, 2.2]})";
    JSONParser json(input);
    const std::vector<double> expected = {1.1, 2.2};
    const std::vector<double> parsed = json.getDoubleArray("key");
    ASSERT_EQ(parsed.size(), expected.size());
    for (size_t i = 0; i < expected.size(); ++i) {
        EXPECT_NEAR(parsed[i], expected[i], 0.0001);
    }
}

TEST(JSONParserTest, ArrayOfLongDoubles) {
    std::string input = R"({"key": [1.1, 2.2]})";
    JSONParser json(input);
    const std::vector<long double> expected = {1.1L, 2.2L};
    const std::vector<long double> parsed = json.getLongDoubleArray("key");
    ASSERT_EQ(parsed.size(), expected.size());
    for (size_t i = 0; i < expected.size(); ++i) {
        EXPECT_NEAR(static_cast<double>(parsed[i]), static_cast<double>(expected[i]), 0.0001);
    }
}

TEST(JSONParserTest, ArrayOfBooleans) {
    std::string input = R"({"key": [true, false]})";
    JSONParser json(input);
    std::vector<bool> expected = {true, false};
    EXPECT_EQ(json.getBoolArray("key"), expected);
}

TEST(JSONParserTest, MixedDataTypes) {
    std::string input = R"({"stringKey": "stringValue", "intKey": 42, "boolKey": true})";
    JSONParser json(input);
    EXPECT_EQ(json.getInt("intKey"), 42);
    EXPECT_TRUE(json.getBool("boolKey"));
    EXPECT_EQ(json.getString("stringKey"), "stringValue");
}

TEST(JSONParserTest, EmptyObject) {
    std::string input = R"({})";
    JSONParser json(input);
    EXPECT_TRUE(json.getString("key").empty());
}

TEST(JSONParserTest, InvalidJSON) {
    // JSONParser doesn't throw for invalid JSON, it parses what it can
    std::string input = R"({"key": "value")";
    JSONParser json(input);
    SUCCEED(); // Parser handles gracefully
}

TEST(JSONParserTest, BooleanValues) {
    std::string input = R"({"trueKey": true, "falseKey": false})";
    JSONParser json(input);
    EXPECT_TRUE(json.getBool("trueKey"));
    EXPECT_FALSE(json.getBool("falseKey"));
}

TEST(JSONParserTest, LargeJSONObject) {
    std::string input = R"({
        "key1": "value1",
        "key2": {"nestedKey1": "nestedValue1", "nestedKey2": "nestedValue2"},
        "key3": ["arrayValue1", "arrayValue2"],
        "key4": 12345,
        "key5": false
    })";
    JSONParser json(input);
    std::vector<std::string> expectedArray = {"arrayValue1", "arrayValue2"};
    
    EXPECT_EQ(json.getString("key1"), "value1");
    EXPECT_EQ(json.getObject("key2").getString("nestedKey1"), "nestedValue1");
    EXPECT_EQ(json.getObject("key2").getString("nestedKey2"), "nestedValue2");
    EXPECT_EQ(json.getStringArray("key3"), expectedArray);
    EXPECT_EQ(json.getInt("key4"), 12345);
    EXPECT_FALSE(json.getBool("key5"));
}

TEST(JSONParserTest, JSONFromFile) {
    auto json = getJSONFromFileSafe("JSONParser/https.json");
    ASSERT_NE(json, nullptr);
    EXPECT_EQ(json->getString("serverRoot"), "/home/ub/Desktop/PortfolioWebsite");
    EXPECT_EQ(json->getInt("port"), 443);
    EXPECT_EQ(json->getString("serverName"), "HTTPS");
    EXPECT_EQ(json->getString("certPath"), "/home/ub/Desktop/cert");
    EXPECT_EQ(json->getString("logPath"), "/home/ub/Desktop/logs");
}

TEST(JSONParserTest, ToStringJSON) {
    auto json = getJSONFromFileSafe("JSONParser/https.json");
    ASSERT_NE(json, nullptr);
    
    std::string str = json->toString();
    std::string expected = getFile("JSONParser/https.json");
    
    str.erase(std::remove_if(str.begin(), str.end(), ::isspace), str.end());
    expected.erase(std::remove_if(expected.begin(), expected.end(), ::isspace), expected.end());
    
    EXPECT_EQ(str, expected);
}

TEST(JSONParserTest, NestedJSONFromFile) {
    auto json = getJSONFromFileSafe("JSONParser/nested.json");
    ASSERT_NE(json, nullptr);
    
    std::string str = json->toString();
    std::string expected = getFile("JSONParser/nested.json");
    
    str.erase(std::remove_if(str.begin(), str.end(), ::isspace), str.end());
    expected.erase(std::remove_if(expected.begin(), expected.end(), ::isspace), expected.end());
    
    EXPECT_EQ(str, expected);
}

TEST(JSONParserTest, DoubleNestedJSONFromFile) {
    auto json = getJSONFromFileSafe("JSONParser/doubleNested.json");
    ASSERT_NE(json, nullptr);
    
    std::string str = json->toString();
    std::string expected = getFile("JSONParser/doubleNested.json");
    
    str.erase(std::remove_if(str.begin(), str.end(), ::isspace), str.end());
    expected.erase(std::remove_if(expected.begin(), expected.end(), ::isspace), expected.end());
    
    EXPECT_EQ(str, expected);
}

TEST(JSONParserTest, SaveToFile) {
    auto json = getJSONFromFileSafe("JSONParser/https.json");
    ASSERT_NE(json, nullptr);
    
    saveJSONToFile(*json, "test_https_copy.json");
    
    std::string str1 = getFile("JSONParser/https.json");
    std::string str2 = getFile("JSONParser/https_copy.json");
    
    str1.erase(std::remove_if(str1.begin(), str1.end(), ::isspace), str1.end());
    str2.erase(std::remove_if(str2.begin(), str2.end(), ::isspace), str2.end());
    
    EXPECT_EQ(str1, str2);
}

TEST(JSONParserTest, SetValues) {
    JSONParser json;
    
    json.setString("key1", "value1");
    json.setShort("key2", 42);
    json.setInt("key3", 42);
    json.setLong("key4", 42);
    json.setLongLong("key5", 42);
    json.setFloat("key6", 42.42f);
    json.setDouble("key7", 42.42);
    json.setLongDouble("key8", 42.42);
    json.setBool("key9", true);
    
    JSONParser nested(R"({"nestedKey": "nestedValue"})");
    json.setJSON("key19", nested);
    
    std::vector<std::string> stringArray = {"value1", "value2"};
    std::vector<short> shortArray = {1, 2};
    std::vector<int> intArray = {1, 2};
    std::vector<long> longArray = {1, 2};
    std::vector<long long> longLongArray = {1, 2};
    std::vector<float> floatArray = {1.1f, 2.2f};
    std::vector<double> doubleArray = {1.1, 2.2};
    std::vector<long double> longDoubleArray = {1.1, 2.2};
    std::vector<bool> boolArray = {true, false};
    std::vector<JSONParser> jsonArray = {nested, nested};
    
    json.setStringArray("key10", stringArray);
    json.setShortArray("key11", shortArray);
    json.setIntArray("key12", intArray);
    json.setLongArray("key13", longArray);
    json.setLongLongArray("key14", longLongArray);
    json.setFloatArray("key15", floatArray);
    json.setDoubleArray("key16", doubleArray);
    json.setLongDoubleArray("key17", longDoubleArray);
    json.setBoolArray("key18", boolArray);
    json.setArrayOfJSON("key20", jsonArray);
    
    EXPECT_EQ(json.getString("key1"), "value1");
    EXPECT_EQ(json.getShort("key2"), 42);
    EXPECT_EQ(json.getInt("key3"), 42);
    EXPECT_EQ(json.getLong("key4"), 42);
    EXPECT_EQ(json.getLongLong("key5"), 42);
    EXPECT_FLOAT_EQ(json.getFloat("key6"), 42.42f);
    EXPECT_NEAR(json.getDouble("key7"), 42.42, 0.0001);
    EXPECT_NEAR(static_cast<double>(json.getLongDouble("key8")), 42.42, 0.0001);
    EXPECT_TRUE(json.getBool("key9"));
    EXPECT_EQ(json.getStringArray("key10"), stringArray);
    EXPECT_EQ(json.getShortArray("key11"), shortArray);
    EXPECT_EQ(json.getIntArray("key12"), intArray);
    EXPECT_EQ(json.getLongArray("key13"), longArray);
    EXPECT_EQ(json.getLongLongArray("key14"), longLongArray);
    EXPECT_EQ(json.getFloatArray("key15"), floatArray);
    
    std::vector<double> parsedDoubleArray = json.getDoubleArray("key16");
    ASSERT_EQ(parsedDoubleArray.size(), doubleArray.size());
    for (size_t i = 0; i < doubleArray.size(); ++i) {
        EXPECT_NEAR(parsedDoubleArray[i], doubleArray[i], 0.0001);
    }
    
    std::vector<long double> parsedLongDoubleArray = json.getLongDoubleArray("key17");
    ASSERT_EQ(parsedLongDoubleArray.size(), longDoubleArray.size());
    for (size_t i = 0; i < longDoubleArray.size(); ++i) {
        EXPECT_NEAR(static_cast<double>(parsedLongDoubleArray[i]),
                    static_cast<double>(longDoubleArray[i]), 0.0001);
    }
    
    EXPECT_EQ(json.getBoolArray("key18"), boolArray);
    EXPECT_EQ(json.getObject("key19").getString("nestedKey"), "nestedValue");
    
    std::vector<JSONParser> jsonArray2 = json.getArrayOfJSON("key20");
    EXPECT_EQ(jsonArray2[0].getString("nestedKey"), "nestedValue");
    EXPECT_EQ(jsonArray2[1].getString("nestedKey"), "nestedValue");
}

TEST(JSONParserTest, ToString_InvalidJsonNumbersRemainQuoted) {
    struct Case {
        std::string input;
        std::string expectedSerializedValue;
    };

    const std::vector<Case> cases = {
        // Lone sign / dot (regression cases)
        {".", "\".\""},
        {"-", "\"-\""},

        // Invalid JSON number forms
        {"", "\"\""},
        {"1.", "\"1.\""},
        {".1", "\".1\""},
        {"-.1", "\"-.1\""},
        {"01", "\"01\""},
        {"00", "\"00\""},
        {"1e", "\"1e\""},
        {"1E", "\"1E\""},
        {"1e+", "\"1e+\""},
        {"1e-", "\"1e-\""},
        {"-.", "\"-.\""},
        {"--1", "\"--1\""},
        {"1..0", "\"1..0\""},
        {"1-2", "\"1-2\""},
        {"NaN", "\"NaN\""},
        {"Infinity", "\"Infinity\""},

        // Valid JSON numbers (should be emitted unquoted)
        {"0", "0"},
        {"-0", "-0"},
        {"10", "10"},
        {"10.0", "10.0"},
        {"0.0", "0.0"},
        {"-12.34", "-12.34"},
        {"1e2", "1e2"},
        {"1E2", "1E2"},
        {"1e+2", "1e+2"},
        {"1e-2", "1e-2"},
        {"-1e-2", "-1e-2"},
    };

    for (const auto& tc : cases) {
        JSONParser json;
        json.setString("k", tc.input);
        const std::string out = json.toString();
        EXPECT_EQ(out, std::string("{\"k\":") + tc.expectedSerializedValue + "}") << "input='" << tc.input << "'";
    }
}

TEST(JSONParserTest, JSONArrayFromFile) {
    auto json = getJSONFromFileSafe("JSONParser/jsonArray.json");
    ASSERT_NE(json, nullptr);
    
    std::string str = json->arrayToString();
    std::string expected = getFile("JSONParser/jsonArray.json");
    
    str.erase(std::remove_if(str.begin(), str.end(), ::isspace), str.end());
    expected.erase(std::remove_if(expected.begin(), expected.end(), ::isspace), expected.end());
    
    EXPECT_EQ(str, expected);
}

TEST(JSONParserTest, JSONArrayWithOneElement) {
    auto json = getJSONFromFileSafe("JSONParser/singleJSONArray.json");
    ASSERT_NE(json, nullptr);
    
    std::string str = json->arrayToString();
    std::string expected = getFile("JSONParser/singleJSONArray.json");
    
    str.erase(std::remove_if(str.begin(), str.end(), ::isspace), str.end());
    expected.erase(std::remove_if(expected.begin(), expected.end(), ::isspace), expected.end());
    
    EXPECT_EQ(str, expected);
}

TEST(JSONParserTest, AddingJSONArrayToJSON) {
    JSONParser json;
    
    auto json1 = getJSONFromFileSafe("JSONParser/https.json");
    auto json2 = getJSONFromFileSafe("JSONParser/nested.json");
    ASSERT_NE(json1, nullptr);
    ASSERT_NE(json2, nullptr);
    
    std::vector<JSONParser> jsonArray = {*json1, *json2};
    json.setArrayOfJSON("key", jsonArray);
    
    std::vector<JSONParser> jsonArray2 = json.getArrayOfJSON("key");
    EXPECT_EQ(jsonArray2[0].getString("serverRoot"), "/home/ub/Desktop/PortfolioWebsite");
    EXPECT_EQ(jsonArray2[0].getInt("port"), 443);
    EXPECT_EQ(jsonArray2[0].getString("serverName"), "HTTPS");
    EXPECT_EQ(jsonArray2[0].getString("certPath"), "/home/ub/Desktop/cert");
    EXPECT_EQ(jsonArray2[0].getString("logPath"), "/home/ub/Desktop/logs");
    
    // nested.json has {"test1": {"Nested": "NestedValue"}}
    JSONParser nested = jsonArray2[1].getObject("test1");
    EXPECT_EQ(nested.getString("Nested"), "NestedValue");
}

TEST(JSONParserTest, AddingJSONArrayToJSONArray) {
    auto json = getJSONFromFileSafe("JSONParser/devices.json");
    auto jsonToAdd = getJSONFromFileSafe("JSONParser/arrayToAdd.json");
    ASSERT_NE(json, nullptr);
    ASSERT_NE(jsonToAdd, nullptr);
    
    std::vector<JSONParser> jsonArray = json->getJSONArray();
    for (auto &device: jsonArray) {
        device.addArrayOfJSON("array", jsonToAdd->getJSONArray());
    }
    json->setJSONArray(jsonArray);
    
    std::string str = json->arrayToString();
    std::string expected = getFile("JSONParser/devicesWithArray.json");
    
    str.erase(std::remove_if(str.begin(), str.end(), ::isspace), str.end());
    expected.erase(std::remove_if(expected.begin(), expected.end(), ::isspace), expected.end());
    
    EXPECT_EQ(str, expected);
}

TEST(JSONParserTest, FunctionsFile) {
    auto json = getJSONFromFileSafe("JSONParser/functions.json");
    ASSERT_NE(json, nullptr);
    
    std::string str = json->toString();
    std::string expected = getFile("JSONParser/functions.json");
    
    str.erase(std::remove_if(str.begin(), str.end(), ::isspace), str.end());
    expected.erase(std::remove_if(expected.begin(), expected.end(), ::isspace), expected.end());
    
    EXPECT_EQ(str, expected);
}

TEST(JSONParserTest, AddToArrayAndArrayToString) {
    JSONParser json;
    std::vector<JSONParser> initialArray;
    
    JSONParser element1;
    element1.setString("name", "Initial Item");
    element1.setInt("id", 1);
    initialArray.push_back(element1);
    json.setJSONArray(initialArray);
    
    JSONParser element2;
    element2.setString("name", "Added Item 1");
    element2.setInt("id", 2);
    json.addJSONToArray(element2);
    
    JSONParser element3;
    element3.setString("name", "Added Item 2");
    element3.setInt("id", 3);
    element3.setBool("active", true);
    json.addJSONToArray(element3);
    
    std::string result = json.arrayToString();
    std::string expected = R"([{"name":"Initial Item","id":1},{"name":"Added Item 1","id":2},{"name":"Added Item 2","id":3,"active":true}])";
    
    result.erase(std::remove_if(result.begin(), result.end(), ::isspace), result.end());
    expected.erase(std::remove_if(expected.begin(), expected.end(), ::isspace), expected.end());
    
    EXPECT_EQ(result, expected);
    
    std::vector<JSONParser> retrievedArray = json.getJSONArray();
    ASSERT_EQ(retrievedArray.size(), 3);
    EXPECT_EQ(retrievedArray[0].getString("name"), "Initial Item");
    EXPECT_EQ(retrievedArray[0].getInt("id"), 1);
    EXPECT_EQ(retrievedArray[1].getString("name"), "Added Item 1");
    EXPECT_EQ(retrievedArray[1].getInt("id"), 2);
    EXPECT_EQ(retrievedArray[2].getString("name"), "Added Item 2");
    EXPECT_EQ(retrievedArray[2].getInt("id"), 3);
    EXPECT_TRUE(retrievedArray[2].getBool("active"));
}

TEST(JSONParserTest, AddToEmptyArrayAndArrayToString) {
    JSONParser json;
    std::vector<JSONParser> emptyArray;
    json.setJSONArray(emptyArray);
    
    JSONParser item1;
    item1.setString("test", "value");
    json.addJSONToArray(item1);
    
    std::string result = json.arrayToString();
    EXPECT_FALSE(result.empty());
    EXPECT_NE(result.find("test"), std::string::npos);
    EXPECT_NE(result.find("value"), std::string::npos);
}
