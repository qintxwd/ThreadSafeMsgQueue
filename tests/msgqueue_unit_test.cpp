#include "ThreadSafeMsgQueue/ThreadSafeMsgQueue.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <vector>
#include <random>
#include <atomic>

using namespace qyh::ThreadSafeMsgQueue;

struct TestMessage {
    int id;
    std::string data;
    double timestamp;
    
    TestMessage(int i, const std::string& d, double ts = 0.0)
        : id(i), data(d), timestamp(ts) {}
};

class MsgQueueUnitTest {
public:
    bool runAllTests() {
        std::cout << "=== ThreadSafeMsgQueue Unit Tests ===" << std::endl;
        
        bool all_passed = true;
        
        all_passed &= testBasicEnqueueDequeue();
        all_passed &= testPriorityOrdering();
        all_passed &= testBatchOperations();
        all_passed &= testThreadSafety();
        all_passed &= testStatistics();
        all_passed &= testOverflowProtection();
        all_passed &= testTimeoutOperations();
        all_passed &= testEmptyQueueBehavior();
        
        std::cout << "\n=== Unit Test Summary ===" << std::endl;
        if (all_passed) {
            std::cout << "✅ ALL UNIT TESTS PASSED!" << std::endl;
        } else {
            std::cout << "❌ Some unit tests failed!" << std::endl;
        }
        
        return all_passed;
    }

private:
    bool testBasicEnqueueDequeue() {
        std::cout << "\n1. Testing Basic Enqueue/Dequeue..." << std::endl;
        
        auto queue = std::make_shared<MsgQueue>();
        bool test_passed = true;
        
        // Test empty queue
        if (!queue->empty()) {
            std::cout << "❌ New queue should be empty" << std::endl;
            test_passed = false;
        }
        
        if (queue->size() != 0) {
            std::cout << "❌ New queue size should be 0" << std::endl;
            test_passed = false;
        }
        
        // Test enqueue
        auto msg = make_msg<TestMessage>(1, TestMessage(42, "test"));
        if (!queue->enqueue(msg)) {
            std::cout << "❌ Failed to enqueue message" << std::endl;
            test_passed = false;
        }
        
        if (queue->empty()) {
            std::cout << "❌ Queue should not be empty after enqueue" << std::endl;
            test_passed = false;
        }
        
        if (queue->size() != 1) {
            std::cout << "❌ Queue size should be 1 after enqueue" << std::endl;
            test_passed = false;
        }
        
        // Test dequeue
        auto dequeued = queue->dequeue();
        if (!dequeued) {
            std::cout << "❌ Failed to dequeue message" << std::endl;
            test_passed = false;
        }
        
        auto typed_msg = std::dynamic_pointer_cast<Msg<TestMessage>>(dequeued);
        if (!typed_msg) {
            std::cout << "❌ Dequeued message has wrong type" << std::endl;
            test_passed = false;
        } else if (typed_msg->getContent().id != 42) {
            std::cout << "❌ Dequeued message has wrong content" << std::endl;
            test_passed = false;
        }
        
        if (!queue->empty()) {
            std::cout << "❌ Queue should be empty after dequeue" << std::endl;
            test_passed = false;
        }
        
        if (test_passed) {
            std::cout << "✅ Basic enqueue/dequeue test passed" << std::endl;
        }
        
        return test_passed;
    }
    
    bool testPriorityOrdering() {
        std::cout << "\n2. Testing Priority Ordering..." << std::endl;
        
        auto queue = std::make_shared<MsgQueue>();
        bool test_passed = true;
        
        // Enqueue messages with different priorities
        auto msg_low = make_msg<TestMessage>(1, TestMessage(1, "low"));
        auto msg_high = make_msg<TestMessage>(5, TestMessage(2, "high"));
        auto msg_medium = make_msg<TestMessage>(3, TestMessage(3, "medium"));
        
        queue->enqueue(msg_low);
        queue->enqueue(msg_high);
        queue->enqueue(msg_medium);
        
        // Dequeue and check order (highest priority first)
        std::vector<int> expected_order = {2, 3, 1}; // high, medium, low
        std::vector<int> actual_order;
        
        while (!queue->empty()) {
            auto msg = queue->dequeue();
            auto typed_msg = std::dynamic_pointer_cast<Msg<TestMessage>>(msg);
            if (typed_msg) {
                actual_order.push_back(typed_msg->getContent().id);
            }
        }
        
        if (actual_order != expected_order) {
            std::cout << "❌ Priority ordering failed. Expected: ";
            for (int id : expected_order) std::cout << id << " ";
            std::cout << ", Got: ";
            for (int id : actual_order) std::cout << id << " ";
            std::cout << std::endl;
            test_passed = false;
        }
        
        if (test_passed) {
            std::cout << "✅ Priority ordering test passed" << std::endl;
        }
        
        return test_passed;
    }
    
    bool testBatchOperations() {
        std::cout << "\n3. Testing Batch Operations..." << std::endl;
        
        auto queue = std::make_shared<MsgQueue>();
        bool test_passed = true;
        
        const int batch_size = 100;
        
        // Create batch messages
        std::vector<BaseMsgPtr> batch_messages;
        for (int i = 0; i < batch_size; ++i) {
            auto msg = make_msg<TestMessage>(1, TestMessage(i, "batch_" + std::to_string(i)));
            batch_messages.push_back(msg);
        }
        
        // Test batch enqueue
        size_t enqueued = queue->enqueue_batch(batch_messages);
        if (enqueued != batch_size) {
            std::cout << "❌ Batch enqueue failed. Expected: " << batch_size 
                      << ", Got: " << enqueued << std::endl;
            test_passed = false;
        }
        
        if (queue->size() != batch_size) {
            std::cout << "❌ Queue size after batch enqueue incorrect. Expected: " << batch_size 
                      << ", Got: " << queue->size() << std::endl;
            test_passed = false;
        }
        
        // Test batch dequeue
        std::vector<BaseMsgPtr> dequeued_batch;
        size_t dequeued_count = queue->dequeue_batch(dequeued_batch, batch_size);
        
        if (dequeued_count != batch_size) {
            std::cout << "❌ Batch dequeue failed. Expected: " << batch_size 
                      << ", Got: " << dequeued_count << std::endl;
            test_passed = false;
        }
        
        if (dequeued_batch.size() != batch_size) {
            std::cout << "❌ Batch dequeue vector size incorrect" << std::endl;
            test_passed = false;
        }
        
        if (!queue->empty()) {
            std::cout << "❌ Queue should be empty after batch dequeue" << std::endl;
            test_passed = false;
        }
        
        if (test_passed) {
            std::cout << "✅ Batch operations test passed" << std::endl;
        }
        
        return test_passed;
    }
    
    bool testThreadSafety() {
        std::cout << "\n4. Testing Thread Safety..." << std::endl;
        
        auto queue = std::make_shared<MsgQueue>(10000);
        bool test_passed = true;
        
        const int num_producers = 4;
        const int num_consumers = 2;
        const int messages_per_producer = 1000;
        
        std::atomic<int> total_produced{0};
        std::atomic<int> total_consumed{0};
        std::atomic<bool> stop_consumers{false};
        
        // Start producer threads
        std::vector<std::thread> producers;
        for (int p = 0; p < num_producers; ++p) {
            producers.emplace_back([&, p]() {
                for (int i = 0; i < messages_per_producer; ++i) {
                    auto msg = make_msg<TestMessage>(1, TestMessage(p * 1000 + i, "producer_" + std::to_string(p)));
                    if (queue->enqueue(msg)) {
                        total_produced.fetch_add(1);
                    }
                }
            });
        }
        
        // Start consumer threads
        std::vector<std::thread> consumers;
        for (int c = 0; c < num_consumers; ++c) {
            consumers.emplace_back([&]() {
                while (!stop_consumers.load()) {
                    auto msg = queue->dequeue();
                    if (msg) {
                        total_consumed.fetch_add(1);
                    } else {
                        std::this_thread::sleep_for(std::chrono::microseconds(10));
                    }
                }
            });
        }
        
        // Wait for producers
        for (auto& producer : producers) {
            producer.join();
        }
        
        // Wait for consumers to catch up
        while (total_consumed.load() < total_produced.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        
        stop_consumers.store(true);
        
        // Wait for consumers
        for (auto& consumer : consumers) {
            consumer.join();
        }
        
        int expected_total = num_producers * messages_per_producer;
        if (total_produced.load() != expected_total) {
            std::cout << "❌ Production count incorrect. Expected: " << expected_total 
                      << ", Got: " << total_produced.load() << std::endl;
            test_passed = false;
        }
        
        if (total_consumed.load() != total_produced.load()) {
            std::cout << "❌ Consumption count incorrect. Produced: " << total_produced.load()
                      << ", Consumed: " << total_consumed.load() << std::endl;
            test_passed = false;
        }
        
        if (test_passed) {
            std::cout << "✅ Thread safety test passed - Produced/Consumed: " 
                      << total_produced.load() << "/" << total_consumed.load() << std::endl;
        }
        
        return test_passed;
    }
    
    bool testStatistics() {
        std::cout << "\n5. Testing Statistics..." << std::endl;
        
        auto queue = std::make_shared<MsgQueue>();
        bool test_passed = true;
        
        // Initial statistics should be zero
        auto stats = queue->getStatistics();
        if (stats.total_enqueued.load() != 0 || stats.total_dequeued.load() != 0 || 
            stats.current_size.load() != 0 || stats.peak_size.load() != 0) {
            std::cout << "❌ Initial statistics should be zero" << std::endl;
            test_passed = false;
        }
        
        const int message_count = 50;
        
        // Enqueue messages
        for (int i = 0; i < message_count; ++i) {
            auto msg = make_msg<TestMessage>(1, TestMessage(i, "stats_test"));
            queue->enqueue(msg);
        }
        
        stats = queue->getStatistics();
        if (stats.total_enqueued.load() != message_count) {
            std::cout << "❌ Enqueue statistics incorrect" << std::endl;
            test_passed = false;
        }
        
        if (stats.current_size.load() != message_count) {
            std::cout << "❌ Current size statistics incorrect" << std::endl;
            test_passed = false;
        }
        
        if (stats.peak_size.load() != message_count) {
            std::cout << "❌ Peak size statistics incorrect" << std::endl;
            test_passed = false;
        }
        
        // Dequeue half the messages
        for (int i = 0; i < message_count / 2; ++i) {
            queue->dequeue();
        }
        
        stats = queue->getStatistics();
        if (stats.total_dequeued.load() != message_count / 2) {
            std::cout << "❌ Dequeue statistics incorrect" << std::endl;
            test_passed = false;
        }
        
        if (stats.current_size.load() != message_count - message_count / 2) {
            std::cout << "❌ Current size after partial dequeue incorrect" << std::endl;
            test_passed = false;
        }
        
        if (test_passed) {
            std::cout << "✅ Statistics test passed" << std::endl;
        }
        
        return test_passed;
    }
    
    bool testOverflowProtection() {
        std::cout << "\n6. Testing Overflow Protection..." << std::endl;
        
        const size_t max_size = 10;
        auto queue = std::make_shared<MsgQueue>(max_size);
        bool test_passed = true;
        
        // Fill queue to capacity
        for (size_t i = 0; i < max_size; ++i) {
            auto msg = make_msg<TestMessage>(1, TestMessage(static_cast<int>(i), "overflow_test"));
            if (!queue->enqueue(msg)) {
                std::cout << "❌ Failed to enqueue message " << i << " within capacity" << std::endl;
                test_passed = false;
            }
        }
        
        // Try to enqueue one more (should fail)
        auto overflow_msg = make_msg<TestMessage>(1, TestMessage(999, "overflow"));
        if (queue->enqueue(overflow_msg)) {
            std::cout << "❌ Enqueue should have failed when queue is at capacity" << std::endl;
            test_passed = false;
        }
        
        if (queue->size() != max_size) {
            std::cout << "❌ Queue size should be at max capacity" << std::endl;
            test_passed = false;
        }
        
        if (test_passed) {
            std::cout << "✅ Overflow protection test passed" << std::endl;
        }
        
        return test_passed;
    }
    
    bool testTimeoutOperations() {
        std::cout << "\n7. Testing Timeout Operations..." << std::endl;
        
        auto queue = std::make_shared<MsgQueue>();
        bool test_passed = true;
        
        // Test timeout on empty queue
        auto start = std::chrono::high_resolution_clock::now();
        auto msg = queue->dequeue_block(std::chrono::milliseconds(100));
        auto end = std::chrono::high_resolution_clock::now();
        
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        
        if (msg != nullptr) {
            std::cout << "❌ Timeout dequeue should return nullptr" << std::endl;
            test_passed = false;
        }
        
        if (duration.count() < 90 || duration.count() > 150) {
            std::cout << "❌ Timeout duration incorrect: " << duration.count() << "ms" << std::endl;
            test_passed = false;
        }
        
        // Test successful dequeue within timeout
        std::thread producer([&]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            auto test_msg = make_msg<TestMessage>(1, TestMessage(123, "timeout_test"));
            queue->enqueue(test_msg);
        });
        
        start = std::chrono::high_resolution_clock::now();
        msg = queue->dequeue_block(std::chrono::milliseconds(200));
        end = std::chrono::high_resolution_clock::now();
        
        producer.join();
        
        if (!msg) {
            std::cout << "❌ Should have received message within timeout" << std::endl;
            test_passed = false;
        }
        
        duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        if (duration.count() > 100) {
            std::cout << "❌ Should have received message quickly" << std::endl;
            test_passed = false;
        }
        
        if (test_passed) {
            std::cout << "✅ Timeout operations test passed" << std::endl;
        }
        
        return test_passed;
    }
    
    bool testEmptyQueueBehavior() {
        std::cout << "\n8. Testing Empty Queue Behavior..." << std::endl;
        
        auto queue = std::make_shared<MsgQueue>();
        bool test_passed = true;
        
        // Test dequeue on empty queue
        auto msg = queue->dequeue();
        if (msg != nullptr) {
            std::cout << "❌ Dequeue on empty queue should return nullptr" << std::endl;
            test_passed = false;
        }
        
        // Test batch dequeue on empty queue
        std::vector<BaseMsgPtr> batch;
        size_t count = queue->dequeue_batch(batch, 10);
        if (count != 0 || !batch.empty()) {
            std::cout << "❌ Batch dequeue on empty queue should return 0" << std::endl;
            test_passed = false;
        }
        
        // Test statistics on empty queue
        auto stats = queue->getStatistics();
        if (stats.current_size.load() != 0) {
            std::cout << "❌ Empty queue current size should be 0" << std::endl;
            test_passed = false;
        }
        
        // Test clear on empty queue (should not crash)
        queue->clear();
        
        if (test_passed) {
            std::cout << "✅ Empty queue behavior test passed" << std::endl;
        }
        
        return test_passed;
    }
};

int main() {
    MsgQueueUnitTest test;
    bool result = test.runAllTests();
    
    return result ? 0 : 1;
}
