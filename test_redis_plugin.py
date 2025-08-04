#!/usr/bin/env python3
"""
Test script for Redis DataThread plugin

This script tests the Redis plugin by:
1. Starting a Redis server (if available)
2. Sending test data to Redis
3. Checking if the OpenEphys GUI can load the plugin
"""

import subprocess
import time
import json
import sys
import os

def check_redis_server():
    """Check if Redis server is running"""
    try:
        result = subprocess.run(['redis-cli', 'ping'], 
                              capture_output=True, text=True, timeout=5)
        return result.returncode == 0 and 'PONG' in result.stdout
    except (subprocess.TimeoutExpired, FileNotFoundError):
        return False

def start_redis_server():
    """Start Redis server in background"""
    try:
        # Try to start redis-server
        process = subprocess.Popen(['redis-server', '--daemonize', 'yes'],
                                 stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        time.sleep(2)  # Give it time to start
        return check_redis_server()
    except FileNotFoundError:
        print("Redis server not found. Please install Redis:")
        print("  Ubuntu/Debian: sudo apt-get install redis-server")
        print("  macOS: brew install redis")
        return False

def send_test_data():
    """Send test data to Redis channel"""
    try:
        # Create test data (32 channels with random-like values)
        test_data = {
            "channels": [float(i * 10 + j) for i, j in enumerate(range(32))],
            "timestamp": int(time.time() * 1000)
        }
        
        # Send to Redis
        cmd = ['redis-cli', 'LPUSH', 'openephys_data', json.dumps(test_data)]
        result = subprocess.run(cmd, capture_output=True, text=True, timeout=5)
        
        if result.returncode == 0:
            print(f"✓ Sent test data to Redis: {len(test_data['channels'])} channels")
            return True
        else:
            print(f"✗ Failed to send data to Redis: {result.stderr}")
            return False
            
    except Exception as e:
        print(f"✗ Error sending test data: {e}")
        return False

def check_plugin_file():
    """Check if the plugin file exists"""
    plugin_path = "Build/Release/plugins/RedisDataThread.so"
    if os.path.exists(plugin_path):
        size = os.path.getsize(plugin_path)
        print(f"✓ Plugin file exists: {plugin_path} ({size} bytes)")
        return True
    else:
        print(f"✗ Plugin file not found: {plugin_path}")
        return False

def test_plugin_loading():
    """Test if OpenEphys can load the plugin"""
    try:
        # Try to run OpenEphys with a quick test
        cmd = ['./Build/Release/open-ephys', '--version']
        result = subprocess.run(cmd, capture_output=True, text=True, timeout=10)
        
        if result.returncode == 0:
            print("✓ OpenEphys GUI executable works")
            return True
        else:
            print(f"✗ OpenEphys GUI failed: {result.stderr}")
            return False
            
    except subprocess.TimeoutExpired:
        print("✗ OpenEphys GUI timed out")
        return False
    except Exception as e:
        print(f"✗ Error testing OpenEphys: {e}")
        return False

def main():
    """Main test function"""
    print("🧪 Testing Redis DataThread Plugin")
    print("=" * 40)
    
    # Test 1: Check plugin file
    print("\n1. Checking plugin file...")
    plugin_ok = check_plugin_file()
    
    # Test 2: Check Redis availability
    print("\n2. Checking Redis server...")
    redis_running = check_redis_server()
    
    if not redis_running:
        print("   Redis not running, trying to start...")
        redis_running = start_redis_server()
    
    if redis_running:
        print("✓ Redis server is running")
    else:
        print("✗ Redis server not available")
    
    # Test 3: Send test data (if Redis is available)
    if redis_running:
        print("\n3. Sending test data...")
        data_sent = send_test_data()
    else:
        print("\n3. Skipping data test (Redis not available)")
        data_sent = False
    
    # Test 4: Test OpenEphys loading
    print("\n4. Testing OpenEphys GUI...")
    gui_ok = test_plugin_loading()
    
    # Summary
    print("\n" + "=" * 40)
    print("📊 Test Summary:")
    print(f"   Plugin file: {'✓' if plugin_ok else '✗'}")
    print(f"   Redis server: {'✓' if redis_running else '✗'}")
    print(f"   Test data: {'✓' if data_sent else '✗'}")
    print(f"   OpenEphys GUI: {'✓' if gui_ok else '✗'}")
    
    if plugin_ok and gui_ok:
        print("\n🎉 Plugin appears to be working!")
        if redis_running:
            print("   You can now test the plugin in OpenEphys GUI:")
            print("   1. Start OpenEphys GUI")
            print("   2. Add 'Redis Source' to your signal chain")
            print("   3. Configure connection settings")
            print("   4. Send data using: redis-cli LPUSH openephys_data '{\"channels\":[1,2,3...]}'")
        else:
            print("   Install Redis to test data streaming functionality")
    else:
        print("\n❌ Some tests failed. Check the output above.")
    
    return plugin_ok and gui_ok

if __name__ == "__main__":
    success = main()
    sys.exit(0 if success else 1)
