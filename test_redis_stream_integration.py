#!/usr/bin/env python3
"""
Test Redis Stream Integration

This script tests the new Redis stream functionality in the RedisDataThread plugin.
It simulates BRANDBCI data streams and verifies the plugin can read them correctly.
"""

import redis
import json
import time
import numpy as np
import threading
import argparse
from typing import Dict, List

class BrandBCISimulator:
    """Simulates BRANDBCI data streams for testing"""
    
    def __init__(self, redis_host="localhost", redis_port=6379):
        self.redis_client = redis.Redis(host=redis_host, port=redis_port, decode_responses=True)
        self.is_running = False
        self.sample_count = 0
        
    def connect(self):
        """Test Redis connection"""
        try:
            self.redis_client.ping()
            print("✓ Connected to Redis")
            return True
        except Exception as e:
            print(f"❌ Failed to connect to Redis: {e}")
            return False
    
    def create_neural_data_sample(self, stream_id="neural_data_primary", channels=96, sample_rate=30000):
        """Create a BRANDBCI-format neural data sample"""
        
        # Generate synthetic neural data
        channel_data = []
        for ch in range(channels):
            # Simulate different signal types per channel
            if ch < 32:
                # LFP-like signals (1-100 Hz)
                freq = 10 + (ch % 20)
                amplitude = 50 + np.random.normal(0, 10)
            elif ch < 64:
                # Spike-like signals (higher frequency)
                freq = 100 + (ch % 50)
                amplitude = 100 + np.random.normal(0, 20)
            else:
                # Mixed signals
                freq = 5 + (ch % 30)
                amplitude = 75 + np.random.normal(0, 15)
            
            # Generate signal with noise
            t = self.sample_count / sample_rate
            signal = amplitude * np.sin(2 * np.pi * freq * t)
            noise = np.random.normal(0, 5)  # 5 µV RMS noise
            
            channel_data.append(float(signal + noise))
        
        # Create BRANDBCI format
        brandbci_data = {
            "stream_id": stream_id,
            "timestamp": f"{int(time.time() * 1000)}-{self.sample_count % 1000}",
            "node_info": {
                "nickname": "neural_acq_sim",
                "status": "NODE_READY"
            },
            "data": {
                "channels": channel_data,
                "sample_rate": sample_rate,
                "metadata": {
                    "electrode_config": "utah_array_96ch",
                    "amplifier_gain": 1000,
                    "session_id": f"test_session_{int(time.time())}"
                }
            }
        }
        
        self.sample_count += 1
        return brandbci_data
    
    def publish_to_stream(self, stream_name, data):
        """Publish data to Redis stream"""
        try:
            # Convert to JSON string for the stream field
            json_data = json.dumps(data)
            
            # Add to Redis stream
            stream_id = self.redis_client.xadd(stream_name, {"brandbci_data": json_data})
            return stream_id
        except Exception as e:
            print(f"❌ Failed to publish to stream {stream_name}: {e}")
            return None
    
    def start_streaming(self, stream_name="neural_data_test", sample_rate=1000, duration=30):
        """Start streaming data at specified rate"""
        print(f"🚀 Starting data stream: {stream_name}")
        print(f"   Sample rate: {sample_rate} Hz")
        print(f"   Duration: {duration} seconds")
        
        self.is_running = True
        start_time = time.time()
        samples_sent = 0
        
        try:
            while self.is_running and (time.time() - start_time) < duration:
                # Generate and publish sample
                sample_data = self.create_neural_data_sample(stream_name)
                stream_id = self.publish_to_stream(stream_name, sample_data)
                
                if stream_id:
                    samples_sent += 1
                    
                    if samples_sent % 100 == 0:
                        elapsed = time.time() - start_time
                        actual_rate = samples_sent / elapsed
                        print(f"📊 Sent {samples_sent} samples, rate: {actual_rate:.1f} Hz")
                
                # Maintain target sample rate
                expected_time = start_time + (samples_sent / sample_rate)
                current_time = time.time()
                if current_time < expected_time:
                    time.sleep(expected_time - current_time)
                    
        except KeyboardInterrupt:
            print("\n⏹️ Streaming stopped by user")
        
        self.is_running = False
        elapsed = time.time() - start_time
        actual_rate = samples_sent / elapsed if elapsed > 0 else 0
        
        print(f"\n📈 Streaming complete:")
        print(f"   Total samples: {samples_sent}")
        print(f"   Elapsed time: {elapsed:.1f} seconds")
        print(f"   Average rate: {actual_rate:.1f} Hz")
        
        return samples_sent, actual_rate
    
    def stop_streaming(self):
        """Stop the streaming"""
        self.is_running = False
    
    def create_node_status_stream(self, stream_name="node_status_test"):
        """Create node status information stream"""
        status_data = {
            "node_id": "neural_acq_sim",
            "status": "NODE_READY",
            "timestamp": int(time.time() * 1000),
            "sample_count": self.sample_count,
            "error_count": 0,
            "session_info": {
                "session_id": f"test_session_{int(time.time())}",
                "start_time": int(time.time()),
                "electrode_count": 96
            }
        }
        
        try:
            stream_id = self.redis_client.xadd(stream_name, {"status": json.dumps(status_data)})
            print(f"✓ Published node status to {stream_name}")
            return stream_id
        except Exception as e:
            print(f"❌ Failed to publish node status: {e}")
            return None
    
    def cleanup_streams(self, stream_names):
        """Clean up test streams"""
        for stream_name in stream_names:
            try:
                self.redis_client.delete(stream_name)
                print(f"🧹 Cleaned up stream: {stream_name}")
            except Exception as e:
                print(f"⚠️ Failed to cleanup stream {stream_name}: {e}")

def test_stream_discovery():
    """Test stream discovery functionality"""
    print("\n=== Testing Stream Discovery ===")
    
    simulator = BrandBCISimulator()
    if not simulator.connect():
        return False
    
    # Create test streams
    test_streams = ["neural_data_test1", "neural_data_test2", "other_stream"]
    
    for stream in test_streams:
        sample_data = simulator.create_neural_data_sample(stream)
        simulator.publish_to_stream(stream, sample_data)
    
    # Test discovery
    try:
        # Discover neural streams
        neural_streams = simulator.redis_client.keys("neural_*")
        print(f"✓ Discovered {len(neural_streams)} neural streams: {neural_streams}")
        
        # Verify they are actually streams
        stream_count = 0
        for stream in neural_streams:
            stream_type = simulator.redis_client.type(stream)
            if stream_type == "stream":
                stream_count += 1
                print(f"  ✓ {stream} is a valid stream")
        
        success = stream_count >= 2
        print(f"{'✓' if success else '❌'} Stream discovery test: {stream_count} valid streams found")
        
        # Cleanup
        simulator.cleanup_streams(test_streams)
        return success
        
    except Exception as e:
        print(f"❌ Stream discovery test failed: {e}")
        return False

def test_data_format_parsing():
    """Test BRANDBCI data format parsing"""
    print("\n=== Testing Data Format Parsing ===")
    
    simulator = BrandBCISimulator()
    if not simulator.connect():
        return False
    
    stream_name = "format_test_stream"
    
    try:
        # Create sample with known data
        test_data = simulator.create_neural_data_sample(stream_name, channels=4)
        stream_id = simulator.publish_to_stream(stream_name, test_data)
        
        if stream_id:
            print(f"✓ Published test data to stream: {stream_id}")
            
            # Read back the data
            result = simulator.redis_client.xread({stream_name: "0-0"}, block=1000)
            
            if result:
                stream_data = result[0][1][0][1]  # [stream][entries][entry][fields]
                brandbci_json = stream_data["brandbci_data"]
                parsed_data = json.loads(brandbci_json)
                
                # Verify structure
                required_fields = ["stream_id", "timestamp", "node_info", "data"]
                success = all(field in parsed_data for field in required_fields)
                
                if success:
                    channels = parsed_data["data"]["channels"]
                    print(f"✓ Data format test passed: {len(channels)} channels parsed")
                else:
                    print("❌ Data format test failed: missing required fields")
                
                # Cleanup
                simulator.cleanup_streams([stream_name])
                return success
            else:
                print("❌ Failed to read back test data")
                return False
        else:
            print("❌ Failed to publish test data")
            return False
            
    except Exception as e:
        print(f"❌ Data format test failed: {e}")
        return False

def test_performance():
    """Test performance requirements"""
    print("\n=== Testing Performance ===")
    
    simulator = BrandBCISimulator()
    if not simulator.connect():
        return False
    
    stream_name = "performance_test_stream"
    
    # Test high-rate streaming
    print("Testing 1kHz streaming for 5 seconds...")
    samples_sent, actual_rate = simulator.start_streaming(stream_name, sample_rate=1000, duration=5)
    
    # Check performance criteria
    target_rate = 1000
    rate_success = actual_rate >= (target_rate * 0.95)  # 95% of target
    samples_success = samples_sent >= (target_rate * 5 * 0.95)  # 95% of expected samples
    
    print(f"{'✓' if rate_success else '❌'} Rate requirement: {actual_rate:.1f} Hz (target: {target_rate} Hz)")
    print(f"{'✓' if samples_success else '❌'} Sample count: {samples_sent} (expected: ~{target_rate * 5})")
    
    # Cleanup
    simulator.cleanup_streams([stream_name])
    
    return rate_success and samples_success

def main():
    parser = argparse.ArgumentParser(description="Test Redis Stream Integration")
    parser.add_argument("--host", default="localhost", help="Redis host")
    parser.add_argument("--port", type=int, default=6379, help="Redis port")
    parser.add_argument("--stream", default="neural_data_test", help="Stream name for testing")
    parser.add_argument("--rate", type=int, default=1000, help="Sample rate for testing")
    parser.add_argument("--duration", type=int, default=30, help="Test duration in seconds")
    parser.add_argument("--test-only", action="store_true", help="Run tests only, no continuous streaming")
    
    args = parser.parse_args()
    
    print("Redis Stream Integration Test")
    print("============================")
    print(f"Redis: {args.host}:{args.port}")
    print(f"Stream: {args.stream}")
    print(f"Rate: {args.rate} Hz")
    print(f"Duration: {args.duration} seconds")
    
    # Run tests
    tests_passed = 0
    total_tests = 3
    
    if test_stream_discovery():
        tests_passed += 1
    
    if test_data_format_parsing():
        tests_passed += 1
    
    if test_performance():
        tests_passed += 1
    
    print(f"\n=== Test Results ===")
    print(f"Tests passed: {tests_passed}/{total_tests}")
    print(f"Success rate: {tests_passed/total_tests:.1%}")
    
    if args.test_only:
        return 0 if tests_passed == total_tests else 1
    
    # Start continuous streaming for plugin testing
    if tests_passed == total_tests:
        print(f"\n🚀 Starting continuous streaming for plugin testing...")
        print(f"   Stream name: {args.stream}")
        print(f"   Sample rate: {args.rate} Hz")
        print(f"   Duration: {args.duration} seconds")
        print(f"\n💡 Configure OpenEphys plugin with:")
        print(f"   - Stream mode: ENABLED")
        print(f"   - Stream pattern: {args.stream}")
        print(f"   - Data format: brandbci")
        print(f"\nPress Ctrl+C to stop streaming early...")
        
        simulator = BrandBCISimulator(args.host, args.port)
        if simulator.connect():
            # Create node status stream
            simulator.create_node_status_stream()
            
            # Start main data streaming
            simulator.start_streaming(args.stream, args.rate, args.duration)
            
            # Cleanup
            simulator.cleanup_streams([args.stream, "node_status_test"])
    else:
        print(f"\n❌ Some tests failed - skipping continuous streaming")
        return 1
    
    return 0

if __name__ == "__main__":
    exit(main())
