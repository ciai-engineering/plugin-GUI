# Redis DataThread Plugin

A plugin for the Open Ephys GUI that enables real-time data streaming from Redis servers.

## Overview

The Redis DataThread plugin allows you to stream electrophysiology data from a Redis server directly into the Open Ephys GUI. This is useful for:

- Real-time data acquisition from external systems
- Streaming preprocessed data from analysis pipelines
- Integration with distributed data processing systems
- Testing and simulation with synthetic data

## Features

- **Real-time streaming**: Uses Redis BLPOP for efficient blocking data retrieval
- **Multiple data formats**: Supports JSON and binary data formats
- **Configurable connection**: Host, port, password, and channel settings
- **Error handling**: Automatic reconnection and robust error recovery
- **Thread-safe**: Safe concurrent data acquisition and GUI interaction
- **Graceful fallback**: Works without Redis (shows configuration error)

## Installation

### Prerequisites

1. **Open Ephys GUI**: This plugin requires the Open Ephys GUI to be compiled
2. **hiredis library** (optional but recommended):
   ```bash
   # Ubuntu/Debian
   sudo apt-get install libhiredis-dev
   
   # macOS
   brew install hiredis
   
   # Windows
   # Download from https://github.com/redis/hiredis
   ```

### Building the Plugin

The plugin is built automatically with the main Open Ephys GUI:

```bash
cd plugin-GUI/Build
cmake -DCMAKE_BUILD_TYPE=Release ..
make RedisDataThread
```

The compiled plugin will be available at: `Build/Release/plugins/RedisDataThread.so`

### Without hiredis

If hiredis is not available, the plugin will compile with a stub implementation that shows an error message in the GUI.

## Usage

### 1. Setup Redis Server

Install and start a Redis server:

```bash
# Install Redis
sudo apt-get install redis-server  # Ubuntu/Debian
brew install redis                 # macOS

# Start Redis server
redis-server

# Test Redis connection
redis-cli ping  # Should return "PONG"
```

### 2. Configure the Plugin

1. Start the Open Ephys GUI
2. Click the "+" button to add a new processor
3. Select "Redis Source" from the Sources category
4. Configure the connection settings:
   - **Host**: Redis server hostname (default: localhost)
   - **Port**: Redis server port (default: 6379)
   - **Password**: Redis password (if required)
   - **Channel**: Redis list key to read from (default: openephys_data)
   - **Sample Rate**: Expected sample rate in Hz (default: 30000)
   - **Channels**: Number of channels in each data packet (default: 32)
   - **Format**: Data format - JSON or Binary (default: JSON)

### 3. Send Data to Redis

#### JSON Format

Send data using the Redis CLI:

```bash
# Single data packet (32 channels)
redis-cli LPUSH openephys_data '{"channels": [1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0, 10.0, 11.0, 12.0, 13.0, 14.0, 15.0, 16.0, 17.0, 18.0, 19.0, 20.0, 21.0, 22.0, 23.0, 24.0, 25.0, 26.0, 27.0, 28.0, 29.0, 30.0, 31.0, 32.0], "timestamp": 1234567890}'
```

#### Python Example

```python
import redis
import json
import numpy as np
import time

# Connect to Redis
r = redis.Redis(host='localhost', port=6379, db=0)

# Generate and send continuous data
def send_data_stream(duration_seconds=10, sample_rate=30000, num_channels=32):
    samples_per_packet = 1  # Send one sample at a time
    interval = 1.0 / sample_rate
    
    for i in range(int(duration_seconds * sample_rate)):
        # Generate synthetic data (sine waves with different frequencies)
        channels = []
        for ch in range(num_channels):
            freq = 10 + ch  # Different frequency for each channel
            value = 100 * np.sin(2 * np.pi * freq * i / sample_rate)
            channels.append(float(value))
        
        # Create data packet
        packet = {
            "channels": channels,
            "timestamp": int(time.time() * 1000)
        }
        
        # Send to Redis
        r.lpush("openephys_data", json.dumps(packet))
        
        # Wait for next sample
        time.sleep(interval)
        
        if i % sample_rate == 0:  # Print progress every second
            print(f"Sent {i} samples...")

# Start streaming
send_data_stream()
```

### 4. Start Acquisition

1. Click the "Connect" button in the Redis Source editor
2. Verify the status shows "Connected to localhost:6379"
3. Start acquisition in the Open Ephys GUI
4. Data should now flow from Redis into the signal chain

## Data Formats

### JSON Format

```json
{
    "channels": [ch1, ch2, ch3, ..., chN],
    "timestamp": 1234567890
}
```

- `channels`: Array of float values, one per channel
- `timestamp`: Optional timestamp (milliseconds since epoch)

### Binary Format

Raw binary data as consecutive 32-bit floats:
```
[float32][float32][float32]...[float32]
```

The number of floats must match the configured channel count.

## Configuration

### Redis Settings

- **Host**: Redis server hostname or IP address
- **Port**: Redis server port (usually 6379)
- **Password**: Authentication password (leave empty if not required)
- **Channel**: Redis list key to read data from

### Data Settings

- **Sample Rate**: Expected sample rate in Hz
- **Channels**: Number of channels per data packet
- **Format**: JSON or Binary data format

## Troubleshooting

### Plugin Not Loading

1. Check that the plugin file exists: `Build/Release/plugins/RedisDataThread.so`
2. Verify the Open Ephys GUI can find the plugin directory
3. Check the console output for plugin loading errors

### Connection Issues

1. Verify Redis server is running: `redis-cli ping`
2. Check host and port settings
3. Verify network connectivity
4. Check Redis authentication settings

### No Data Received

1. Verify data is being sent to the correct Redis channel
2. Check the data format matches the plugin configuration
3. Ensure the number of channels matches the configuration
4. Check Redis server logs for errors

### Performance Issues

1. Reduce the data sending rate
2. Use binary format instead of JSON for better performance
3. Increase Redis server memory limits
4. Use Redis pipelining for bulk data sending

## Development

### Building from Source

```bash
git clone https://github.com/open-ephys/plugin-GUI.git
cd plugin-GUI
# Add RedisDataThread plugin files to Plugins/RedisDataThread/
mkdir Build && cd Build
cmake -DCMAKE_BUILD_TYPE=Release ..
make RedisDataThread
```

### Testing

Run the included test script:

```bash
python3 test_redis_plugin.py
```

## License

This plugin is licensed under the GNU General Public License v3.0, same as the Open Ephys GUI.

## Contributing

1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Add tests for new functionality
5. Submit a pull request

## Support

For issues and questions:

1. Check the troubleshooting section above
2. Search existing GitHub issues
3. Create a new issue with detailed information about your setup and the problem
