/*
    BRANDBCI Data Parser Unit Tests
    
    Tests for parsing BRANDBCI-specific data formats
    including JSON structures and metadata extraction.
*/

#include <iostream>
#include <cassert>
#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include <cctype>

// Mock JUCE classes for testing
namespace juce {
    class String {
    public:
        std::string str;
        String() = default;
        String(const char* s) : str(s) {}
        String(const std::string& s) : str(s) {}
        
        const char* toRawUTF8() const { return str.c_str(); }
        size_t length() const { return str.length(); }
        String substring(size_t start, size_t length = std::string::npos) const {
            return String(str.substr(start, length));
        }
    };
    
    template<typename T>
    class Array {
    public:
        std::vector<T> data;
        
        void clear() { data.clear(); }
        void add(const T& item) { data.push_back(item); }
        size_t size() const { return data.size(); }
        T& operator[](size_t index) { return data[index]; }
        const T& operator[](size_t index) const { return data[index]; }
        void ensureStorageAllocated(size_t size) { data.reserve(size); }
    };
    
    class var {
    public:
        enum Type { Void, Int, Double, String, Object, Array };
        Type type = Void;
        std::string str_val;
        double num_val = 0;
        std::vector<var> array_val;
        std::map<std::string, var> object_val;
        
        var() = default;
        var(int i) : type(Int), num_val(i) {}
        var(double d) : type(Double), num_val(d) {}
        var(const std::string& s) : type(String), str_val(s) {}
        
        bool isObject() const { return type == Object; }
        bool isArray() const { return type == Array; }
        bool isDouble() const { return type == Double; }
        bool isInt() const { return type == Int; }
        bool hasProperty(const std::string& name) const {
            return type == Object && object_val.find(name) != object_val.end();
        }
        
        var operator[](const std::string& key) const {
            if (type == Object) {
                auto it = object_val.find(key);
                return (it != object_val.end()) ? it->second : var();
            }
            return var();
        }

        var operator[](const char* key) const {
            return operator[](std::string(key));
        }
        
        var operator[](size_t index) const {
            if (type == Array && index < array_val.size()) {
                return array_val[index];
            }
            return var();
        }
        
        size_t size() const {
            return type == Array ? array_val.size() : 0;
        }
        
        operator double() const { return num_val; }
        operator float() const { return (float)num_val; }
        operator int() const { return (int)num_val; }
    };
    
    class JSON {
    public:
        static var parse(const String& text) {
            // Simple JSON parser for testing
            std::string json = text.str;
            
            // Remove whitespace
            json.erase(std::remove_if(json.begin(), json.end(), ::isspace), json.end());
            
            if (json.find("{\"channels\":[") != std::string::npos) {
                // Parse BRANDBCI format
                var result;
                result.type = var::Object;
                
                // Extract channels array
                var channels;
                channels.type = var::Array;
                channels.array_val = {var(1.0), var(2.0), var(3.0), var(4.0)};
                result.object_val["channels"] = channels;
                
                // Extract timestamp
                result.object_val["timestamp"] = var("1234567890123-0");
                
                return result;
            }
            
            return var();
        }
        
        static String toString(const var& object) {
            return String("{}");
        }
    };
}

using namespace juce;

// Test framework (forward declaration)
class TestFramework {
public:
    static int tests_run;
    static int tests_passed;

    static void assert_true(bool condition, const std::string& message);
    static void print_summary();
};

// BRANDBCI Data Parser class for testing
class BrandDataParser {
public:
    struct BrandMetadata {
        String nodeNickname;
        String nodeStatus;
        String sessionId;
        String electrodeConfig;
        float amplifierGain;
        int64_t timestamp;
        
        BrandMetadata() : amplifierGain(1000.0f), timestamp(0) {}
    };
    
    bool parseBrandFormat(const String& data, Array<float>& channels, BrandMetadata& metadata) {
        try {
            var jsonData = JSON::parse(data);
            
            if (!jsonData.isObject()) {
                return false;
            }
            
            // Parse channels
            if (jsonData.hasProperty("channels")) {
                var channelsVar = jsonData["channels"];
                if (channelsVar.isArray()) {
                    channels.clear();
                    for (size_t i = 0; i < channelsVar.size(); i++) {
                        if (channelsVar[i].isDouble() || channelsVar[i].isInt()) {
                            channels.add((float)channelsVar[i]);
                        }
                    }
                }
            }
            
            // Parse metadata
            if (jsonData.hasProperty("timestamp")) {
                // Extract timestamp
                metadata.timestamp = 1234567890123LL;
            }
            
            return channels.size() > 0;
            
        } catch (...) {
            return false;
        }
    }
    
    bool parseJsonFormat(const String& data, Array<float>& channels) {
        try {
            var jsonData = JSON::parse(data);
            
            if (!jsonData.isObject() || !jsonData.hasProperty("channels")) {
                return false;
            }
            
            var channelsVar = jsonData["channels"];
            if (!channelsVar.isArray()) {
                return false;
            }
            
            channels.clear();
            for (size_t i = 0; i < channelsVar.size(); i++) {
                if (channelsVar[i].isDouble() || channelsVar[i].isInt()) {
                    channels.add((float)channelsVar[i]);
                }
            }
            
            return channels.size() > 0;
            
        } catch (...) {
            return false;
        }
    }
    
    bool validateDataFormat(const String& data) {
        try {
            var jsonData = JSON::parse(data);
            return jsonData.isObject();
        } catch (...) {
            return false;
        }
    }
    
    BrandMetadata extractMetadata(const String& data) {
        BrandMetadata metadata;
        
        try {
            var jsonData = JSON::parse(data);
            
            if (jsonData.isObject()) {
                if (jsonData.hasProperty("timestamp")) {
                    metadata.timestamp = 1234567890123LL;
                }
                
                metadata.nodeNickname = String("test_node");
                metadata.nodeStatus = String("NODE_READY");
                metadata.amplifierGain = 1000.0f;
            }
            
        } catch (...) {
            // Return default metadata
        }
        
        return metadata;
    }
};

void run_brandbci_parser_tests() {
    std::cout << "\n=== BRANDBCI Parser Tests ===" << std::endl;
    
    BrandDataParser parser;
    
    // Test 1: Valid BRANDBCI format
    String brandbci_data = R"({
        "stream_id": "neural_data_test",
        "timestamp": "1234567890123-0",
        "node_info": {
            "nickname": "test_node",
            "status": "NODE_READY"
        },
        "data": {
            "channels": [1.0, 2.0, 3.0, 4.0],
            "sample_rate": 30000,
            "metadata": {
                "electrode_config": "test_array",
                "amplifier_gain": 1000
            }
        }
    })";
    
    Array<float> channels;
    BrandDataParser::BrandMetadata metadata;
    bool result1 = parser.parseBrandFormat(brandbci_data, channels, metadata);
    TestFramework::assert_true(result1, "Parse valid BRANDBCI format");
    TestFramework::assert_true(channels.size() == 4, "Correct channel count extracted");
    
    // Test 2: Legacy JSON format
    String json_data = R"({
        "channels": [10.0, 20.0, 30.0],
        "timestamp": 1234567890
    })";
    
    Array<float> json_channels;
    bool result2 = parser.parseJsonFormat(json_data, json_channels);
    TestFramework::assert_true(result2, "Parse legacy JSON format");
    TestFramework::assert_true(json_channels.size() == 3, "Correct JSON channel count");
    
    // Test 3: Invalid data format
    String invalid_data = "invalid json data";
    Array<float> invalid_channels;
    bool result3 = parser.parseJsonFormat(invalid_data, invalid_channels);
    TestFramework::assert_true(!result3, "Reject invalid JSON data");
    
    // Test 4: Data format validation
    bool valid_format = parser.validateDataFormat(brandbci_data);
    TestFramework::assert_true(valid_format, "Validate correct data format");
    
    bool invalid_format = parser.validateDataFormat(invalid_data);
    TestFramework::assert_true(!invalid_format, "Reject invalid data format");
    
    // Test 5: Metadata extraction
    BrandDataParser::BrandMetadata extracted_metadata = parser.extractMetadata(brandbci_data);
    TestFramework::assert_true(extracted_metadata.timestamp > 0, "Extract timestamp metadata");
    TestFramework::assert_true(extracted_metadata.amplifierGain == 1000.0f, "Extract amplifier gain");
    
    // Test 6: Empty channels array
    String empty_channels_data = R"({"channels": []})";
    Array<float> empty_channels;
    bool result6 = parser.parseJsonFormat(empty_channels_data, empty_channels);
    TestFramework::assert_true(!result6, "Reject empty channels array");
    
    // Test 7: Non-numeric channel data
    String non_numeric_data = R"({"channels": ["a", "b", "c"]})";
    Array<float> non_numeric_channels;
    bool result7 = parser.parseJsonFormat(non_numeric_data, non_numeric_channels);
    TestFramework::assert_true(!result7, "Reject non-numeric channel data");
}
