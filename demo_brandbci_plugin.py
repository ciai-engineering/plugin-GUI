#!/usr/bin/env python3
"""
BRANDBCI Plugin Demo

This script demonstrates the new Redis stream functionality in the OpenEphys
RedisDataThread plugin. It simulates a BRANDBCI neural acquisition system
and shows how to configure the plugin for real-time data streaming.
"""

import redis
import json
import time
import numpy as np
import threading
import signal
import sys

class BrandBCIDemo:
    """Demo class for BRANDBCI integration"""
    
    def __init__(self, redis_host="localhost", redis_port=6379):
        self.redis_client = redis.Redis(host=redis_host, port=redis_port, decode_responses=True)
        self.is_running = False
        self.sample_count = 0
        
    def connect(self):
        """Connect to Redis"""
        try:
            self.redis_client.ping()
            print("✓ Connected to Redis server")
            return True
        except Exception as e:
            print(f"❌ Failed to connect to Redis: {e}")
            return False
    
    def create_neural_sample(self, stream_id="neural_data_primary", channels=96):
        """Create a realistic neural data sample"""
        
        # Generate realistic neural signals
        channel_data = []
        for ch in range(channels):
            # Different signal characteristics per channel group
            if ch < 32:  # LFP channels
                freq = 5 + (ch % 15)  # 5-20 Hz
                amplitude = 50 + np.random.normal(0, 10)
            elif ch < 64:  # Spike channels  
                freq = 300 + (ch % 200)  # 300-500 Hz
                amplitude = 100 + np.random.normal(0, 20)
            else:  # Mixed signals
                freq = 50 + (ch % 100)  # 50-150 Hz
                amplitude = 75 + np.random.normal(0, 15)
            
            # Generate signal with realistic noise
            t = self.sample_count / 30000.0  # 30 kHz sampling
            signal = amplitude * np.sin(2 * np.pi * freq * t)
            noise = np.random.normal(0, 5)  # 5 µV RMS noise
            
            channel_data.append(float(signal + noise))
        
        # Create BRANDBCI format
        return {
            "stream_id": stream_id,
            "timestamp": f"{int(time.time() * 1000)}-{self.sample_count % 1000}",
            "node_info": {
                "nickname": "neural_acq_demo",
                "status": "NODE_READY"
            },
            "data": {
                "channels": channel_data,
                "sample_rate": 30000,
                "metadata": {
                    "electrode_config": "utah_array_96ch",
                    "amplifier_gain": 1000,
                    "session_id": f"demo_session_{int(time.time())}"
                }
            }
        }
    
    def publish_sample(self, stream_name, sample_data):
        """Publish sample to Redis stream"""
        try:
            json_data = json.dumps(sample_data)
            stream_id = self.redis_client.xadd(stream_name, {"brandbci_data": json_data})
            return stream_id
        except Exception as e:
            print(f"❌ Failed to publish sample: {e}")
            return None
    
    def create_node_status(self):
        """Create node status information"""
        status_data = {
            "node_id": "neural_acq_demo",
            "status": "NODE_READY",
            "timestamp": int(time.time() * 1000),
            "sample_count": self.sample_count,
            "error_count": 0,
            "performance": {
                "cpu_usage": 15.5,
                "memory_usage": 45.2,
                "data_rate": 1000.0
            }
        }
        
        try:
            stream_id = self.redis_client.xadd("node_status_demo", {"status": json.dumps(status_data)})
            return stream_id
        except Exception as e:
            print(f"❌ Failed to publish node status: {e}")
            return None
    
    def start_demo(self, duration=60):
        """Start the demo streaming"""
        print(f"\n🚀 Starting BRANDBCI Demo")
        print(f"   Duration: {duration} seconds")
        print(f"   Sample rate: 1000 Hz (simulated)")
        print(f"   Channels: 96")
        print(f"\n💡 OpenEphys Plugin Configuration:")
        print(f"   1. Add 'Redis Source' to your signal chain")
        print(f"   2. Configure connection:")
        print(f"      - Host: localhost")
        print(f"      - Port: 6379")
        print(f"      - Stream mode: ENABLED")
        print(f"      - Stream pattern: neural_*")
        print(f"      - Data format: brandbci")
        print(f"   3. Start acquisition in OpenEphys")
        print(f"\n📊 Demo will stream data to: neural_data_primary")
        print(f"📊 Node status will be published to: node_status_demo")
        print(f"\nPress Ctrl+C to stop early...\n")
        
        self.is_running = True
        start_time = time.time()
        samples_sent = 0
        last_status_time = 0
        
        try:
            while self.is_running and (time.time() - start_time) < duration:
                # Generate and publish neural data
                sample_data = self.create_neural_sample()
                stream_id = self.publish_sample("neural_data_primary", sample_data)
                
                if stream_id:
                    samples_sent += 1
                    self.sample_count += 1
                    
                    # Publish node status every 5 seconds
                    current_time = time.time()
                    if current_time - last_status_time >= 5:
                        self.create_node_status()
                        last_status_time = current_time
                    
                    # Progress update every 100 samples
                    if samples_sent % 100 == 0:
                        elapsed = time.time() - start_time
                        rate = samples_sent / elapsed
                        print(f"📈 Sent {samples_sent} samples, rate: {rate:.1f} Hz, elapsed: {elapsed:.1f}s")
                
                # Maintain 1000 Hz rate
                expected_time = start_time + (samples_sent / 1000.0)
                current_time = time.time()
                if current_time < expected_time:
                    time.sleep(expected_time - current_time)
                    
        except KeyboardInterrupt:
            print("\n⏹️ Demo stopped by user")
        
        self.is_running = False
        elapsed = time.time() - start_time
        actual_rate = samples_sent / elapsed if elapsed > 0 else 0
        
        print(f"\n📊 Demo Summary:")
        print(f"   Total samples: {samples_sent}")
        print(f"   Duration: {elapsed:.1f} seconds")
        print(f"   Average rate: {actual_rate:.1f} Hz")
        print(f"   Data published to: neural_data_primary")
        
        return samples_sent, actual_rate
    
    def cleanup(self):
        """Clean up demo streams"""
        streams_to_clean = ["neural_data_primary", "node_status_demo"]
        for stream in streams_to_clean:
            try:
                self.redis_client.delete(stream)
                print(f"🧹 Cleaned up stream: {stream}")
            except Exception as e:
                print(f"⚠️ Failed to cleanup {stream}: {e}")

def signal_handler(signum, frame):
    """Handle Ctrl+C gracefully"""
    print("\n🛑 Received interrupt signal, stopping demo...")
    sys.exit(0)

def main():
    print("BRANDBCI OpenEphys Plugin Demo")
    print("==============================")
    print("This demo simulates BRANDBCI neural data streams for testing")
    print("the new Redis stream functionality in the OpenEphys plugin.")
    
    # Set up signal handler for graceful shutdown
    signal.signal(signal.SIGINT, signal_handler)
    
    # Create demo instance
    demo = BrandBCIDemo()
    
    # Connect to Redis
    if not demo.connect():
        print("❌ Cannot connect to Redis. Please ensure Redis server is running.")
        print("   Start Redis with: redis-server")
        return 1
    
    # Check if OpenEphys is ready
    print("\n🔧 Setup Instructions:")
    print("1. Start OpenEphys GUI")
    print("2. Add 'Redis Source' plugin to your signal chain")
    print("3. Configure the plugin with the settings shown below")
    print("4. Start acquisition in OpenEphys")
    print("5. Press Enter here to start the demo")
    
    input("\nPress Enter when OpenEphys is configured and ready...")
    
    try:
        # Run the demo
        samples_sent, rate = demo.start_demo(duration=60)
        
        # Show results
        if samples_sent > 0:
            print(f"\n✅ Demo completed successfully!")
            print(f"   Check OpenEphys for real-time neural data visualization")
        else:
            print(f"\n⚠️ No samples were sent - check Redis connection")
            
    except Exception as e:
        print(f"\n❌ Demo failed: {e}")
        return 1
    
    finally:
        # Cleanup
        demo.cleanup()
        print("\n👋 Demo finished. Thank you!")
    
    return 0

if __name__ == "__main__":
    exit(main())
