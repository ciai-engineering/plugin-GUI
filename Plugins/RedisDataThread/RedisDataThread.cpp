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
    , redisChannel("openephys_data")
    , sampleRate(30000.0f)
    , numChannels(32)
    , dataFormat("json")
    , isAcquiring(false)
    , connectionStatus(false)
    , currentSampleNumber(0)
{
    LOGD("🚀 RedisDataThread constructor called");
    LOGD("Default configuration:");
    LOGD("  - Host: ", redisHost, ":", redisPort);
    LOGD("  - Channel: ", redisChannel);
    LOGD("  - Format: ", dataFormat);
    LOGD("  - Channels: ", numChannels);
    LOGD("  - Sample Rate: ", sampleRate);
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
    // Try to connect to Redis to verify the source is available
    return connectToRedis(redisHost, redisPort, redisPassword);
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

    // Test channel access
    LOGD("Testing access to Redis channel: ", redisChannel);
    redisReply* testReply = (redisReply*)redisCommand(redisCtx, "LLEN %s", redisChannel.toRawUTF8());
    if (testReply == nullptr || testReply->type == REDIS_REPLY_ERROR)
    {
        LOGE("Cannot access Redis channel '", redisChannel, "': ", (testReply && testReply->str) ? testReply->str : "unknown error");
        if (testReply) freeReplyObject(testReply);
        disconnectFromRedis();
        return false;
    }
    else
    {
        int queueLength = (testReply->type == REDIS_REPLY_INTEGER) ? testReply->integer : -1;
        LOGD("Redis channel '", redisChannel, "' accessible, current length: ", queueLength);
        freeReplyObject(testReply);
    }

    // Save connection parameters
    redisHost = host;
    redisPort = port;
    redisPassword = password;
    connectionStatus = true;

    LOGD("✓ Successfully connected to Redis server: ", host, ":", port);
    LOGD("✓ Redis connection status set to TRUE");
    LOGD("✓ Channel: ", redisChannel, ", Format: ", dataFormat, ", Channels: ", numChannels, ", Sample Rate: ", sampleRate);
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
    redisHost = host;
}

void RedisDataThread::setRedisPort(int port)
{
    redisPort = port;
}

void RedisDataThread::setRedisPassword(const String& password)
{
    redisPassword = password;
}

void RedisDataThread::setRedisChannel(const String& channel)
{
    redisChannel = channel;
}

void RedisDataThread::setSampleRate(float rate)
{
    sampleRate = rate;
}

void RedisDataThread::setNumChannels(int channels)
{
    numChannels = channels;
}

void RedisDataThread::setDataFormat(const String& format)
{
    dataFormat = format;
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

    LOGD("Retrieving latest ", numRecords, " records from Redis channel: ", redisChannel);

    // Use LRANGE to get the latest records without removing them from the list
    // LRANGE key start stop - gets elements from start to stop (inclusive)
    // Use negative indices to get from the end: -1 is last element, -2 is second to last, etc.
    int startIndex = -(numRecords);
    int stopIndex = -1;

    redisReply* reply = (redisReply*)redisCommand(redisCtx,
        "LRANGE %s %d %d", redisChannel.toRawUTF8(), startIndex, stopIndex);

    if (reply == nullptr)
    {
        LOGE("LRANGE command returned null reply");
        handleRedisError("LRANGE command failed");
        return records;
    }

    LOGD("LRANGE reply type: ", reply->type, ", elements: ",
         (reply->type == REDIS_REPLY_ARRAY ? reply->elements : 0));

    if (reply->type == REDIS_REPLY_ARRAY)
    {
        LOGD("Retrieved ", reply->elements, " records from Redis");

        // Add records to array (they come in chronological order, oldest first)
        for (size_t i = 0; i < reply->elements; i++)
        {
            if (reply->element[i]->type == REDIS_REPLY_STRING)
            {
                String record(reply->element[i]->str, reply->element[i]->len);
                records.add(record);
                LOGD("Record ", i, " length: ", record.length(), " bytes");
            }
        }
    }
    else if (reply->type == REDIS_REPLY_NIL)
    {
        LOGD("No data available in Redis channel");
    }
    else
    {
        LOGE("Unexpected LRANGE reply type: ", reply->type);
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
    LOGD("Redis channel: ", redisChannel);
    LOGD("Data format: ", dataFormat);
    LOGD("Expected channels: ", numChannels);
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

        // Check data availability
        redisReply* lenReply = (redisReply*)redisCommand(redisCtx, "LLEN %s", redisChannel.toRawUTF8());
        if (lenReply && lenReply->type == REDIS_REPLY_INTEGER)
        {
            LOGD("✓ Redis channel '", redisChannel, "' has ", lenReply->integer, " samples available");
            freeReplyObject(lenReply);
        }
        else
        {
            LOGE("❌ Cannot check Redis channel length");
            if (lenReply) freeReplyObject(lenReply);
        }
    }
    else
    {
        LOGE("❌ Redis context is null");
        return false;
    }
#endif

    isAcquiring = true;
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
    for (int ch = 0; ch < numChannels; ch++)
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

    DataBuffer* buffer = new DataBuffer(numChannels, bufferSize);
    sourceBuffers.add(buffer);

    LOGD("Redis DataThread buffers resized: ", numChannels, " channels, buffer size: ", bufferSize);
}

bool RedisDataThread::updateBuffer()
{
    static int64 lastLogTime = 0;
    static int64 samplesProcessed = 0;
    static int64 lastSampleCount = 0;

    // Check acquisition state
    if (!isAcquiring.load())
    {
        LOGD("updateBuffer: Not acquiring, returning false");
        return false;
    }

    if (!connectionStatus.load())
    {
        LOGE("updateBuffer: Redis not connected, returning false");
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
        lastLogTime = currentTime;
        lastSampleCount = samplesProcessed;
    }

    // Log BLPOP command execution
    LOGD("Executing BLPOP command on channel: ", redisChannel);

    // Get data from Redis (blocking call with 1 second timeout)
    redisReply* reply = (redisReply*)redisCommand(redisCtx,
        "BLPOP %s 1", redisChannel.toRawUTF8());

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

        if (success && channelData.size() == numChannels)
        {
            LOGD("Channel data validated: ", channelData.size(), " channels (expected: ", numChannels, ")");

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
                    samplesProcessed++;
                    if (samplesProcessed % 1000 == 0) // Log every 1000 samples
                    {
                        LOGD("Successfully processed ", samplesProcessed, " samples total");
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
            LOGE("Channel count mismatch: got ", channelData.size(), " channels, expected ", numChannels);
        }
    }
    else
    {
        LOGE("Unexpected BLPOP reply format: type=", reply->type, ", elements=", (reply->type == REDIS_REPLY_ARRAY ? reply->elements : 0));
    }

    freeReplyObject(reply);
    LOGD("updateBuffer returning: ", success);
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
        LOGD("Found channels array with ", channelCount, " elements (expected: ", numChannels, ")");

        if (channelCount != numChannels)
        {
            LOGE("Channel count mismatch: JSON has ", channelCount, " channels, expected ", numChannels);
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
    if (numSamples != numChannels)
    {
        LOGE("Binary data contains ", numSamples, " samples, expected ", numChannels);
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
