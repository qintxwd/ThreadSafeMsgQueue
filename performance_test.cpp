#include "ThreadSafeMsgQueue.h"
#include <iostream>
#include <chrono>
#include <thread>
#include <vector>
#include <random>

struct PerformanceTestData {
    int id;
    double timestamp;
    std::vector<float> payload;
    
    PerformanceTestData(int i, double ts, size_t payload_size = 10) 
        : id(i), timestamp(ts) {
        payload.resize(payload_size);
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<float> dis(0.0f, 100.0f);
        for (auto& val : payload) {
            val = dis(gen);
        }
    }
};

class PerformanceBenchmark {
public:
    void runBenchmark() {
        std::cout << "=== ThreadSafeMsgQueue Performance Benchmark ===" << std::endl;
        
        // Test different scenarios
        testSingleThreadPerformance();
        testMultiThreadPerformance();
        testBatchOperations();
        testMemoryUsage();
        
        std::cout << "\n=== Benchmark Complete ===" << std::endl;
    }
    
private:
    void testSingleThreadPerformance() {
        std::cout << "\n--- Single Thread Performance ---" << std::endl;
        
        auto queue = std::make_shared<MsgQueue>(10000);
        const int message_count = 10000;
        
        // Test enqueue performance
        auto start = std::chrono::high_resolution_clock::now();
        
        for (int i = 0; i < message_count; ++i) {
            auto msg = make_msg<PerformanceTestData>(i % 10, PerformanceTestData{i, static_cast<double>(i)});
            queue->enqueue(msg);
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        auto enqueue_time = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        
        std::cout << "Enqueue " << message_count << " messages: " 
                  << enqueue_time.count() << " μs" << std::endl;
        std::cout << "Enqueue rate: " 
                  << (message_count * 1000000.0 / enqueue_time.count()) << " msgs/sec" << std::endl;
        
        // Test dequeue performance
        start = std::chrono::high_resolution_clock::now();
        
        int dequeued = 0;
        while (!queue->empty()) {
            auto msg = queue->dequeue();
            if (msg) ++dequeued;
        }
        
        end = std::chrono::high_resolution_clock::now();
        auto dequeue_time = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        
        std::cout << "Dequeue " << dequeued << " messages: " 
                  << dequeue_time.count() << " μs" << std::endl;
        std::cout << "Dequeue rate: " 
                  << (dequeued * 1000000.0 / dequeue_time.count()) << " msgs/sec" << std::endl;
    }
    
    void testMultiThreadPerformance() {
        std::cout << "\n--- Multi-Thread Performance ---" << std::endl;
        
        auto queue = std::make_shared<MsgQueue>(50000);
        const int producer_count = 4;
        const int consumer_count = 2;
        const int messages_per_producer = 5000;
        
        std::atomic<int> total_produced{0};
        std::atomic<int> total_consumed{0};
        std::atomic<bool> stop_consumers{false};
        
        auto start = std::chrono::high_resolution_clock::now();
        
        // Start producer threads
        std::vector<std::thread> producers;
        for (int p = 0; p < producer_count; ++p) {
            producers.emplace_back([&, p]() {
                for (int i = 0; i < messages_per_producer; ++i) {
                    auto msg = make_msg<PerformanceTestData>(
                        (i + p) % 10, 
                        PerformanceTestData{p * 10000 + i, static_cast<double>(i)}
                    );
                    while (!queue->enqueue(msg)) {
                        std::this_thread::yield();
                    }
                    total_produced.fetch_add(1);
                }
            });
        }
        
        // Start consumer threads
        std::vector<std::thread> consumers;
        for (int c = 0; c < consumer_count; ++c) {
            consumers.emplace_back([&]() {
                while (!stop_consumers.load()) {
                    auto msg = queue->dequeue_block(std::chrono::milliseconds(10));
                    if (msg) {
                        total_consumed.fetch_add(1);
                    }
                }
                
                // Consume remaining messages
                while (auto msg = queue->dequeue()) {
                    total_consumed.fetch_add(1);
                }
            });
        }
        
        // Wait for producers to finish
        for (auto& producer : producers) {
            producer.join();
        }
        
        // Wait a bit for consumers to catch up
        while (total_consumed.load() < total_produced.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        
        stop_consumers.store(true);
        
        // Wait for consumers to finish
        for (auto& consumer : consumers) {
            consumer.join();
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        auto total_time = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        
        std::cout << "Producers: " << producer_count << ", Consumers: " << consumer_count << std::endl;
        std::cout << "Total messages: " << total_produced.load() << std::endl;
        std::cout << "Messages consumed: " << total_consumed.load() << std::endl;
        std::cout << "Total time: " << total_time.count() << " μs" << std::endl;
        std::cout << "Throughput: " 
                  << (total_consumed.load() * 1000000.0 / total_time.count()) << " msgs/sec" << std::endl;
    }
    
    void testBatchOperations() {
        std::cout << "\n--- Batch Operations Performance ---" << std::endl;
        
        auto queue = std::make_shared<MsgQueue>(10000);
        const int batch_size = 100;
        const int batch_count = 100;
        
        // Prepare batch messages
        std::vector<BaseMsgPtr> batch;
        batch.reserve(batch_size);
        
        for (int i = 0; i < batch_size; ++i) {
            batch.push_back(make_msg<PerformanceTestData>(
                i % 10, 
                PerformanceTestData{i, static_cast<double>(i)}
            ));
        }
        
        // Test batch enqueue
        auto start = std::chrono::high_resolution_clock::now();
        
        for (int b = 0; b < batch_count; ++b) {
            queue->enqueue_batch(batch);
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        auto batch_enqueue_time = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        
        std::cout << "Batch enqueue " << (batch_size * batch_count) << " messages: " 
                  << batch_enqueue_time.count() << " μs" << std::endl;
        std::cout << "Batch enqueue rate: " 
                  << (batch_size * batch_count * 1000000.0 / batch_enqueue_time.count()) << " msgs/sec" << std::endl;
        
        // Test batch dequeue
        start = std::chrono::high_resolution_clock::now();
        
        std::vector<BaseMsgPtr> dequeued_batch;
        int total_dequeued = 0;
        
        while (!queue->empty()) {
            size_t count = queue->dequeue_batch(dequeued_batch, batch_size);
            total_dequeued += count;
        }
        
        end = std::chrono::high_resolution_clock::now();
        auto batch_dequeue_time = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        
        std::cout << "Batch dequeue " << total_dequeued << " messages: " 
                  << batch_dequeue_time.count() << " μs" << std::endl;
        std::cout << "Batch dequeue rate: " 
                  << (total_dequeued * 1000000.0 / batch_dequeue_time.count()) << " msgs/sec" << std::endl;
    }
    
    void testMemoryUsage() {
        std::cout << "\n--- Memory Usage Test ---" << std::endl;
        
        auto queue = std::make_shared<MsgQueue>(100000);
        const int message_count = 50000;
        
        // Fill queue
        for (int i = 0; i < message_count; ++i) {
            auto msg = make_msg<PerformanceTestData>(
                i % 10, 
                PerformanceTestData{i, static_cast<double>(i), 100}  // Larger payload
            );
            queue->enqueue(msg);
        }
        
        auto stats = queue->getStatistics();
        std::cout << "Queue size: " << queue->size() << " messages" << std::endl;
        std::cout << "Peak size reached: " << stats.peak_size.load() << " messages" << std::endl;
        std::cout << "Total enqueued: " << stats.total_enqueued.load() << std::endl;
        
        // Clear queue
        queue->clear();
        std::cout << "Queue size after clear: " << queue->size() << " messages" << std::endl;
    }
};

int main() {
    PerformanceBenchmark benchmark;
    benchmark.runBenchmark();
    return 0;
}
