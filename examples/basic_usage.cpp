#include "ThreadSafeMsgQueue/ThreadSafeMsgQueue.h"
#include <iostream>
#include <vector>
#include <thread>
#include <chrono>

using namespace qyh::ThreadSafeMsgQueue;

// Basic usage demonstration
struct Message {
    int id;
    std::string content;
    
    Message(int i, const std::string& c) : id(i), content(c) {}
};

int main() {
    std::cout << "=== ThreadSafeMsgQueue Basic Usage ===" << std::endl;
    
    // 1. Create a message queue
    std::cout << "\n1. Creating message queue..." << std::endl;
    auto queue = std::make_shared<MsgQueue>(100);
    
    // 2. Basic enqueue/dequeue
    std::cout << "\n2. Basic enqueue/dequeue..." << std::endl;
    auto msg = make_msg<Message>(1, Message(42, "Hello World"));
    
    if (queue->enqueue(msg)) {
        std::cout << "�?Message enqueued successfully" << std::endl;
    }
    
    auto received = queue->dequeue();
    if (received) {
        auto typed_msg = std::dynamic_pointer_cast<Msg<Message>>(received);
        if (typed_msg) {
            std::cout << "�?Received message: ID=" << typed_msg->getContent().id 
                      << ", Content=\"" << typed_msg->getContent().content << "\"" << std::endl;
        }
    }
    
    // 3. Priority handling
    std::cout << "\n3. Priority handling..." << std::endl;
    auto low_msg = make_msg<Message>(1, Message(1, "Low priority"));
    auto high_msg = make_msg<Message>(5, Message(2, "High priority"));
    auto medium_msg = make_msg<Message>(3, Message(3, "Medium priority"));
    
    queue->enqueue(low_msg);
    queue->enqueue(high_msg);
    queue->enqueue(medium_msg);
    
    std::cout << "Dequeue order (by priority):" << std::endl;
    while (!queue->empty()) {
        auto msg = queue->dequeue();
        if (auto typed_msg = std::dynamic_pointer_cast<Msg<Message>>(msg)) {
            std::cout << "  Priority " << typed_msg->getPriority() 
                      << ": " << typed_msg->getContent().content << std::endl;
        }
    }
    
    // 4. Batch operations
    std::cout << "\n4. Batch operations..." << std::endl;
    std::vector<BaseMsgPtr> batch;
    for (int i = 0; i < 10; ++i) {
        batch.push_back(make_msg<Message>(1, Message(i, "Batch message " + std::to_string(i))));
    }
    
    size_t enqueued = queue->enqueue_batch(batch);
    std::cout << "Batch enqueued: " << enqueued << " messages" << std::endl;
    
    std::vector<BaseMsgPtr> dequeued_batch;
    size_t dequeued = queue->dequeue_batch(dequeued_batch, 5);
    std::cout << "Batch dequeued: " << dequeued << " messages" << std::endl;
    
    // 5. Threading example
    std::cout << "\n5. Multi-threading example..." << std::endl;
    
    std::atomic<int> producer_count{0};
    std::atomic<int> consumer_count{0};
    
    // Producer thread
    std::thread producer([&]() {
        for (int i = 0; i < 20; ++i) {
            auto msg = make_msg<Message>(1, Message(i, "Producer message " + std::to_string(i)));
            if (queue->enqueue(msg)) {
                producer_count.fetch_add(1);
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    });
    
    // Consumer thread
    std::thread consumer([&]() {
        while (consumer_count.load() < 20) {
            auto msg = queue->dequeue();
            if (msg) {
                consumer_count.fetch_add(1);
            } else {
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            }
        }
    });
    
    producer.join();
    consumer.join();
    
    std::cout << "Producer sent: " << producer_count.load() << " messages" << std::endl;
    std::cout << "Consumer received: " << consumer_count.load() << " messages" << std::endl;
    
    // 6. Statistics
    std::cout << "\n6. Queue statistics..." << std::endl;
    auto stats = queue->getStatistics();
    std::cout << "Total enqueued: " << stats.total_enqueued.load() << std::endl;
    std::cout << "Total dequeued: " << stats.total_dequeued.load() << std::endl;
    std::cout << "Peak size: " << stats.peak_size.load() << std::endl;
    
    std::cout << "\n�?Basic usage example completed!" << std::endl;
    
    return 0;
}

