#include "ThreadSafeMsgQueue/ThreadSafeMsgQueue.h"
#include "ThreadSafeMsgQueue/PubSub.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <vector>
#include <cmath>
#include <atomic>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

using namespace qyh::ThreadSafeMsgQueue;

// Common data structures
struct SensorReading {
    int sensor_id;
    double timestamp;
    double value;
    std::string unit;
    
    SensorReading(int id, double ts, double val, const std::string& u)
        : sensor_id(id), timestamp(ts), value(val), unit(u) {}
};

struct SystemAlert {
    std::string level;
    std::string message;
    double timestamp;
    
    SystemAlert(const std::string& lvl, const std::string& msg, double ts)
        : level(lvl), message(msg), timestamp(ts) {}
};

struct ControlCommand {
    std::string target;
    std::string action;
    std::vector<double> parameters;
    
    ControlCommand(const std::string& tgt, const std::string& act, const std::vector<double>& params)
        : target(tgt), action(act), parameters(params) {}
};

// SLAM-specific data structures
struct LaserScan {
    int scan_id;
    double timestamp;
    std::vector<float> ranges;
    float angle_min, angle_max, angle_increment;
    
    LaserScan(int id, double ts, const std::vector<float>& r, float min_ang, float max_ang, float inc)
        : scan_id(id), timestamp(ts), ranges(r), angle_min(min_ang), angle_max(max_ang), angle_increment(inc) {}
};

struct Odometry {
    double timestamp;
    double x, y, theta;
    double linear_vel, angular_vel;
    
    Odometry(double ts, double px, double py, double angle, double lin_v, double ang_v)
        : timestamp(ts), x(px), y(py), theta(angle), linear_vel(lin_v), angular_vel(ang_v) {}
};

struct MapUpdate {
    int update_id;
    double timestamp;
    std::string region;
    bool is_obstacle;
    
    MapUpdate(int id, double ts, const std::string& reg, bool obstacle)
        : update_id(id), timestamp(ts), region(reg), is_obstacle(obstacle) {}
};

class PubSubSystemDemo {
public:
    void runDemo() {
        std::cout << "=== PubSubSystem Basic Demo ===" << std::endl;
        std::cout << "Demonstrates basic functionality of type-safe publish-subscribe system" << std::endl;
        
        PubSubSystem::Config config;
        config.default_queue_size = 1000;
        config.worker_thread_count = 2;
        config.enable_statistics = true;
        
        PubSubSystem pubsub(config);
        if (!pubsub.start()) {
            std::cout << "❌ Failed to start PubSub system" << std::endl;
            return;
        }
        
        std::cout << "✅ PubSubSystem started successfully" << std::endl;
        
        demoBasicPubSub(pubsub);
        demoMultipleSubscribers(pubsub);
        demoTopicFiltering(pubsub);
        demoBatchPublishing(pubsub);
        demoStatisticsMonitoring(pubsub);
        
        pubsub.stop();
        std::cout << "🎉 PubSubSystem demo completed" << std::endl;
    }

private:
    void demoBasicPubSub(PubSubSystem& pubsub) {
        std::cout << "\n📡 1. Basic Publish-Subscribe Demo" << std::endl;
        
        std::atomic<int> sensor_readings_received{0};
        std::atomic<int> alerts_received{0};
        
        auto sensor_sub = pubsub.subscribe<SensorReading>("sensors/temperature",
            [&](const MsgPtr<SensorReading>& msg) {
                const auto& reading = msg->getContent();
                std::cout << "  📊 Received temperature sensor data: " << reading.value << reading.unit 
                         << " (Sensor ID: " << reading.sensor_id << ")" << std::endl;
                sensor_readings_received.fetch_add(1);
            });
        
        auto alert_sub = pubsub.subscribe<SystemAlert>("system/alerts",
            [&](const MsgPtr<SystemAlert>& msg) {
                const auto& alert = msg->getContent();
                std::cout << "  🚨 System alert [" << alert.level << "]: " << alert.message << std::endl;
                alerts_received.fetch_add(1);
            });
        
        pubsub.publish("sensors/temperature", SensorReading(1, getCurrentTime(), 23.5, "°C"));
        pubsub.publish("sensors/temperature", SensorReading(2, getCurrentTime(), 24.1, "°C"));
        pubsub.publish("system/alerts", SystemAlert("INFO", "System startup completed", getCurrentTime()));
        pubsub.publish("system/alerts", SystemAlert("WARNING", "Temperature sensor response slow", getCurrentTime()));
        
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        
        std::cout << "  📈 Processing results: Sensor data " << sensor_readings_received.load() 
                  << "/2, Alerts " << alerts_received.load() << "/2" << std::endl;
    }
    
    void demoMultipleSubscribers(PubSubSystem& pubsub) {
        std::cout << "\n👥 2. Multiple Subscribers Demo" << std::endl;
        
        std::atomic<int> total_received{0};
        const int subscriber_count = 5;
        
        std::vector<uint64_t> subscription_ids;
        for (int i = 0; i < subscriber_count; ++i) {
            auto sub_id = pubsub.subscribe<SensorReading>("sensors/multi_test",
                [&total_received, i](const MsgPtr<SensorReading>& msg) {
                    total_received.fetch_add(1);
                    std::cout << "    Subscriber " << i << " received message" << std::endl;
                });
            subscription_ids.push_back(sub_id);
        }
        
        pubsub.publish("sensors/multi_test", SensorReading(99, getCurrentTime(), 42.0, "units"));
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        std::cout << "  📈 " << subscriber_count << " subscribers received a total of " << total_received.load() << " messages" << std::endl;
        
        for (auto sub_id : subscription_ids) {
            pubsub.unsubscribe("sensors/multi_test", sub_id);
        }
    }
    
    void demoTopicFiltering(PubSubSystem& pubsub) {
        std::cout << "\n🔍 3. Topic Filtering Demo" << std::endl;
        
        std::atomic<int> temp_received{0}, pressure_received{0}, humidity_received{0};
        
        auto temp_sub = pubsub.subscribe<SensorReading>("sensors/temperature",
            [&](const MsgPtr<SensorReading>& msg) {
                temp_received.fetch_add(1);
            });
            
        auto pressure_sub = pubsub.subscribe<SensorReading>("sensors/pressure", 
            [&](const MsgPtr<SensorReading>& msg) {
                pressure_received.fetch_add(1);
            });
            
        auto humidity_sub = pubsub.subscribe<SensorReading>("sensors/humidity",
            [&](const MsgPtr<SensorReading>& msg) {
                humidity_received.fetch_add(1);
            });
        
        pubsub.publish("sensors/temperature", SensorReading(1, getCurrentTime(), 25.0, "°C"));
        pubsub.publish("sensors/temperature", SensorReading(2, getCurrentTime(), 26.0, "°C"));
        pubsub.publish("sensors/pressure", SensorReading(3, getCurrentTime(), 1013.25, "hPa"));
        pubsub.publish("sensors/humidity", SensorReading(4, getCurrentTime(), 65.0, "%"));
        pubsub.publish("sensors/humidity", SensorReading(5, getCurrentTime(), 68.0, "%"));
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        std::cout << "  📊 Topic distribution results:" << std::endl;
        std::cout << "    Temperature: " << temp_received.load() << "/2" << std::endl;
        std::cout << "    Pressure: " << pressure_received.load() << "/1" << std::endl;
        std::cout << "    Humidity: " << humidity_received.load() << "/2" << std::endl;
    }
    
    void demoBatchPublishing(PubSubSystem& pubsub) {
        std::cout << "\n📦 4. Batch Publishing Demo" << std::endl;
        
        std::atomic<int> batch_received{0};
        
        auto batch_sub = pubsub.subscribe<SensorReading>("sensors/batch_test",
            [&](const MsgPtr<SensorReading>& msg) {
                batch_received.fetch_add(1);
            });
        
        std::vector<SensorReading> batch_data;
        for (int i = 0; i < 50; ++i) {
            batch_data.emplace_back(i, getCurrentTime(), 20.0 + i * 0.1, "°C");
        }
        
        auto start = std::chrono::high_resolution_clock::now();
        size_t published = pubsub.publishBatch("sensors/batch_test", batch_data);
        auto end = std::chrono::high_resolution_clock::now();
        
        auto batch_time = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        
        while (batch_received.load() < published) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        
        std::cout << "  📊 Batch publishing results: Published " << published << ", Received " << batch_received.load() << std::endl;
        std::cout << "  ⏱️ Batch publishing time: " << batch_time.count() << " μs" << std::endl;
        
        double rate = published * 1000000.0 / batch_time.count();
        std::cout << "  🚀 Publishing rate: " << static_cast<int>(rate) << " msgs/sec" << std::endl;
    }
    
    void demoStatisticsMonitoring(PubSubSystem& pubsub) {
        std::cout << "\n📊 5. Statistics Monitoring Demo" << std::endl;
        
        auto monitor_sub = pubsub.subscribe<ControlCommand>("control/commands",
            [](const MsgPtr<ControlCommand>& msg) {
                // Process command
            });
        
        for (int i = 0; i < 20; ++i) {
            std::vector<double> params = {static_cast<double>(i), static_cast<double>(i * 2)};
            pubsub.publish("control/commands", ControlCommand("robot", "move", params));
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        
        auto stats = pubsub.getTopicStatistics("control/commands");
        auto topic_names = pubsub.getTopicNames();
        auto subscriber_count = pubsub.getSubscriberCount("control/commands");
        
        std::cout << "  📈 Topic statistics:" << std::endl;
        std::cout << "    Published messages: " << stats.messages_published.load() << std::endl;
        std::cout << "    Processed messages: " << stats.messages_processed.load() << std::endl;
        std::cout << "    Active subscribers: " << stats.active_subscribers.load() << std::endl;
        std::cout << "    Subscriber count: " << subscriber_count << std::endl;
        
        std::cout << "  📋 Active topic list (" << topic_names.size() << " topics):" << std::endl;
        for (const auto& topic : topic_names) {
            std::cout << "    - " << topic << std::endl;
        }
    }
    
    double getCurrentTime() {
        auto now = std::chrono::high_resolution_clock::now();
        auto duration = now.time_since_epoch();
        return std::chrono::duration<double>(duration).count();
    }
};

// SLAM sensor node
class SLAMSensorNode {
public:
    SLAMSensorNode(const std::string& node_name) : name_(node_name), running_(false) {}
    
    void start() {
        running_ = true;
        sensor_thread_ = std::thread(&SLAMSensorNode::sensorLoop, this);
        std::cout << "[" << name_ << "] SLAM sensor node started" << std::endl;
    }
    
    void stop() {
        running_ = false;
        if (sensor_thread_.joinable()) {
            sensor_thread_.join();
        }
        std::cout << "[" << name_ << "] SLAM sensor node stopped" << std::endl;
    }
    
private:
    void sensorLoop() {
        int counter = 0;
        auto start_time = std::chrono::high_resolution_clock::now();
        
        while (running_) {
            auto current_time = std::chrono::high_resolution_clock::now();
            auto elapsed = std::chrono::duration<double>(current_time - start_time).count();
            
            // Publish laser scan data
            std::vector<float> ranges;
            for (int i = 0; i < 360; ++i) {
                float angle = i * M_PI / 180.0f;
                float range = 5.0f + 2.0f * sin(angle + elapsed);
                ranges.push_back(range);
            }
            
            LaserScan scan(counter, elapsed, ranges, static_cast<float>(-M_PI), static_cast<float>(M_PI), static_cast<float>(M_PI/180.0));
            GlobalPubSub::publish("laser_scan", scan, 8);
            
            // Publish odometry data every 5 iterations
            if (counter % 5 == 0) {
                double x = elapsed * 0.1;
                double y = sin(elapsed * 0.1) * 0.5;
                double theta = elapsed * 0.05;
                Odometry odom(elapsed, x, y, theta, 0.1, 0.05);
                GlobalPubSub::publish("odometry", odom, 7);
            }
            
            counter++;
            std::this_thread::sleep_for(std::chrono::milliseconds(50)); // 20Hz
        }
    }
    
    std::string name_;
    std::atomic<bool> running_;
    std::thread sensor_thread_;
};

// SLAM processing node
class SLAMProcessor {
public:
    SLAMProcessor(const std::string& node_name) : name_(node_name), scan_count_(0), odom_count_(0) {}
    
    void start() {
        laser_sub_id_ = GlobalPubSub::subscribe<LaserScan>("laser_scan",
            [this](const MsgPtr<LaserScan>& msg) {
                this->processLaserScan(msg);
            });
        
        odom_sub_id_ = GlobalPubSub::subscribe<Odometry>("odometry",
            [this](const MsgPtr<Odometry>& msg) {
                this->processOdometry(msg);
            });
        
        std::cout << "[" << name_ << "] SLAM processor started" << std::endl;
    }
    
    void stop() {
        GlobalPubSub::unsubscribe("laser_scan", laser_sub_id_);
        GlobalPubSub::unsubscribe("odometry", odom_sub_id_);
        
        std::cout << "[" << name_ << "] SLAM processor stopped" << std::endl;
        std::cout << "[" << name_ << "] Processed " << scan_count_ << " scans, " << odom_count_ << " odometry messages" << std::endl;
    }
    
private:
    void processLaserScan(const MsgPtr<LaserScan>& msg) {
        const auto& scan = msg->getContent();
        scan_count_++;
        
        if (scan_count_ % 10 == 0) {
            std::cout << "[" << name_ << "] Processing scan " << scan.scan_id 
                      << " with " << scan.ranges.size() << " points, t=" << scan.timestamp << std::endl;
            
            // Publish map update
            MapUpdate update(scan_count_, scan.timestamp, "sector_" + std::to_string(scan_count_ % 10), scan_count_ % 3 == 0);
            GlobalPubSub::publish("map_updates", update, 6);
        }
    }
    
    void processOdometry(const MsgPtr<Odometry>& msg) {
        const auto& odom = msg->getContent();
        odom_count_++;
        
        if (odom_count_ % 5 == 0) {
            std::cout << "[" << name_ << "] Processing odometry: position(" << odom.x << ", " << odom.y 
                      << "), angle=" << odom.theta << std::endl;
        }
    }
    
    std::string name_;
    uint64_t laser_sub_id_;
    uint64_t odom_sub_id_;
    std::atomic<int> scan_count_;
    std::atomic<int> odom_count_;
};

// Map management node
class MapManager {
public:
    MapManager(const std::string& node_name) : name_(node_name), updates_count_(0) {}
    
    void start() {
        map_sub_id_ = GlobalPubSub::subscribe<MapUpdate>("map_updates",
            [this](const MsgPtr<MapUpdate>& msg) {
                this->handleMapUpdate(msg);
            });
        
        std::cout << "[" << name_ << "] Map manager started" << std::endl;
    }
    
    void stop() {
        GlobalPubSub::unsubscribe("map_updates", map_sub_id_);
        std::cout << "[" << name_ << "] Map manager stopped" << std::endl;
        std::cout << "[" << name_ << "] Processed " << updates_count_ << " map updates" << std::endl;
    }
    
private:
    void handleMapUpdate(const MsgPtr<MapUpdate>& msg) {
        const auto& update = msg->getContent();
        updates_count_++;
        
        std::cout << "[" << name_ << "] Map update " << update.update_id 
                  << " in region " << update.region 
                  << (update.is_obstacle ? " (obstacle detected)" : " (clear)") << std::endl;
    }
    
    std::string name_;
    uint64_t map_sub_id_;
    std::atomic<int> updates_count_;
};

class GlobalPubSubDemo {
public:
    void runDemo() {
        std::cout << "\n=== GlobalPubSub SLAM System Demo ===" << std::endl;
        std::cout << "Demonstrating GlobalPubSub application in SLAM systems" << std::endl;
        
        // Start GlobalPubSub system
        std::cout << "Starting GlobalPubSub system..." << std::endl;
        if (!GlobalPubSub::start()) {
            std::cerr << "Failed to start GlobalPubSub system!" << std::endl;
            return;
        }
        std::cout << "✅ GlobalPubSub system started successfully!" << std::endl;
        
        // Create SLAM system nodes
        SLAMSensorNode lidar_node("LiDAR Node");
        SLAMProcessor slam_processor("SLAM Core");
        SLAMProcessor backup_processor("SLAM Backup");
        MapManager map_manager("Map Manager");
        
        // Start all nodes
        std::cout << "\nStarting SLAM system nodes..." << std::endl;
        slam_processor.start();
        backup_processor.start();
        map_manager.start();
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        lidar_node.start();
        
        std::cout << "\nSLAM system running..." << std::endl;
        std::cout << "Publishing topics: laser_scan, odometry" << std::endl;
        std::cout << "Subscribing topics: map_updates" << std::endl;
        std::cout << "Running for 5 seconds..." << std::endl;
        
        // Let the system run for a while
        std::this_thread::sleep_for(std::chrono::seconds(5));
        
        // Stop all nodes
        std::cout << "\nStopping SLAM system..." << std::endl;
        lidar_node.stop();
        slam_processor.stop();
        backup_processor.stop();
        map_manager.stop();
        
        // Stop GlobalPubSub system
        std::cout << "\nStopping GlobalPubSub system..." << std::endl;
        GlobalPubSub::stop();
        std::cout << "✅ GlobalPubSub system stopped" << std::endl;
    }
};

void showUsageGuide() {
    std::cout << "\n" << std::string(70, '=') << std::endl;
    std::cout << "ThreadSafeMsgQueue PubSub Usage Guide" << std::endl;
    std::cout << std::string(70, '=') << std::endl;
    
    std::cout << "\n🔧 PubSubSystem Usage:" << std::endl;
    std::cout << "1. Create and configure:" << std::endl;
    std::cout << "   PubSubSystem::Config config;" << std::endl;
    std::cout << "   PubSubSystem pubsub(config);" << std::endl;
    std::cout << "   pubsub.start();" << std::endl;
    
    std::cout << "\n2. Publish messages:" << std::endl;
    std::cout << "   pubsub.publish<DataType>(\"topic\", data, priority);" << std::endl;
    
    std::cout << "\n3. Subscribe to messages:" << std::endl;
    std::cout << "   auto sub_id = pubsub.subscribe<DataType>(\"topic\"," << std::endl;
    std::cout << "       [](const MsgPtr<DataType>& msg) {" << std::endl;
    std::cout << "           // Process message" << std::endl;
    std::cout << "       });" << std::endl;
    
    std::cout << "\n🌐 GlobalPubSub Usage:" << std::endl;
    std::cout << "1. Start system:" << std::endl;
    std::cout << "   GlobalPubSub::start();" << std::endl;
    
    std::cout << "\n2. Publish messages:" << std::endl;
    std::cout << "   GlobalPubSub::publish<DataType>(\"topic\", data, priority);" << std::endl;
    
    std::cout << "\n3. Subscribe to messages:" << std::endl;
    std::cout << "   auto sub_id = GlobalPubSub::subscribe<DataType>(\"topic\"," << std::endl;
    std::cout << "       [](const MsgPtr<DataType>& msg) {" << std::endl;
    std::cout << "           // Process message" << std::endl;
    std::cout << "       });" << std::endl;
    
    std::cout << "\n✨ Key Features:" << std::endl;
    std::cout << "   • Type Safety - Compile-time message type checking" << std::endl;
    std::cout << "   • High Performance - Efficient implementation based on ThreadSafeMsgQueue" << std::endl;
    std::cout << "   • Thread Safety - Multi-producer multi-consumer support" << std::endl;
    std::cout << "   • Topic Isolation - Independent processing for different topics" << std::endl;
    std::cout << "   • Batch Operations - Efficient batch publishing support" << std::endl;
    std::cout << "   • Statistics Monitoring - Real-time performance and status monitoring" << std::endl;
    std::cout << "   • Priority Support - Important messages processed first" << std::endl;
    std::cout << "   • Zero Configuration - GlobalPubSub ready out of the box" << std::endl;
    std::cout << std::string(70, '=') << std::endl;
}

int main() {
    std::cout << "=== ThreadSafeMsgQueue PubSub Comprehensive Demo ===" << std::endl;
    std::cout << "This demo includes both PubSubSystem and GlobalPubSub usage modes" << std::endl;
    
    // Demo 1: PubSubSystem basic functionality
    PubSubSystemDemo basic_demo;
    basic_demo.runDemo();
    
    std::cout << "\n" << std::string(50, '-') << std::endl;
    
    // Demo 2: GlobalPubSub SLAM application
    GlobalPubSubDemo slam_demo;
    slam_demo.runDemo();
    
    // Show usage guide
    showUsageGuide();
    
    std::cout << "\n🎉 Comprehensive demo completed!" << std::endl;
    std::cout << "You can choose PubSubSystem or GlobalPubSub to build your application as needed." << std::endl;
    
    return 0;
}
