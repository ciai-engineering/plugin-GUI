# RedisToOpenEphys Plugin Installation Guide

## Table of Contents
1. [System Requirements](#system-requirements)
2. [Prerequisites Installation](#prerequisites-installation)
3. [Plugin Installation](#plugin-installation)
4. [BRANDBCI Integration Setup](#brandbci-integration-setup)
5. [Configuration](#configuration)
6. [Testing and Validation](#testing-and-validation)
7. [Troubleshooting](#troubleshooting)

## System Requirements

### Minimum Requirements
- **Operating System**: Linux (Ubuntu 20.04+), macOS (10.15+), Windows (10+)
- **CPU**: Intel i5 or AMD Ryzen 5 (2.0 GHz+)
- **Memory**: 4GB RAM minimum, 8GB recommended
- **Storage**: 1GB free space for installation
- **Network**: Stable connection to BRANDBCI system

### Recommended Requirements
- **CPU**: Intel i7 or AMD Ryzen 7 (3.0 GHz+)
- **Memory**: 16GB RAM for high-channel count applications
- **Storage**: SSD for optimal performance
- **Network**: Gigabit Ethernet for high-throughput applications

### Real-time Requirements (Optional)
- **Kernel**: PREEMPT_RT kernel for deterministic latency
- **CPU Isolation**: Dedicated CPU cores for real-time processing
- **Memory**: Locked memory pages to prevent swapping

## Prerequisites Installation

### 1. Install Redis Server

#### Ubuntu/Debian
```bash
sudo apt-get update
sudo apt-get install redis-server redis-tools

# Start Redis service
sudo systemctl start redis-server
sudo systemctl enable redis-server

# Verify installation
redis-cli ping  # Should return "PONG"
```

#### macOS
```bash
# Install using Homebrew
brew install redis

# Start Redis service
brew services start redis

# Verify installation
redis-cli ping  # Should return "PONG"
```

#### Windows
1. Download Redis from [https://redis.io/download](https://redis.io/download)
2. Extract and run `redis-server.exe`
3. Test with `redis-cli.exe ping`

### 2. Install hiredis Library

#### Ubuntu/Debian
```bash
sudo apt-get install libhiredis-dev
```

#### macOS
```bash
brew install hiredis
```

#### Windows
1. Download hiredis from [https://github.com/redis/hiredis](https://github.com/redis/hiredis)
2. Build using Visual Studio or MinGW
3. Add to system PATH

### 3. Install Development Tools

#### Ubuntu/Debian
```bash
sudo apt-get install build-essential cmake git
sudo apt-get install libjack-jackd2-dev  # For audio support
```

#### macOS
```bash
xcode-select --install
brew install cmake git
```

#### Windows
1. Install Visual Studio 2019 or later
2. Install CMake from [https://cmake.org/download/](https://cmake.org/download/)
3. Install Git from [https://git-scm.com/download/win](https://git-scm.com/download/win)

## Plugin Installation

### Method 1: Build with OpenEphys GUI (Recommended)

#### 1. Clone OpenEphys GUI Repository
```bash
git clone https://github.com/open-ephys/plugin-GUI.git
cd plugin-GUI
```

#### 2. Add RedisToOpenEphys Plugin
```bash
# The plugin should already be in Plugins/RedisDataThread/
# If not, clone or copy the plugin files there
ls Plugins/RedisDataThread/  # Verify plugin files exist
```

#### 3. Build the Plugin
```bash
mkdir Build && cd Build
cmake -DCMAKE_BUILD_TYPE=Release ..
make RedisDataThread

# For debug build
cmake -DCMAKE_BUILD_TYPE=Debug ..
make RedisDataThread
```

#### 4. Verify Plugin Build
```bash
# Check if plugin was built successfully
ls Release/plugins/RedisDataThread.so  # Linux/macOS
ls Release/plugins/RedisDataThread.dll  # Windows
```

### Method 2: Standalone Plugin Build

#### 1. Navigate to Plugin Directory
```bash
cd plugin-GUI/Plugins/RedisDataThread
```

#### 2. Create Build Directory
```bash
mkdir build && cd build
```

#### 3. Configure and Build
```bash
cmake ..
make

# For Windows with Visual Studio
cmake .. -G "Visual Studio 16 2019"
cmake --build . --config Release
```

### Method 3: Pre-built Binaries (If Available)

#### 1. Download Plugin Binary
```bash
# Download from releases page
wget https://github.com/your-repo/releases/download/v1.0.0/RedisDataThread.so
```

#### 2. Copy to Plugin Directory
```bash
cp RedisDataThread.so /path/to/openephys/plugins/
```

## BRANDBCI Integration Setup

### 1. Install BRANDBCI System

#### Clone BRANDBCI Repository
```bash
git clone https://github.com/brandbci/brand.git
cd brand
```

#### Setup BRANDBCI Environment
```bash
./bootstrap.sh
conda activate rt
make
```

### 2. Configure BRANDBCI for OpenEphys

#### Create Integration Graph
```yaml
# graphs/openephys_integration/openephys_neural.yaml
nodes:
  - name: neural_acquisition
    nickname: neural_acq
    module: ../brand-modules/neural-interface
    parameters:
      redis_host: "localhost"
      redis_port: 6379
      output_stream: "neural_data"
      sample_rate: 30000
      channel_count: 96
      
  - name: data_forwarder
    nickname: data_forward
    module: ../brand-modules/data-bridge
    parameters:
      input_stream: "neural_data"
      output_format: "openephys_compatible"
      redis_host: "localhost"
      redis_port: 6379
```

#### Start BRANDBCI with Integration Graph
```bash
source setup.sh
supervisor -i localhost -p 6379

# In another terminal
redis-cli
XADD supervisor_ipstream * commands startGraph file graphs/openephys_integration/openephys_neural.yaml
```

### 3. Network Configuration

#### For Local Setup
```bash
# Redis configuration for local access
# Edit /etc/redis/redis.conf
bind 127.0.0.1 ::1
port 6379
```

#### For Remote BRANDBCI System
```bash
# Redis configuration for network access
# Edit /etc/redis/redis.conf
bind 0.0.0.0
port 6379
protected-mode no  # Only for trusted networks

# Restart Redis
sudo systemctl restart redis-server
```

#### Firewall Configuration
```bash
# Ubuntu/Debian
sudo ufw allow 6379/tcp

# CentOS/RHEL
sudo firewall-cmd --permanent --add-port=6379/tcp
sudo firewall-cmd --reload
```

## Configuration

### 1. OpenEphys Plugin Configuration

#### Basic Configuration
1. Start OpenEphys GUI
2. Click "+" to add processor
3. Select "Redis Source" from Sources
4. Configure connection:
   - Host: BRANDBCI system IP
   - Port: 6379
   - Stream Pattern: "neural_*"
   - Data Format: "brandbci"

#### Advanced Configuration
```cpp
// Copy brandbci_integration_config.yaml to OpenEphys config directory
cp brandbci_integration_config.yaml ~/.openephys/configs/
```

### 2. Performance Tuning

#### Redis Server Optimization
```bash
# Edit /etc/redis/redis.conf
maxmemory 2gb
maxmemory-policy allkeys-lru
save ""  # Disable persistence for real-time use
tcp-nodelay yes
```

#### System Optimization
```bash
# Increase network buffer sizes
echo 'net.core.rmem_max = 134217728' | sudo tee -a /etc/sysctl.conf
echo 'net.core.wmem_max = 134217728' | sudo tee -a /etc/sysctl.conf
sudo sysctl -p

# Set CPU governor to performance
echo performance | sudo tee /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor
```

## Testing and Validation

### 1. Basic Connectivity Test
```bash
# Test Redis connection
redis-cli -h <brandbci-host> -p 6379 ping

# Test stream creation
redis-cli -h <brandbci-host> -p 6379 XADD test_stream * data "hello"
redis-cli -h <brandbci-host> -p 6379 XREAD STREAMS test_stream 0
```

### 2. Plugin Functionality Test
```bash
# Run the included test script
cd plugin-GUI
python3 test_redis_plugin.py

# Check plugin loading
./Build/Release/open-ephys --test-plugins
```

### 3. Integration Test
```python
# Run BRANDBCI integration test
python3 test_brandbci_integration.py --host <brandbci-host> --port 6379
```

### 4. Performance Validation
```bash
# Monitor Redis performance
redis-cli --latency-history -h <brandbci-host> -p 6379

# Monitor system resources
htop
iotop
```

## Troubleshooting

### Common Issues

#### Plugin Not Loading
**Symptoms**: Plugin doesn't appear in OpenEphys GUI
**Solutions**:
1. Check plugin file exists: `ls Build/Release/plugins/RedisDataThread.*`
2. Verify dependencies: `ldd Build/Release/plugins/RedisDataThread.so`
3. Check OpenEphys logs for loading errors
4. Ensure hiredis library is installed

#### Redis Connection Failed
**Symptoms**: "Failed to connect to Redis server"
**Solutions**:
1. Verify Redis is running: `redis-cli ping`
2. Check network connectivity: `telnet <host> 6379`
3. Verify firewall settings
4. Check Redis configuration (bind address, protected mode)

#### No Data Received
**Symptoms**: Plugin connects but no data flows
**Solutions**:
1. Verify BRANDBCI is publishing data: `redis-cli XREAD STREAMS neural_data 0`
2. Check stream names match configuration
3. Verify data format compatibility
4. Monitor BRANDBCI node status

#### Performance Issues
**Symptoms**: High latency, dropped samples
**Solutions**:
1. Optimize Redis configuration
2. Increase system buffer sizes
3. Use binary data format
4. Enable real-time kernel features
5. Dedicate CPU cores to critical processes

### Log Analysis
```bash
# OpenEphys logs
tail -f ~/.openephys/logs/openephys.log

# Redis logs
tail -f /var/log/redis/redis-server.log

# System logs
journalctl -f -u redis-server
```

### Getting Help
1. Check the [troubleshooting guide](RedisToOpenEphys_Documentation.md#troubleshooting-guide)
2. Search existing GitHub issues
3. Create a new issue with:
   - System information
   - Configuration files
   - Error logs
   - Steps to reproduce

## Next Steps
After successful installation:
1. Review the [Usage Guidelines](RedisToOpenEphys_Documentation.md#usage-guidelines)
2. Explore [Advanced Configuration](RedisToOpenEphys_Documentation.md#advanced-configuration)
3. Set up [Performance Monitoring](RedisToOpenEphys_Documentation.md#performance-monitoring)
4. Configure [Data Recording](RedisToOpenEphys_Documentation.md#data-recording)
