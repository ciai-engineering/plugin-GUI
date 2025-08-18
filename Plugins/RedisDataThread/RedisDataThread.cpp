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

#include "RedisDataThread.h"
#include "RedisDataThreadEditor.h"

RedisDataThread::RedisDataThread(SourceNode* sn)
    : DataThread(sn)
    , redisCtx(nullptr)
    , redisHost("localhost")
    , redisPort(6379)
    , redisChannelName("neural_data_primary")  // Renamed for clarity
    , sampleRate(30000.0f)
    , numDataChannels(32)          // More reasonable default
    , maxDataChannels(1024)        // Set maximum limit
    , dataFormat("brandbci")
    , autoDetectChannels(true)     // Enable auto-detection by default
    , useStreamMode(true)
    , currentStreamId("0-0")
    , isAcquiring(false)
    , connectionStatus(false)
    , currentSampleNumber(0)
    , enableOpenEphysFormat(true)  // Enable by default
    , enableDataValidation(true)   // Enable validation by default
{
    LOGD("🚀 RedisDataThread constructor called");
    LOGD("Default configuration:");
    LOGD("  - Host: ", redisHost, ":", redisPort);
    LOGD("  - Channel Name: ", redisChannelName);
    LOGD("  - Format: ", dataFormat);
    LOGD("  - Data Channels: ", numDataChannels, " (max: ", maxDataChannels, ")");
    LOGD("  - Auto-detect channels: ", autoDetectChannels ? "enabled" : "disabled");
    LOGD("  - Stream mode: ", useStreamMode ? "ENABLED" : "DISABLED");

    // Validate initial configuration
    if (!validateConfiguration()) {
        LOGD("Initial configuration validation failed - using safe defaults");
    }
    LOGD("  - Sample Rate: ", sampleRate);
    LOGD("  - Stream Mode: ", useStreamMode ? "ENABLED" : "DISABLED");
    LOGD("  - Open Ephys Format: ", enableOpenEphysFormat ? "ENABLED" : "DISABLED");
    LOGD("  - Data Validation: ", enableDataValidation ? "ENABLED" : "DISABLED");
    LOGD("  - Redis context: ", (redisCtx ? "initialized" : "null"));
    LOGD("  - Acquiring: ", isAcquiring.load());
    LOGD("  - Connected: ", connectionStatus.load());

#ifdef REDIS_ENABLED
    LOGD("✓ Redis support is ENABLED");
#else
    LOGE("❌ Redis support is DISABLED - recompile with REDIS_ENABLED=1");
#endif
}

RedisDataThread::~RedisDataThread()
{
    if (isAcquiring.load())
    {
        stopAcquisition();
    }
    disconnectFromRedis();
}

std::unique_ptr<GenericEditor> RedisDataThread::createEditor(SourceNode* sn)
{
    std::unique_ptr<RedisDataThreadEditor> editor = std::make_unique<RedisDataThreadEditor>(sn, this);
    return std::move(editor);
}

bool RedisDataThread::foundInputSource()
{
    LOGD("foundInputSource() called - connection status: ", connectionStatus.load());
    // Return true if already connected, otherwise try to connect
    if (connectionStatus.load()) {
        LOGD("foundInputSource() returning true - already connected");
        return true;
    }

    // Try to connect to Redis to verify the source is available
    bool result = connectToRedis(redisHost, redisPort, redisPassword);
    LOGD("foundInputSource() connectToRedis result: ", result);
    return result;
}

bool RedisDataThread::isReady()
{
    return connectionStatus.load();
}

bool RedisDataThread::connectToRedis(const String& host, int port, const String& password)
{
#ifdef REDIS_ENABLED
    LOGD("Attempting to connect to Redis server: ", host, ":", port);

    // Disconnect existing connection
    if (connectionStatus.load())
    {
        LOGD("Disconnecting existing Redis connection");
        disconnectFromRedis();
    }

    // Create new connection
    LOGD("Creating Redis connection context");
    redisCtx = redisConnect(host.toRawUTF8(), port);

    if (redisCtx == nullptr || redisCtx->err)
    {
        if (redisCtx)
        {
            LOGE("Redis connection failed: ", redisCtx->errstr, " (error code: ", redisCtx->err, ")");
            redisFree(redisCtx);
            redisCtx = nullptr;
        }
        else
        {
            LOGE("Redis connection failed: Cannot allocate redis context");
        }
        connectionStatus = false;
        LOGE("Redis connection status set to FALSE");
        return false;
    }

    LOGD("Redis context created successfully");

    // Test connection with PING
    LOGD("Testing Redis connection with PING command");
    redisReply* pingReply = (redisReply*)redisCommand(redisCtx, "PING");
    if (pingReply == nullptr || pingReply->type == REDIS_REPLY_ERROR)
    {
        LOGE("Redis PING failed - connection not working");
        if (pingReply) freeReplyObject(pingReply);
        disconnectFromRedis();
        return false;
    }
    LOGD("Redis PING successful: ", (pingReply->str ? pingReply->str : "PONG"));
    freeReplyObject(pingReply);

    // Authenticate if password is provided
    if (password.isNotEmpty())
    {
        LOGD("Authenticating with Redis using provided password");
        redisReply* reply = (redisReply*)redisCommand(redisCtx, "AUTH %s", password.toRawUTF8());
        if (reply == nullptr || reply->type == REDIS_REPLY_ERROR)
        {
            LOGE("Redis authentication failed: ", (reply && reply->str) ? reply->str : "unknown error");
            if (reply) freeReplyObject(reply);
            disconnectFromRedis();
            return false;
        }
        LOGD("Redis authentication successful");
        freeReplyObject(reply);
    }
    else
    {
        LOGD("No password provided - skipping authentication");
    }

    // Test channel access based on mode
    LOGD("Testing access to Redis channel: ", redisChannelName, " (mode: ", useStreamMode ? "STREAM" : "LIST", ")");

    redisReply* testReply = nullptr;
    if (useStreamMode)
    {
        // For stream mode, check if key exists and is a stream
        testReply = (redisReply*)redisCommand(redisCtx, "TYPE %s", redisChannelName.toRawUTF8());
        if (testReply && testReply->type == REDIS_REPLY_STATUS)
        {
            String keyType(testReply->str);
            if (keyType == "stream")
            {
                LOGD("Redis stream '", redisChannelName, "' found and accessible");
            }
            else if (keyType == "none")
            {
                LOGD("Redis stream '", redisChannelName, "' does not exist yet (will be created when data arrives)");
            }
            else
            {
                LOGE("Redis key '", redisChannelName, "' exists but is not a stream (type: ", keyType, ")");
                freeReplyObject(testReply);
                disconnectFromRedis();
                return false;
            }
        }
        else
        {
            LOGE("Cannot check Redis key type for '", redisChannelName, "'");
            if (testReply) freeReplyObject(testReply);
            disconnectFromRedis();
            return false;
        }
    }
    else
    {
        // For list mode, use LLEN
        testReply = (redisReply*)redisCommand(redisCtx, "LLEN %s", redisChannelName.toRawUTF8());
        if (testReply == nullptr || testReply->type == REDIS_REPLY_ERROR)
        {
            LOGE("Cannot access Redis channel '", redisChannelName, "': ", (testReply && testReply->str) ? testReply->str : "unknown error");
            if (testReply) freeReplyObject(testReply);
            disconnectFromRedis();
            return false;
        }
        else
        {
            int queueLength = (testReply->type == REDIS_REPLY_INTEGER) ? testReply->integer : -1;
            LOGD("Redis channel '", redisChannelName, "' accessible, current length: ", queueLength);
        }
    }

    if (testReply) freeReplyObject(testReply);

    // Save connection parameters
    redisHost = host;
    redisPort = port;
    redisPassword = password;
    connectionStatus = true;

    LOGD("✓ Successfully connected to Redis server: ", host, ":", port);
    LOGD("✓ Redis connection status set to TRUE");
    LOGD("✓ Channel: ", redisChannelName, ", Format: ", dataFormat, ", Channels: ", numDataChannels, ", Sample Rate: ", sampleRate);
    return true;
#else
    LOGE("Redis support not compiled. Install hiredis library and recompile.");
    LOGE("To enable Redis: install libhiredis-dev and recompile with REDIS_ENABLED=1");
    return false;
#endif
}

void RedisDataThread::disconnectFromRedis()
{
#ifdef REDIS_ENABLED
    if (redisCtx)
    {
        redisFree(redisCtx);
        redisCtx = nullptr;
    }
#endif
    connectionStatus = false;
}

bool RedisDataThread::isConnected() const
{
    return connectionStatus.load();
}

void RedisDataThread::setRedisHost(const String& host)
{
    if (redisHost != host)
    {
        redisHost = host;
        LOGD("Redis host changed to: ", host);
    }
}

void RedisDataThread::setRedisPort(int port)
{
    if (redisPort != port)
    {
        redisPort = port;
        LOGD("Redis port changed to: ", port);
    }
}

void RedisDataThread::setRedisPassword(const String& password)
{
    if (redisPassword != password)
    {
        redisPassword = password;
        LOGD("Redis password ", password.isEmpty() ? "cleared" : "updated");
    }
}

void RedisDataThread::setRedisChannel(const String& channel)
{
    if (redisChannelName != channel)
    {
        redisChannelName = channel;
        LOGD("Redis channel changed to: ", channel);
    }
}

void RedisDataThread::setSampleRate(float rate)
{
    if (sampleRate != rate)
    {
        sampleRate = rate;
        LOGD("Sample rate changed to: ", rate, " Hz");

        // Sample rate change affects the data stream configuration
        // Note: We don't need to resize buffers for sample rate changes,
        // but downstream processors need to be notified
    }
}

void RedisDataThread::setNumChannels(int channels)
{
    if (numDataChannels != channels)
    {
        numDataChannels = channels;
        LOGD("Number of channels changed to: ", channels);

        // Resize buffers to match new channel count
        resizeBuffers();
    }
}

void RedisDataThread::setDataFormat(const String& format)
{
    if (dataFormat != format)
    {
        dataFormat = format;
        LOGD("Data format changed to: ", format);

        // Auto-enable stream mode for BRANDBCI format
        if (format == "brandbci")
        {
            useStreamMode = true;
            LOGD("Auto-enabled stream mode for BRANDBCI format");
        }
    }
}

String RedisDataThread::getConnectionStatus() const
{
    if (connectionStatus.load())
    {
        return "Connected to " + redisHost + ":" + String(redisPort);
    }
    else
    {
        return "Disconnected";
    }
}

Array<String> RedisDataThread::getLatestRecords(int numRecords)
{
    Array<String> records;

#ifdef REDIS_ENABLED
    if (!connectionStatus.load() || redisCtx == nullptr)
    {
        LOGE("Cannot retrieve data: Redis not connected");
        return records;
    }

    LOGD("Retrieving latest ", numRecords, " records from Redis channel: ", redisChannelName, " (mode: ", useStreamMode ? "STREAM" : "LIST", ")");

    redisReply* reply = nullptr;

    if (useStreamMode)
    {
        // For streams, use XREVRANGE to get latest entries
        reply = (redisReply*)redisCommand(redisCtx,
            "XREVRANGE %s + - COUNT %d", redisChannelName.toRawUTF8(), numRecords);
    }
    else
    {
        // For lists, use LRANGE to get the latest records without removing them from the list
        // LRANGE key start stop - gets elements from start to stop (inclusive)
        // Use negative indices to get from the end: -1 is last element, -2 is second to last, etc.
        int startIndex = -(numRecords);
        int stopIndex = -1;

        reply = (redisReply*)redisCommand(redisCtx,
            "LRANGE %s %d %d", redisChannelName.toRawUTF8(), startIndex, stopIndex);
    }

    if (reply == nullptr)
    {
        LOGE("Redis command returned null reply");
        handleRedisError("Redis command failed");
        return records;
    }

    LOGD("Redis reply type: ", reply->type, ", elements: ",
         (reply->type == REDIS_REPLY_ARRAY ? reply->elements : 0));

    if (reply->type == REDIS_REPLY_ARRAY)
    {
        if (useStreamMode)
        {
            // Parse stream entries: each entry is [id, [field1, value1, field2, value2, ...]]
            LOGD("Retrieved ", reply->elements, " stream entries from Redis");

            for (size_t i = 0; i < reply->elements; i++)
            {
                redisReply* entryReply = reply->element[i];
                if (entryReply->type == REDIS_REPLY_ARRAY && entryReply->elements >= 2)
                {
                    // Get the field-value pairs
                    redisReply* fieldsReply = entryReply->element[1];
                    if (fieldsReply->type == REDIS_REPLY_ARRAY)
                    {
                        // Look for brandbci_data field
                        for (size_t j = 0; j < fieldsReply->elements; j += 2)
                        {
                            if (j + 1 < fieldsReply->elements &&
                                fieldsReply->element[j]->type == REDIS_REPLY_STRING &&
                                String(fieldsReply->element[j]->str) == "brandbci_data")
                            {
                                String record(fieldsReply->element[j + 1]->str, fieldsReply->element[j + 1]->len);
                                records.add(record);
                                LOGD("Stream record ", i, " length: ", record.length(), " bytes");
                                break;
                            }
                        }
                    }
                }
            }
        }
        else
        {
            // Parse list entries: each element is a string
            LOGD("Retrieved ", reply->elements, " list records from Redis");

            for (size_t i = 0; i < reply->elements; i++)
            {
                if (reply->element[i]->type == REDIS_REPLY_STRING)
                {
                    String record(reply->element[i]->str, reply->element[i]->len);
                    records.add(record);
                    LOGD("List record ", i, " length: ", record.length(), " bytes");
                }
            }
        }
    }
    else if (reply->type == REDIS_REPLY_NIL)
    {
        LOGD("No data available in Redis channel");
    }
    else
    {
        LOGE("Unexpected Redis reply type: ", reply->type);
    }

    freeReplyObject(reply);

#else
    LOGE("Redis not compiled - REDIS_ENABLED not defined");
#endif

    LOGD("Returning ", records.size(), " records");
    return records;
}

bool RedisDataThread::startAcquisition()
{
    LOGD("=== STARTING REDIS ACQUISITION ===");
    LOGD("Connection status: ", connectionStatus.load());
    LOGD("Current acquiring status: ", isAcquiring.load());
    LOGD("Redis host: ", redisHost, ":", redisPort);
    LOGD("Redis channel: ", redisChannelName);
    LOGD("Data format: ", dataFormat);
    LOGD("Expected channels: ", numDataChannels);
    LOGD("Sample rate: ", sampleRate);

    if (!isConnected())
    {
        LOGE("❌ Cannot start acquisition: not connected to Redis");
        LOGE("Connection status: ", connectionStatus.load());
        return false;
    }

    if (isAcquiring.load())
    {
        LOGC("⚠️ Acquisition already running");
        return true;
    }

    // Reset sample counter
    currentSampleNumber = 0;
    LOGD("Reset sample counter to 0");

    // Test Redis connection before starting
    LOGD("Testing Redis connection before starting acquisition...");
#ifdef REDIS_ENABLED
    if (redisCtx)
    {
        redisReply* testReply = (redisReply*)redisCommand(redisCtx, "PING");
        if (testReply && testReply->type != REDIS_REPLY_ERROR)
        {
            LOGD("✓ Redis PING test successful");
            freeReplyObject(testReply);
        }
        else
        {
            LOGE("❌ Redis PING test failed");
            if (testReply) freeReplyObject(testReply);
            return false;
        }

        // Check data availability based on mode
        if (useStreamMode)
        {
            // For streams, check if stream exists and has data
            redisReply* lenReply = (redisReply*)redisCommand(redisCtx, "XLEN %s", redisChannelName.toRawUTF8());
            if (lenReply && lenReply->type == REDIS_REPLY_INTEGER)
            {
                LOGD("✓ Redis stream '", redisChannelName, "' has ", lenReply->integer, " entries available");
                freeReplyObject(lenReply);
            }
            else if (lenReply && lenReply->type == REDIS_REPLY_ERROR)
            {
                LOGD("✓ Redis stream '", redisChannelName, "' does not exist yet (will be created when data arrives)");
                freeReplyObject(lenReply);
            }
            else
            {
                LOGE("❌ Cannot check Redis stream length");
                if (lenReply) freeReplyObject(lenReply);
            }
        }
        else
        {
            // For lists, use LLEN
            redisReply* lenReply = (redisReply*)redisCommand(redisCtx, "LLEN %s", redisChannelName.toRawUTF8());
            if (lenReply && lenReply->type == REDIS_REPLY_INTEGER)
            {
                LOGD("✓ Redis channel '", redisChannelName, "' has ", lenReply->integer, " samples available");
                freeReplyObject(lenReply);
            }
            else
            {
                LOGE("❌ Cannot check Redis channel length");
                if (lenReply) freeReplyObject(lenReply);
            }
        }
    }
    else
    {
        LOGE("❌ Redis context is null");
        return false;
    }
#endif

    isAcquiring = true;

    // Start the data thread - this is critical!
    LOGD("🚀 Starting data thread...");
    if (!isThreadRunning()) {
        startThread();
        LOGD("✅ Data thread started successfully");
    } else {
        LOGD("⚠️ Data thread was already running");
    }

    LOGD("✅ Redis data acquisition started successfully");
    LOGD("=== ACQUISITION ACTIVE ===");
    return true;
}

bool RedisDataThread::stopAcquisition()
{
    LOGD("=== STOPPING REDIS ACQUISITION ===");
    LOGD("Current acquiring status: ", isAcquiring.load());

    if (!isAcquiring.load())
    {
        LOGD("Acquisition already stopped");
        return true;
    }

    isAcquiring = false;

    // Stop the data thread
    LOGD("🛑 Stopping data thread...");
    if (isThreadRunning()) {
        signalThreadShouldExit();
        waitForThreadToExit(5000); // Wait up to 5 seconds
        LOGD("✅ Data thread stopped successfully");
    } else {
        LOGD("⚠️ Data thread was not running");
    }

    LOGD("✅ Redis data acquisition stopped");
    LOGD("Final sample count: ", currentSampleNumber.load());
    LOGD("=== ACQUISITION STOPPED ===");
    return true;
}

void RedisDataThread::updateSettings(OwnedArray<ContinuousChannel>* continuousChannels,
                                     OwnedArray<EventChannel>* eventChannels,
                                     OwnedArray<SpikeChannel>* spikeChannels,
                                     OwnedArray<DataStream>* sourceStreams,
                                     OwnedArray<DeviceInfo>* devices,
                                     OwnedArray<ConfigurationObject>* configurationObjects)
{
    // Clear existing configuration
    sourceStreams->clear();
    continuousChannels->clear();

    // Create data stream
    DataStream* stream = new DataStream(DataStream::Settings{
        "Redis Stream",
        "Redis data source",
        "redis.stream.1",
        sampleRate,
        false // generates_timestamps
    });
    sourceStreams->add(stream);

    // Create continuous channels
    for (int ch = 0; ch < numDataChannels; ch++)
    {
        ContinuousChannel* chan = new ContinuousChannel(ContinuousChannel::Settings{
            ContinuousChannel::Type::ELECTRODE,
            "Redis_CH" + String(ch + 1),
            "Redis channel " + String(ch + 1),
            "redis.channel." + String(ch + 1),
            0.195f, // microvolts conversion factor
            stream
        });
        continuousChannels->add(chan);
    }
}

void RedisDataThread::resizeBuffers()
{
    // Clear existing buffers
    sourceBuffers.clear();

    // Create one DataBuffer for our single data stream
    // Buffer size: 10000 samples should be sufficient for Redis data
    const int bufferSize = 10000;

    DataBuffer* buffer = new DataBuffer(numDataChannels, bufferSize);
    sourceBuffers.add(buffer);

    LOGD("Redis DataThread buffers resized: ", numDataChannels, " channels, buffer size: ", bufferSize);
}

bool RedisDataThread::updateBuffer()
{
    static int64 lastLogTime = 0;
    static int64 samplesProcessed = 0;
    static int64 lastSampleCount = 0;
    static int64 callCount = 0;

    callCount++;

    // Log every call for the first 10 calls, then every 100 calls
    if (callCount <= 10 || callCount % 100 == 0) {
        LOGD("🔄 updateBuffer() called #", callCount, " - acquiring: ", isAcquiring.load(), ", connected: ", connectionStatus.load());
    }

    // Check acquisition state
    if (!isAcquiring.load())
    {
        if (callCount <= 5) LOGD("updateBuffer: Not acquiring, returning false");
        return false;
    }

    if (!connectionStatus.load())
    {
        if (callCount <= 5) LOGE("updateBuffer: Redis not connected, returning false");
        return false;
    }

#ifdef REDIS_ENABLED
    // Periodic status logging (every 5 seconds)
    int64 currentTime = Time::getMillisecondCounterHiRes();
    if (currentTime - lastLogTime > 5000) // 5 seconds
    {
        int64 samplesDelta = samplesProcessed - lastSampleCount;
        double rate = samplesDelta / 5.0; // samples per second
        LOGD("Redis Status: processed=", samplesProcessed, " samples, rate=", rate, " samples/sec, acquiring=", isAcquiring.load(), ", connected=", connectionStatus.load());
        LOGD("  Mode: ", useStreamMode ? "STREAMS" : "LISTS", ", Stream: ", redisChannelName);
        lastLogTime = currentTime;
        lastSampleCount = samplesProcessed;
    }

    // Route to appropriate update method based on mode
    LOGD("updateBuffer: Using ", useStreamMode ? "STREAM" : "LIST", " mode");
    bool result = useStreamMode ? updateBufferFromStream() : updateBufferFromList();
    if (result) {
        samplesProcessed++;
    }
    return result;

    return false; // This should not be reached
#else
    LOGE("Redis not compiled - REDIS_ENABLED not defined");
    return false;
#endif
}

bool RedisDataThread::updateBufferFromList()
{
#ifdef REDIS_ENABLED
    // Log BLPOP command execution
    LOGD("Executing BLPOP command on channel: ", redisChannelName);

    // Get data from Redis (blocking call with 1 second timeout)
    redisReply* reply = (redisReply*)redisCommand(redisCtx,
        "BLPOP %s 1", redisChannelName.toRawUTF8());

    if (reply == nullptr)
    {
        LOGE("BLPOP command returned null reply");
        handleRedisError("BLPOP command failed");
        return false;
    }

    LOGD("BLPOP reply type: ", reply->type, ", elements: ", (reply->type == REDIS_REPLY_ARRAY ? reply->elements : 0));

    if (reply->type == REDIS_REPLY_NIL)
    {
        // Timeout, no data available
        LOGD("BLPOP timeout - no data available in Redis channel");
        freeReplyObject(reply);
        return true; // Not an error
    }

    bool success = false;
    if (reply->type == REDIS_REPLY_ARRAY && reply->elements == 2)
    {
        LOGD("BLPOP returned array with 2 elements - processing data");

        // Parse data based on format
        Array<float> channelData;
        String dataStr(reply->element[1]->str, reply->element[1]->len);

        LOGD("Data format: ", dataFormat, ", data length: ", dataStr.length(), " bytes");
        LOGD("Raw data preview (first 100 chars): ", dataStr.substring(0, 100));

        if (dataFormat == "json")
        {
            success = parseJsonData(dataStr, channelData);
            LOGD("JSON parsing result: ", success ? "SUCCESS" : "FAILED", ", channels parsed: ", channelData.size());
        }
        else if (dataFormat == "binary")
        {
            success = parseBinaryData(reply->element[1]->str,
                                    reply->element[1]->len, channelData);
            LOGD("Binary parsing result: ", success ? "SUCCESS" : "FAILED", ", channels parsed: ", channelData.size());
        }
        else
        {
            LOGE("Unknown data format: ", dataFormat);
        }

        if (success && channelData.size() == numDataChannels)
        {
            LOGD("Channel data validated: ", channelData.size(), " channels (expected: ", numDataChannels, ")");

            // Log sample data range
            float minVal = *std::min_element(channelData.begin(), channelData.end());
            float maxVal = *std::max_element(channelData.begin(), channelData.end());
            LOGD("Sample data range: [", minVal, ", ", maxVal, "] µV");

            // Prepare timestamp and sample number
            int64 sampleNumber = currentSampleNumber.fetch_add(1);
            double timestamp = Time::getMillisecondCounterHiRes() / 1000.0;
            uint64 eventCode = 0;

            LOGD("Adding to buffer: sampleNumber=", sampleNumber, ", timestamp=", timestamp);

            // Add to DataBuffer
            DataBuffer* buffer = getBufferAddress(0);
            if (buffer)
            {
                LOGD("DataBuffer found, attempting to add data");
                int written = buffer->addToBuffer(channelData.getRawDataPointer(),
                                                &sampleNumber,
                                                &timestamp,
                                                &eventCode,
                                                1);
                success = (written > 0);
                LOGD("Buffer write result: written=", written, ", success=", success);

                if (success)
                {
                    currentSampleNumber++;
                    if (currentSampleNumber.load() % 1000 == 0) // Log every 1000 samples
                    {
                        LOGD("Successfully processed ", currentSampleNumber.load(), " samples total");
                    }
                }
                else
                {
                    LOGE("Failed to write to DataBuffer");
                }
            }
            else
            {
                LOGE("DataBuffer is null - cannot add data to buffer");
                success = false;
            }
        }
        else if (!success)
        {
            LOGE("Data parsing failed for format: ", dataFormat);
        }
        else
        {
            LOGE("Channel count mismatch: got ", channelData.size(), " channels, expected ", numDataChannels);
        }
    }
    else
    {
        LOGE("Unexpected BLPOP reply format: type=", reply->type, ", elements=", (reply->type == REDIS_REPLY_ARRAY ? reply->elements : 0));
    }

    freeReplyObject(reply);
    LOGD("updateBufferFromList returning: ", success);
    return success;
#else
    LOGE("Redis not compiled - REDIS_ENABLED not defined");
    return false;
#endif
}

bool RedisDataThread::updateBufferFromStream()
{
#ifdef REDIS_ENABLED
    if (redisChannelName.isEmpty())
    {
        LOGE("Stream name is empty - cannot read from stream");
        return false;
    }

    LOGD("Reading from stream: ", redisChannelName, " with ID: ", currentStreamId);

    // Use XREAD to read from single stream
    redisReply* reply = (redisReply*)redisCommand(redisCtx,
        "XREAD BLOCK 1000 STREAMS %s %s",
        redisChannelName.toRawUTF8(),
        currentStreamId.toRawUTF8());

    if (reply == nullptr)
    {
        LOGE("XREAD command returned null reply");
        handleRedisError("XREAD command failed");
        return false;
    }

    LOGD("XREAD reply type: ", reply->type, ", elements: ", (reply->type == REDIS_REPLY_ARRAY ? reply->elements : 0));

    if (reply->type == REDIS_REPLY_NIL)
    {
        // Timeout, no data available
        LOGD("XREAD timeout - no data available in stream");
        freeReplyObject(reply);
        return true; // Not an error
    }

    bool success = false;
    if (reply->type == REDIS_REPLY_ARRAY && reply->elements > 0)
    {
        LOGD("XREAD returned array with ", reply->elements, " stream(s) - processing data");

        // Process the stream data
        redisReply* streamReply = reply->element[0];
        if (streamReply->type == REDIS_REPLY_ARRAY && streamReply->elements == 2)
        {
            // Get stream name and entries
            String receivedStreamName(streamReply->element[0]->str);
            redisReply* entriesReply = streamReply->element[1];

            LOGD("Processing stream: ", receivedStreamName, " with ", entriesReply->elements, " entries");

            // Process each entry in the stream
            for (size_t entryIdx = 0; entryIdx < entriesReply->elements; entryIdx++)
            {
                redisReply* entryReply = entriesReply->element[entryIdx];
                if (entryReply->type == REDIS_REPLY_ARRAY && entryReply->elements >= 2)
                {
                    // Update stream position
                    currentStreamId = String(entryReply->element[0]->str);

                    // Get field-value pairs
                    redisReply* fieldsReply = entryReply->element[1];
                    if (fieldsReply->type == REDIS_REPLY_ARRAY)
                    {
                        success = processStreamEntry(fieldsReply);
                        if (success) break; // Process one entry at a time
                    }
                }
            }
        }
    }
    else
    {
        LOGE("Unexpected XREAD reply format: type=", reply->type, ", elements=", (reply->type == REDIS_REPLY_ARRAY ? reply->elements : 0));
    }

    freeReplyObject(reply);
    LOGD("updateBufferFromStream returning: ", success);
    return success;
#else
    LOGE("Redis not compiled - REDIS_ENABLED not defined");
    return false;
#endif
}

bool RedisDataThread::parseJsonData(const String& jsonStr, Array<float>& channelData)
{
    LOGD("Parsing JSON data, length: ", jsonStr.length(), " characters");

    try
    {
        var jsonData = JSON::parse(jsonStr);
        LOGD("JSON parsing successful");

        if (!jsonData.isObject())
        {
            LOGE("Invalid JSON format: not an object");
            return false;
        }
        LOGD("JSON is valid object");

        // Check for required fields
        if (!jsonData.hasProperty("channels"))
        {
            LOGE("JSON missing 'channels' property");
            LOGD("Available properties: ", JSON::toString(jsonData));
            return false;
        }

        var channels = jsonData["channels"];
        if (!channels.isArray())
        {
            LOGE("Invalid JSON format: 'channels' is not an array");
            return false;
        }

        int channelCount = channels.size();
        LOGD("Found channels array with ", channelCount, " elements (expected: ", numDataChannels, ")");

        if (channelCount != numDataChannels)
        {
            LOGE("Channel count mismatch: JSON has ", channelCount, " channels, expected ", numDataChannels);
            return false;
        }

        channelData.clear();
        channelData.ensureStorageAllocated(channelCount);

        // Parse channel values with detailed logging
        float minVal = std::numeric_limits<float>::max();
        float maxVal = std::numeric_limits<float>::lowest();
        int validChannels = 0;

        for (int i = 0; i < channelCount; i++)
        {
            if (channels[i].isDouble() || channels[i].isInt())
            {
                float value = (float)channels[i];
                channelData.add(value);
                validChannels++;

                // Track min/max for logging
                minVal = std::min(minVal, value);
                maxVal = std::max(maxVal, value);
            }
            else
            {
                LOGE("Non-numeric value in channels array at index ", i);
                return false;
            }
        }

        LOGD("Successfully parsed ", validChannels, " channels, range: [", minVal, ", ", maxVal, "]");

        // Log timestamp if present
        if (jsonData.hasProperty("timestamp"))
        {
            var timestamp = jsonData["timestamp"];
            LOGD("JSON timestamp: ", (int64)timestamp);
        }
        else
        {
            LOGD("No timestamp in JSON data");
        }

        return true;
    }
    catch (const std::exception& e)
    {
        LOGE("Exception parsing JSON data: ", e.what());
        return false;
    }
    catch (...)
    {
        LOGE("Unknown exception parsing JSON data");
        LOGD("JSON string that failed: ", jsonStr.substring(0, 200), "...");
        return false;
    }
}

bool RedisDataThread::parseBinaryData(const char* data, size_t length, Array<float>& channelData)
{
    // Expect binary data as array of floats
    if (length % sizeof(float) != 0)
    {
        LOGE("Binary data length not multiple of float size");
        return false;
    }

    int numSamples = length / sizeof(float);
    if (numSamples != numDataChannels)
    {
        LOGE("Binary data contains ", numSamples, " samples, expected ", numDataChannels);
        return false;
    }

    channelData.clear();
    const float* floatData = reinterpret_cast<const float*>(data);

    for (int i = 0; i < numSamples; i++)
    {
        channelData.add(floatData[i]);
    }

    return true;
}

bool RedisDataThread::processStreamEntry(redisReply* fieldsReply)
{
#ifdef REDIS_ENABLED
    // Check if this is Open Ephys format by looking for specific fields
    bool hasOpenEphysFields = false;

    // Pre-scan to detect format
    for (size_t i = 0; i < fieldsReply->elements; i += 2)
    {
        if (i + 1 < fieldsReply->elements)
        {
            String fieldName(fieldsReply->element[i]->str);
            if (fieldName == "data_shape" || fieldName == "data_dtype" || fieldName == "n_samples")
            {
                hasOpenEphysFields = true;
                break;
            }
        }
    }

    // Route to appropriate processing method
    if (hasOpenEphysFields && enableOpenEphysFormat)
    {
        LOGD("Processing as Open Ephys format");
        return processOpenEphysStreamEntry(fieldsReply);
    }
    else
    {
        LOGD("Processing as legacy format");
        return processLegacyStreamEntry(fieldsReply);
    }
#else
    return false;
#endif
}

bool RedisDataThread::processLegacyStreamEntry(redisReply* fieldsReply)
{
#ifdef REDIS_ENABLED
    // Legacy processing logic (original implementation)
    for (size_t i = 0; i < fieldsReply->elements; i += 2)
    {
        if (i + 1 < fieldsReply->elements)
        {
            String fieldName(fieldsReply->element[i]->str);
            String fieldValue(fieldsReply->element[i + 1]->str);

            LOGD("Legacy stream field: ", fieldName, " = ", fieldValue.substring(0, 100), "...");

            // Process different field types
            if (fieldName == "brandbci_data" || fieldName == "data")
            {
                Array<float> channelData;
                bool parseSuccess = false;

                if (dataFormat == "brandbci")
                {
                    parseSuccess = parseBrandBCIData(fieldValue, channelData);
                }
                else if (dataFormat == "json")
                {
                    parseSuccess = parseJsonData(fieldValue, channelData);
                }

                if (parseSuccess && channelData.size() == numDataChannels)
                {
                    // Add data to buffer
                    if (sourceBuffers.size() > 0)
                    {
                        DataBuffer* buffer = sourceBuffers[0];
                        if (buffer != nullptr)
                        {
                            // Prepare data for buffer (channel-major order)
                            int numSamples = 1; // One sample per stream entry
                            int64 sampleNumber = currentSampleNumber.load();
                            double timestamp = Time::getMillisecondCounterHiRes() / 1000.0;
                            uint64 eventCode = 0;

                            int written = buffer->addToBuffer(channelData.getRawDataPointer(),
                                                            &sampleNumber,
                                                            &timestamp,
                                                            &eventCode,
                                                            numSamples);

                            if (written > 0)
                            {
                                currentSampleNumber++;
                                LOGD("Added ", numDataChannels, " channels to buffer, sample #", currentSampleNumber.load());
                                return true;
                            }
                            else
                            {
                                LOGE("Failed to write to DataBuffer");
                            }
                        }
                    }
                }
                else if (!parseSuccess)
                {
                    LOGE("Failed to parse stream data in format: ", dataFormat);
                }
                else
                {
                    LOGE("Channel count mismatch: got ", channelData.size(), " channels, expected ", numDataChannels);
                }
            }
        }
    }

    return false;
#else
    return false;
#endif
}

bool RedisDataThread::parseBrandBCIData(const String& jsonStr, Array<float>& channelData)
{
    LOGD("Parsing BRANDBCI data, length: ", jsonStr.length(), " characters");

    try
    {
        var jsonData = JSON::parse(jsonStr);
        LOGD("BRANDBCI JSON parsing successful");

        if (!jsonData.isObject())
        {
            LOGE("Invalid BRANDBCI format: not an object");
            return false;
        }

        // Check for BRANDBCI structure: data.channels
        if (jsonData.hasProperty("data"))
        {
            var dataObj = jsonData["data"];
            if (dataObj.isObject() && dataObj.hasProperty("channels"))
            {
                var channels = dataObj["channels"];
                if (channels.isArray())
                {
                    channelData.clear();
                    channelData.ensureStorageAllocated(channels.size());

                    for (int i = 0; i < channels.size(); i++)
                    {
                        if (channels[i].isDouble() || channels[i].isInt())
                        {
                            channelData.add((float)channels[i]);
                        }
                        else
                        {
                            LOGE("Non-numeric value in BRANDBCI channels array at index ", i);
                            return false;
                        }
                    }

                    LOGD("Successfully parsed ", channelData.size(), " BRANDBCI channels");
                    return true;
                }
            }
        }

        // Fallback: try direct channels array (legacy format)
        if (jsonData.hasProperty("channels"))
        {
            return parseJsonData(jsonStr, channelData);
        }

        LOGE("BRANDBCI data missing required structure");
        return false;

    }
    catch (const std::exception& e)
    {
        LOGE("Exception parsing BRANDBCI data: ", e.what());
        return false;
    }
    catch (...)
    {
        LOGE("Unknown exception parsing BRANDBCI data");
        return false;
    }
}

void RedisDataThread::handleRedisError(const String& operation)
{
#ifdef REDIS_ENABLED
    if (redisCtx && redisCtx->err)
    {
        LOGE("Redis error in ", operation, ": ", redisCtx->errstr);

        // Try to reconnect on connection errors
        if (redisCtx->err == REDIS_ERR_IO || redisCtx->err == REDIS_ERR_EOF)
        {
            attemptReconnection();
        }
    }
    else
    {
        LOGE("Redis error in ", operation, ": unknown error");
    }
#endif
}

bool RedisDataThread::attemptReconnection()
{
    LOGD("Attempting to reconnect to Redis...");

    // Disconnect and try to reconnect
    disconnectFromRedis();

    if (connectToRedis(redisHost, redisPort, redisPassword))
    {
        LOGD("Redis reconnection successful");
        return true;
    }
    else
    {
        LOGE("Redis reconnection failed");
        return false;
    }
}

// Stream management methods
void RedisDataThread::setStreamMode(bool useStreams)
{
    if (useStreamMode != useStreams)
    {
        useStreamMode = useStreams;
        LOGD("Stream mode changed to: ", useStreams ? "ENABLED" : "DISABLED");

        if (useStreams && dataFormat == "json")
        {
            // Auto-switch to brandbci format for better stream support
            dataFormat = "brandbci";
            LOGD("Auto-switched data format to 'brandbci' for stream mode");
        }
    }
}





// ============================================================================
// Open Ephys Format Processing Methods
// ============================================================================

bool RedisDataThread::processOpenEphysStreamEntry(redisReply* fieldsReply)
{
#ifdef REDIS_ENABLED
    OpenEphysStreamData streamData = {};

    LOGD("Processing Open Ephys stream entry with ", fieldsReply->elements / 2, " fields");

    // Parse all fields
    for (size_t i = 0; i < fieldsReply->elements; i += 2)
    {
        if (i + 1 < fieldsReply->elements)
        {
            redisReply* keyReply = fieldsReply->element[i];
            redisReply* valueReply = fieldsReply->element[i + 1];

            String fieldName(keyReply->str);

            if (fieldName == "run")
            {
                streamData.run = String(valueReply->str).getIntValue();
                LOGD("  run: ", streamData.run);
            }
            else if (fieldName == "timestamp")
            {
                streamData.timestamp = String(valueReply->str).getDoubleValue();
                LOGD("  timestamp: ", streamData.timestamp);
            }
            else if (fieldName == "sample_rate")
            {
                streamData.sample_rate = String(valueReply->str).getIntValue();
                LOGD("  sample_rate: ", streamData.sample_rate);
            }
            else if (fieldName == "n_channels")
            {
                streamData.n_channels = String(valueReply->str).getIntValue();
                LOGD("  n_channels: ", streamData.n_channels);
            }
            else if (fieldName == "n_samples")
            {
                streamData.n_samples = String(valueReply->str).getIntValue();
                LOGD("  n_samples: ", streamData.n_samples);
            }
            else if (fieldName == "data")
            {
                // ✅ Correct: Handle binary data directly, don't convert to string
                streamData.binary_data = valueReply->str;
                streamData.data_length = valueReply->len;
                LOGD("  data: ", streamData.data_length, " bytes of binary data");
            }
            else if (fieldName == "data_shape")
            {
                streamData.data_shape = String(valueReply->str);
                LOGD("  data_shape: ", streamData.data_shape);
            }
            else if (fieldName == "data_dtype")
            {
                streamData.data_dtype = String(valueReply->str);
                LOGD("  data_dtype: ", streamData.data_dtype);
            }
            else
            {
                LOGD("  unknown field: ", fieldName);
            }
        }
    }

    // Validate and process the parsed data
    if (streamData.isValid())
    {
        return processOpenEphysData(streamData);
    }
    else
    {
        LOGE("Invalid or incomplete Open Ephys stream data");
        return false;
    }
#else
    return false;
#endif
}

bool RedisDataThread::processOpenEphysData(const OpenEphysStreamData& data)
{
    LOGD("Processing Open Ephys data: ", data.n_samples, " samples, ",
         data.n_channels, " channels, type: ", data.data_dtype);

    // 1. Validate data if enabled
    if (enableDataValidation && !validateStreamData(data))
    {
        LOGE("Open Ephys stream data validation failed");
        return false;
    }

    // 2. Parse data shape
    Array<int> shape = parseDataShape(data.data_shape);
    if (shape.size() != 2)
    {
        LOGE("Invalid data shape format: ", data.data_shape);
        return false;
    }

    // Verify shape consistency with metadata
    // Support both [samples, channels] and [channels, samples] formats
    bool isValidShape = false;
    bool needsTranspose = false;

    if (shape[0] == data.n_samples && shape[1] == data.n_channels)
    {
        // Standard format: [samples, channels]
        isValidShape = true;
        needsTranspose = false;
        LOGD("Data format: [samples, channels] = [", shape[0], ", ", shape[1], "]");
    }
    else if (shape[0] == data.n_channels && shape[1] == data.n_samples)
    {
        // Alternative format: [channels, samples] - needs transpose
        isValidShape = true;
        needsTranspose = true;
        LOGD("Data format: [channels, samples] = [", shape[0], ", ", shape[1], "] - will transpose");
    }

    if (!isValidShape)
    {
        LOGE("Shape mismatch: shape=", data.data_shape,
             " vs metadata=[", data.n_samples, ",", data.n_channels, "]");
        return false;
    }

    // 3. Decode binary data
    Array<float> channelData;
    if (!decodeBinaryData(data.binary_data, data.data_length,
                         data.data_dtype, shape, channelData))
    {
        LOGE("Failed to decode binary data");
        return false;
    }

    // 4. Verify decoded data size
    int expectedSize = data.n_channels * data.n_samples;
    if (channelData.size() != expectedSize)
    {
        LOGE("Decoded data size mismatch: expected ", expectedSize,
             ", got ", channelData.size());
        return false;
    }

    // 5. Transpose data if needed (from [channels, samples] to [samples, channels])
    if (needsTranspose)
    {
        LOGD("Transposing data from [channels, samples] to [samples, channels]");
        Array<float> transposedData;
        transposedData.ensureStorageAllocated(channelData.size());

        // Transpose: input[channel][sample] -> output[sample][channel]
        for (int sample = 0; sample < data.n_samples; sample++)
        {
            for (int channel = 0; channel < data.n_channels; channel++)
            {
                int inputIndex = channel * data.n_samples + sample;  // [channels, samples] indexing
                transposedData.add(channelData[inputIndex]);
            }
        }

        channelData = std::move(transposedData);
        LOGD("Data transposition completed");
    }

    LOGD("Successfully processed ", channelData.size(), " data points");

    // 5. Add to data buffer
    return addMultiSampleDataToBuffer(channelData, data.n_channels,
                                    data.n_samples, data.timestamp,
                                    data.sample_rate);
}

bool RedisDataThread::validateStreamData(const OpenEphysStreamData& data)
{
    // Basic field validation
    if (data.n_channels <= 0)
    {
        LOGE("Invalid channel count: ", data.n_channels);
        return false;
    }

    if (data.n_samples <= 0)
    {
        LOGE("Invalid sample count: ", data.n_samples);
        return false;
    }

    if (data.sample_rate <= 0)
    {
        LOGE("Invalid sample rate: ", data.sample_rate);
        return false;
    }

    // Binary data validation
    if (!data.binary_data || data.data_length == 0)
    {
        LOGE("Missing or empty binary data");
        return false;
    }

    // Metadata validation
    if (data.data_shape.isEmpty())
    {
        LOGE("Missing data shape information");
        return false;
    }

    if (data.data_dtype.isEmpty())
    {
        LOGE("Missing data type information");
        return false;
    }

    // Data type validation
    Array<String> supportedTypes = {"float32", "<f4", "float64", "<f8",
                                   "int16", "<i2", "int32", "<i4", "uint16", "<u2"};
    if (!supportedTypes.contains(data.data_dtype))
    {
        LOGE("Unsupported data type: ", data.data_dtype);
        return false;
    }

    // Data shape validation
    Array<int> shape = parseDataShape(data.data_shape);
    if (shape.size() != 2)
    {
        LOGE("Invalid data shape format");
        return false;
    }

    // Support both [samples, channels] and [channels, samples] formats
    bool isValidShape = (shape[0] == data.n_samples && shape[1] == data.n_channels) ||
                       (shape[0] == data.n_channels && shape[1] == data.n_samples);

    if (!isValidShape)
    {
        LOGE("Shape-metadata mismatch: shape=[", shape[0], ",", shape[1],
             "] vs metadata=[", data.n_samples, ",", data.n_channels, "]");
        return false;
    }

    // Data size validation
    size_t expectedSize = calculateExpectedDataSize(data.data_dtype, shape);
    if (data.data_length != expectedSize)
    {
        LOGE("Data size mismatch: expected ", expectedSize,
             " bytes, got ", data.data_length);
        return false;
    }

    // Reasonableness checks
    if (data.n_channels > 1024)
    {
        LOGD("Warning: Unusually high channel count: ", data.n_channels);
    }

    if (data.n_samples > 10000)
    {
        LOGD("Warning: Unusually high sample count: ", data.n_samples);
    }

    LOGD("Open Ephys stream data validation passed");
    return true;
}

Array<int> RedisDataThread::parseDataShape(const String& shapeStr)
{
    Array<int> shape;

    try
    {
        // Parse JSON format shape information, e.g., "[1000, 96]"
        var shapeJson = JSON::parse(shapeStr);
        if (shapeJson.isArray())
        {
            for (int i = 0; i < shapeJson.size(); i++)
            {
                if (shapeJson[i].isInt())
                {
                    int dimension = (int)shapeJson[i];
                    if (dimension > 0)
                    {
                        shape.add(dimension);
                    }
                    else
                    {
                        LOGE("Invalid dimension value: ", dimension);
                        return Array<int>(); // Return empty array for error
                    }
                }
            }
        }
        else
        {
            LOGE("Data shape is not a JSON array: ", shapeStr);
        }
    }
    catch (const std::exception& e)
    {
        LOGE("Failed to parse data shape JSON: ", shapeStr, " - ", e.what());
    }
    catch (...)
    {
        LOGE("Unknown error parsing data shape: ", shapeStr);
    }

    // Validate shape reasonableness
    if (shape.size() == 2)
    {
        LOGD("Parsed data shape: [", shape[0], ", ", shape[1], "]");
    }
    else
    {
        LOGE("Invalid shape dimensions: expected 2D, got ", shape.size(), "D");
        return Array<int>(); // Return empty array for error
    }

    return shape;
}

size_t RedisDataThread::calculateExpectedDataSize(const String& dtype, const Array<int>& shape)
{
    int totalElements = 1;
    for (int dim : shape)
    {
        totalElements *= dim;
    }

    size_t elementSize = 0;
    if (dtype == "float32" || dtype == "<f4")
    {
        elementSize = sizeof(float);
    }
    else if (dtype == "float64" || dtype == "<f8")
    {
        elementSize = sizeof(double);
    }
    else if (dtype == "int16" || dtype == "<i2")
    {
        elementSize = sizeof(int16);
    }
    else if (dtype == "int32" || dtype == "<i4")
    {
        elementSize = sizeof(int32);
    }
    else if (dtype == "uint16" || dtype == "<u2")
    {
        elementSize = sizeof(uint16);
    }

    return totalElements * elementSize;
}

bool RedisDataThread::decodeBinaryData(const char* data, size_t length, const String& dtype,
                                     const Array<int>& shape, Array<float>& output)
{
    output.clear();

    // Calculate expected number of elements
    int totalElements = 1;
    for (int dim : shape)
    {
        totalElements *= dim;
    }

    LOGD("Decoding binary data: type=", dtype, ", elements=", totalElements, ", bytes=", length);

    // Route to appropriate decoder based on data type
    if (dtype == "float32" || dtype == "<f4")
    {
        return decodeFloat32(data, length, totalElements, output);
    }
    else if (dtype == "float64" || dtype == "<f8")
    {
        return decodeFloat64(data, length, totalElements, output);
    }
    else if (dtype == "int16" || dtype == "<i2")
    {
        return decodeInt16(data, length, totalElements, output);
    }
    else if (dtype == "int32" || dtype == "<i4")
    {
        return decodeInt32(data, length, totalElements, output);
    }
    else if (dtype == "uint16" || dtype == "<u2")
    {
        return decodeUInt16(data, length, totalElements, output);
    }
    else
    {
        LOGE("Unsupported data type: ", dtype);
        return false;
    }
}

bool RedisDataThread::decodeFloat32(const char* data, size_t length, int expectedElements, Array<float>& output)
{
    // Validate data length
    if (length != expectedElements * sizeof(float))
    {
        LOGE("Float32 data length mismatch: expected ",
             expectedElements * sizeof(float), " bytes, got ", length);
        return false;
    }

    // Direct copy (most efficient for float32)
    const float* floatData = reinterpret_cast<const float*>(data);
    output.ensureStorageAllocated(expectedElements);

    for (int i = 0; i < expectedElements; i++)
    {
        output.add(floatData[i]);
    }

    LOGD("Decoded ", expectedElements, " float32 values");
    return true;
}

bool RedisDataThread::decodeFloat64(const char* data, size_t length, int expectedElements, Array<float>& output)
{
    if (length != expectedElements * sizeof(double))
    {
        LOGE("Float64 data length mismatch: expected ",
             expectedElements * sizeof(double), " bytes, got ", length);
        return false;
    }

    const double* doubleData = reinterpret_cast<const double*>(data);
    output.ensureStorageAllocated(expectedElements);

    for (int i = 0; i < expectedElements; i++)
    {
        output.add(static_cast<float>(doubleData[i]));
    }

    LOGD("Decoded ", expectedElements, " float64 values (converted to float32)");
    return true;
}

bool RedisDataThread::decodeInt16(const char* data, size_t length, int expectedElements, Array<float>& output)
{
    if (length != expectedElements * sizeof(int16))
    {
        LOGE("Int16 data length mismatch: expected ",
             expectedElements * sizeof(int16), " bytes, got ", length);
        return false;
    }

    const int16* intData = reinterpret_cast<const int16*>(data);
    output.ensureStorageAllocated(expectedElements);

    // Optional: apply scale factor (configurable)
    float scaleFactor = 1.0f; // Can be made configurable later

    for (int i = 0; i < expectedElements; i++)
    {
        output.add(static_cast<float>(intData[i]) * scaleFactor);
    }

    LOGD("Decoded ", expectedElements, " int16 values with scale factor ", scaleFactor);
    return true;
}

bool RedisDataThread::decodeInt32(const char* data, size_t length, int expectedElements, Array<float>& output)
{
    if (length != expectedElements * sizeof(int32))
    {
        LOGE("Int32 data length mismatch: expected ",
             expectedElements * sizeof(int32), " bytes, got ", length);
        return false;
    }

    const int32* intData = reinterpret_cast<const int32*>(data);
    output.ensureStorageAllocated(expectedElements);

    for (int i = 0; i < expectedElements; i++)
    {
        output.add(static_cast<float>(intData[i]));
    }

    LOGD("Decoded ", expectedElements, " int32 values");
    return true;
}

bool RedisDataThread::decodeUInt16(const char* data, size_t length, int expectedElements, Array<float>& output)
{
    if (length != expectedElements * sizeof(uint16))
    {
        LOGE("UInt16 data length mismatch: expected ",
             expectedElements * sizeof(uint16), " bytes, got ", length);
        return false;
    }

    const uint16* uintData = reinterpret_cast<const uint16*>(data);
    output.ensureStorageAllocated(expectedElements);

    for (int i = 0; i < expectedElements; i++)
    {
        output.add(static_cast<float>(uintData[i]));
    }

    LOGD("Decoded ", expectedElements, " uint16 values");
    return true;
}

bool RedisDataThread::addMultiSampleDataToBuffer(const Array<float>& channelData,
                                                int nChannels, int nSamples,
                                                double baseTimestamp, int sampleRate)
{
    // Validate data size
    if (channelData.size() != nChannels * nSamples)
    {
        LOGE("Channel data size mismatch: expected ", nChannels * nSamples,
             ", got ", channelData.size());
        return false;
    }

    if (sourceBuffers.size() == 0)
    {
        LOGE("No source buffers available");
        return false;
    }

    DataBuffer* buffer = sourceBuffers[0];
    if (!buffer)
    {
        LOGE("DataBuffer is null - cannot add data");
        return false;
    }

    LOGD("Processing ", nSamples, " samples with ", nChannels, " channels each");

    // Process samples one by one
    for (int sample = 0; sample < nSamples; sample++)
    {
        // Extract current sample's channel data
        Array<float> sampleData;
        sampleData.ensureStorageAllocated(nChannels);

        // Assume data format is [samples, channels] (row-major)
        for (int ch = 0; ch < nChannels; ch++)
        {
            int index = sample * nChannels + ch;
            sampleData.add(channelData[index]);
        }

        // Calculate timestamp (assuming uniform sampling)
        int64 sampleNumber = currentSampleNumber.load() + sample;
        double timestamp = baseTimestamp + (sample / (double)sampleRate);
        uint64 eventCode = 0;

        // Add to buffer
        int written = buffer->addToBuffer(sampleData.getRawDataPointer(),
                                        &sampleNumber,
                                        &timestamp,
                                        &eventCode,
                                        1);

        if (written <= 0)
        {
            LOGE("Failed to write sample ", sample, " to buffer");
            return false;
        }

        // Log progress every 100 samples
        if (sample % 100 == 0 && sample > 0)
        {
            LOGD("Processed ", sample, "/", nSamples, " samples");
        }
    }

    // Update sample counter
    currentSampleNumber += nSamples;

    LOGD("Successfully added ", nSamples, " samples to buffer. Total samples: ",
         currentSampleNumber.load());
    return true;
}

bool RedisDataThread::validateConfiguration() const
{
    // Validate host
    if (redisHost.isEmpty()) {
        LOGE("Configuration validation failed: Redis host cannot be empty");
        return false;
    }

    // Validate port
    if (redisPort <= 0 || redisPort > 65535) {
        LOGE("Configuration validation failed: Redis port must be between 1 and 65535, got: ", redisPort);
        return false;
    }

    // Validate channel name
    if (redisChannelName.isEmpty()) {
        LOGE("Configuration validation failed: Redis channel name cannot be empty");
        return false;
    }

    // Validate sample rate
    if (sampleRate <= 0.0f || sampleRate > 1000000.0f) {
        LOGE("Configuration validation failed: Sample rate must be between 0 and 1MHz, got: ", sampleRate);
        return false;
    }

    // Validate channel configuration
    if (!validateChannelConfiguration(numDataChannels)) {
        return false;
    }

    // Validate stream name (use channel name as stream name in stream mode)
    if (useStreamMode && redisChannelName.isEmpty()) {
        LOGE("Configuration validation failed: Channel name cannot be empty in stream mode");
        return false;
    }

    // Validate data format
    if (dataFormat != "json" && dataFormat != "binary" && dataFormat != "brandbci") {
        LOGE("Configuration validation failed: Unsupported data format: ", dataFormat);
        return false;
    }

    LOGD("✓ Configuration validation passed");
    return true;
}

bool RedisDataThread::validateChannelConfiguration(int channels) const
{
    if (channels <= 0) {
        LOGE("Channel validation failed: Number of channels must be positive, got: ", channels);
        return false;
    }

    if (channels > maxDataChannels) {
        LOGE("Channel validation failed: Number of channels (", channels,
             ") exceeds maximum (", maxDataChannels, ")");
        return false;
    }

    // Check for reasonable channel counts based on common hardware
    if (channels > 512) {
        LOGD("Warning: Large number of channels (", channels, ") - ensure this is correct");
    }

    return true;
}


