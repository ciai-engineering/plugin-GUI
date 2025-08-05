# RedisToOpenEphys Plugin Documentation

## Table of Contents
1. [Project Overview](#project-overview)
2. [Technical Specifications](#technical-specifications)
3. [Installation and Setup](#installation-and-setup)
4. [Usage Guidelines](#usage-guidelines)
5. [API Documentation](#api-documentation)
6. [Troubleshooting Guide](#troubleshooting-guide)
7. [Development Guide](#development-guide)

## Project Overview

### Objectives
The RedisToOpenEphys plugin enables seamless integration between BRANDBCI (Backend for Real-time Asynchronous Neural Decoding) and the OpenEphys GUI, providing real-time neural data visualization and analysis capabilities.

### Key Features
- **Real-time Streaming**: Direct integration with BRANDBCI Redis streams
- **Multi-stream Support**: Handle multiple concurrent neural data streams
- **Format Flexibility**: Support for various neural data formats and protocols
- **Stream Discovery**: Automatic detection and configuration of available streams
- **Performance Optimized**: Low-latency data processing for real-time applications
- **Robust Error Handling**: Automatic reconnection and graceful error recovery

### Architecture Overview
```
BRANDBCI Nodes → Redis Server → RedisToOpenEphys Plugin → OpenEphys GUI
                     ↓                    ↓                      ↓
                Stream Data         Data Processing        Visualization
                Metadata           Buffer Management       Signal Analysis
                Node Status        Error Handling         Recording
```

## Technical Specifications

### System Requirements
- **Operating System**: Linux (Ubuntu 20.04+), macOS (10.15+), Windows (10+)
- **OpenEphys GUI**: Version 0.6.0 or later
- **Redis Server**: Version 6.0 or later
- **Memory**: Minimum 4GB RAM (8GB+ recommended for high-channel count)
- **Network**: Stable connection to BRANDBCI system

### Supported Data Formats

#### BRANDBCI Stream Format
```json
{
    "stream_id": "neural_data_001",
    "timestamp": "1234567890123-0",
    "data": {
        "channels": [ch1, ch2, ..., chN],
        "sample_rate": 30000,
        "metadata": {
            "node_id": "acquisition_node",
            "session_id": "session_001"
        }
    }
}
```

#### Legacy JSON Format (Backward Compatibility)
```json
{
    "channels": [ch1, ch2, ch3, ..., chN],
    "timestamp": 1234567890
}
```

#### Binary Format
- Raw 32-bit float arrays
- Channel-interleaved data
- Configurable byte order

### Performance Specifications
- **Latency**: < 10ms end-to-end
- **Throughput**: Up to 1000 Hz sample rate
- **Channels**: Support for 1-512 channels
- **Memory Usage**: < 100MB for typical configurations
- **CPU Usage**: < 5% on modern systems

## Installation and Setup

### Prerequisites

#### 1. Install Redis Server
```bash
# Ubuntu/Debian
sudo apt-get update
sudo apt-get install redis-server

# macOS
brew install redis

# Windows
# Download from https://redis.io/download
```

#### 2. Install hiredis Library
```bash
# Ubuntu/Debian
sudo apt-get install libhiredis-dev

# macOS
brew install hiredis

# Windows
# Download from https://github.com/redis/hiredis
```

#### 3. OpenEphys GUI
Ensure you have the OpenEphys GUI compiled and working. See [OpenEphys documentation](https://open-ephys.github.io/gui-docs/) for details.

### Plugin Installation

#### Method 1: Build with OpenEphys GUI
```bash
cd plugin-GUI
mkdir -p Build && cd Build
cmake -DCMAKE_BUILD_TYPE=Release ..
make RedisDataThread
```

#### Method 2: Standalone Build
```bash
cd plugin-GUI/Plugins/RedisDataThread
mkdir build && cd build
cmake ..
make
```

### Configuration

#### 1. Redis Server Setup
```bash
# Start Redis server
redis-server

# Test connection
redis-cli ping  # Should return "PONG"
```

#### 2. BRANDBCI Integration
```bash
# Configure BRANDBCI to use Redis streams
# Edit your BRANDBCI graph configuration:
nodes:
  - name: neural_acquisition
    parameters:
      redis_host: "localhost"
      redis_port: 6379
      output_stream: "neural_data"
```

#### 3. Plugin Configuration
1. Start OpenEphys GUI
2. Add "Redis Source" from Sources category
3. Configure connection settings:
   - Host: BRANDBCI Redis server address
   - Port: Redis port (default 6379)
   - Stream Pattern: "neural_*" (for auto-discovery)
   - Data Format: "brandbci" or "json"

## Usage Guidelines

### Basic Workflow

#### 1. Start BRANDBCI System
```bash
# Start BRANDBCI supervisor
source setup.sh
supervisor -i 192.168.1.100 -p 6379

# Load and start neural acquisition graph
redis-cli -h 192.168.1.100 -p 6379
XADD supervisor_ipstream * commands startGraph file graphs/neural_acquisition.yaml
```

#### 2. Configure OpenEphys Plugin
1. Open OpenEphys GUI
2. Add Redis Source to signal chain
3. Set connection parameters:
   - Host: 192.168.1.100
   - Port: 6379
   - Stream: neural_data (or use auto-discovery)
4. Click "Connect" button

#### 3. Start Data Acquisition
1. Verify connection status shows "Connected"
2. Check stream list shows available BRANDBCI streams
3. Select desired streams for visualization
4. Start OpenEphys acquisition
5. Monitor real-time neural data

### Advanced Configuration

#### Stream Selection and Filtering
```cpp
// Configure multiple streams
streams:
  - name: "neural_data_primary"
    channels: 64
    sample_rate: 30000
  - name: "neural_data_secondary" 
    channels: 32
    sample_rate: 1000
```

#### Data Preprocessing
- **Filtering**: Built-in bandpass filtering options
- **Scaling**: Automatic gain adjustment
- **Artifact Rejection**: Real-time artifact detection
- **Downsampling**: Configurable decimation

#### Performance Tuning
- **Buffer Size**: Adjust for latency vs stability
- **Thread Priority**: Real-time scheduling options
- **Memory Management**: Configurable buffer pools
- **Network Optimization**: TCP/Unix socket options

## API Documentation

### Core Classes

#### RedisToOpenEphysPlugin
Main plugin class extending DataThread.

**Methods:**
```cpp
bool connectToRedis(const String& host, int port, const String& password);
void disconnectFromRedis();
bool startAcquisition() override;
bool stopAcquisition() override;
bool updateBuffer() override;
```

#### BrandStreamManager
Manages BRANDBCI stream connections and discovery.

**Methods:**
```cpp
Array<String> discoverStreams(const String& pattern);
bool subscribeToStream(const String& streamName);
StreamData readStreamData(const String& streamName, int timeout);
```

#### BrandDataParser
Parses BRANDBCI-specific data formats.

**Methods:**
```cpp
bool parseBrandData(const String& data, Array<float>& channels);
bool validateDataFormat(const String& data);
StreamMetadata extractMetadata(const String& data);
```

### Configuration Parameters

#### Connection Settings
- `redis_host`: Redis server hostname/IP
- `redis_port`: Redis server port
- `redis_password`: Authentication password
- `connection_timeout`: Connection timeout in seconds
- `reconnect_attempts`: Number of reconnection attempts

#### Stream Settings
- `stream_pattern`: Pattern for stream discovery
- `auto_discovery`: Enable automatic stream detection
- `stream_buffer_size`: Buffer size per stream
- `max_streams`: Maximum concurrent streams

#### Data Processing
- `data_format`: "brandbci", "json", or "binary"
- `sample_rate`: Expected sample rate
- `channel_count`: Number of channels per stream
- `timestamp_sync`: Timestamp synchronization mode

### Events and Callbacks

#### Connection Events
```cpp
void onConnectionEstablished();
void onConnectionLost();
void onReconnectionAttempt(int attempt);
```

#### Data Events
```cpp
void onStreamDataReceived(const String& streamName, const StreamData& data);
void onStreamError(const String& streamName, const String& error);
void onBufferOverflow(const String& streamName);
```

## Troubleshooting Guide

### Common Issues

#### Connection Problems

**Issue**: "Failed to connect to Redis server"
**Solutions:**
1. Verify Redis server is running: `redis-cli ping`
2. Check host/port settings
3. Verify network connectivity
4. Check firewall settings
5. Ensure Redis accepts external connections

**Issue**: "Connection timeout"
**Solutions:**
1. Increase connection timeout value
2. Check network latency
3. Verify Redis server performance
4. Check for network congestion

#### Data Issues

**Issue**: "No data received from streams"
**Solutions:**
1. Verify BRANDBCI is publishing data
2. Check stream names and patterns
3. Verify data format compatibility
4. Check Redis memory usage
5. Monitor BRANDBCI node status

**Issue**: "Data format errors"
**Solutions:**
1. Verify data format setting matches BRANDBCI output
2. Check JSON syntax if using JSON format
3. Validate channel count configuration
4. Check for data corruption

#### Performance Issues

**Issue**: "High latency or dropped samples"
**Solutions:**
1. Reduce buffer sizes for lower latency
2. Increase buffer sizes for stability
3. Check system CPU/memory usage
4. Optimize Redis configuration
5. Use binary format for better performance

### Diagnostic Tools

#### Built-in Diagnostics
```cpp
// Enable debug logging
setLogLevel(LogLevel::DEBUG);

// Monitor connection status
String status = getConnectionStatus();

// Check buffer statistics
BufferStats stats = getBufferStatistics();
```

#### External Tools
```bash
# Monitor Redis streams
redis-cli XINFO STREAM neural_data

# Check Redis performance
redis-cli --latency-history

# Monitor network traffic
tcpdump -i any port 6379
```

### Log Analysis

#### Log Levels
- **ERROR**: Critical errors requiring attention
- **WARN**: Warnings that may affect performance
- **INFO**: General operational information
- **DEBUG**: Detailed debugging information

#### Common Log Messages
```
[INFO] Connected to Redis server at 192.168.1.100:6379
[WARN] Stream buffer approaching capacity
[ERROR] Failed to parse data format
[DEBUG] Received 1024 samples from stream neural_data
```

## Development Guide

### Building from Source
```bash
git clone https://github.com/open-ephys/plugin-GUI.git
cd plugin-GUI/Plugins/RedisDataThread
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Debug ..
make
```

### Testing
```bash
# Run unit tests
./test_redis_plugin

# Integration testing with BRANDBCI
python3 test_brandbci_integration.py
```

### Contributing
1. Fork the repository
2. Create feature branch
3. Implement changes with tests
4. Submit pull request
5. Follow code review process

For detailed development guidelines, see [CONTRIBUTING.md](CONTRIBUTING.md).
