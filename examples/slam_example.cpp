#include "ThreadSafeMsgQueue/ThreadSafeMsgQueue.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <vector>
#include <random>
#include <atomic>

using namespace qyh::ThreadSafeMsgQueue;

// SLAM-related data structures
struct LaserScanData {
    double timestamp;
    int scan_id;
    std::vector<float> ranges;
    double angle_min, angle_max, angle_increment;
    
    LaserScanData(double ts, int id, size_t num_points = 360) 
        : timestamp(ts), scan_id(id) {
        angle_min = -M_PI;
        angle_max = M_PI;
        angle_increment = 2.0 * M_PI / num_points;
        
        ranges.resize(num_points);
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<float> dist(0.1f, 10.0f);
        
        for (auto& range : ranges) {
            range = dist(gen);
        }
    }
    
private:
    static constexpr double M_PI = 3.14159265358979323846;
};

struct OdometryData {
    double timestamp;
    double x, y, theta;
    double linear_vel, angular_vel;
    
    OdometryData(double ts, double _x, double _y, double _theta, double lin_v = 0, double ang_v = 0)
        : timestamp(ts), x(_x), y(_y), theta(_theta), linear_vel(lin_v), angular_vel(ang_v) {}
};

struct IMUData {
    double timestamp;
    double accel_x, accel_y, accel_z;
    double gyro_x, gyro_y, gyro_z;
    
    IMUData(double ts) : timestamp(ts) {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<double> accel_dist(-2.0, 2.0);
        std::uniform_real_distribution<double> gyro_dist(-1.0, 1.0);
        
        accel_x = accel_dist(gen);
        accel_y = accel_dist(gen);
        accel_z = 9.81 + accel_dist(gen); // gravity + noise
        
        gyro_x = gyro_dist(gen);
        gyro_y = gyro_dist(gen);
        gyro_z = gyro_dist(gen);
    }
};

struct MapUpdateCommand {
    double timestamp;
    std::string command_type;
    std::vector<uint8_t> map_data;
    
    MapUpdateCommand(double ts, const std::string& cmd) 
        : timestamp(ts), command_type(cmd) {
        // Simulate map data
        map_data.resize(100);
        std::fill(map_data.begin(), map_data.end(), 127); // unknown area
    }
};

class SLAMSystem {
public:
    SLAMSystem() {
        // Create specialized queues for different types of data
        laser_queue_ = std::make_shared<MsgQueue>(500);    // Laser data
        odom_queue_ = std::make_shared<MsgQueue>(1000);    // Odometry data
        imu_queue_ = std::make_shared<MsgQueue>(5000);     // High-frequency IMU data
        map_queue_ = std::make_shared<MsgQueue>(100);      // Map update commands
        
        running_ = true;
    }
    
    ~SLAMSystem() {
        stop();
    }
    
    void start() {
        std::cout << "🚀 Starting SLAM system..." << std::endl;
        
        // Start sensor data producers
        sensor_threads_.emplace_back(&SLAMSystem::laserScanProducer, this);
        sensor_threads_.emplace_back(&SLAMSystem::odometryProducer, this);
        sensor_threads_.emplace_back(&SLAMSystem::imuProducer, this);
        sensor_threads_.emplace_back(&SLAMSystem::mapCommandProducer, this);
        
        // Start data processors
        processor_threads_.emplace_back(&SLAMSystem::localizationProcessor, this);
        processor_threads_.emplace_back(&SLAMSystem::mappingProcessor, this);
        processor_threads_.emplace_back(&SLAMSystem::navigationProcessor, this);
        
        std::cout << "✅ SLAM system started with 3 sensor data sources and 3 processing modules" << std::endl;
    }
    
    void runDemo(int duration_seconds = 10) {
        std::cout << "🎬 Running SLAM demo for " << duration_seconds << " seconds..." << std::endl;
        
        auto start_time = std::chrono::steady_clock::now();
        
        while (std::chrono::steady_clock::now() - start_time < std::chrono::seconds(duration_seconds)) {
            // Display statistics every second
            std::this_thread::sleep_for(std::chrono::seconds(1));
            displayStatistics();
        }
        
        std::cout << "\n🏁 Demo completed" << std::endl;
    }
    
    void stop() {
        if (running_) {
            std::cout << "🛑 Stopping SLAM system..." << std::endl;
            running_ = false;
            
            // Wait for all threads to finish
            for (auto& thread : sensor_threads_) {
                if (thread.joinable()) thread.join();
            }
            for (auto& thread : processor_threads_) {
                if (thread.joinable()) thread.join();
            }
            
            // Display final statistics
            displayFinalStatistics();
            std::cout << "✅ SLAM system stopped" << std::endl;
        }
    }

private:
    // Sensor data producers
    void laserScanProducer() {
        int scan_count = 0;
        while (running_) {
            auto msg = make_msg<LaserScanData>(5, LaserScanData(getCurrentTime(), scan_count++));
            laser_queue_->enqueue(msg);
            laser_produced_.fetch_add(1);
            std::this_thread::sleep_for(std::chrono::milliseconds(100)); // 10Hz
        }
    }
    
    void odometryProducer() {
        double x = 0, y = 0, theta = 0;
        while (running_) {
            // Simulate robot movement
            x += 0.01;
            y += 0.005;
            theta += 0.02;
            
            auto msg = make_msg<OdometryData>(3, OdometryData(getCurrentTime(), x, y, theta, 0.1, 0.02));
            odom_queue_->enqueue(msg);
            odom_produced_.fetch_add(1);
            std::this_thread::sleep_for(std::chrono::milliseconds(50)); // 20Hz
        }
    }
    
    void imuProducer() {
        while (running_) {
            auto msg = make_msg<IMUData>(1, IMUData(getCurrentTime()));
            imu_queue_->enqueue(msg);
            imu_produced_.fetch_add(1);
            std::this_thread::sleep_for(std::chrono::milliseconds(10)); // 100Hz
        }
    }
    
    void mapCommandProducer() {
        int cmd_count = 0;
        while (running_) {
            auto msg = make_msg<MapUpdateCommand>(2, MapUpdateCommand(getCurrentTime(), "UPDATE_MAP_" + std::to_string(cmd_count++)));
            map_queue_->enqueue(msg);
            map_produced_.fetch_add(1);
            std::this_thread::sleep_for(std::chrono::milliseconds(500)); // 2Hz
        }
    }
    
    // Data processors
    void localizationProcessor() {
        while (running_) {
            // Process laser and odometry data for localization
            auto laser_msg = laser_queue_->dequeue();
            if (laser_msg) {
                // Simulate laser localization processing
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
                laser_processed_.fetch_add(1);
            }
            
            auto odom_msg = odom_queue_->dequeue();
            if (odom_msg) {
                // Simulate odometry fusion
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                odom_processed_.fetch_add(1);
            }
            
            if (!laser_msg && !odom_msg) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }
    }
    
    void mappingProcessor() {
        while (running_) {
            // Process map update commands
            auto map_msg = map_queue_->dequeue();
            if (map_msg) {
                // Simulate map building processing
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                map_processed_.fetch_add(1);
            } else {
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            }
        }
    }
    
    void navigationProcessor() {
        while (running_) {
            // Batch process IMU data
            std::vector<BaseMsgPtr> imu_batch;
            size_t batch_size = imu_queue_->dequeue_batch(imu_batch, 10);
            
            if (batch_size > 0) {
                // Simulate IMU data processing
                std::this_thread::sleep_for(std::chrono::microseconds(500 * batch_size));
                imu_processed_.fetch_add(batch_size);
            } else {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }
    }
    
    void displayStatistics() {
        auto laser_stats = laser_queue_->getStatistics();
        auto odom_stats = odom_queue_->getStatistics();
        auto imu_stats = imu_queue_->getStatistics();
        auto map_stats = map_queue_->getStatistics();
        
        std::cout << "\n📊 SLAM System Real-time Status" << std::endl;
        std::cout << "  Laser:    Produced " << laser_produced_.load() << ", Processed " << laser_processed_.load() 
                  << ", Queue " << laser_stats.current_size.load() << std::endl;
        std::cout << "  Odometry: Produced " << odom_produced_.load() << ", Processed " << odom_processed_.load() 
                  << ", Queue " << odom_stats.current_size.load() << std::endl;
        std::cout << "  IMU:      Produced " << imu_produced_.load() << ", Processed " << imu_processed_.load() 
                  << ", Queue " << imu_stats.current_size.load() << std::endl;
        std::cout << "  Map Update: Produced " << map_produced_.load() << ", Processed " << map_processed_.load() 
                  << ", Queue " << map_stats.current_size.load() << std::endl;
    }
    
    void displayFinalStatistics() {
        std::cout << "\n📈 SLAM System Final Statistics" << std::endl;
        
        auto laser_stats = laser_queue_->getStatistics();
        auto odom_stats = odom_queue_->getStatistics();
        auto imu_stats = imu_queue_->getStatistics();
        auto map_stats = map_queue_->getStatistics();
        
        std::cout << "Laser data processing rate: " << 
            (laser_processed_.load() * 100.0 / std::max(1, laser_produced_.load())) << "%" << std::endl;
        std::cout << "Odometry data processing rate: " << 
            (odom_processed_.load() * 100.0 / std::max(1, odom_produced_.load())) << "%" << std::endl;
        std::cout << "IMU data processing rate: " << 
            (imu_processed_.load() * 100.0 / std::max(1, imu_produced_.load())) << "%" << std::endl;
        std::cout << "Map update processing rate: " << 
            (map_processed_.load() * 100.0 / std::max(1, map_produced_.load())) << "%" << std::endl;
        
        std::cout << "\nQueue Peak Sizes" << std::endl;
        std::cout << "  Laser: " << laser_stats.peak_size.load() << std::endl;
        std::cout << "  Odometry: " << odom_stats.peak_size.load() << std::endl;
        std::cout << "  IMU: " << imu_stats.peak_size.load() << std::endl;
        std::cout << "  Map Update: " << map_stats.peak_size.load() << std::endl;
    }
    
    double getCurrentTime() {
        auto now = std::chrono::high_resolution_clock::now();
        auto duration = now.time_since_epoch();
        return std::chrono::duration<double>(duration).count();
    }
    
private:
    // Message queues
    MsgQueuePtr laser_queue_;
    MsgQueuePtr odom_queue_;
    MsgQueuePtr imu_queue_;
    MsgQueuePtr map_queue_;
    
    // Thread management
    std::vector<std::thread> sensor_threads_;
    std::vector<std::thread> processor_threads_;
    std::atomic<bool> running_{false};
    
    // Statistics counters
    std::atomic<int> laser_produced_{0}, laser_processed_{0};
    std::atomic<int> odom_produced_{0}, odom_processed_{0};
    std::atomic<int> imu_produced_{0}, imu_processed_{0};
    std::atomic<int> map_produced_{0}, map_processed_{0};
};

int main() {
    std::cout << "=== ThreadSafeMsgQueue SLAM System Demo ===" << std::endl;
    std::cout << "Demonstrates multi-sensor data processing pipeline in a complete SLAM system" << std::endl;
    std::cout << "Includes: Laser(10Hz) + Odometry(20Hz) + IMU(100Hz) + Map Updates(2Hz)" << std::endl;
    std::cout << "Features: Priority processing + Batch operations + Real-time monitoring" << std::endl;
    
    SLAMSystem slam_system;
    slam_system.start();
    slam_system.runDemo(8);  // Run 8-second demo
    slam_system.stop();
    
    std::cout << "\n🎉 SLAM demo completed" << std::endl;
    std::cout << "ThreadSafeMsgQueue perfectly supports real-time data processing requirements of complex SLAM systems!" << std::endl;
    
    return 0;
}

