# ThreadSafeMsgQueue 线程安全消息队列

一个高性能、线程安全的消息队列框架，专为SLAM等复杂系统中的模块间通信而设计。

## 功能特性

### 核心消息队列
- **类型安全的消息处理** - 基于C++模板
- **基于优先级的消息队列** - 确定性排序
- **线程安全操作** - 最小锁开销
- **批量操作** - 支持高吞吐量场景
- **性能监控** - 内置统计功能
- **异常安全** - 保证异常安全操作
- **现代C++11/14/17** - 利用现代C++特性
- **仅头文件库** - 易于集成

### 发布订阅系统
- **类型安全的发布订阅** 消息模式
- **基于主题的消息路由** 自动过滤
- **多订阅者支持** 每个主题支持多个订阅者
- **批量发布** 高吞吐量场景
- **实时统计** 和监控
- **全局发布订阅** 系统级通信的单例模式
- **优先级支持** 关键消息优先处理
- **零配置** 全局发布订阅开箱即用

## 快速开始

### 基础用法

```cpp
#include <ThreadSafeMsgQueue/ThreadSafeMsgQueue.h>

using namespace qyh::ThreadSafeMsgQueue;

// 创建消息队列
auto queue = std::make_shared<MsgQueue>(1000);

// 创建并入队消息
struct MyData { int value; };
auto msg = make_msg<MyData>(0, MyData{42});
queue->enqueue(msg);

// 出队并处理
auto received = queue->dequeue();
if (auto typed_msg = std::dynamic_pointer_cast<Msg<MyData>>(received)) {
    int value = typed_msg->getContent().value;
}
```

### SLAM应用示例

```cpp
// SLAM传感器数据结构
struct LaserScanData {
    double timestamp;
    std::vector<float> ranges;
    std::vector<float> angles;
};

struct OdometryData {
    double timestamp;
    double x, y, theta;
};

// 为不同数据类型创建队列
auto laser_queue = std::make_shared<MsgQueue>(500);
auto odom_queue = std::make_shared<MsgQueue>(100);

// 按优先级入队传感器数据
auto laser_msg = make_msg<LaserScanData>(1, laser_data);
auto odom_msg = make_msg<OdometryData>(2, odom_data);

laser_queue->enqueue(laser_msg);
odom_queue->enqueue(odom_msg);
```

### 发布订阅系统使用

#### 基础发布订阅示例

```cpp
#include <ThreadSafeMsgQueue/PubSub.h>

using namespace qyh::ThreadSafeMsgQueue;

// 定义数据结构
struct SensorReading {
    int sensor_id;
    double timestamp;
    double value;
    std::string unit;
};

// 创建并配置发布订阅系统
PubSubSystem::Config config;
config.default_queue_size = 1000;
config.worker_thread_count = 2;
config.enable_statistics = true;

PubSubSystem pubsub(config);
pubsub.start();

// 订阅消息
auto sub_id = pubsub.subscribe<SensorReading>("sensors/temperature",
    [](const MsgPtr<SensorReading>& msg) {
        const auto& reading = msg->getContent();
        std::cout << "温度: " << reading.value << reading.unit << std::endl;
    });

// 发布消息
SensorReading temp_data{1, getCurrentTime(), 23.5, "°C"};
pubsub.publish("sensors/temperature", temp_data, 5); // 优先级5

// 清理
pubsub.unsubscribe("sensors/temperature", sub_id);
pubsub.stop();
```

#### 全局发布订阅示例

```cpp
#include <ThreadSafeMsgQueue/PubSub.h>

using namespace qyh::ThreadSafeMsgQueue;

// 启动全局系统
GlobalPubSub::start();

// 在应用程序的任何地方订阅
auto sub_id = GlobalPubSub::subscribe<SensorReading>("sensors/pressure",
    [](const MsgPtr<SensorReading>& msg) {
        // 处理压力数据
    });

// 在任何地方发布
SensorReading pressure_data{2, getCurrentTime(), 1013.25, "hPa"};
GlobalPubSub::publish("sensors/pressure", pressure_data, 7);

// 清理
GlobalPubSub::unsubscribe("sensors/pressure", sub_id);
GlobalPubSub::stop();
```

#### SLAM系统中的发布订阅

```cpp
// SLAM传感器节点
class SLAMSensorNode {
public:
    void start() {
        running_ = true;
        sensor_thread_ = std::thread(&SLAMSensorNode::sensorLoop, this);
    }
    
private:
    void sensorLoop() {
        while (running_) {
            // 生成激光扫描数据
            LaserScan scan = generateLaserScan();
            GlobalPubSub::publish("laser_scan", scan, 8);
            
            // 生成里程计数据
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

// SLAM处理器节点
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
        // 处理激光扫描并发布地图更新
        MapUpdate update = processAndGenerateMapUpdate(msg->getContent());
        GlobalPubSub::publish("map_updates", update, 6);
    }
    
    uint64_t laser_sub_id_;
    uint64_t odom_sub_id_;
};
```

## 命名空间

所有代码都在 `qyh::ThreadSafeMsgQueue` 命名空间下：

```cpp
using namespace qyh::ThreadSafeMsgQueue;
// 或者
using qyh::ThreadSafeMsgQueue::MsgQueue;
using qyh::ThreadSafeMsgQueue::make_msg;
```

## 构建

这是一个仅头文件库，只需包含头文件并链接pthread：

```bash
mkdir build && cd build
cmake ..
make
```

### CMake集成

```cmake
find_package(ThreadSafeMsgQueue REQUIRED)
target_link_libraries(your_target ThreadSafeMsgQueue::ThreadSafeMsgQueue)
```

## API参考

### PubSubSystem类

#### 配置
```cpp
struct Config {
    size_t default_queue_size = 1000;     // 主题的默认队列大小
    size_t worker_thread_count = 1;       // 工作线程数量
    bool enable_statistics = false;       // 启用性能统计
    size_t max_batch_size = 100;         // 操作的最大批量大小
};
```

#### 核心方法
```cpp
// 系统生命周期
bool start();                          // 启动发布订阅系统
void stop();                           // 停止发布订阅系统
bool isRunning() const;                // 检查系统是否运行

// 发布
template<typename T>
bool publish(const std::string& topic, const T& data, int priority = 0);

template<typename T>
bool publishBatch(const std::string& topic, const std::vector<T>& data_list, int priority = 0);

// 订阅
template<typename T>
uint64_t subscribe(const std::string& topic, std::function<void(const MsgPtr<T>&)> callback);

bool unsubscribe(const std::string& topic, uint64_t subscriber_id);

// 统计
PubSubStatistics getStatistics() const;
void resetStatistics();
```

### GlobalPubSub类

#### 静态方法
```cpp
// 系统生命周期
static bool start(const PubSubSystem::Config& config = PubSubSystem::Config{});
static void stop();
static bool isRunning();

// 发布
template<typename T>
static bool publish(const std::string& topic, const T& data, int priority = 0);

template<typename T>
static bool publishBatch(const std::string& topic, const std::vector<T>& data_list, int priority = 0);

// 订阅
template<typename T>
static uint64_t subscribe(const std::string& topic, std::function<void(const MsgPtr<T>&)> callback);

static bool unsubscribe(const std::string& topic, uint64_t subscriber_id);

// 统计
static PubSubStatistics getStatistics();
static void resetStatistics();
```

### 统计结构
```cpp
struct PubSubStatistics {
    size_t total_published_messages = 0;   // 总发布消息数
    size_t total_processed_messages = 0;   // 总处理消息数
    size_t active_topics = 0;              // 活跃主题数
    size_t active_subscribers = 0;         // 活跃订阅者数
    std::map<std::string, size_t> topic_message_counts; // 每个主题的消息计数
};
```

### 最佳实践

1. **主题命名**：使用分层命名如 `"sensors/temperature"`、`"slam/laser_scan"`
2. **优先级使用**：数字越大优先级越高（推荐0-10范围）
3. **批量操作**：在高吞吐量场景中使用批量发布
4. **资源管理**：始终调用 `stop()` 或 `unsubscribe()` 进行清理
5. **线程安全**：所有操作都是线程安全的，无需外部同步
6. **错误处理**：检查 `start()`、`publish()` 等的返回值

## 性能特性

### 核心消息队列
在Intel i7-8700K @ 3.70GHz上的基准测试结果：

- **单线程入队**：约15M操作/秒
- **单线程出队**：约12M操作/秒
- **多线程（4生产者，4消费者）**：约8M操作/秒
- **优先队列操作**：约10M操作/秒
- **内存开销**：每消息约24字节

### 发布订阅系统
发布订阅操作的基准测试结果：

- **单主题发布**：约5M消息/秒
- **多主题发布（10个主题）**：约3M消息/秒
- **订阅者通知**：约8M回调/秒
- **批量发布（100条消息）**：约20M消息/秒
- **主题过滤开销**：<5%性能影响
- **内存开销**：每消息约32字节+主题路由

### SLAM系统性能
真实SLAM应用基准测试：

- **激光扫描处理**：50Hz，延迟<1ms
- **里程计更新**：200Hz，延迟<0.5ms
- **地图更新**：10Hz，延迟<2ms
- **跨节点通信**：<100μs消息传递
- **系统吞吐量**：>100K消息/秒持续

### 复杂度分析
- **入队操作**：O(log n) - 基于优先队列
- **出队操作**：O(log n) - 基于优先队列
- **批量操作**：O(k log n) - k为批量大小
- **内存开销**：最小化 - 高效的shared_ptr使用
- **线程竞争**：减少 - 精心设计的锁粒度

## 测试

运行测试套件：

```bash
mkdir build && cd build
cmake ..
make
./test_runner
```

运行综合示例：

```bash
# 核心消息队列示例
./slam_example

# 发布订阅系统综合演示
./comprehensive_pubsub_demo
```

### 测试覆盖

#### 核心消息队列测试
- 多生产者/消费者的线程安全性
- 优先级排序验证
- 内存泄漏检测
- 性能基准测试
- 异常安全性

#### 发布订阅系统测试
- 基于主题的消息路由
- 每个主题的多订阅者
- 批量发布操作
- 统计准确性
- 全局发布订阅单例行为
- 跨线程通信
- 资源清理验证

#### SLAM集成测试
- 实时传感器数据处理
- 多节点通信模式
- 高频消息处理
- 负载下的系统稳定性
- 内存使用优化

### ODR合规性测试

框架包含完整的ODR（一个定义规则）合规性测试：

```cpp
#include "tests/ODRTest.h"
bool result = ODRTest::runCompleteTest();
```

## 系统要求

- C++11或更高版本
- CMake 3.14+
- pthread支持

## 架构设计

### 核心组件

1. **BaseMsg/Msg<T>** - 类型安全的消息基类
2. **MsgQueue** - 线程安全的优先消息队列
3. **SubCallback** - 回调机制支持
4. **PubSub** - 发布订阅系统（可选）

### 设计原则

- **零拷贝** - 通过完美转发和移动语义
- **RAII** - 自动资源管理
- **异常安全** - 强异常安全保证
- **高性能** - 原子操作和最小锁竞争

## 文档

- **[英文文档](README.md)**：英文文档
- **中文文档**：本文档

## 系统要求

- C++11或更高版本
- CMake 3.14+
- pthread支持

## 许可证

详见[LICENSE](LICENSE)文件。

## 作者

QYH - 2025

## 版本

2.0.0
