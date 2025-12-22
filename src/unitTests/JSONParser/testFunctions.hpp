//
// Created by ursbe on 21.09.2024.
//

#ifndef GERUEST_TEST_FUNCTIONS_HPP
#define GERUEST_TEST_FUNCTIONS_HPP

#include <vector>
#include <string>
#include <cassert>
#include <fstream>
#include <sstream>
#include "../../parser/JSONParser.hpp" // Your JSON parser header

using namespace geruest;

void test_simple_key_strings() {
    std::string input = R"({"key": "value"})";
    JSONParser json(input);

    assert(json.getString("key") == "value");
}

void test_simple_key_integers1() {
    std::string input = R"({"key": 42})";
    JSONParser json(input);

    assert(json.getShort("key") == 42);
}

void test_simple_key_integers2() {
    std::string input = R"({"key": 42})";
    JSONParser json(input);

    assert(json.getInt("key") == 42);
}

void test_simple_key_integers3() {
    std::string input = R"({"key": 42})";
    JSONParser json(input);

    assert(json.getLong("key") == 42);
}

void test_simple_key_integers4() {
    std::string input = R"({"key": 42})";
    JSONParser json(input);

    assert(json.getLongLong("key") == 42);
}

void test_simple_key_floats1() {
    std::string input = R"({"key": 42.42})";
    JSONParser json(input);

    assert(json.getFloat("key") == 42.42f);
}

void test_simple_key_floats2() {
    std::string input = R"({"key": 42.42})";
    JSONParser json(input);

    float f = 42.42;
    double d = f;

    assert(json.getDouble("key") == d);
}

void test_simple_key_floats3() {
    std::string input = R"({"key": 42.42})";
    JSONParser json(input);

    float f = 42.42;
    long double ld = f;

    assert(json.getLongDouble("key") == ld);
}

void test_simple_key_booleans() {
    std::string input = R"({"key": true})";
    JSONParser json(input);

    assert(json.getBool("key") == true);
}

void test_simple_key_null() {
    std::string input = R"({"key": null})";
    JSONParser json(input);

    assert(json.getString("key") .empty());
}

void test_multiple_key_value_pairs() {
    std::string input = R"({"key1": "value1", "key2": "value2"})";
    JSONParser json(input);

    assert(json.getString("key1") == "value1");
    assert(json.getString("key2") == "value2");
}

void test_nested_objects() {
    std::string input = R"({"key": {"nestedKey": "nestedValue"}})";
    JSONParser json(input);

    assert(json.getObject("key").getString("nestedKey") == "nestedValue");
}

void test_array_of_values_strings() {
    std::string input = R"({"key": ["value1", "value2"]})";
    JSONParser json(input);
    std::vector<std::string> array = {std::string("value1"), std::string("value2")};

    assert(json.getStringArray("key") == array);
}

void test_array_of_values_shorts() {
    std::string input = R"({"key": [1, 2]})";
    JSONParser json(input);
    std::vector<short> array = {1, 2};

    assert(json.getShortArray("key") == array);
}

void test_array_of_values_integers() {
    std::string input = R"({"key": [1, 2]})";
    JSONParser json(input);
    std::vector<int> array = {1, 2};

    assert(json.getIntArray("key") == array);
}

void test_array_of_values_longs() {
    std::string input = R"({"key": [1, 2]})";
    JSONParser json(input);
    std::vector<long> array = {1, 2};

    assert(json.getLongArray("key") == array);
}

void test_array_of_values_longlongs() {
    std::string input = R"({"key": [1, 2]})";
    JSONParser json(input);
    std::vector<long long> array = {1, 2};

    assert(json.getLongLongArray("key") == array);
}

void test_array_of_values_floats() {
    std::string input = R"({"key": [1.1, 2.2]})";
    JSONParser json(input);
    std::vector<float> array = {1.1f, 2.2f};

    assert(json.getFloatArray("key") == array);
}

void test_array_of_values_doubles() {
    std::string input = R"({"key": [1.1, 2.2]})";
    JSONParser json(input);

    float f1 = 1.1;
    float f2 = 2.2;

    std::vector<double> array = {f1, f2};

    assert(json.getDoubleArray("key") == array);
}

void test_array_of_values_longdoubles() {
    std::string input = R"({"key": [1.1, 2.2]})";
    JSONParser json(input);

    float f1 = 1.1;
    float f2 = 2.2;

    std::vector<long double> array = {f1, f2};

    assert(json.getLongDoubleArray("key") == array);
}

void test_array_of_values_booleans() {
    std::string input = R"({"key": [true, false]})";
    JSONParser json(input);
    std::vector<bool> array = {true, false};

    assert(json.getBoolArray("key") == array);
}

void test_array_of_values_null() {
    std::string input = R"({"key": [null, null]})";
    JSONParser json(input);
    std::vector<std::string> array = {std::string(), std::string()};

    assert(json.getStringArray("key") == array);
}

void test_mixed_data_types() {
    std::string input = R"({"stringKey": "stringValue", "intKey": 42, "boolKey": true})";
    JSONParser json(input);

    assert(json.getInt("intKey") == 42);
    assert(json.getBool("boolKey") == true);
    assert(json.getString("stringKey") == "stringValue");
}

void test_empty_object() {
    std::string input = R"({})";
    JSONParser json(input);

    assert(json.getString("key").empty());
}

void test_invalid_json() {
    try {
        std::string input = R"({"key": "value")";
        JSONParser json(input);

        assert(false); // Should not reach here
    } catch (const std::invalid_argument &e) {

        assert(true); // Expected exception
    }
}

void test_boolean_values() {
    std::string input = R"({"trueKey": true, "falseKey": false})";
    JSONParser json(input);

    assert(json.getBool("trueKey") == true);
    assert(json.getBool("falseKey") == false);
}

void test_large_json_object() {
    std::string input = R"({
        "key1": "value1",
        "key2": {"nestedKey1": "nestedValue1", "nestedKey2": "nestedValue2"},
        "key3": ["arrayValue1", "arrayValue2"],
        "key4": 12345,
        "key5": false
    })";

    JSONParser json(input);

    std::vector<std::string> array = {"arrayValue1", "arrayValue2"};

    assert(json.getString("key1") == "value1");
    assert(json.getObject("key2").getString("nestedKey1") == "nestedValue1");
    assert(json.getObject("key2").getString("nestedKey2") == "nestedValue2");
    assert(json.getStringArray("key3") == array);
    assert(json.getInt("key4") == 12345);
    assert(json.getBool("key5") == false);
}

// Test with the https.json file
void test_json_from_file() {
    JSONParser* json = getJSONFromFile("JSONParser/https.json");

    assert(json->getString("serverRoot") == "/home/ub/Desktop/PortfolioWebsite");
    assert(json->getInt("port") == 443);
    assert(json->getString("serverName") == "HTTPS");
    assert(json->getString("certPath") == "/home/ub/Desktop/cert");
    assert(json->getString("logPath") == "/home/ub/Desktop/logs");
    
    delete json;
}

inline std::string getFile(const std::string &filePath) {
    std::ifstream file(filePath);
    if (!file.is_open()) {
        throw std::runtime_error("Could not open file");
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();

    return content;
}

// Test with the http.json file
void test_toString_json() {
    JSONParser* json = getJSONFromFile("JSONParser/https.json");

    std::string str = json->toString();
    std::string expected = getFile("JSONParser/https.json");

    // Remove all whitespaces
    str.erase(std::remove_if(str.begin(), str.end(), ::isspace), str.end());
    expected.erase(std::remove_if(expected.begin(), expected.end(), ::isspace), expected.end());

    assert(str == expected);
    
    delete json;
}

// test with nested json file
void test_nested_json_from_file() {
    JSONParser* json = geruest::getJSONFromFile("JSONParser/nested.json");

    std::string str = json->toString();
    std::string expected = getFile("JSONParser/nested.json");

#ifdef DEBUG
    std::cout << "Expected: " << expected << std::endl;
    std::cout << "Got:      " << str << std::endl;
#endif

    // Remove all whitespaces
    str.erase(std::remove_if(str.begin(), str.end(), ::isspace), str.end());
    expected.erase(std::remove_if(expected.begin(), expected.end(), ::isspace), expected.end());

    assert(str == expected);
    
    delete json;
}

// test with double nested json file
void test_double_nested_json_from_file() {
    JSONParser* json = geruest::getJSONFromFile("JSONParser/doubleNested.json");

    std::string str = json->toString();
    std::string expected = getFile("JSONParser/doubleNested.json");

    // Remove all whitespaces
    str.erase(std::remove_if(str.begin(), str.end(), ::isspace), str.end());
    expected.erase(std::remove_if(expected.begin(), expected.end(), ::isspace), expected.end());

    assert(str == expected);
    
    delete json;
}

// Test save to file
void test_save_to_file() {
    JSONParser* json = getJSONFromFile("JSONParser/https.json");

    geruest::saveJSONToFile(*json, "test_https_copy.json");

    std::string str1 = getFile("JSONParser/https.json");
    std::string str2 = getFile("JSONParser/https_copy.json");

    // Remove all whitespaces
    str1.erase(std::remove_if(str1.begin(), str1.end(), ::isspace), str1.end());
    str2.erase(std::remove_if(str2.begin(), str2.end(), ::isspace), str2.end());

    assert(str1 == str2);
    
    delete json;
}

// Test setting values
void test_set_values() {
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

    JSONParser nested = JSONParser(R"({"nestedKey": "nestedValue"})");

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

    assert(json.getString("key1") == "value1");
    assert(json.getShort("key2") == 42);
    assert(json.getInt("key3") == 42);
    assert(json.getLong("key4") == 42);
    assert(json.getLongLong("key5") == 42);
    assert(json.getFloat("key6") == 42.42f);
    assert(json.getDouble("key7") == 42.42);
    assert(json.getLongDouble("key8") == 42.42);
    assert(json.getBool("key9") == true);
    assert(json.getStringArray("key10") == stringArray);
    assert(json.getShortArray("key11") == shortArray);
    assert(json.getIntArray("key12") == intArray);
    assert(json.getLongArray("key13") == longArray);
    assert(json.getLongLongArray("key14") == longLongArray);
    assert(json.getFloatArray("key15") == floatArray);
    assert(json.getDoubleArray("key16") == doubleArray);
    assert(json.getLongDoubleArray("key17") == longDoubleArray);
    assert(json.getBoolArray("key18") == boolArray);
    assert(json.getObject("key19").getString("nestedKey") == "nestedValue");

    std::vector<JSONParser> jsonArray2 = json.getArrayOfJSON("key20");
    assert(jsonArray2[0].getString("nestedKey") == "nestedValue");
    assert(jsonArray2[1].getString("nestedKey") == "nestedValue");
}

// Test json array from file
void test_json_array_from_file() {
    JSONParser* json = geruest::getJSONFromFile("JSONParser/jsonArray.json");

    std::string str = json->arrayToString();
    std::string expected = getFile("JSONParser/jsonArray.json");

    // Remove all whitespaces
    str.erase(std::remove_if(str.begin(), str.end(), ::isspace), str.end());
    expected.erase(std::remove_if(expected.begin(), expected.end(), ::isspace), expected.end());

    assert(str == expected);
    
    delete json;
}

// Test json array with only one element
void test_json_array_with_one_element() {
    JSONParser* json = geruest::getJSONFromFile("JSONParser/singleJSONArray.json");

    std::string str = json->arrayToString();
    std::string expected = getFile("JSONParser/singleJSONArray.json");

    // Remove all whitespaces
    str.erase(std::remove_if(str.begin(), str.end(), ::isspace), str.end());
    expected.erase(std::remove_if(expected.begin(), expected.end(), ::isspace), expected.end());

    assert(str == expected);
    
    delete json;
}

// Test adding an array of json to a json object
void test_adding_json_array_to_json() {
    JSONParser json;

    JSONParser* json1 = geruest::getJSONFromFile("JSONParser/https.json");
    JSONParser* json2 = geruest::getJSONFromFile("JSONParser/nested.json");

    std::vector<JSONParser> jsonArray = {*json1, *json2};

    json.setArrayOfJSON("key", jsonArray);

    std::vector<JSONParser> jsonArray2 = json.getArrayOfJSON("key");

    assert(jsonArray2[0].getString("serverRoot") == "/home/ub/Desktop/PortfolioWebsite");
    assert(jsonArray2[0].getInt("port") == 443);
    assert(jsonArray2[0].getString("serverName") == "HTTPS");
    assert(jsonArray2[0].getString("certPath") == "/home/ub/Desktop/cert");
    assert(jsonArray2[0].getString("logPath") == "/home/ub/Desktop/logs");

    assert(jsonArray2[1].getString("key") == "value");
    
    delete json1;
    delete json2;
}

// adding a json array to a json object in a json array
void test_adding_json_array_to_json_array() {
    JSONParser* json = geruest::getJSONFromFile("JSONParser/devices.json");
    JSONParser* jsonToAdd = geruest::getJSONFromFile("JSONParser/arrayToAdd.json");

    std::vector<JSONParser> jsonArray = json->getJSONArray();

    for (auto &device: jsonArray) {

        device.addArrayOfJSON("array", jsonToAdd->getJSONArray());
    }

    json->setJSONArray(jsonArray);

    std::string str = json->arrayToString();
    std::string expected = getFile("JSONParser/devicesWithArray.json");

    // Remove all whitespaces
    str.erase(std::remove_if(str.begin(), str.end(), ::isspace), str.end());
    expected.erase(std::remove_if(expected.begin(), expected.end(), ::isspace), expected.end());

    assert(str == expected);
    
    delete json;
    delete jsonToAdd;
}

void test_functions_file(){
    JSONParser* json = geruest::getJSONFromFile("JSONParser/functions.json");

    std::string str = json->toString();
    std::string expected = getFile("JSONParser/functions.json");

    // Remove all whitespaces
    str.erase(std::remove_if(str.begin(), str.end(), ::isspace), str.end());
    expected.erase(std::remove_if(expected.begin(), expected.end(), ::isspace), expected.end());

    assert(str == expected);
    
    delete json;
}

// Test adding elements to a JSON array and using arrayToString
void test_add_to_array_and_array_to_string() {
    // Start with an initial JSON array
    JSONParser json;
    std::vector<JSONParser> initialArray;
    
    // Create first element
    JSONParser element1;
    element1.setString("name", "Initial Item");
    element1.setInt("id", 1);
    initialArray.push_back(element1);
    
    // Set the initial array
    json.setJSONArray(initialArray);
    
    // Add more elements using addJSONToArray
    JSONParser element2;
    element2.setString("name", "Added Item 1");
    element2.setInt("id", 2);
    json.addJSONToArray(element2);
    
    JSONParser element3;
    element3.setString("name", "Added Item 2");
    element3.setInt("id", 3);
    element3.setBool("active", true);
    json.addJSONToArray(element3);
    
    // Convert back to string using arrayToString
    std::string result = json.arrayToString();
    
    // Expected JSON structure (whitespace will be removed for comparison)
    std::string expected = R"([
        {
            "name": "Initial Item",
            "id": 1
        },
        {
            "name": "Added Item 1",
            "id": 2
        },
        {
            "name": "Added Item 2",
            "id": 3,
            "active": true
        }
    ])";
    
    // Remove all whitespaces for comparison
    result.erase(std::remove_if(result.begin(), result.end(), ::isspace), result.end());
    expected.erase(std::remove_if(expected.begin(), expected.end(), ::isspace), expected.end());
    
    assert(result == expected);
    
    // Also test that we can retrieve the array properly
    std::vector<JSONParser> retrievedArray = json.getJSONArray();
    assert(retrievedArray.size() == 3);
    assert(retrievedArray[0].getString("name") == "Initial Item");
    assert(retrievedArray[0].getInt("id") == 1);
    assert(retrievedArray[1].getString("name") == "Added Item 1");
    assert(retrievedArray[1].getInt("id") == 2);
    assert(retrievedArray[2].getString("name") == "Added Item 2");
    assert(retrievedArray[2].getInt("id") == 3);
    assert(retrievedArray[2].getBool("active") == true);
}

// Test the specific problem scenario: add to empty array then arrayToString
void test_add_to_empty_array_and_array_to_string() {
    // Start with completely empty JSONParser
    JSONParser json;
    
    // Initialize as empty array
    std::vector<JSONParser> emptyArray;
    json.setJSONArray(emptyArray);
    
    // Add a simple element
    JSONParser item1;
    item1.setString("test", "value");
    json.addJSONToArray(item1);
    
    // The critical test: arrayToString should work correctly after adding
    std::string result = json.arrayToString();
    
    // Basic verification
    assert(!result.empty());
    assert(result.find("test") != std::string::npos);
    assert(result.find("value") != std::string::npos);
}

#endif // GERUEST_TEST_FUNCTIONS_HPP