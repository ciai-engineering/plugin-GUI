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
    LOGD("RedisDataThread constructor");
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
    // Disconnect existing connection
    disconnectFromRedis();

    // Create new connection
    redisCtx = redisConnect(host.toRawUTF8(), port);

    if (redisCtx == nullptr || redisCtx->err)
    {
        if (redisCtx)
        {
            LOGE("Redis connection error: ", redisCtx->errstr);
            redisFree(redisCtx);
            redisCtx = nullptr;
        }
        else
        {
            LOGE("Redis connection error: Can't allocate redis context");
        }
        connectionStatus = false;
        return false;
    }

    // Authenticate if password is provided
    if (password.isNotEmpty())
    {
        redisReply* reply = (redisReply*)redisCommand(redisCtx, "AUTH %s", password.toRawUTF8());
        if (reply == nullptr || reply->type == REDIS_REPLY_ERROR)
        {
            LOGE("Redis authentication failed");
            if (reply) freeReplyObject(reply);
            disconnectFromRedis();
            return false;
        }
        freeReplyObject(reply);
    }

    // Save connection parameters
    redisHost = host;
    redisPort = port;
    redisPassword = password;
    connectionStatus = true;

    LOGD("Connected to Redis server: ", host, ":", port);
    return true;
#else
    LOGE("Redis support not compiled. Install hiredis library and recompile.");
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

bool RedisDataThread::startAcquisition()
{
    if (!isConnected())
    {
        LOGE("Cannot start acquisition: not connected to Redis");
        return false;
    }

    if (isAcquiring.load())
    {
        LOGC("Acquisition already running");
        return true;
    }

    // Reset sample counter
    currentSampleNumber = 0;

    isAcquiring = true;
    LOGD("Redis data acquisition started");
    return true;
}

bool RedisDataThread::stopAcquisition()
{
    if (!isAcquiring.load())
    {
        return true;
    }

    isAcquiring = false;
    LOGD("Redis data acquisition stopped");
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
    if (!isAcquiring.load() || !connectionStatus.load())
    {
        return false;
    }

#ifdef REDIS_ENABLED
    // Get data from Redis (blocking call with 1 second timeout)
    redisReply* reply = (redisReply*)redisCommand(redisCtx,
        "BLPOP %s 1", redisChannel.toRawUTF8());

    if (reply == nullptr)
    {
        handleRedisError("BLPOP command failed");
        return false;
    }

    if (reply->type == REDIS_REPLY_NIL)
    {
        // Timeout, no data available
        freeReplyObject(reply);
        return true; // Not an error
    }

    bool success = false;
    if (reply->type == REDIS_REPLY_ARRAY && reply->elements == 2)
    {
        // Parse data based on format
        Array<float> channelData;

        if (dataFormat == "json")
        {
            String dataStr(reply->element[1]->str, reply->element[1]->len);
            success = parseJsonData(dataStr, channelData);
        }
        else if (dataFormat == "binary")
        {
            success = parseBinaryData(reply->element[1]->str,
                                    reply->element[1]->len, channelData);
        }

        if (success && channelData.size() == numChannels)
        {
            // Prepare timestamp and sample number
            int64 sampleNumber = currentSampleNumber.fetch_add(1);
            double timestamp = Time::getMillisecondCounterHiRes() / 1000.0;
            uint64 eventCode = 0;

            // Add to DataBuffer
            DataBuffer* buffer = getBufferAddress(0);
            if (buffer)
            {
                int written = buffer->addToBuffer(channelData.getRawDataPointer(),
                                                &sampleNumber,
                                                &timestamp,
                                                &eventCode,
                                                1);
                success = (written > 0);
            }
        }
    }

    freeReplyObject(reply);
    return success;
#else
    // Redis not available, return false to stop acquisition
    return false;
#endif
}

bool RedisDataThread::parseJsonData(const String& jsonStr, Array<float>& channelData)
{
    try
    {
        var jsonData = JSON::parse(jsonStr);

        if (!jsonData.isObject())
        {
            LOGE("Invalid JSON format: not an object");
            return false;
        }

        var channels = jsonData["channels"];
        if (!channels.isArray())
        {
            LOGE("Invalid JSON format: missing 'channels' array");
            return false;
        }

        channelData.clear();
        for (int i = 0; i < channels.size(); i++)
        {
            if (channels[i].isDouble() || channels[i].isInt())
            {
                channelData.add((float)channels[i]);
            }
            else
            {
                LOGE("Non-numeric value in channels array at index ", i);
                return false;
            }
        }

        return true;
    }
    catch (...)
    {
        LOGE("Exception parsing JSON data");
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
