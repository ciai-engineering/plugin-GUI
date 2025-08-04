#!/usr/bin/env python3
"""
Redis Data Sender Example

This script demonstrates how to send data to the Redis DataThread plugin.
It generates synthetic electrophysiology-like data and streams it to Redis.

Usage:
    python3 redis_data_sender.py [options]

Requirements:
    pip install redis numpy
"""

import redis
import json
import numpy as np
import time
import argparse
import signal
import sys

class RedisDataSender:
    def __init__(self, host='localhost', port=6379, password=None, channel='openephys_data'):
        """Initialize Redis connection"""
        self.host = host
        self.port = port
        self.channel = channel
        self.running = False
        
        # Connect to Redis
        try:
            self.redis_client = redis.Redis(
                host=host, 
                port=port, 
                password=password,
                decode_responses=False  # Keep binary data as bytes
            )
            # Test connection
            self.redis_client.ping()
            print(f"✓ Connected to Redis at {host}:{port}")
        except redis.ConnectionError as e:
            print(f"✗ Failed to connect to Redis: {e}")
            sys.exit(1)
    
    def generate_synthetic_data(self, num_channels, sample_index, sample_rate):
        """Generate synthetic electrophysiology-like data"""
        channels = []
        
        for ch in range(num_channels):
            # Create different signal types for different channels
            if ch < 8:
                # LFP-like signals (low frequency)
                freq = 1 + ch * 0.5  # 1-4.5 Hz
                amplitude = 50 + ch * 10  # 50-120 µV
                signal = amplitude * np.sin(2 * np.pi * freq * sample_index / sample_rate)
                
            elif ch < 16:
                # Theta rhythm (4-8 Hz)
                freq = 4 + (ch - 8) * 0.5  # 4-8 Hz
                amplitude = 30 + (ch - 8) * 5  # 30-65 µV
                signal = amplitude * np.sin(2 * np.pi * freq * sample_index / sample_rate)
                
            elif ch < 24:
                # Gamma rhythm (30-100 Hz)
                freq = 30 + (ch - 16) * 8  # 30-86 Hz
                amplitude = 10 + (ch - 16) * 2  # 10-26 µV
                signal = amplitude * np.sin(2 * np.pi * freq * sample_index / sample_rate)
                
            else:
                # High frequency noise + spikes
                noise = np.random.normal(0, 5)  # 5 µV noise
                # Occasional spike
                if sample_index % (sample_rate // 2) < 10:  # Spike every 0.5 seconds
                    spike = 200 * np.exp(-((sample_index % (sample_rate // 2)) / 2)**2)
                else:
                    spike = 0
                signal = noise + spike
            
            # Add some common noise to all channels
            common_noise = 2 * np.sin(2 * np.pi * 60 * sample_index / sample_rate)  # 60 Hz line noise
            signal += common_noise + np.random.normal(0, 1)  # 1 µV random noise
            
            channels.append(float(signal))
        
        return channels
    
    def send_json_data(self, num_channels=32, sample_rate=30000, duration=None):
        """Send data in JSON format"""
        print(f"Streaming JSON data: {num_channels} channels at {sample_rate} Hz")
        if duration:
            print(f"Duration: {duration} seconds")
        else:
            print("Duration: infinite (press Ctrl+C to stop)")
        
        sample_index = 0
        start_time = time.time()
        
        try:
            while self.running:
                # Generate data
                channels = self.generate_synthetic_data(num_channels, sample_index, sample_rate)
                
                # Create packet
                packet = {
                    "channels": channels,
                    "timestamp": int(time.time() * 1000)
                }
                
                # Send to Redis
                self.redis_client.lpush(self.channel, json.dumps(packet))
                
                sample_index += 1
                
                # Print progress
                if sample_index % sample_rate == 0:
                    elapsed = time.time() - start_time
                    print(f"Sent {sample_index} samples ({elapsed:.1f}s)")
                
                # Check duration
                if duration and (time.time() - start_time) >= duration:
                    break
                
                # Control timing (approximate)
                time.sleep(1.0 / sample_rate)
                
        except KeyboardInterrupt:
            print("\nStopping data stream...")
    
    def send_binary_data(self, num_channels=32, sample_rate=30000, duration=None):
        """Send data in binary format"""
        print(f"Streaming binary data: {num_channels} channels at {sample_rate} Hz")
        if duration:
            print(f"Duration: {duration} seconds")
        else:
            print("Duration: infinite (press Ctrl+C to stop)")
        
        sample_index = 0
        start_time = time.time()
        
        try:
            while self.running:
                # Generate data
                channels = self.generate_synthetic_data(num_channels, sample_index, sample_rate)
                
                # Convert to binary (little-endian float32)
                binary_data = np.array(channels, dtype=np.float32).tobytes()
                
                # Send to Redis
                self.redis_client.lpush(self.channel, binary_data)
                
                sample_index += 1
                
                # Print progress
                if sample_index % sample_rate == 0:
                    elapsed = time.time() - start_time
                    print(f"Sent {sample_index} samples ({elapsed:.1f}s)")
                
                # Check duration
                if duration and (time.time() - start_time) >= duration:
                    break
                
                # Control timing (approximate)
                time.sleep(1.0 / sample_rate)
                
        except KeyboardInterrupt:
            print("\nStopping data stream...")
    
    def start(self, format_type='json', **kwargs):
        """Start sending data"""
        self.running = True
        
        if format_type.lower() == 'json':
            self.send_json_data(**kwargs)
        elif format_type.lower() == 'binary':
            self.send_binary_data(**kwargs)
        else:
            raise ValueError(f"Unknown format: {format_type}")
    
    def stop(self):
        """Stop sending data"""
        self.running = False

def signal_handler(sig, frame):
    """Handle Ctrl+C gracefully"""
    print('\nReceived interrupt signal, stopping...')
    global sender
    if sender:
        sender.stop()
    sys.exit(0)

def main():
    parser = argparse.ArgumentParser(description='Send synthetic data to Redis for OpenEphys')
    parser.add_argument('--host', default='localhost', help='Redis host (default: localhost)')
    parser.add_argument('--port', type=int, default=6379, help='Redis port (default: 6379)')
    parser.add_argument('--password', help='Redis password (if required)')
    parser.add_argument('--channel', default='openephys_data', help='Redis channel (default: openephys_data)')
    parser.add_argument('--format', choices=['json', 'binary'], default='json', help='Data format (default: json)')
    parser.add_argument('--channels', type=int, default=32, help='Number of channels (default: 32)')
    parser.add_argument('--sample-rate', type=int, default=30000, help='Sample rate in Hz (default: 30000)')
    parser.add_argument('--duration', type=float, help='Duration in seconds (default: infinite)')
    
    args = parser.parse_args()
    
    # Set up signal handler
    signal.signal(signal.SIGINT, signal_handler)
    
    global sender
    sender = RedisDataSender(
        host=args.host,
        port=args.port,
        password=args.password,
        channel=args.channel
    )
    
    print("Starting Redis data sender...")
    print("Make sure the Redis DataThread plugin is configured and ready in OpenEphys GUI")
    print("Press Ctrl+C to stop")
    print("-" * 60)
    
    sender.start(
        format_type=args.format,
        num_channels=args.channels,
        sample_rate=args.sample_rate,
        duration=args.duration
    )
    
    print("Data sending completed.")

if __name__ == "__main__":
    sender = None
    main()
