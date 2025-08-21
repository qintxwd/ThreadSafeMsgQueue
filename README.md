# ThreadSafeMsgQueue v2.0

高性能的C++ header-only线程安全消息队列框架，专为复杂系统（如SLAM应用）的模块间通信而设计。

## ✨ 主要特性

### 🔧 **核心功能**
- **Header-Only**: 无需编译，包含即用
- **线程安全**: 完全线程安全的操作，支持多生产者多消费者
- **类型安全**: 基于模板的消息处理，编译时类型检查
- **优先级队列**: 支持消息优先级，确定性排序
- **ODR合规**: 解决了全局变量冲突，可安全用于大型项目

### ⚡ **高性能特性**
- **批量操作**: 支持批量入队/出队，提供20倍+性能提升
- **无锁统计**: 原子操作实现的性能监控
- **零拷贝**: 完美转发和移动语义，减少内存拷贝
- **内存高效**: 智能指针管理，最小化动态分配

### 📊 **高级功能**
- **性能统计**: 实时吞吐量、延迟、队列大小监控
- **溢出保护**: 可配置的队列大小限制
- **超时支持**: 阻塞操作支持超时机制
- **异常安全**: 全面的异常安全保证

## 🚀 性能基准

在典型硬件上的测试结果：

```
=== 性能测试结果 ===
单线程入队:     ~586K msgs/sec
单线程出队:     ~3.1M msgs/sec  
多线程吞吐:     ~493K msgs/sec
批量入队:       ~22.9M msgs/sec  ⭐ (40倍提升!)
批量出队:       ~15.0M msgs/sec  ⭐ (5倍提升!)
```

## 📦 快速开始

### 基本使用

```cpp
#include "ThreadSafeMsgQueue.h"

// 定义消息类型
struct SensorData {
    int sensor_id;
    double timestamp;
    std::vector<double> values;
};

int main() {
    // 创建消息队列
    auto queue = std::make_shared<MsgQueue>(1000);
    
    // 创建并发送消息
    auto msg = make_msg<SensorData>(5, SensorData{1, 1.0, {1.1, 2.2, 3.3}});
    queue->enqueue(msg);
    
    // 接收并处理消息
    auto received = queue->dequeue();
    if (auto sensor_msg = std::dynamic_pointer_cast<Msg<SensorData>>(received)) {
        const auto& data = sensor_msg->getContent();
        std::cout << "传感器ID: " << data.sensor_id << std::endl;
    }
    
    return 0;
}
```

### 高性能批量处理

```cpp
// 批量发送消息
std::vector<BaseMsgPtr> batch;
for (int i = 0; i < 100; ++i) {
    batch.push_back(make_msg<SensorData>(1, SensorData{i, i*0.1, {i, i+1, i+2}}));
}
size_t sent = queue->enqueue_batch(batch);

// 批量接收消息
std::vector<BaseMsgPtr> received_batch;
size_t count = queue->dequeue_batch(received_batch, 50);
```

### 性能监控

```cpp
auto stats = queue->getStatistics();
std::cout << "总发送: " << stats.total_enqueued.load() << std::endl;
std::cout << "总接收: " << stats.total_dequeued.load() << std::endl;
std::cout << "当前大小: " << stats.current_size.load() << std::endl;
std::cout << "峰值大小: " << stats.peak_size.load() << std::endl;
```

## 🏗️ 编译和测试

### 前提条件
- CMake 3.10+
- C++14兼容的编译器 (MSVC 2017+, GCC 7+, Clang 5+)

### 编译步骤

```bash
mkdir build && cd build
cmake ..
cmake --build . --config Release
```

### 运行测试

```bash
# ODR合规性测试
./Release/odr_test.exe

# 功能示例测试  
./Release/simple_example.exe

# 性能基准测试
./Release/performance_test.exe

# CMake测试套件
ctest -C Release --verbose
```

## 🎯 SLAM系统集成

本框架专为SLAM系统优化，完美适配：

- **激光雷达数据** (10-40Hz): ✅ 轻松处理
- **相机数据** (30-60FPS): ✅ 绰绰有余  
- **IMU数据** (100-1000Hz): ✅ 完全胜任
- **里程计数据** (50-100Hz): ✅ 无压力

### SLAM模块间通信示例

```cpp
// 激光雷达模块
struct LaserScanData { /* ... */ };
auto laser_msg = make_msg<LaserScanData>(5, scan_data);  // 高优先级
laser_queue->enqueue(laser_msg);

// 建图模块  
struct MapData { /* ... */ };
auto map_msg = make_msg<MapData>(1, map_data);  // 低优先级
mapping_queue->enqueue(map_msg);
```

## 📚 文档

- [构建和测试指南](BuildAndTestGuide.md) - 详细的编译和测试说明
- [部署指南](DeploymentGuide.md) - Header-Only vs 动态库部署方案对比
- [API文档](ThreadSafeMsgQueue.h) - 完整的API说明和使用示例

## 🔄 版本历史

### v2.0.0 (2025-08-21)
- ✅ **重大更新**: 完全解决ODR问题，支持header-only部署
- ✅ **性能优化**: 批量操作提供20倍+性能提升
- ✅ **现代C++**: 升级到C++14，使用移动语义和完美转发
- ✅ **统计监控**: 添加实时性能监控和统计功能
- ✅ **全面测试**: ODR合规性测试、性能基准测试、功能测试

### v1.0.0 (原始版本)
- 基础的线程安全消息队列
- 主题订阅发布功能
- 消息优先级支持

## 📄 许可证

本项目采用 [LICENSE](LICENSE) 许可证。

## 🤝 贡献

欢迎提交Issue和Pull Request！

## 📞 联系方式

- 作者: QYH
- 项目地址: https://github.com/qintxwd/ThreadSafeMsgQueue

---

**🚀 现在就开始使用ThreadSafeMsgQueue，为您的SLAM系统提供高性能的模块间通信！**
