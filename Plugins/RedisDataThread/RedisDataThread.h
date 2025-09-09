/*
    ------------------------------------------------------------------

    This file is part of the Open Ephys GUI
    Copyright (C) 2024 Open Ephys

    ------------------------------------------------------------------

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

*/

#ifndef __REDISDATATHREAD_H_INCLUDED__
#define __REDISDATATHREAD_H_INCLUDED__

#include <DataThreadHeaders.h>

#ifdef REDIS_ENABLED
#include <hiredis/hiredis.h>
#else
// Forward declaration to allow pointer usage without hiredis
struct redisReply;
#endif

#include <atomic>
#include <memory>

class SourceNode;

/**
 * Redis DataThread for streaming data from Redis server to OpenEphys
 * 
 * This plugin connects to a Redis server and subscribes to channels
 * to receive real-time data streams. Supports JSON and binary data formats.
 */
class TESTABLE RedisDataThread : public DataThread
{
public:
    /** Constructor */
    RedisDataThread(SourceNode* sn);

    /** Destructor */
    ~RedisDataThread();

    /** Creates the custom editor for this DataThread */
    std::unique_ptr<GenericEditor> createEditor(SourceNode* sn) override;

    /** Initializes data transfer */
    bool startAcquisition() override;

    /** Stops data transfer */
    bool stopAcquisition() override;

    /** Returns true if the data source is connected and ready */
    bool foundInputSource() override;

    /** Returns true if the data source is connected and ready */
    bool isReady() override;

    /** Updates buffer with new data from Redis */
    bool updateBuffer() override;

    /** Called when the chain updates */
    void updateSettings(OwnedArray<ContinuousChannel>* continuousChannels,
                       OwnedArray<EventChannel>* eventChannels,
                       OwnedArray<SpikeChannel>* spikeChannels,
                       OwnedArray<DataStream>* sourceStreams,
                       OwnedArray<DeviceInfo>* devices,
                       OwnedArray<ConfigurationObject>* configurationObjects) override;

    /** Called when the chain updates, to resize the sourceBuffers */
    void resizeBuffers() override;

    /** Save configuration to XML element */
    void saveConfigurationToXml(XmlElement* parentElement);

    /** Load configuration from XML element */
    void loadConfigurationFromXml(XmlElement* customParamsXml);

    /** Redis connection management */
    bool connectToRedis(const String& host, int port, const String& password = "");
    void disconnectFromRedis();
    bool isConnected() const;

    /** Data retrieval methods */
    Array<String> getLatestRecords(int numRecords = 10);

    /** Get available channels/streams from Redis */
    Array<String> getAvailableChannels();

    /** Field discovery structures and methods */
    enum class FieldDataType
    {
        SCALAR,
        ARRAY_1D,
        ARRAY_2D,
        UNKNOWN
    };

    enum class Array2DProcessing
    {
        FIRST_ROW,
        SUM,
        MEAN
    };

    struct FieldInfo
    {
        String fieldName;
        FieldDataType dataType;
        Array<int> dimensions;  // [rows, cols] for 2D, [length] for 1D
        String sampleData;      // Preview of the data
        bool isSuitableForNeural; // Whether this field contains neural data

        FieldInfo() : dataType(FieldDataType::UNKNOWN), isSuitableForNeural(false) {}

        String getDisplayName() const
        {
            String result = fieldName;
            if (dataType == FieldDataType::ARRAY_1D && dimensions.size() >= 1)
            {
                result += " (1D array, " + String(dimensions[0]) + " channels)";
            }
            else if (dataType == FieldDataType::ARRAY_2D && dimensions.size() >= 2)
            {
                result += " (2D array, " + String(dimensions[0]) + "x" + String(dimensions[1]) + ")";
            }
            else if (dataType == FieldDataType::SCALAR)
            {
                result += " (scalar - not suitable)";
            }
            return result;
        }
    };

    /** Discover available data fields from Redis */
    Array<FieldInfo> discoverDataFields();

    /** Get sample data for field analysis */
    String getSampleDataFromRedis(int numSamples = 3);

    /** Analyze field structure from sample data */
    Array<FieldInfo> analyzeFieldStructure(const String& sampleData);

    /** Set selected data field for processing */
    void setSelectedDataField(const String& fieldName);
    String getSelectedDataField() const { return selectedDataField; }

    /** Set 2D array processing method */
    void setArray2DProcessing(Array2DProcessing method);
    Array2DProcessing getArray2DProcessing() const { return array2DProcessing; }

    /** Configuration setters */
    void setRedisHost(const String& host);
    void setRedisPort(int port);
    void setRedisPassword(const String& password);
    void setRedisChannel(const String& channel);
    void setSampleRate(float sampleRate);
    void setNumChannels(int numChannels);
    void setDataFormat(const String& format);
    void setBufferSize(int size);

    /** Stream support methods */
    void setStreamMode(bool useStreams);
    void setAlwaysReadLatest(bool alwaysLatest);

    /** Configuration getters */
    String getRedisHost() const { return redisHost; }
    int getRedisPort() const { return redisPort; }
    String getRedisPassword() const { return redisPassword; }
    String getRedisChannelName() const { return redisChannelName; }  // Updated name
    float getSampleRate() const { return sampleRate; }
    int getNumDataChannels() const { return numDataChannels; }       // Updated name
    int getMaxDataChannels() const { return maxDataChannels; }
    String getDataFormat() const { return dataFormat; }
    String getConnectionStatus() const;
    bool isAutoDetectChannelsEnabled() const { return autoDetectChannels; }
    int getBufferSize() const { return bufferSize; }
    bool getStreamMode() const { return useStreamMode; }
    bool getAlwaysReadLatest() const { return alwaysReadLatest; }

    /** Configuration validation */
    bool validateConfiguration() const;
    bool validateChannelConfiguration(int channels) const;
    bool validateBufferSize(int size) const;

private:
#ifdef REDIS_ENABLED
    redisContext* redisCtx;
#else
    void* redisCtx; // placeholder when Redis is not available
#endif

    // Connection parameters
    String redisHost;
    int redisPort;
    String redisPassword;
    String redisChannelName;  // Renamed for clarity: Redis channel/topic name

    // Data configuration
    float sampleRate;
    int numDataChannels;      // Renamed for clarity: number of data channels
    int maxDataChannels;      // Maximum supported channels
    String dataFormat;        // "json", "binary", "brandbci"
    bool autoDetectChannels;  // Auto-detect channel count from data
    int bufferSize;           // Configurable buffer size in samples

    // Stream configuration
    bool useStreamMode;
    String currentStreamId;        // Last read stream ID for XREAD
    bool alwaysReadLatest;         // If true, always read from latest data ($) instead of sequential

    // Field discovery configuration
    String selectedDataField;     // Selected field name for data extraction
    Array2DProcessing array2DProcessing; // How to process 2D arrays

    // State management
    std::atomic<bool> isAcquiring;
    std::atomic<bool> connectionStatus;

    // Sample counting
    std::atomic<int64> currentSampleNumber;

    // Open Ephys stream data structure
    struct OpenEphysStreamData {
        int run = 0;
        double timestamp = 0.0;
        int sample_rate = 0;
        int n_channels = 0;
        int n_samples = 0;
        const char* binary_data = nullptr;
        size_t data_length = 0;
        String data_shape;
        String data_dtype;

        bool isValid() const {
            return n_channels > 0 && n_samples > 0 &&
                   sample_rate > 0 && binary_data != nullptr &&
                   data_length > 0 && !data_shape.isEmpty() &&
                   !data_dtype.isEmpty();
        }
    };

    // Validation flag (kept)
    bool enableDataValidation;

    // Data type configuration
    String dataType;  // Default data type for parsing (float32, int16, etc.)

public:
    // Configuration getters and setters
    void setDataValidationEnabled(bool enabled) { enableDataValidation = enabled; }
    bool isDataValidationEnabled() const { return enableDataValidation; }

    // Data type configuration
    void setDataType(const String& dtype) { dataType = dtype; }
    String getDataType() const { return dataType; }

private:

    // Neural binary parsing
    bool parseBinarySpikeRates(redisReply* fieldReply, Array<float>& channelData);
    // Legacy JSON/BRANDBCI parsing (stub, returns false)
    bool parseBrandBCIData(const String& jsonStr, Array<float>& channelData);

    // Stream methods
    bool updateBufferFromStream();
    bool updateBufferFromList(); // Legacy BLPOP method (now disabled)
    bool processStreamEntry(redisReply* fieldsReply);
    bool processLegacyStreamEntry(redisReply* fieldsReply);

    // Error handling
    void handleRedisError(const String& operation);
    bool attemptReconnection();

    // Removed Open Ephys / JSON helper declarations in hard lock cleanup
    Array<int> parseDataShape(const String& shapeStr);
    size_t calculateExpectedDataSize(const String& dtype, const Array<int>& shape);
    bool decodeBinaryData(const char* data, size_t length, const String& dtype,
                          const Array<int>& shape, Array<float>& output);
    bool decodeFloat32(const char* data, size_t length, int expectedElements, Array<float>& output);
    bool decodeFloat64(const char* data, size_t length, int expectedElements, Array<float>& output);
    bool decodeInt16(const char* data, size_t length, int expectedElements, Array<float>& output);
    bool decodeInt32(const char* data, size_t length, int expectedElements, Array<float>& output);
    bool decodeUInt16(const char* data, size_t length, int expectedElements, Array<float>& output);
    bool addMultiSampleDataToBuffer(const Array<float>& channelData, int nChannels, int nSamples,
                                    double baseTimestamp, int sampleRate);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RedisDataThread);
};

#endif // __REDISDATATHREAD_H_INCLUDED__
