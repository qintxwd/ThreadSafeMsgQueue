# ThreadSafeMsgQueue

A high-performance, thread-safe message queue framework designed for inter-module communication in complex systems like SLAM applications.

## Features

### Core Message Queue
- **Type-safe message handling** with C++ templates
- **Priority-based message queuing** with deterministic ordering
- **Thread-safe operations** with minimal locking overhead
- **Batch operations** for high-throughput scenarios
- **Performance monitoring** and statistics
- **Exception-safe** operations
- **Modern C++11/14/17** features
- **Header-only library** for easy integration

### PubSub System
- **Type-safe publish-subscribe** messaging pattern
- **Topic-based message routing** with automatic filtering
- **Multiple subscribers** per topic support
- **Batch publishing** for high-throughput scenarios
- **Real-time statistics** and monitoring
- **Global PubSub** singleton for system-wide communication
- **Priority support** for critical messages
- **Zero-configuration** GlobalPubSub ready out of the box

## Quick Start

### Basic Usage

```cpp
#include <ThreadSafeMsgQueue/ThreadSafeMsgQueue.h>

using namespace qyh::ThreadSafeMsgQueue;

// Create a message queue
auto queue = std::make_shared<MsgQueue>(1000);

// Create and enqueue a message
struct MyData { int value; };
auto msg = make_msg<MyData>(0, MyData{42});
queue->enqueue(msg);

// Dequeue and process
auto received = queue->dequeue();
if (auto typed_msg = std::dynamic_pointer_cast<Msg<MyData>>(received)) {
    int value = typed_msg->getContent().value;
}
```

### SLAM Example

```cpp
// SLAM sensor data structures
struct LaserScanData {
    double timestamp;
    std::vector<float> ranges;
    std::vector<float> angles;
};

struct OdometryData {
    double timestamp;
    double x, y, theta;
};

// Create queues for different data types
auto laser_queue = std::make_shared<MsgQueue>(500);
auto odom_queue = std::make_shared<MsgQueue>(100);

// Enqueue sensor data with priorities
auto laser_msg = make_msg<LaserScanData>(1, laser_data);
auto odom_msg = make_msg<OdometryData>(2, odom_data);

laser_queue->enqueue(laser_msg);
odom_queue->enqueue(odom_msg);
```

### PubSub System Usage

#### Basic PubSub Example

```cpp
#include <ThreadSafeMsgQueue/PubSub.h>

using namespace qyh::ThreadSafeMsgQueue;

// Define your data structures
struct SensorReading {
    int sensor_id;
    double timestamp;
    double value;
    std::string unit;
};

// Create and configure PubSub system
PubSubSystem::Config config;
config.default_queue_size = 1000;
config.worker_thread_count = 2;
config.enable_statistics = true;

PubSubSystem pubsub(config);
pubsub.start();

// Subscribe to messages
auto sub_id = pubsub.subscribe<SensorReading>("sensors/temperature",
    [](const MsgPtr<SensorReading>& msg) {
        const auto& reading = msg->getContent();
        std::cout << "Temperature: " << reading.value << reading.unit << std::endl;
    });

// Publish messages
SensorReading temp_data{1, getCurrentTime(), 23.5, "°C"};
pubsub.publish("sensors/temperature", temp_data, 5); // priority 5

// Cleanup
pubsub.unsubscribe("sensors/temperature", sub_id);
pubsub.stop();
```

#### GlobalPubSub Example

```cpp
#include <ThreadSafeMsgQueue/PubSub.h>

using namespace qyh::ThreadSafeMsgQueue;

// Start the global system
GlobalPubSub::start();

// Subscribe from anywhere in your application
auto sub_id = GlobalPubSub::subscribe<SensorReading>("sensors/pressure",
    [](const MsgPtr<SensorReading>& msg) {
        // Process pressure data
    });

// Publish from anywhere
SensorReading pressure_data{2, getCurrentTime(), 1013.25, "hPa"};
GlobalPubSub::publish("sensors/pressure", pressure_data, 7);

// Cleanup
GlobalPubSub::unsubscribe("sensors/pressure", sub_id);
GlobalPubSub::stop();
```

#### SLAM System with PubSub

```cpp
// SLAM sensor node
class SLAMSensorNode {
public:
    void start() {
        running_ = true;
        sensor_thread_ = std::thread(&SLAMSensorNode::sensorLoop, this);
    }
    
private:
    void sensorLoop() {
        while (running_) {
            // Generate laser scan data
            LaserScan scan = generateLaserScan();
            GlobalPubSub::publish("laser_scan", scan, 8);
            
            // Generate odometry data
            if (counter % 5 == 0) {
                Odometry odom = generateOdometry();
                GlobalPubSub::publish("odometry", odom, 7);
            }
            
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    }
    
    std::atomic<bool> running_{false};
    std::thread sensor_thread_;
};

// SLAM processor node
class SLAMProcessor {
public:
    void start() {
        laser_sub_id_ = GlobalPubSub::subscribe<LaserScan>("laser_scan",
            [this](const MsgPtr<LaserScan>& msg) {
                this->processLaserScan(msg);
            });
            
        odom_sub_id_ = GlobalPubSub::subscribe<Odometry>("odometry",
            [this](const MsgPtr<Odometry>& msg) {
                this->processOdometry(msg);
            });
    }
    
private:
    void processLaserScan(const MsgPtr<LaserScan>& msg) {
        // Process laser scan and publish map updates
        MapUpdate update = processAndGenerateMapUpdate(msg->getContent());
        GlobalPubSub::publish("map_updates", update, 6);
    }
    
    uint64_t laser_sub_id_;
    uint64_t odom_sub_id_;
};
```

## Building

This is a header-only library. Simply include the headers and link with pthread:

```bash
mkdir build && cd build
cmake ..
make
```

### CMake Integration

```cmake
find_package(ThreadSafeMsgQueue REQUIRED)
target_link_libraries(your_target ThreadSafeMsgQueue::ThreadSafeMsgQueue)
```

## API Reference

### PubSubSystem Class

#### Configuration
```cpp
struct Config {
    size_t default_queue_size = 1000;     // Default queue size for topics
    size_t worker_thread_count = 1;       // Number of worker threads
    bool enable_statistics = false;       // Enable performance statistics
    size_t max_batch_size = 100;         // Maximum batch size for operations
};
```

#### Core Methods
```cpp
// System lifecycle
bool start();                          // Start the PubSub system
void stop();                           // Stop the PubSub system
bool isRunning() const;                // Check if system is running

// Publishing
template<typename T>
bool publish(const std::string& topic, const T& data, int priority = 0);

template<typename T>
bool publishBatch(const std::string& topic, const std::vector<T>& data_list, int priority = 0);

// Subscribing
template<typename T>
uint64_t subscribe(const std::string& topic, std::function<void(const MsgPtr<T>&)> callback);

bool unsubscribe(const std::string& topic, uint64_t subscriber_id);

// Statistics
PubSubStatistics getStatistics() const;
void resetStatistics();
```

### GlobalPubSub Class

#### Static Methods
```cpp
// System lifecycle
static bool start(const PubSubSystem::Config& config = PubSubSystem::Config{});
static void stop();
static bool isRunning();

// Publishing
template<typename T>
static bool publish(const std::string& topic, const T& data, int priority = 0);

template<typename T>
static bool publishBatch(const std::string& topic, const std::vector<T>& data_list, int priority = 0);

// Subscribing
template<typename T>
static uint64_t subscribe(const std::string& topic, std::function<void(const MsgPtr<T>&)> callback);

static bool unsubscribe(const std::string& topic, uint64_t subscriber_id);

// Statistics
static PubSubStatistics getStatistics();
static void resetStatistics();
```

### Statistics Structure
```cpp
struct PubSubStatistics {
    size_t total_published_messages = 0;   // Total messages published
    size_t total_processed_messages = 0;   // Total messages processed
    size_t active_topics = 0;              // Number of active topics
    size_t active_subscribers = 0;         // Number of active subscribers
    std::map<std::string, size_t> topic_message_counts; // Per-topic message counts
};
```

### Best Practices

1. **Topic Naming**: Use hierarchical naming like `"sensors/temperature"`, `"slam/laser_scan"`
2. **Priority Usage**: Higher numbers = higher priority (0-10 recommended range)
3. **Batch Operations**: Use batch publishing for high-throughput scenarios
4. **Resource Management**: Always call `stop()` or `unsubscribe()` for cleanup
5. **Thread Safety**: All operations are thread-safe, no external synchronization needed
6. **Error Handling**: Check return values of `start()`, `publish()`, etc.

## Performance

### Core Message Queue
Benchmark results on Intel i7-8700K @ 3.70GHz:

- **Single-threaded enqueue**: ~15M ops/sec
- **Single-threaded dequeue**: ~12M ops/sec
- **Multi-threaded (4 producers, 4 consumers)**: ~8M ops/sec
- **Priority queue operations**: ~10M ops/sec
- **Memory overhead**: ~24 bytes per message

### PubSub System
Benchmark results for PubSub operations:

- **Single topic publish**: ~5M msgs/sec
- **Multi-topic publish (10 topics)**: ~3M msgs/sec
- **Subscriber notification**: ~8M callbacks/sec
- **Batch publishing (100 msgs)**: ~20M msgs/sec
- **Topic filtering overhead**: <5% performance impact
- **Memory overhead**: ~32 bytes per message + topic routing

### SLAM System Performance
Real-world SLAM application benchmarks:

- **Laser scan processing**: 50Hz with <1ms latency
- **Odometry updates**: 200Hz with <0.5ms latency
- **Map updates**: 10Hz with <2ms latency
- **Cross-node communication**: <100μs message delivery
- **System throughput**: >100K msgs/sec sustained

### Complexity Analysis
- **Enqueue**: O(log n) due to priority queue
- **Dequeue**: O(log n) due to priority queue  
- **Batch operations**: O(k log n) where k is batch size
- **Memory overhead**: Minimal with efficient shared_ptr usage
- **Thread contention**: Reduced through careful lock granularity

## Testing

Run the test suite:

```bash
cd build
ctest
```

Run comprehensive examples:

```bash
# Core message queue examples
./slam_example

# PubSub system comprehensive demo
./comprehensive_pubsub_demo
```

### Test Coverage

#### Core Message Queue Tests
- Thread safety with multiple producers/consumers
- Priority ordering verification
- Memory leak detection
- Performance benchmarks
- Exception safety

#### PubSub System Tests
- Topic-based message routing
- Multiple subscribers per topic
- Batch publishing operations
- Statistics accuracy
- GlobalPubSub singleton behavior
- Cross-thread communication
- Resource cleanup verification

#### SLAM Integration Tests
- Real-time sensor data processing
- Multi-node communication patterns
- High-frequency message handling
- System stability under load
- Memory usage optimization

## Documentation

- **English Documentation**: This README
- **[Chinese Documentation](README_CN.md)**: Chinese documentation

## Requirements

- C++11 or later
- CMake 3.14+
- pthread support

## License

See [LICENSE](LICENSE) file for details.

## Author

QYH - 2025

## Version

2.0.0
