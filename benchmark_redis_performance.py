#!/usr/bin/env python3
"""
Redis Performance Benchmark for BRANDBCI Integration

This script benchmarks Redis performance for neural data streaming
to validate real-time capabilities for the RedisToOpenEphys plugin.
"""

import redis
import json
import time
import numpy as np
import argparse
import threading
import statistics
from typing import List, Dict, Tuple
from dataclasses import dataclass
import matplotlib.pyplot as plt
import sys

@dataclass
class BenchmarkConfig:
    """Configuration for performance benchmarks"""
    redis_host: str = "localhost"
    redis_port: int = 6379
    redis_password: str = ""
    sample_rates: List[int] = None
    channel_counts: List[int] = None
    test_duration: int = 10  # seconds
    data_formats: List[str] = None
    
    def __post_init__(self):
        if self.sample_rates is None:
            self.sample_rates = [1000, 10000, 30000]
        if self.channel_counts is None:
            self.channel_counts = [32, 64, 96, 128]
        if self.data_formats is None:
            self.data_formats = ["json", "binary"]

class PerformanceMetrics:
    """Container for performance metrics"""
    
    def __init__(self):
        self.latencies: List[float] = []
        self.throughput_samples: List[float] = []
        self.throughput_bytes: List[float] = []
        self.cpu_usage: List[float] = []
        self.memory_usage: List[float] = []
        self.error_count: int = 0
        self.total_samples: int = 0
        self.total_bytes: int = 0
        self.start_time: float = 0
        self.end_time: float = 0
    
    def add_latency(self, latency: float):
        """Add latency measurement"""
        self.latencies.append(latency)
    
    def add_throughput_sample(self, samples_per_sec: float, bytes_per_sec: float):
        """Add throughput measurement"""
        self.throughput_samples.append(samples_per_sec)
        self.throughput_bytes.append(bytes_per_sec)
    
    def get_summary(self) -> Dict:
        """Get performance summary statistics"""
        duration = self.end_time - self.start_time
        
        return {
            "duration": duration,
            "total_samples": self.total_samples,
            "total_bytes": self.total_bytes,
            "avg_sample_rate": self.total_samples / duration if duration > 0 else 0,
            "avg_throughput_mbps": (self.total_bytes * 8) / (duration * 1e6) if duration > 0 else 0,
            "latency_stats": {
                "mean": statistics.mean(self.latencies) if self.latencies else 0,
                "median": statistics.median(self.latencies) if self.latencies else 0,
                "p95": np.percentile(self.latencies, 95) if self.latencies else 0,
                "p99": np.percentile(self.latencies, 99) if self.latencies else 0,
                "max": max(self.latencies) if self.latencies else 0
            },
            "error_rate": self.error_count / self.total_samples if self.total_samples > 0 else 0
        }

class DataGenerator:
    """Generate test data in various formats"""
    
    @staticmethod
    def generate_json_data(channel_count: int) -> Tuple[str, int]:
        """Generate JSON format neural data"""
        channels = [float(np.random.randn()) * 100 for _ in range(channel_count)]
        data = {
            "stream_id": "benchmark_stream",
            "timestamp": f"{int(time.time() * 1000)}-0",
            "data": {
                "channels": channels,
                "sample_rate": 30000,
                "metadata": {
                    "electrode_config": "test_array",
                    "amplifier_gain": 1000
                }
            }
        }
        json_str = json.dumps(data)
        return json_str, len(json_str.encode('utf-8'))
    
    @staticmethod
    def generate_binary_data(channel_count: int) -> Tuple[bytes, int]:
        """Generate binary format neural data"""
        channels = np.random.randn(channel_count).astype(np.float32)
        binary_data = channels.tobytes()
        return binary_data, len(binary_data)

class RedisBenchmark:
    """Redis performance benchmark runner"""
    
    def __init__(self, config: BenchmarkConfig):
        self.config = config
        self.redis_client = None
        self.metrics = PerformanceMetrics()
        self.stop_event = threading.Event()
    
    def connect(self) -> bool:
        """Connect to Redis server"""
        try:
            self.redis_client = redis.Redis(
                host=self.config.redis_host,
                port=self.config.redis_port,
                password=self.config.redis_password if self.config.redis_password else None,
                decode_responses=False,  # Handle binary data
                socket_timeout=5
            )
            self.redis_client.ping()
            print(f"Connected to Redis at {self.config.redis_host}:{self.config.redis_port}")
            return True
        except Exception as e:
            print(f"Failed to connect to Redis: {e}")
            return False
    
    def benchmark_throughput(self, sample_rate: int, channel_count: int, data_format: str) -> PerformanceMetrics:
        """Benchmark throughput for given parameters"""
        print(f"Benchmarking: {sample_rate} Hz, {channel_count} channels, {data_format} format")
        
        metrics = PerformanceMetrics()
        metrics.start_time = time.time()
        
        stream_name = f"benchmark_{sample_rate}_{channel_count}_{data_format}"
        sample_interval = 1.0 / sample_rate
        
        samples_sent = 0
        bytes_sent = 0
        
        try:
            while time.time() - metrics.start_time < self.config.test_duration:
                # Generate data
                if data_format == "json":
                    data, data_size = DataGenerator.generate_json_data(channel_count)
                    # Use XADD for JSON data
                    start_time = time.perf_counter()
                    self.redis_client.xadd(stream_name, {"data": data})
                    end_time = time.perf_counter()
                else:
                    data, data_size = DataGenerator.generate_binary_data(channel_count)
                    # Use LPUSH for binary data
                    start_time = time.perf_counter()
                    self.redis_client.lpush(stream_name, data)
                    end_time = time.perf_counter()
                
                # Record metrics
                latency = (end_time - start_time) * 1000  # Convert to milliseconds
                metrics.add_latency(latency)
                
                samples_sent += 1
                bytes_sent += data_size
                
                # Maintain target sample rate
                expected_time = metrics.start_time + (samples_sent * sample_interval)
                current_time = time.time()
                if current_time < expected_time:
                    time.sleep(expected_time - current_time)
        
        except Exception as e:
            print(f"Error during benchmark: {e}")
            metrics.error_count += 1
        
        metrics.end_time = time.time()
        metrics.total_samples = samples_sent
        metrics.total_bytes = bytes_sent
        
        # Cleanup
        try:
            self.redis_client.delete(stream_name)
        except:
            pass
        
        return metrics
    
    def benchmark_latency(self, channel_count: int = 96) -> PerformanceMetrics:
        """Benchmark latency for single operations"""
        print(f"Benchmarking latency with {channel_count} channels")
        
        metrics = PerformanceMetrics()
        metrics.start_time = time.time()
        
        stream_name = "latency_benchmark"
        num_operations = 1000
        
        try:
            for i in range(num_operations):
                # JSON format test
                data, data_size = DataGenerator.generate_json_data(channel_count)
                
                start_time = time.perf_counter()
                self.redis_client.xadd(stream_name, {"data": data})
                end_time = time.perf_counter()
                
                latency = (end_time - start_time) * 1000  # milliseconds
                metrics.add_latency(latency)
                metrics.total_bytes += data_size
                
                # Small delay between operations
                time.sleep(0.001)
        
        except Exception as e:
            print(f"Error during latency benchmark: {e}")
            metrics.error_count += 1
        
        metrics.end_time = time.time()
        metrics.total_samples = num_operations
        
        # Cleanup
        try:
            self.redis_client.delete(stream_name)
        except:
            pass
        
        return metrics
    
    def run_comprehensive_benchmark(self) -> Dict:
        """Run comprehensive performance benchmark"""
        if not self.connect():
            return {}
        
        results = {}
        
        # Latency benchmark
        print("\n=== Latency Benchmark ===")
        latency_metrics = self.benchmark_latency()
        results["latency"] = latency_metrics.get_summary()
        
        # Throughput benchmarks
        print("\n=== Throughput Benchmarks ===")
        results["throughput"] = {}
        
        for data_format in self.config.data_formats:
            results["throughput"][data_format] = {}
            
            for sample_rate in self.config.sample_rates:
                results["throughput"][data_format][sample_rate] = {}
                
                for channel_count in self.config.channel_counts:
                    metrics = self.benchmark_throughput(sample_rate, channel_count, data_format)
                    results["throughput"][data_format][sample_rate][channel_count] = metrics.get_summary()
        
        return results
    
    def print_results(self, results: Dict):
        """Print benchmark results"""
        print("\n" + "="*60)
        print("REDIS PERFORMANCE BENCHMARK RESULTS")
        print("="*60)
        
        # Latency results
        if "latency" in results:
            latency = results["latency"]["latency_stats"]
            print(f"\nLatency Statistics (ms):")
            print(f"  Mean:   {latency['mean']:.2f}")
            print(f"  Median: {latency['median']:.2f}")
            print(f"  95th %: {latency['p95']:.2f}")
            print(f"  99th %: {latency['p99']:.2f}")
            print(f"  Max:    {latency['max']:.2f}")
        
        # Throughput results
        if "throughput" in results:
            print(f"\nThroughput Results:")
            for data_format in results["throughput"]:
                print(f"\n{data_format.upper()} Format:")
                print(f"{'Sample Rate':<12} {'Channels':<10} {'Samples/sec':<12} {'Mbps':<8} {'Errors':<8}")
                print("-" * 60)
                
                for sample_rate in results["throughput"][data_format]:
                    for channel_count in results["throughput"][data_format][sample_rate]:
                        data = results["throughput"][data_format][sample_rate][channel_count]
                        print(f"{sample_rate:<12} {channel_count:<10} {data['avg_sample_rate']:<12.1f} "
                              f"{data['avg_throughput_mbps']:<8.2f} {data['error_rate']:<8.3f}")
        
        # Performance recommendations
        print(f"\n" + "="*60)
        print("PERFORMANCE RECOMMENDATIONS")
        print("="*60)
        
        if "latency" in results:
            latency_mean = results["latency"]["latency_stats"]["mean"]
            if latency_mean < 1.0:
                print("✓ Excellent latency performance (< 1ms)")
            elif latency_mean < 5.0:
                print("✓ Good latency performance (< 5ms)")
            elif latency_mean < 10.0:
                print("⚠ Acceptable latency performance (< 10ms)")
            else:
                print("❌ Poor latency performance (> 10ms)")
                print("  Consider: Redis optimization, network tuning, or hardware upgrade")
        
        # Check if any configuration meets real-time requirements
        real_time_configs = []
        if "throughput" in results:
            for data_format in results["throughput"]:
                for sample_rate in results["throughput"][data_format]:
                    for channel_count in results["throughput"][data_format][sample_rate]:
                        data = results["throughput"][data_format][sample_rate][channel_count]
                        if (data["avg_sample_rate"] >= sample_rate * 0.95 and  # 95% of target rate
                            data["error_rate"] < 0.01):  # < 1% error rate
                            real_time_configs.append(f"{sample_rate}Hz/{channel_count}ch/{data_format}")
        
        if real_time_configs:
            print(f"\n✓ Real-time capable configurations:")
            for config in real_time_configs:
                print(f"  - {config}")
        else:
            print(f"\n❌ No configurations meet real-time requirements")
            print(f"  Consider reducing sample rate or channel count")

def main():
    """Main function"""
    parser = argparse.ArgumentParser(description="Redis Performance Benchmark")
    parser.add_argument("--host", default="localhost", help="Redis host")
    parser.add_argument("--port", type=int, default=6379, help="Redis port")
    parser.add_argument("--password", default="", help="Redis password")
    parser.add_argument("--duration", type=int, default=10, help="Test duration per configuration")
    parser.add_argument("--sample-rates", nargs="+", type=int, default=[1000, 10000, 30000],
                       help="Sample rates to test")
    parser.add_argument("--channels", nargs="+", type=int, default=[32, 64, 96, 128],
                       help="Channel counts to test")
    parser.add_argument("--formats", nargs="+", default=["json", "binary"],
                       help="Data formats to test")
    
    args = parser.parse_args()
    
    config = BenchmarkConfig(
        redis_host=args.host,
        redis_port=args.port,
        redis_password=args.password,
        test_duration=args.duration,
        sample_rates=args.sample_rates,
        channel_counts=args.channels,
        data_formats=args.formats
    )
    
    benchmark = RedisBenchmark(config)
    results = benchmark.run_comprehensive_benchmark()
    
    if results:
        benchmark.print_results(results)
        
        # Save results to file
        import json
        with open("redis_benchmark_results.json", "w") as f:
            json.dump(results, f, indent=2)
        print(f"\nResults saved to redis_benchmark_results.json")
    else:
        print("Benchmark failed to run")
        sys.exit(1)

if __name__ == "__main__":
    main()
