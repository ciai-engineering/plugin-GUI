#!/usr/bin/env python3
"""
BRANDBCI to OpenEphys Integration Test Suite

This script provides comprehensive testing for the RedisToOpenEphys plugin
integration with BRANDBCI systems.
"""

import redis
import json
import time
import numpy as np
import argparse
import sys
import threading
import logging
from typing import Dict, List, Optional, Tuple
from dataclasses import dataclass
from datetime import datetime

# Configure logging
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(levelname)s - %(message)s'
)
logger = logging.getLogger(__name__)

@dataclass
class TestConfig:
    """Configuration for integration tests"""
    redis_host: str = "localhost"
    redis_port: int = 6379
    redis_password: str = ""
    test_duration: int = 30  # seconds
    sample_rate: int = 30000
    channel_count: int = 96
    stream_name: str = "neural_data_test"
    node_nickname: str = "test_neural_acq"

class BrandDataGenerator:
    """Generates BRANDBCI-compatible test data"""
    
    def __init__(self, config: TestConfig):
        self.config = config
        self.sample_counter = 0
        self.start_time = time.time()
    
    def generate_neural_sample(self) -> Dict:
        """Generate a single neural data sample in BRANDBCI format"""
        current_time = time.time()
        timestamp_ms = int(current_time * 1000)
        
        # Generate synthetic neural data (sine waves with noise)
        channels = []
        for ch in range(self.config.channel_count):
            # Different frequency for each channel
            freq = 10 + (ch % 20)  # 10-30 Hz range
            
            # Sine wave with noise
            t = self.sample_counter / self.config.sample_rate
            signal = 100 * np.sin(2 * np.pi * freq * t)
            noise = np.random.normal(0, 10)  # 10 µV RMS noise
            
            channels.append(float(signal + noise))
        
        # BRANDBCI format data structure
        data = {
            "stream_id": self.config.stream_name,
            "timestamp": f"{timestamp_ms}-{self.sample_counter % 1000}",
            "node_info": {
                "nickname": self.config.node_nickname,
                "status": "NODE_READY"
            },
            "data": {
                "channels": channels,
                "sample_rate": self.config.sample_rate,
                "metadata": {
                    "electrode_config": "utah_array_96ch",
                    "amplifier_gain": 1000,
                    "session_id": f"test_session_{int(self.start_time)}"
                }
            }
        }
        
        self.sample_counter += 1
        return data
    
    def generate_node_status(self) -> Dict:
        """Generate node status information"""
        return {
            "node_id": self.config.node_nickname,
            "status": "NODE_READY",
            "timestamp": int(time.time() * 1000),
            "sample_count": self.sample_counter,
            "error_count": 0
        }

class RedisTestClient:
    """Redis client for testing BRANDBCI integration"""
    
    def __init__(self, config: TestConfig):
        self.config = config
        self.redis_client = None
        self.is_connected = False
    
    def connect(self) -> bool:
        """Connect to Redis server"""
        try:
            self.redis_client = redis.Redis(
                host=self.config.redis_host,
                port=self.config.redis_port,
                password=self.config.redis_password if self.config.redis_password else None,
                decode_responses=True,
                socket_timeout=5
            )
            
            # Test connection
            self.redis_client.ping()
            self.is_connected = True
            logger.info(f"Connected to Redis at {self.config.redis_host}:{self.config.redis_port}")
            return True
            
        except Exception as e:
            logger.error(f"Failed to connect to Redis: {e}")
            return False
    
    def disconnect(self):
        """Disconnect from Redis server"""
        if self.redis_client:
            self.redis_client.close()
            self.is_connected = False
            logger.info("Disconnected from Redis")
    
    def publish_stream_data(self, stream_name: str, data: Dict) -> bool:
        """Publish data to Redis stream"""
        try:
            # Convert data to Redis stream format
            fields = {}
            for key, value in data.items():
                if isinstance(value, dict):
                    fields[key] = json.dumps(value)
                else:
                    fields[key] = str(value)
            
            # Add to stream
            stream_id = self.redis_client.xadd(stream_name, fields)
            return stream_id is not None
            
        except Exception as e:
            logger.error(f"Failed to publish to stream {stream_name}: {e}")
            return False
    
    def read_stream_data(self, stream_name: str, last_id: str = "0-0", timeout: int = 1000) -> List:
        """Read data from Redis stream"""
        try:
            result = self.redis_client.xread({stream_name: last_id}, block=timeout)
            return result
        except Exception as e:
            logger.error(f"Failed to read from stream {stream_name}: {e}")
            return []
    
    def get_stream_info(self, stream_name: str) -> Optional[Dict]:
        """Get information about a Redis stream"""
        try:
            return self.redis_client.xinfo_stream(stream_name)
        except Exception as e:
            logger.error(f"Failed to get stream info for {stream_name}: {e}")
            return None

class IntegrationTester:
    """Main integration testing class"""
    
    def __init__(self, config: TestConfig):
        self.config = config
        self.redis_client = RedisTestClient(config)
        self.data_generator = BrandDataGenerator(config)
        self.test_results = {}
        self.stop_event = threading.Event()
    
    def run_all_tests(self) -> bool:
        """Run all integration tests"""
        logger.info("Starting BRANDBCI to OpenEphys integration tests")
        
        tests = [
            ("Redis Connection", self.test_redis_connection),
            ("Stream Creation", self.test_stream_creation),
            ("Data Publishing", self.test_data_publishing),
            ("Data Format Validation", self.test_data_format),
            ("Performance Test", self.test_performance),
            ("Error Handling", self.test_error_handling),
            ("Cleanup", self.test_cleanup)
        ]
        
        all_passed = True
        for test_name, test_func in tests:
            logger.info(f"Running test: {test_name}")
            try:
                result = test_func()
                self.test_results[test_name] = result
                status = "PASS" if result else "FAIL"
                logger.info(f"Test {test_name}: {status}")
                
                if not result:
                    all_passed = False
                    
            except Exception as e:
                logger.error(f"Test {test_name} failed with exception: {e}")
                self.test_results[test_name] = False
                all_passed = False
        
        self.print_test_summary()
        return all_passed
    
    def test_redis_connection(self) -> bool:
        """Test Redis server connection"""
        return self.redis_client.connect()
    
    def test_stream_creation(self) -> bool:
        """Test Redis stream creation"""
        try:
            # Create test stream
            test_data = self.data_generator.generate_neural_sample()
            success = self.redis_client.publish_stream_data(
                self.config.stream_name, test_data
            )
            
            if success:
                # Verify stream exists
                info = self.redis_client.get_stream_info(self.config.stream_name)
                return info is not None
            
            return False
            
        except Exception as e:
            logger.error(f"Stream creation test failed: {e}")
            return False
    
    def test_data_publishing(self) -> bool:
        """Test continuous data publishing"""
        try:
            samples_to_send = 100
            successful_sends = 0
            
            for i in range(samples_to_send):
                data = self.data_generator.generate_neural_sample()
                if self.redis_client.publish_stream_data(self.config.stream_name, data):
                    successful_sends += 1
                
                # Small delay to simulate real-time data
                time.sleep(1.0 / self.config.sample_rate)
            
            success_rate = successful_sends / samples_to_send
            logger.info(f"Data publishing success rate: {success_rate:.2%}")
            
            return success_rate > 0.95  # 95% success rate threshold
            
        except Exception as e:
            logger.error(f"Data publishing test failed: {e}")
            return False
    
    def test_data_format(self) -> bool:
        """Test BRANDBCI data format compliance"""
        try:
            data = self.data_generator.generate_neural_sample()
            
            # Validate required fields
            required_fields = ["stream_id", "timestamp", "node_info", "data"]
            for field in required_fields:
                if field not in data:
                    logger.error(f"Missing required field: {field}")
                    return False
            
            # Validate data structure
            if "channels" not in data["data"]:
                logger.error("Missing channels in data")
                return False
            
            channels = data["data"]["channels"]
            if len(channels) != self.config.channel_count:
                logger.error(f"Channel count mismatch: {len(channels)} != {self.config.channel_count}")
                return False
            
            # Validate channel data types
            for i, ch in enumerate(channels):
                if not isinstance(ch, (int, float)):
                    logger.error(f"Invalid channel data type at index {i}: {type(ch)}")
                    return False
            
            logger.info("Data format validation passed")
            return True
            
        except Exception as e:
            logger.error(f"Data format test failed: {e}")
            return False
    
    def test_performance(self) -> bool:
        """Test performance characteristics"""
        try:
            # Test high-rate data publishing
            start_time = time.time()
            samples_sent = 0
            target_rate = 1000  # 1 kHz for testing
            test_duration = 5  # seconds
            
            while time.time() - start_time < test_duration:
                data = self.data_generator.generate_neural_sample()
                if self.redis_client.publish_stream_data(self.config.stream_name, data):
                    samples_sent += 1
                
                # Maintain target rate
                expected_time = start_time + (samples_sent / target_rate)
                current_time = time.time()
                if current_time < expected_time:
                    time.sleep(expected_time - current_time)
            
            actual_duration = time.time() - start_time
            actual_rate = samples_sent / actual_duration
            
            logger.info(f"Performance test: {actual_rate:.1f} samples/sec")
            
            # Check if we achieved at least 90% of target rate
            return actual_rate >= (target_rate * 0.9)
            
        except Exception as e:
            logger.error(f"Performance test failed: {e}")
            return False
    
    def test_error_handling(self) -> bool:
        """Test error handling scenarios"""
        try:
            # Test invalid data format
            invalid_data = {"invalid": "data"}
            result1 = self.redis_client.publish_stream_data("invalid_stream", invalid_data)
            
            # Test connection recovery (simulate by disconnecting and reconnecting)
            self.redis_client.disconnect()
            time.sleep(1)
            result2 = self.redis_client.connect()
            
            logger.info("Error handling tests completed")
            return result2  # Connection recovery should succeed
            
        except Exception as e:
            logger.error(f"Error handling test failed: {e}")
            return False
    
    def test_cleanup(self) -> bool:
        """Clean up test data"""
        try:
            # Delete test stream
            if self.redis_client.is_connected:
                self.redis_client.redis_client.delete(self.config.stream_name)
                self.redis_client.disconnect()
            
            logger.info("Cleanup completed")
            return True
            
        except Exception as e:
            logger.error(f"Cleanup failed: {e}")
            return False
    
    def print_test_summary(self):
        """Print test results summary"""
        print("\n" + "="*50)
        print("BRANDBCI Integration Test Results")
        print("="*50)
        
        passed = sum(1 for result in self.test_results.values() if result)
        total = len(self.test_results)
        
        for test_name, result in self.test_results.items():
            status = "✓ PASS" if result else "✗ FAIL"
            print(f"{test_name:<25} {status}")
        
        print("-"*50)
        print(f"Tests passed: {passed}/{total}")
        print(f"Success rate: {passed/total:.1%}")
        
        if passed == total:
            print("🎉 All tests passed!")
        else:
            print("❌ Some tests failed. Check logs for details.")

def main():
    """Main function"""
    parser = argparse.ArgumentParser(description="BRANDBCI to OpenEphys Integration Test")
    parser.add_argument("--host", default="localhost", help="Redis host")
    parser.add_argument("--port", type=int, default=6379, help="Redis port")
    parser.add_argument("--password", default="", help="Redis password")
    parser.add_argument("--duration", type=int, default=30, help="Test duration in seconds")
    parser.add_argument("--channels", type=int, default=96, help="Number of channels")
    parser.add_argument("--sample-rate", type=int, default=30000, help="Sample rate")
    parser.add_argument("--stream", default="neural_data_test", help="Stream name")
    parser.add_argument("--verbose", action="store_true", help="Verbose logging")
    
    args = parser.parse_args()
    
    if args.verbose:
        logging.getLogger().setLevel(logging.DEBUG)
    
    # Create test configuration
    config = TestConfig(
        redis_host=args.host,
        redis_port=args.port,
        redis_password=args.password,
        test_duration=args.duration,
        channel_count=args.channels,
        sample_rate=args.sample_rate,
        stream_name=args.stream
    )
    
    # Run tests
    tester = IntegrationTester(config)
    success = tester.run_all_tests()
    
    sys.exit(0 if success else 1)

if __name__ == "__main__":
    main()
