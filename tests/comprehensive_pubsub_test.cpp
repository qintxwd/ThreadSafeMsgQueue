#include "ThreadSafeMsgQueue/ThreadSafeMsgQueue.h"
#include "ThreadSafeMsgQueue/PubSub.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <vector>
#include <atomic>
#include <unordered_map>
#include <memory>
#include <mutex>
#include <algorithm>

using namespace qyh::ThreadSafeMsgQueue;

// Test data structures
struct SensorData {
    int sensor_id;
    double timestamp;
    std::vector<float> values;
    
    SensorData(int id, double ts, const std::vector<float>& vals)
        : sensor_id(id), timestamp(ts), values(vals) {}
};

struct ControlCommand {
    std::string command;
    double parameter;
    
    ControlCommand(const std::string& cmd, double param)
        : command(cmd), parameter(param) {}
};

struct LaserScan {
    int scan_id;
    double timestamp;
    std::vector<float> ranges;
    
    LaserScan(int id, double ts, const std::vector<float>& r)
        : scan_id(id), timestamp(ts), ranges(r) {}
};

// Simple Pub/Sub system implementation using MsgQueue for testing
class SimplePubSub {
public:
    struct Subscription {
        uint64_t id;
        std::function<void(BaseMsgPtr)> callback;
        
        Subscription(uint64_t i, std::function<void(BaseMsgPtr)> cb) 
            : id(i), callback(std::move(cb)) {}
    };
    
    SimplePubSub() : next_sub_id_(1), running_(false) {}
    
    ~SimplePubSub() {
        stop();
    }
    
    void start() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!running_.load()) {
            running_.store(true);
            worker_thread_ = std::thread(&SimplePubSub::workerLoop, this);
        }
    }
    
    void stop() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            running_.store(false);
        }
        if (worker_thread_.joinable()) {
            worker_thread_.join();
        }
    }
    
    template<typename T>
    uint64_t subscribe(const std::string& topic, std::function<void(const MsgPtr<T>&)> callback) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (topic_queues_.find(topic) == topic_queues_.end()) {
            topic_queues_[topic] = std::make_shared<MsgQueue>(1000);
        }
        
        uint64_t sub_id = next_sub_id_.fetch_add(1);
        subscribers_[topic].emplace_back(sub_id, [callback](BaseMsgPtr base_msg) {
            if (auto typed_msg = std::dynamic_pointer_cast<Msg<T>>(base_msg)) {
                callback(typed_msg);
            }
        });
        
        return sub_id;
    }
    
    template<typename T>
    void publish(const std::string& topic, const T& content, int priority = 0) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (topic_queues_.find(topic) != topic_queues_.end()) {
            auto msg = make_msg<T>(priority, content);
            topic_queues_[topic]->enqueue(msg);
        }
    }
    
    bool unsubscribe(const std::string& topic, uint64_t sub_id) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = subscribers_.find(topic);
        if (it != subscribers_.end()) {
            auto& subs = it->second;
            subs.erase(std::remove_if(subs.begin(), subs.end(),
                [sub_id](const Subscription& sub) { return sub.id == sub_id; }),
                subs.end());
            return true;
        }
        return false;
    }
    
    size_t getSubscriberCount(const std::string& topic) const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = subscribers_.find(topic);
        return (it != subscribers_.end()) ? it->second.size() : 0;
    }
    
private:
    void workerLoop() {
        while (running_.load()) {
            std::lock_guard<std::mutex> lock(mutex_);
            for (auto& [topic, queue] : topic_queues_) {
                auto msg = queue->tryDequeue();
                if (msg) {
                    auto sub_it = subscribers_.find(topic);
                    if (sub_it != subscribers_.end()) {
                        for (const auto& subscription : sub_it->second) {
                            try {
                                subscription.callback(msg);
                            } catch (const std::exception& e) {
                                std::cerr << "Error in subscription callback: " << e.what() << std::endl;
                            }
                        }
                    }
                }
            }
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
    }
    
    mutable std::mutex mutex_;
    std::atomic<uint64_t> next_sub_id_;
    std::atomic<bool> running_;
    std::thread worker_thread_;
    
    std::unordered_map<std::string, std::vector<Subscription>> subscribers_;
    std::unordered_map<std::string, std::shared_ptr<MsgQueue>> topic_queues_;
};

class ComprehensivePubSubTest {
public:
    bool runAllTests() {
        std::cout << "=== Comprehensive PubSub System Test ===" << std::endl;
        std::cout << "Testing PubSubSystem, GlobalPubSub, and SimplePubSub implementations..." << std::endl;
        
        bool all_passed = true;
        
        // PubSubSystem tests
        std::cout << "\n--- PubSubSystem Tests ---" << std::endl;
        all_passed &= testBasicPubSub();
        all_passed &= testMultipleSubscribers();
        all_passed &= testMultipleTopics();
        all_passed &= testHighFrequencyData();
        all_passed &= testBatchPublishing();
        
        // GlobalPubSub tests
        std::cout << "\n--- GlobalPubSub Tests ---" << std::endl;
        all_passed &= testGlobalPubSubBasic();
        all_passed &= testGlobalMultiplePublishersToMultipleSubscribers();
        all_passed &= testGlobalTopicSeparation();
        all_passed &= testGlobalDynamicSubscriptions();
        all_passed &= testGlobalPerformance();
        
        // SimplePubSub tests
        std::cout << "\n--- SimplePubSub Tests ---" << std::endl;
        all_passed &= testSimpleMultiplePublishersToMultipleSubscribers();
        all_passed &= testSimpleTopicSeparation();
        all_passed &= testSimpleDynamicSubscriptions();
        all_passed &= testSimpleHighThroughput();
        all_passed &= testSimplePriority();
        
        std::cout << "\n=== Test Summary ===" << std::endl;
        if (all_passed) {
            std::cout << "✅ ALL TESTS PASSED!" << std::endl;
        } else {
            std::cout << "❌ Some tests failed!" << std::endl;
        }
        
        return all_passed;
    }

private:
    // PubSubSystem Tests
    bool testBasicPubSub() {
        std::cout << "\n1. Testing Basic PubSubSystem..." << std::endl;
        
        PubSubSystem pubsub;
        if (!pubsub.start()) {
            std::cout << "❌ Failed to start PubSub system" << std::endl;
            return false;
        }
        
        std::atomic<int> messages_received{0};
        std::atomic<int> received_sensor_id{-1};
        
        auto subscription_id = pubsub.subscribe<SensorData>("sensor_data", 
            [&](const MsgPtr<SensorData>& msg) {
                received_sensor_id.store(msg->getContent().sensor_id);
                messages_received.fetch_add(1);
            });
        
        SensorData data(42, 1.23, {1.1f, 2.2f, 3.3f});
        pubsub.publish("sensor_data", data, 5);
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        bool success = (messages_received.load() == 1) && (received_sensor_id.load() == 42);
        pubsub.stop();
        
        std::cout << "   Basic PubSub: " << (success ? "PASSED" : "FAILED") << std::endl;
        return success;
    }
    
    bool testMultipleSubscribers() {
        std::cout << "\n2. Testing Multiple Subscribers..." << std::endl;
        
        PubSubSystem pubsub;
        if (!pubsub.start()) return false;
        
        std::atomic<int> total_received{0};
        const int num_subscribers = 3;
        
        std::vector<uint64_t> subscription_ids;
        for (int i = 0; i < num_subscribers; ++i) {
            auto sub_id = pubsub.subscribe<SensorData>("multi_test",
                [&total_received](const MsgPtr<SensorData>& msg) {
                    total_received.fetch_add(1);
                });
            subscription_ids.push_back(sub_id);
        }
        
        SensorData data(1, 1.0, {1.0f});
        pubsub.publish("multi_test", data);
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        bool success = (total_received.load() == num_subscribers);
        pubsub.stop();
        
        std::cout << "   Multiple Subscribers: " << (success ? "PASSED" : "FAILED") 
                  << " (" << total_received.load() << "/" << num_subscribers << ")" << std::endl;
        return success;
    }
    
    bool testMultipleTopics() {
        std::cout << "\n3. Testing Multiple Topics..." << std::endl;
        
        PubSubSystem pubsub;
        if (!pubsub.start()) return false;
        
        std::atomic<int> sensor_received{0};
        std::atomic<int> control_received{0};
        
        pubsub.subscribe<SensorData>("sensors", 
            [&sensor_received](const MsgPtr<SensorData>& msg) {
                sensor_received.fetch_add(1);
            });
            
        pubsub.subscribe<ControlCommand>("control",
            [&control_received](const MsgPtr<ControlCommand>& msg) {
                control_received.fetch_add(1);
            });
        
        SensorData sensor_data(1, 1.0, {1.0f});
        ControlCommand control_cmd("move", 5.0);
        
        pubsub.publish("sensors", sensor_data);
        pubsub.publish("control", control_cmd);
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        bool success = (sensor_received.load() == 1) && (control_received.load() == 1);
        pubsub.stop();
        
        std::cout << "   Multiple Topics: " << (success ? "PASSED" : "FAILED") << std::endl;
        return success;
    }
    
    bool testHighFrequencyData() {
        std::cout << "\n4. Testing High Frequency Data..." << std::endl;
        
        PubSubSystem pubsub;
        if (!pubsub.start()) return false;
        
        std::atomic<int> messages_received{0};
        const int message_count = 1000;
        
        pubsub.subscribe<SensorData>("high_freq",
            [&messages_received](const MsgPtr<SensorData>& msg) {
                messages_received.fetch_add(1);
            });
        
        auto start = std::chrono::high_resolution_clock::now();
        
        for (int i = 0; i < message_count; ++i) {
            SensorData data(i, i * 0.001, {static_cast<float>(i)});
            pubsub.publish("high_freq", data);
        }
        
        while (messages_received.load() < message_count) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        double throughput = message_count * 1000000.0 / duration.count();
        
        bool success = (messages_received.load() == message_count);
        pubsub.stop();
        
        std::cout << "   High Frequency: " << (success ? "PASSED" : "FAILED") 
                  << " (" << static_cast<int>(throughput) << " msg/sec)" << std::endl;
        return success;
    }
    
    bool testBatchPublishing() {
        std::cout << "\n5. Testing Batch Publishing..." << std::endl;
        
        PubSubSystem pubsub;
        if (!pubsub.start()) return false;
        
        std::atomic<int> messages_received{0};
        const int batch_size = 100;
        
        pubsub.subscribe<SensorData>("batch_test",
            [&messages_received](const MsgPtr<SensorData>& msg) {
                messages_received.fetch_add(1);
            });
        
        std::vector<SensorData> batch_data;
        for (int i = 0; i < batch_size; ++i) {
            batch_data.emplace_back(i, i * 0.01, std::vector<float>{static_cast<float>(i)});
        }
        
        size_t published = pubsub.publishBatch("batch_test", batch_data);
        
        while (messages_received.load() < batch_size) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        
        bool success = (published == batch_size) && (messages_received.load() == batch_size);
        pubsub.stop();
        
        std::cout << "   Batch Publishing: " << (success ? "PASSED" : "FAILED") 
                  << " (" << published << "/" << batch_size << ")" << std::endl;
        return success;
    }
    
    // GlobalPubSub Tests
    bool testGlobalPubSubBasic() {
        std::cout << "\n6. Testing GlobalPubSub Basic..." << std::endl;
        
        if (!GlobalPubSub::start()) {
            std::cout << "❌ Failed to start GlobalPubSub system" << std::endl;
            return false;
        }
        
        std::atomic<int> messages_received{0};
        std::atomic<int> received_scan_id{-1};
        
        auto subscription_id = GlobalPubSub::subscribe<LaserScan>("laser_scan",
            [&](const MsgPtr<LaserScan>& msg) {
                received_scan_id.store(msg->getContent().scan_id);
                messages_received.fetch_add(1);
            });
        
        LaserScan scan(123, 2.34, {1.0f, 2.0f, 3.0f});
        GlobalPubSub::publish("laser_scan", scan, 3);
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        bool success = (messages_received.load() == 1) && (received_scan_id.load() == 123);
        GlobalPubSub::stop();
        
        std::cout << "   GlobalPubSub Basic: " << (success ? "PASSED" : "FAILED") << std::endl;
        return success;
    }
    
    bool testGlobalMultiplePublishersToMultipleSubscribers() {
        std::cout << "\n7. Testing Global Multiple Publishers to Multiple Subscribers..." << std::endl;
        
        if (!GlobalPubSub::start()) return false;
        
        std::atomic<int> total_received{0};
        const int num_publishers = 2;
        const int num_subscribers = 3;
        const int messages_per_publisher = 5;
        
        std::vector<uint64_t> sub_ids;
        for (int i = 0; i < num_subscribers; ++i) {
            auto sub_id = GlobalPubSub::subscribe<SensorData>("global_multi_test",
                [&total_received](const MsgPtr<SensorData>& msg) {
                    total_received.fetch_add(1);
                });
            sub_ids.push_back(sub_id);
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        
        std::vector<std::thread> publishers;
        for (int p = 0; p < num_publishers; ++p) {
            publishers.emplace_back([p, messages_per_publisher]() {
                for (int i = 0; i < messages_per_publisher; ++i) {
                    SensorData data(p * 100 + i, i * 0.1, {static_cast<float>(i)});
                    GlobalPubSub::publish("global_multi_test", data);
                }
            });
        }
        
        for (auto& pub : publishers) {
            pub.join();
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        
        int expected = num_publishers * messages_per_publisher * num_subscribers;
        bool success = (total_received.load() == expected);
        GlobalPubSub::stop();
        
        std::cout << "   Global Multi Pub/Sub: " << (success ? "PASSED" : "FAILED")
                  << " (" << total_received.load() << "/" << expected << ")" << std::endl;
        return success;
    }
    
    bool testGlobalTopicSeparation() {
        std::cout << "\n8. Testing Global Topic Separation..." << std::endl;
        
        if (!GlobalPubSub::start()) return false;
        
        std::atomic<int> topic1_received{0};
        std::atomic<int> topic2_received{0};
        
        GlobalPubSub::subscribe<SensorData>("topic1",
            [&topic1_received](const MsgPtr<SensorData>& msg) {
                topic1_received.fetch_add(1);
            });
            
        GlobalPubSub::subscribe<SensorData>("topic2",
            [&topic2_received](const MsgPtr<SensorData>& msg) {
                topic2_received.fetch_add(1);
            });
        
        SensorData data1(1, 1.0, {1.0f});
        SensorData data2(2, 2.0, {2.0f});
        
        GlobalPubSub::publish("topic1", data1);
        GlobalPubSub::publish("topic2", data2);
        GlobalPubSub::publish("topic1", data1);
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        bool success = (topic1_received.load() == 2) && (topic2_received.load() == 1);
        GlobalPubSub::stop();
        
        std::cout << "   Global Topic Separation: " << (success ? "PASSED" : "FAILED") << std::endl;
        return success;
    }
    
    bool testGlobalDynamicSubscriptions() {
        std::cout << "\n9. Testing Global Dynamic Subscriptions..." << std::endl;
        
        if (!GlobalPubSub::start()) return false;
        
        std::atomic<int> total_received{0};
        
        auto sub1 = GlobalPubSub::subscribe<SensorData>("dynamic_global",
            [&total_received](const MsgPtr<SensorData>& msg) {
                total_received.fetch_add(1);
            });
            
        auto sub2 = GlobalPubSub::subscribe<SensorData>("dynamic_global",
            [&total_received](const MsgPtr<SensorData>& msg) {
                total_received.fetch_add(1);
            });
        
        SensorData data1(1, 1.0, {1.0f});
        GlobalPubSub::publish("dynamic_global", data1);
        
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        
        auto sub3 = GlobalPubSub::subscribe<SensorData>("dynamic_global",
            [&total_received](const MsgPtr<SensorData>& msg) {
                total_received.fetch_add(1);
            });
        
        SensorData data2(2, 2.0, {2.0f});
        GlobalPubSub::publish("dynamic_global", data2);
        
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        
        GlobalPubSub::unsubscribe(sub2);
        
        SensorData data3(3, 3.0, {3.0f});
        GlobalPubSub::publish("dynamic_global", data3);
        
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        
        // Expected: 2 (first) + 3 (second) + 2 (third) = 7
        int expected = 7;
        bool success = (total_received.load() == expected);
        GlobalPubSub::stop();
        
        std::cout << "   Global Dynamic Subscriptions: " << (success ? "PASSED" : "FAILED")
                  << " (" << total_received.load() << "/" << expected << ")" << std::endl;
        return success;
    }
    
    bool testGlobalPerformance() {
        std::cout << "\n10. Testing Global Performance..." << std::endl;
        
        if (!GlobalPubSub::start()) return false;
        
        std::atomic<int> total_received{0};
        const int num_publishers = 2;
        const int num_subscribers = 3;
        const int messages_per_publisher = 1000;
        
        std::vector<uint64_t> sub_ids;
        for (int i = 0; i < num_subscribers; ++i) {
            auto sub_id = GlobalPubSub::subscribe<SensorData>("perf_test",
                [&total_received](const MsgPtr<SensorData>& msg) {
                    total_received.fetch_add(1);
                });
            sub_ids.push_back(sub_id);
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        
        auto start_time = std::chrono::high_resolution_clock::now();
        
        std::vector<std::thread> publishers;
        for (int p = 0; p < num_publishers; ++p) {
            publishers.emplace_back([p, messages_per_publisher]() {
                for (int i = 0; i < messages_per_publisher; ++i) {
                    SensorData data(p * 1000 + i, i * 0.001, {static_cast<float>(i % 10)});
                    GlobalPubSub::publish("perf_test", data);
                }
            });
        }
        
        for (auto& pub : publishers) {
            pub.join();
        }
        
        int expected_total = num_publishers * messages_per_publisher * num_subscribers;
        while (total_received.load() < expected_total) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        double throughput = total_received.load() * 1000.0 / duration.count();
        
        bool success = (total_received.load() == expected_total);
        GlobalPubSub::stop();
        
        std::cout << "   Global Performance: " << (success ? "PASSED" : "FAILED")
                  << " (" << static_cast<int>(throughput) << " msg/sec)" << std::endl;
        return success;
    }
    
    // SimplePubSub Tests
    bool testSimpleMultiplePublishersToMultipleSubscribers() {
        std::cout << "\n11. Testing Simple Multiple Publishers to Multiple Subscribers..." << std::endl;
        
        SimplePubSub pubsub;
        pubsub.start();
        
        std::atomic<int> total_received{0};
        std::vector<std::atomic<int>> subscriber_counts(3);
        for (auto& count : subscriber_counts) {
            count.store(0);
        }
        
        std::vector<uint64_t> sub_ids;
        for (int i = 0; i < 3; ++i) {
            auto sub_id = pubsub.subscribe<SensorData>("simple_sensor_data", 
                [&total_received, &subscriber_counts, i](const MsgPtr<SensorData>& msg) {
                    total_received.fetch_add(1);
                    subscriber_counts[i].fetch_add(1);
                });
            sub_ids.push_back(sub_id);
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        
        std::thread publisher1([&pubsub]() {
            for (int i = 0; i < 3; ++i) {
                SensorData data(100 + i, i * 0.1, {1.0f + i, 2.0f + i});
                pubsub.publish("simple_sensor_data", data, 5);
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            }
        });
        
        std::thread publisher2([&pubsub]() {
            for (int i = 0; i < 3; ++i) {
                SensorData data(200 + i, i * 0.1, {3.0f + i, 4.0f + i});
                pubsub.publish("simple_sensor_data", data, 3);
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            }
        });
        
        publisher1.join();
        publisher2.join();
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        pubsub.stop();
        
        int expected_total = 6 * 3; // 6 messages × 3 subscribers
        bool success = (total_received.load() == expected_total);
        
        std::cout << "   Simple Multi Pub/Sub: " << (success ? "PASSED" : "FAILED")
                  << " (" << total_received.load() << "/" << expected_total << ")" << std::endl;
        return success;
    }
    
    bool testSimpleTopicSeparation() {
        std::cout << "\n12. Testing Simple Topic Separation..." << std::endl;
        
        SimplePubSub pubsub;
        pubsub.start();
        
        std::atomic<int> sensor_received{0};
        std::atomic<int> control_received{0};
        
        auto sensor_sub = pubsub.subscribe<SensorData>("simple_sensors", 
            [&sensor_received](const MsgPtr<SensorData>& msg) {
                sensor_received.fetch_add(1);
            });
            
        auto control_sub = pubsub.subscribe<ControlCommand>("simple_control",
            [&control_received](const MsgPtr<ControlCommand>& msg) {
                control_received.fetch_add(1);
            });
        
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        
        SensorData sensor_data(42, 1.0, {1.0f, 2.0f});
        ControlCommand control_cmd("move_forward", 5.0);
        
        pubsub.publish("simple_sensors", sensor_data);
        pubsub.publish("simple_control", control_cmd);
        pubsub.publish("simple_sensors", sensor_data);
        
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        pubsub.stop();
        
        bool success = (sensor_received.load() == 2) && (control_received.load() == 1);
        
        std::cout << "   Simple Topic Separation: " << (success ? "PASSED" : "FAILED") << std::endl;
        return success;
    }
    
    bool testSimpleDynamicSubscriptions() {
        std::cout << "\n13. Testing Simple Dynamic Subscriptions..." << std::endl;
        
        SimplePubSub pubsub;
        pubsub.start();
        
        std::atomic<int> total_received{0};
        
        auto sub1 = pubsub.subscribe<SensorData>("simple_dynamic_test",
            [&total_received](const MsgPtr<SensorData>& msg) {
                total_received.fetch_add(1);
            });
            
        auto sub2 = pubsub.subscribe<SensorData>("simple_dynamic_test",
            [&total_received](const MsgPtr<SensorData>& msg) {
                total_received.fetch_add(1);
            });
        
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        
        SensorData data1(1, 1.0, {1.0f});
        pubsub.publish("simple_dynamic_test", data1);
        
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        
        auto sub3 = pubsub.subscribe<SensorData>("simple_dynamic_test",
            [&total_received](const MsgPtr<SensorData>& msg) {
                total_received.fetch_add(1);
            });
        
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        
        SensorData data2(2, 2.0, {2.0f});
        pubsub.publish("simple_dynamic_test", data2);
        
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        
        pubsub.unsubscribe("simple_dynamic_test", sub2);
        
        SensorData data3(3, 3.0, {3.0f});
        pubsub.publish("simple_dynamic_test", data3);
        
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        pubsub.stop();
        
        // Expected: 2 (first msg) + 3 (second msg) + 2 (third msg) = 7
        int expected = 7;
        bool success = (total_received.load() == expected);
        
        std::cout << "   Simple Dynamic Subscriptions: " << (success ? "PASSED" : "FAILED")
                  << " (" << total_received.load() << "/" << expected << ")" << std::endl;
        return success;
    }
    
    bool testSimpleHighThroughput() {
        std::cout << "\n14. Testing Simple High Throughput..." << std::endl;
        
        SimplePubSub pubsub;
        pubsub.start();
        
        std::atomic<int> total_received{0};
        const int num_publishers = 3;
        const int num_subscribers = 4;
        const int messages_per_publisher = 100;
        
        std::vector<uint64_t> sub_ids;
        for (int i = 0; i < num_subscribers; ++i) {
            auto sub_id = pubsub.subscribe<SensorData>("simple_high_throughput",
                [&total_received](const MsgPtr<SensorData>& msg) {
                    total_received.fetch_add(1);
                });
            sub_ids.push_back(sub_id);
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        
        auto start_time = std::chrono::high_resolution_clock::now();
        
        std::vector<std::thread> publishers;
        for (int p = 0; p < num_publishers; ++p) {
            publishers.emplace_back([&pubsub, p, messages_per_publisher]() {
                for (int i = 0; i < messages_per_publisher; ++i) {
                    SensorData data(p * 1000 + i, i * 0.001, {static_cast<float>(i), static_cast<float>(p)});
                    pubsub.publish("simple_high_throughput", data, i % 10);
                    
                    if (i % 10 == 0) {
                        std::this_thread::yield();
                    }
                }
            });
        }
        
        for (auto& pub : publishers) {
            pub.join();
        }
        
        int expected_total = num_publishers * messages_per_publisher * num_subscribers;
        while (total_received.load() < expected_total) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        
        pubsub.stop();
        
        double throughput = total_received.load() * 1000.0 / duration.count();
        bool success = (total_received.load() == expected_total);
        
        std::cout << "   Simple High Throughput: " << (success ? "PASSED" : "FAILED")
                  << " (" << static_cast<int>(throughput) << " msg/sec)" << std::endl;
        return success;
    }
    
    bool testSimplePriority() {
        std::cout << "\n15. Testing Simple Priority..." << std::endl;
        
        SimplePubSub pubsub;
        pubsub.start();
        
        std::vector<int> received_priorities;
        std::mutex priority_mutex;
        
        auto sub_id = pubsub.subscribe<SensorData>("simple_priority_test",
            [&received_priorities, &priority_mutex](const MsgPtr<SensorData>& msg) {
                std::lock_guard<std::mutex> lock(priority_mutex);
                received_priorities.push_back(msg->getPriority());
            });
        
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        
        SensorData data1(1, 1.0, {1.0f});
        SensorData data2(2, 2.0, {2.0f});
        SensorData data3(3, 3.0, {3.0f});
        SensorData data4(4, 4.0, {4.0f});
        
        pubsub.publish("simple_priority_test", data1, 2);
        pubsub.publish("simple_priority_test", data2, 8);
        pubsub.publish("simple_priority_test", data3, 5);
        pubsub.publish("simple_priority_test", data4, 1);
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        pubsub.stop();
        
        bool priority_correct = (received_priorities.size() == 4 &&
                                received_priorities[0] == 8 &&
                                received_priorities[1] == 5 &&
                                received_priorities[2] == 2 &&
                                received_priorities[3] == 1);
        
        std::cout << "   Simple Priority: " << (priority_correct ? "PASSED" : "FAILED") << std::endl;
        return priority_correct;
    }
};

int main() {
    ComprehensivePubSubTest test;
    return test.runAllTests() ? 0 : 1;
}