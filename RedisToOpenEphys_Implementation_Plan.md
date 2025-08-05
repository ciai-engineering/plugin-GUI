# RedisToOpenEphys Plugin Implementation Plan

## Project Overview

The RedisToOpenEphys plugin is designed to integrate BRANDBCI (Backend for Real-time Asynchronous Neural Decoding) data streams with the OpenEphys GUI for real-time neural data visualization and analysis. This plugin extends the existing Redis DataThread plugin to support BRANDBCI's specific data formats and streaming protocols.

## 1. Technical Analysis

### 1.1 BRANDBCI Redis Data Structure

Based on the BRANDBCI documentation, the system uses Redis streams for inter-process communication with the following characteristics:

**Stream Format:**
```
stream_key ID key value key value ...
```

**Key Data Streams:**
- `<node_nickname>_state`: Node status information
- `<data_stream>`: Arbitrary data streams for neural data
- `supergraph_stream`: Graph metadata and configuration
- `supervisor_status`: System status information

**Data Format:**
- Timestamped entries with millisecond precision
- JSON-based data structures
- Real-time streaming with Redis XREAD commands
- Support for multiple concurrent data streams

### 1.2 Current OpenEphys Redis Plugin Analysis

**Existing Features:**
- Redis connection management (host, port, password)
- JSON and binary data format support
- BLPOP-based data retrieval (blocking list operations)
- Configurable sample rate and channel count
- Real-time data buffering and visualization

**Current Data Format:**
```json
{
    "channels": [ch1, ch2, ch3, ..., chN],
    "timestamp": 1234567890
}
```

### 1.3 Integration Requirements

**Key Differences to Address:**
1. **Stream vs List Operations**: BRANDBCI uses Redis streams (XREAD), current plugin uses lists (BLPOP)
2. **Data Structure**: BRANDBCI may have more complex metadata and multiple data types
3. **Multi-stream Support**: Need to handle multiple concurrent neural data streams
4. **Timestamp Handling**: BRANDBCI uses stream-native timestamps vs embedded timestamps

## 2. Implementation Strategy

### 2.1 Plugin Architecture Enhancement

**Phase 1: Core Infrastructure**
- Extend RedisDataThread to support Redis streams (XREAD operations)
- Add multi-stream connection management
- Implement BRANDBCI-specific data parsers
- Add stream discovery and auto-configuration

**Phase 2: Data Processing**
- Neural data format standardization
- Real-time data validation and error handling
- Buffer management for multiple streams
- Timestamp synchronization across streams

**Phase 3: Advanced Features**
- Stream filtering and selection
- Data preprocessing and conditioning
- Integration with BRANDBCI node status monitoring
- Configuration persistence and profiles

### 2.2 Development Milestones

**Milestone 1: Stream Support (Week 1-2)**
- Implement Redis stream reading capabilities
- Add XREAD command support with timeout handling
- Create stream discovery mechanism
- Basic multi-stream data handling

**Milestone 2: BRANDBCI Integration (Week 3-4)**
- BRANDBCI data format parsing
- Node status monitoring integration
- Supergraph metadata reading
- Stream selection and filtering UI

**Milestone 3: Enhanced Features (Week 5-6)**
- Advanced data preprocessing
- Performance optimization
- Comprehensive error handling
- Configuration management

**Milestone 4: Testing and Documentation (Week 7-8)**
- Integration testing with BRANDBCI
- Performance benchmarking
- Documentation completion
- User guide and tutorials

## 3. Technical Specifications

### 3.1 New Classes and Components

**BrandRedisDataThread** (extends RedisDataThread):
- Stream-based data acquisition
- Multi-stream management
- BRANDBCI protocol support

**BrandRedisStreamManager**:
- Stream discovery and monitoring
- Connection pooling
- Stream metadata management

**BrandDataParser**:
- BRANDBCI data format parsing
- Neural data validation
- Timestamp synchronization

**BrandRedisEditor** (extends RedisDataThreadEditor):
- Stream selection interface
- BRANDBCI-specific configuration
- Real-time stream monitoring

### 3.2 Configuration Parameters

**Connection Settings:**
- Redis host/port/password (existing)
- Stream discovery mode (auto/manual)
- Stream name patterns/filters
- Connection timeout and retry settings

**Data Processing:**
- Neural data format selection
- Channel mapping and scaling
- Timestamp synchronization mode
- Buffer size and management

**BRANDBCI Integration:**
- Node status monitoring
- Supergraph metadata reading
- Stream health monitoring
- Error recovery strategies

### 3.3 Data Flow Architecture

```
BRANDBCI System → Redis Streams → RedisToOpenEphys Plugin → OpenEphys GUI
     ↓                ↓                    ↓                      ↓
Neural Nodes    Stream Data         Data Processing        Visualization
Status Info     Metadata           Buffer Management       Signal Chain
Supergraph      Timestamps         Error Handling         Recording
```

## 4. Dependencies and Requirements

### 4.1 System Requirements
- OpenEphys GUI (latest version)
- Redis server (6.0+)
- hiredis library (1.0+)
- BRANDBCI system (for testing)

### 4.2 Development Dependencies
- CMake (3.15+)
- C++17 compiler
- JUCE framework (included with OpenEphys)
- Redis C++ client libraries

### 4.3 Runtime Dependencies
- Redis server instance
- Network connectivity to BRANDBCI system
- Sufficient memory for data buffering
- Real-time capable system (recommended)

## 5. Risk Assessment and Mitigation

### 5.1 Technical Risks
**Risk**: Stream data format incompatibility
**Mitigation**: Flexible parser architecture with format detection

**Risk**: Performance bottlenecks with multiple streams
**Mitigation**: Efficient buffering and threading strategies

**Risk**: Network connectivity issues
**Mitigation**: Robust reconnection and error recovery

### 5.2 Integration Risks
**Risk**: BRANDBCI system changes
**Mitigation**: Modular design with configurable protocols

**Risk**: OpenEphys API changes
**Mitigation**: Use stable plugin interfaces and version checking

## 6. Success Criteria

### 6.1 Functional Requirements
- [ ] Successfully connect to BRANDBCI Redis streams
- [ ] Real-time neural data visualization in OpenEphys
- [ ] Multi-stream support with stream selection
- [ ] Robust error handling and recovery
- [ ] Configuration persistence and management

### 6.2 Performance Requirements
- [ ] < 10ms latency for data streaming
- [ ] Support for 1000+ Hz sample rates
- [ ] Stable operation for extended periods
- [ ] Minimal CPU and memory overhead
- [ ] Graceful handling of network interruptions

### 6.3 Usability Requirements
- [ ] Intuitive configuration interface
- [ ] Clear status and error reporting
- [ ] Comprehensive documentation
- [ ] Example configurations and tutorials
- [ ] Integration with existing OpenEphys workflows

## 7. Detailed Implementation Steps

### 7.1 Phase 1: Redis Stream Support (Week 1-2)

#### Task 1.1: Replace BLPOP with XREAD
**Objective**: Migrate from Redis list operations to stream operations
**Implementation**:
```cpp
// Current implementation uses BLPOP
redisReply* reply = (redisReply*)redisCommand(redisCtx, "BLPOP %s 1", channel);

// New implementation with XREAD
redisReply* reply = (redisReply*)redisCommand(redisCtx,
    "XREAD BLOCK %d STREAMS %s %s", timeout, streamName, lastId);
```

**Key Changes**:
- Add stream ID tracking for each monitored stream
- Implement stream position management
- Handle stream-specific error conditions
- Add support for multiple concurrent streams

#### Task 1.2: Stream Discovery Mechanism
**Objective**: Automatically discover available BRANDBCI streams
**Implementation**:
```cpp
Array<String> BrandStreamManager::discoverStreams(const String& pattern) {
    Array<String> streams;
    redisReply* reply = (redisReply*)redisCommand(redisCtx, "KEYS %s", pattern.toRawUTF8());
    // Parse reply and filter for valid streams
    return streams;
}
```

#### Task 1.3: Multi-stream Connection Management
**Objective**: Handle multiple simultaneous stream connections
**Features**:
- Connection pooling for efficiency
- Per-stream error handling
- Load balancing across connections
- Automatic reconnection strategies

### 7.2 Phase 2: BRANDBCI Data Parser (Week 2-3)

#### Task 2.1: BRANDBCI Format Parser
**Objective**: Parse BRANDBCI-specific data structures
**Data Format Support**:
```json
{
    "stream_id": "neural_acquisition_001",
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

#### Task 2.2: Metadata Extraction
**Objective**: Extract and utilize BRANDBCI metadata
**Implementation**:
```cpp
struct BrandMetadata {
    String nodeNickname;
    String nodeStatus;
    float sampleRate;
    int channelCount;
    String electrodeConfig;
    float amplifierGain;
    int64 timestamp;
};
```

#### Task 2.3: Data Validation
**Objective**: Validate incoming neural data integrity
**Validation Checks**:
- Channel count consistency
- Sample rate verification
- Timestamp continuity
- Data range validation
- Format compliance

### 7.3 Phase 3: Node Status Monitoring (Week 3-4)

#### Task 3.1: Node Status Integration
**Objective**: Monitor BRANDBCI node health and status
**Implementation**:
```cpp
class StatusMonitor {
    void monitorNodeStatus() {
        // Subscribe to node status streams
        // Parse node state information
        // Update UI status indicators
        // Handle node error conditions
    }
};
```

#### Task 3.2: Supergraph Metadata Reading
**Objective**: Read and utilize BRANDBCI supergraph information
**Features**:
- Graph configuration parsing
- Node parameter extraction
- Stream mapping information
- Session metadata

### 7.4 Phase 4: Enhanced UI (Week 4-5)

#### Task 4.1: Stream Selection Interface
**Objective**: Allow users to select and configure streams
**UI Components**:
- Stream discovery and listing
- Multi-stream selection
- Stream-specific configuration
- Real-time stream status

#### Task 4.2: BRANDBCI Configuration Panel
**Objective**: Provide BRANDBCI-specific configuration options
**Configuration Options**:
- Node status monitoring settings
- Stream filtering and selection
- Data format preferences
- Performance tuning parameters

### 7.5 Phase 5: Performance Optimization (Week 5-6)

#### Task 5.1: Efficient Buffering
**Objective**: Optimize data buffering for real-time performance
**Optimizations**:
- Lock-free circular buffers
- Memory pool allocation
- Cache-friendly data structures
- NUMA-aware memory management

#### Task 5.2: Threading Optimization
**Objective**: Implement efficient multi-threading
**Threading Strategy**:
- Separate threads for each stream
- Producer-consumer pattern
- Thread-safe data structures
- Real-time thread priorities

## 8. Dependencies and Libraries

### 8.1 Required Libraries
- **hiredis**: Redis C client library (>= 1.0.0)
- **JUCE**: GUI framework (included with OpenEphys)
- **nlohmann/json**: JSON parsing (optional, for enhanced JSON support)

### 8.2 Build System Integration
```cmake
# CMakeLists.txt additions
find_package(PkgConfig REQUIRED)
pkg_check_modules(HIREDIS REQUIRED hiredis)

target_link_libraries(RedisDataThread
    ${HIREDIS_LIBRARIES}
    ${JUCE_LIBRARIES}
)
```

### 8.3 Conditional Compilation
```cpp
#ifdef BRANDBCI_SUPPORT
    #include "BrandRedisDataThread.h"
    #define REDIS_PLUGIN_CLASS BrandRedisDataThread
#else
    #define REDIS_PLUGIN_CLASS RedisDataThread
#endif
```

## 9. Testing Strategy

### 9.1 Unit Testing
- Data parser validation
- Stream connection management
- Buffer management
- Error handling scenarios

### 9.2 Integration Testing
- BRANDBCI system integration
- OpenEphys GUI integration
- Performance benchmarking
- Long-duration stability testing

### 9.3 Test Data Generation
```python
# BRANDBCI test data generator
def generate_test_stream():
    return {
        "stream_id": "test_neural_data",
        "timestamp": f"{int(time.time() * 1000)}-0",
        "data": {
            "channels": [random.gauss(0, 100) for _ in range(96)],
            "sample_rate": 30000
        }
    }
```

## Next Steps

1. **Environment Setup**: Prepare development environment with BRANDBCI and OpenEphys
2. **Prototype Development**: Create basic stream reading functionality
3. **Integration Testing**: Test with live BRANDBCI data streams
4. **Feature Implementation**: Add advanced features and optimizations
5. **Documentation**: Complete user guides and technical documentation
6. **Community Testing**: Beta testing with BRANDBCI users
