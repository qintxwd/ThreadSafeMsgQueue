#include "ThreadSafeMsgQueue/ThreadSafeMsgQueue.h"
#include <iostream>
#include <thread>
#include <chrono>

using namespace qyh::ThreadSafeMsgQueue;

struct SensorData {
    int sensor_id;
    double timestamp;
    std::vector<double> values;
    
    SensorData(int id, double ts, std::vector<double> vals) 
        : sensor_id(id), timestamp(ts), values(std::move(vals)) {}
};

int main() {
    std::cout << "=== ThreadSafeMsgQueue Simple Example ===" << std::endl;
    
    // Create a message queue
    auto queue = std::make_shared<MsgQueue>(100);
    
    // Create some test messages
    std::cout << "Creating messages..." << std::endl;
    
    // Create messages with different priorities
    auto msg1 = make_msg<SensorData>(1, SensorData{1, 1.0, {1.1, 2.2, 3.3}});
    auto msg2 = make_msg<SensorData>(5, SensorData{2, 2.0, {4.4, 5.5, 6.6}});
    auto msg3 = make_msg<SensorData>(3, SensorData{3, 3.0, {7.7, 8.8, 9.9}});
    
    std::cout << "Message IDs: " 
              << msg1->getMessageId() << ", "
              << msg2->getMessageId() << ", "
              << msg3->getMessageId() << std::endl;
              
    std::cout << "Message priorities: "
              << msg1->getPriority() << ", "
              << msg2->getPriority() << ", "
              << msg3->getPriority() << std::endl;
    
    // Enqueue messages
    std::cout << "Enqueueing messages..." << std::endl;
    queue->enqueue(msg1);
    queue->enqueue(msg2);
    queue->enqueue(msg3);
    
    // Dequeue messages (should come out in priority order: 5, 3, 1)
    std::cout << "Dequeueing messages in priority order..." << std::endl;
    
    for (int i = 0; i < 3; ++i) {
        auto msg = queue->dequeue();
        if (auto sensor_msg = std::dynamic_pointer_cast<Msg<SensorData>>(msg)) {
            const auto& data = sensor_msg->getContent();
            std::cout << "Received: Sensor " << data.sensor_id 
                      << ", Priority " << sensor_msg->getPriority()
                      << ", Timestamp " << data.timestamp 
                      << ", Values: ";
            for (double val : data.values) {
                std::cout << val << " ";
            }
            std::cout << std::endl;
        }
    }
    
    // Test statistics
    auto stats = queue->getStatistics();
    std::cout << "\n=== Queue Statistics ===" << std::endl;
    std::cout << "Total enqueued: " << stats.total_enqueued.load() << std::endl;
    std::cout << "Total dequeued: " << stats.total_dequeued.load() << std::endl;
    std::cout << "Current size: " << stats.current_size.load() << std::endl;
    std::cout << "Peak size: " << stats.peak_size.load() << std::endl;
    
    std::cout << "\n�?Simple example completed successfully!" << std::endl;
    return 0;
}

