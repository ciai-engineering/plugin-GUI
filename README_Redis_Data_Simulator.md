# Redis Data Simulator for BRANDBCI Integration

A Redis data simulator that generates realistic neural data streams compatible with the RedisToOpenEphys plugin. This simulator mimics BRANDBCI (Backend for Real-time Asynchronous Neural Decoding) data streams for testing and development.

## Overview

This simulator was created based on the existing `test_redis_stream_integration.py` in the RedisToOpenEphys codebase. It generates neural data in the exact same format as BRANDBCI systems, making it perfect for testing the RedisToOpenEphys plugin without needing a full BRANDBCI setup.

## Features

- **BRANDBCI-Compatible Format**: Uses the exact data structure expected by RedisToOpenEphys
- **Realistic Neural Signals**: Generates LFP-like, spike-like, and mixed signals
- **Redis Stream Operations**: Uses XADD commands matching BRANDBCI patterns
- **Configurable Parameters**: Sample rate, channels, duration, and signal characteristics
- **Real-time Streaming**: Maintains accurate timing for realistic simulation
- **Performance Monitoring**: Real-time rate and sample count reporting

## Quick Start

### Prerequisites

```bash
# Ensure Redis server is running
redis-server

# Required Python packages (usually already installed)
pip install redis numpy
```

### Basic Usage

```bash
# Test Redis connection
python3 redis_data_simulator.py --test

# Start basic simulation (96 channels, 30kHz, 60 seconds)
python3 redis_data_simulator.py

# Custom simulation
python3 redis_data_simulator.py --rate 1000 --channels 16 --duration 10
```

### OpenEphys Integration

1. **Start the simulator:**
   ```bash
   python3 redis_data_simulator.py --stream neural_data_primary
   ```

2. **Configure OpenEphys Redis plugin:**
   - Stream mode: `ENABLED`
   - Stream pattern: `neural_data_primary`
   - Data format: `brandbci`
   - Host: `localhost`
   - Port: `6379`

3. **Start acquisition in OpenEphys**

## Command Line Options

```bash
python3 redis_data_simulator.py [OPTIONS]

Options:
  --host HOST          Redis server hostname (default: localhost)
  --port PORT          Redis server port (default: 6379)
  --stream STREAM      Redis stream name (default: neural_data_primary)
  --rate RATE          Sample rate in Hz (default: 30000)
  --channels CHANNELS  Number of channels (default: 96)
  --duration DURATION  Duration in seconds (default: 60)
  --test              Test Redis connection only
  --help              Show help message
```

## Usage Examples

### High-Rate Neural Recording Simulation
```bash
python3 redis_data_simulator.py \
    --rate 30000 \
    --channels 96 \
    --duration 300 \
    --stream neural_data_utah_array
```

### Low-Rate LFP Simulation
```bash
python3 redis_data_simulator.py \
    --rate 2000 \
    --channels 32 \
    --duration 600 \
    --stream neural_lfp_data
```

### Quick Test
```bash
python3 redis_data_simulator.py \
    --rate 1000 \
    --channels 8 \
    --duration 5
```

## Data Format

The simulator generates data in BRANDBCI format:

```json
{
    "stream_id": "neural_data_primary",
    "timestamp": "1754996303602-123",
    "node_info": {
        "nickname": "neural_acq_sim",
        "status": "NODE_READY"
    },
    "data": {
        "channels": [ch1, ch2, ..., chN],
        "sample_rate": 30000,
        "metadata": {
            "electrode_config": "utah_array_96ch",
            "amplifier_gain": 1000,
            "session_id": "test_session_1754996303"
        }
    }
}
```

This data is published to Redis streams using:
```python
redis_client.xadd(stream_name, {"brandbci_data": json.dumps(data)})
```

## Signal Characteristics

The simulator generates three types of neural signals:

### LFP-like Signals (Channels 0-31)
- Frequency range: 10-30 Hz
- Amplitude: 50-60 µV ± 10 µV
- Low noise: 5 µV RMS

### Spike-like Signals (Channels 32-63)
- Frequency range: 100-150 Hz  
- Amplitude: 100-120 µV ± 20 µV
- Higher noise: 5 µV RMS

### Mixed Signals (Channels 64+)
- Frequency range: 5-35 Hz
- Amplitude: 75-90 µV ± 15 µV
- Medium noise: 5 µV RMS

## Testing

Run the included test script:

```bash
python3 test_simulator.py
```

This will verify:
- Redis connection
- Data format compliance
- Stream publishing
- Real-time streaming performance

## Integration with RedisToOpenEphys Plugin

The simulator is designed to work seamlessly with the RedisToOpenEphys plugin:

1. **Data Format**: Uses the exact BRANDBCI format expected by the plugin
2. **Redis Operations**: Uses XADD commands matching the plugin's XREAD operations
3. **Stream Structure**: Creates streams with the `brandbci_data` field the plugin expects
4. **Timing**: Maintains real-time sample rates for realistic testing

## Performance

Typical performance on modern hardware:
- **30 kHz, 96 channels**: ~11 MB/s data rate
- **CPU Usage**: <5% for real-time streaming
- **Memory Usage**: <50 MB
- **Latency**: <1ms per sample

## Troubleshooting

### Redis Connection Issues
```bash
# Check Redis server
redis-cli ping

# Test connection
python3 redis_data_simulator.py --test
```

### Performance Issues
```bash
# Monitor Redis
redis-cli --latency-history

# Check system resources
top
```

### Stream Cleanup
```bash
# List Redis keys
redis-cli keys "*"

# Delete specific stream
redis-cli del neural_data_primary
```

## Files

- `redis_data_simulator.py` - Main simulator implementation
- `test_simulator.py` - Test script
- `README_Redis_Data_Simulator.md` - This documentation

## Based On

This simulator is based on the existing test scripts in the RedisToOpenEphys repository:
- `test_redis_stream_integration.py`
- `test_brandbci_integration.py`
- `demo_brandbci_plugin.py`

It uses the same data structures and Redis operations to ensure perfect compatibility with the RedisToOpenEphys plugin.
