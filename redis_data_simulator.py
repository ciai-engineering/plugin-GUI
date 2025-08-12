#!/usr/bin/env python3
"""
Redis Data Simulator for BRANDBCI Integration

This simulator creates realistic neural data streams and publishes them to Redis
using the same format and approach as BRANDBCI systems. It's designed to work
with the RedisToOpenEphys plugin for testing and development.

Based on the existing test_redis_stream_integration.py in the repository.
"""

import redis
import json
import time
import numpy as np
import argparse

class BrandBCIRedisSimulator:
    """
    Redis data simulator that mimics BRANDBCI data streams
    Based on the existing test_redis_stream_integration.py
    """
    
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
        
        # Create BRANDBCI format - matches the format in test_redis_stream_integration.py
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
        """Publish data to Redis stream using XADD - matches existing implementation"""
        try:
            # Convert to JSON string for the stream field
            json_data = json.dumps(data)
            
            # Add to Redis stream - this is the exact format expected by RedisToOpenEphys
            stream_id = self.redis_client.xadd(stream_name, {"brandbci_data": json_data})
            return stream_id
        except Exception as e:
            print(f"❌ Failed to publish to stream {stream_name}: {e}")
            return None
    
    def start_streaming(self, stream_name="neural_data_primary", sample_rate=30000, channels=96, duration=60):
        """Start streaming data at specified rate"""
        print(f"🚀 Starting data stream: {stream_name}")
        print(f"   Sample rate: {sample_rate} Hz")
        print(f"   Channels: {channels}")
        print(f"   Duration: {duration} seconds")
        
        self.is_running = True
        start_time = time.time()
        samples_sent = 0
        
        try:
            while self.is_running and (time.time() - start_time) < duration:
                # Generate and publish sample
                sample_data = self.create_neural_data_sample(stream_name, channels, sample_rate)
                stream_id = self.publish_to_stream(stream_name, sample_data)
                
                if stream_id:
                    samples_sent += 1
                    
                    if samples_sent % 1000 == 0:
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
    
    def cleanup_streams(self, stream_names):
        """Clean up test streams"""
        for stream_name in stream_names:
            try:
                self.redis_client.delete(stream_name)
                print(f"🧹 Cleaned up stream: {stream_name}")
            except Exception as e:
                print(f"⚠️ Failed to cleanup stream {stream_name}: {e}")

def main():
    """Main CLI interface"""
    parser = argparse.ArgumentParser(description="Redis Data Simulator for BRANDBCI Integration")
    parser.add_argument("--host", default="localhost", help="Redis host")
    parser.add_argument("--port", type=int, default=6379, help="Redis port")
    parser.add_argument("--stream", default="neural_data_primary", help="Stream name")
    parser.add_argument("--rate", type=int, default=30000, help="Sample rate in Hz")
    parser.add_argument("--channels", type=int, default=96, help="Number of channels")
    parser.add_argument("--duration", type=int, default=60, help="Duration in seconds")
    parser.add_argument("--test", action="store_true", help="Run connection test only")
    
    args = parser.parse_args()
    
    print("Redis Data Simulator for BRANDBCI Integration")
    print("=" * 50)
    print(f"Redis: {args.host}:{args.port}")
    print(f"Stream: {args.stream}")
    print(f"Rate: {args.rate} Hz")
    print(f"Channels: {args.channels}")
    print(f"Duration: {args.duration} seconds")
    
    simulator = BrandBCIRedisSimulator(args.host, args.port)
    
    if not simulator.connect():
        return 1
    
    if args.test:
        print("✓ Redis connection test successful")
        return 0
    
    print(f"\n💡 Configure OpenEphys plugin with:")
    print(f"   - Stream mode: ENABLED")
    print(f"   - Stream pattern: {args.stream}")
    print(f"   - Data format: brandbci")
    print(f"\nPress Ctrl+C to stop streaming...")
    
    try:
        simulator.start_streaming(args.stream, args.rate, args.channels, args.duration)
        simulator.cleanup_streams([args.stream])
        return 0
    except Exception as e:
        print(f"❌ Error: {e}")
        return 1

if __name__ == "__main__":
    exit(main())
