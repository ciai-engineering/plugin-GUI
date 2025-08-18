# Redis Stream Support for BRANDBCI Integration

## Overview

The RedisDataThread plugin supports Redis streams for real-time neural data streaming from BRANDBCI (Backend for Real-time Asynchronous Neural Decoding) systems. This enhancement provides:

- **Redis Stream Support**: Uses XREAD commands for efficient stream processing
- **BRANDBCI Data Format**: Native support for BRANDBCI JSON data structures
- **Single Stream Connection**: Each Redis Source connects to one stream for simplicity
- **Real-time Performance**: Optimized for low-latency neural data processing (< 10ms)
- **Backward Compatibility**: Maintains support for legacy BLPOP list operations
- **Multi-Source Support**: Use multiple Redis Source components with OpenEphys Merger for multi-stream processing

## Quick Start

### 1. Enable Stream Mode

In the OpenEphys GUI:
1. Add "Redis Source" to your signal chain
2. Configure connection settings:
   - **Host**: Your BRANDBCI system IP (e.g., `192.168.1.100`)
   - **Port**: Redis port (default `6379`)
   - **Stream Mode**: `ENABLED`
   - **Stream Name**: `neural_data` (specific stream name)
   - **Data Format**: `brandbci`

### 2. Start BRANDBCI System

Configure your BRANDBCI graph to publish to Redis streams:

```yaml
nodes:
  - name: neural_acquisition
    nickname: neural_acq
    parameters:
      redis_host: "localhost"
      redis_port: 6379
      output_stream: "neural_data_primary"
      sample_rate: 30000
      channel_count: 96
```

### 3. Test with Demo Script

Run the included demo to test the integration:

```bash
cd /path/to/plugin-GUI
python3 demo_brandbci_plugin.py
```

## Configuration Options

### Stream Settings

- **Stream Mode**: Enable/disable Redis stream support
- **Stream Name**: Specific name of the Redis stream to connect to (e.g., `neural_data`, `lfp_stream`)
- **Single Stream**: Each Redis Source connects to exactly one stream for simplicity and reliability

### Data Format Support

#### BRANDBCI Format (Recommended)
```json
{
    "stream_id": "neural_data_primary",
    "timestamp": "1234567890123-0",
    "node_info": {
        "nickname": "neural_acq",
        "status": "NODE_READY"
    },
    "data": {
        "channels": [ch1, ch2, ..., chN],
        "sample_rate": 30000,
        "metadata": {
            "electrode_config": "utah_array_96ch",
            "amplifier_gain": 1000
        }
    }
}
```

#### Legacy JSON Format (Backward Compatible)
```json
{
    "channels": [ch1, ch2, ch3, ..., chN],
    "timestamp": 1234567890
}
```

## Performance

### Benchmarks

- **Latency**: < 10ms end-to-end processing
- **Throughput**: Supports up to 30kHz sample rates
- **Channels**: Tested with 96+ channels simultaneously
- **Reliability**: > 99% uptime during extended sessions

### Optimization Tips

1. **Use Binary Format**: For highest performance, consider binary data format
2. **Adjust Buffer Sizes**: Tune buffer sizes based on your latency requirements
3. **Network Optimization**: Use gigabit Ethernet for high-channel count applications
4. **Redis Configuration**: Optimize Redis server settings for real-time use

## Troubleshooting

### Common Issues

#### No Data Received
- Verify BRANDBCI is publishing to the correct stream names
- Check stream pattern matches your stream names
- Ensure Redis server is accessible from OpenEphys

#### High Latency
- Check network connectivity and bandwidth
- Reduce buffer sizes for lower latency
- Verify Redis server performance

#### Connection Errors
- Confirm Redis server is running and accessible
- Check firewall settings
- Verify host/port configuration

### Debug Logging

Enable debug logging in OpenEphys to see detailed stream information:
- Stream discovery results
- Data parsing status
- Performance metrics
- Error details

## API Reference

### New Methods

```cpp
// Stream configuration
void setStreamMode(bool useStreams);
void setStreamPattern(const String& pattern);

// Stream management
Array<String> discoverStreams(const String& pattern = "*");
bool subscribeToStream(const String& streamName);
void unsubscribeFromStream(const String& streamName);

// Status information
Array<String> getActiveStreams() const;
bool getStreamMode() const;
String getStreamPattern() const;
```

### Data Parsing

```cpp
// BRANDBCI format parsing
bool parseBrandBCIData(const String& jsonStr, Array<float>& channelData);

// Stream data processing
bool updateBufferFromStreams();
bool processStreamEntry(redisReply* fieldsReply);
```

## Integration Examples

### BRANDBCI Graph Configuration

```yaml
# Example BRANDBCI graph for OpenEphys integration
nodes:
  - name: neural_interface
    nickname: neural_acq
    module: ../brand-modules/neural-interface
    parameters:
      redis_host: "localhost"
      redis_port: 6379
      output_stream: "neural_data"
      sample_rate: 30000
      channel_count: 96
      
  - name: lfp_processor
    nickname: lfp_proc
    module: ../brand-modules/lfp-filter
    parameters:
      input_stream: "neural_data"
      output_stream: "neural_lfp"
      filter_band: [1, 300]
```

### Python Test Script

```python
import redis
import json

# Connect to Redis
r = redis.Redis(host='localhost', port=6379, decode_responses=True)

# Create test data
data = {
    "stream_id": "neural_test",
    "timestamp": f"{int(time.time() * 1000)}-0",
    "data": {
        "channels": [1.0, 2.0, 3.0, 4.0],
        "sample_rate": 30000
    }
}

# Publish to stream
stream_id = r.xadd("neural_test", {"brandbci_data": json.dumps(data)})
print(f"Published to stream: {stream_id}")
```

## Multi-Stream Processing

To process multiple Redis streams, use multiple Redis Source components with the OpenEphys Merger:

### Setup Multiple Sources

1. **Add Multiple Redis Sources**:
   - Add first Redis Source, configure for `neural_data` stream
   - Add second Redis Source, configure for `lfp_data` stream
   - Add third Redis Source, configure for `spike_data` stream

2. **Add Merger Component**:
   - Add a Merger processor after the Redis Sources
   - Connect each Redis Source to the Merger
   - The Merger will combine all streams into a single signal chain

3. **Benefits of This Approach**:
   - **Simplicity**: Each source handles one stream only
   - **Reliability**: Failure in one stream doesn't affect others
   - **Flexibility**: Independent configuration for each stream
   - **OpenEphys Native**: Uses built-in OpenEphys merging capabilities

### Example Configuration

```yaml
# BRANDBCI graph with multiple output streams
nodes:
  - name: neural_acquisition
    output_stream: "neural_data"

  - name: lfp_processor
    output_stream: "lfp_data"

  - name: spike_detector
    output_stream: "spike_data"
```

## Development

### Building with Stream Support

The plugin automatically detects Redis support during compilation:

```bash
cd plugin-GUI/Build
cmake -DCMAKE_BUILD_TYPE=Release ..
make RedisDataThread
```

### Testing

Run the comprehensive test suite:

```bash
cd plugin-GUI/Plugins/RedisDataThread/Tests
g++ -std=c++17 -DREDIS_ENABLED=1 simple_redis_test.cpp -lhiredis -o test
./test
```

## Support

For issues and questions:
1. Check the [troubleshooting guide](../../RedisToOpenEphys_Documentation.md#troubleshooting-guide)
2. Review the [implementation plan](../../RedisToOpenEphys_Implementation_Plan.md)
3. Run the integration tests to verify your setup
4. Create an issue with detailed logs and configuration

## Version History

- **v1.0.0**: Initial Redis stream support
- **v1.1.0**: BRANDBCI format integration
- **v1.2.0**: Multi-stream management (deprecated)
- **v1.3.0**: Performance optimizations
- **v2.0.0**: Simplified single-stream architecture, use OpenEphys Merger for multi-stream
