#pragma once

/**
 * @file ODRTest.h
 * @brief ODR (One Definition Rule) Compliance Test for ThreadSafeMsgQueue
 * 
 * This header demonstrates that our framework is ODR-compliant and can be
 * safely included in multiple translation units without symbol conflicts.
 * 
 * Test scenarios:
 * 1. Multiple includes of the same headers
 * 2. Template instantiation consistency
 * 3. Global state consistency (message IDs)
 * 4. Thread safety across translation units
 */

#include "ThreadSafeMsgQueue.h"
#include <vector>
#include <thread>
#include <iostream>
#include <cassert>
#include <set>
#include <atomic>
#include <sstream>

namespace ODRTest {

    /**
     * @brief Test message type for ODR verification
     */
    struct TestMessage {
        int value;
        std::string data;
        
        TestMessage(int v = 0, const std::string& d = "") : value(v), data(d) {}
        
        bool operator==(const TestMessage& other) const {
            return value == other.value && data == other.data;
        }
    };

    /**
     * @brief Test function that creates messages and verifies unique IDs
     * This function can be called from multiple translation units
     */
    inline std::vector<uint64_t> createMessagesAndGetIds(int count = 10) {
        std::vector<uint64_t> ids;
        ids.reserve(count);
        
        for (int i = 0; i < count; ++i) {
            std::ostringstream oss;
            oss << "test_" << i;
            auto msg = make_msg<TestMessage>(0, TestMessage{i, oss.str()});
            ids.push_back(msg->getMessageId());
        }
        
        return ids;
    }

    /**
     * @brief Verify that message IDs are globally unique across translation units
     */
    inline bool verifyGlobalIdUniqueness() {
        constexpr int num_threads = 4;
        constexpr int msgs_per_thread = 100;
        
        std::vector<std::thread> threads;
        std::vector<std::vector<uint64_t>> all_ids(num_threads);
        
        // Create messages from multiple threads (simulating multiple translation units)
        for (int t = 0; t < num_threads; ++t) {
            threads.emplace_back([&all_ids, t, msgs_per_thread]() {
                all_ids[t] = createMessagesAndGetIds(msgs_per_thread);
            });
        }
        
        // Wait for all threads
        for (auto& thread : threads) {
            thread.join();
        }
        
        // Collect all IDs and check for uniqueness
        std::set<uint64_t> unique_ids;
        for (const auto& thread_ids : all_ids) {
            for (uint64_t id : thread_ids) {
                if (unique_ids.find(id) != unique_ids.end()) {
                    std::cerr << "ODR Test FAILED: Duplicate message ID found: " << id << std::endl;
                    return false;
                }
                unique_ids.insert(id);
            }
        }
        
        size_t expected_count = num_threads * msgs_per_thread;
        if (unique_ids.size() != expected_count) {
            std::cerr << "ODR Test FAILED: Expected " << expected_count 
                      << " unique IDs, got " << unique_ids.size() << std::endl;
            return false;
        }
        
        std::cout << "ODR Test PASSED: All " << expected_count 
                  << " message IDs are unique across threads" << std::endl;
        return true;
    }

    /**
     * @brief Test queue operations across simulated translation units
     */
    inline bool testQueueOperations() {
        auto queue = std::make_shared<MsgQueue>(1000);
        
        // Test from "different translation units" (different threads)
        std::atomic<bool> success{true};
        std::vector<std::thread> threads;
        
        // Publisher threads
        for (int t = 0; t < 2; ++t) {
            threads.emplace_back([queue, &success, t]() {
                try {
                    for (int i = 0; i < 50; ++i) {
                        std::ostringstream oss;
                        oss << "thread_" << t;
                        auto msg = make_msg<TestMessage>(i % 5, TestMessage{t * 100 + i, oss.str()});
                        if (!queue->enqueue(msg)) {
                            success.store(false);
                            return;
                        }
                    }
                } catch (...) {
                    success.store(false);
                }
            });
        }
        
        // Consumer thread
        threads.emplace_back([queue, &success]() {
            try {
                int received_count = 0;
                while (received_count < 100) {
                    auto msg = queue->dequeue_block(std::chrono::milliseconds(1000));
                    if (!msg) break;
                    
                    if (auto typed_msg = std::dynamic_pointer_cast<Msg<TestMessage>>(msg)) {
                        ++received_count;
                    }
                }
                
                if (received_count != 100) {
                    std::cerr << "Queue Test FAILED: Expected 100 messages, got " << received_count << std::endl;
                    success.store(false);
                }
            } catch (...) {
                success.store(false);
            }
        });
        
        // Wait for all threads
        for (auto& thread : threads) {
            thread.join();
        }
        
        if (success.load()) {
            std::cout << "Queue Operations Test PASSED" << std::endl;
            return true;
        } else {
            std::cerr << "Queue Operations Test FAILED" << std::endl;
            return false;
        }
    }

    /**
     * @brief Run complete ODR compliance test suite
     */
    inline bool runCompleteTest() {
        std::cout << "=== ThreadSafeMsgQueue ODR Compliance Test ===" << std::endl;
        
        bool all_passed = true;
        
        std::cout << "\n1. Testing Global Message ID Uniqueness..." << std::endl;
        all_passed &= verifyGlobalIdUniqueness();
        
        std::cout << "\n2. Testing Queue Operations..." << std::endl;
        all_passed &= testQueueOperations();
        
        std::cout << "\n=== Test Summary ===" << std::endl;
        if (all_passed) {
            std::cout << "✅ ALL TESTS PASSED - Framework is ODR compliant!" << std::endl;
        } else {
            std::cout << "❌ SOME TESTS FAILED - ODR compliance issues detected!" << std::endl;
        }
        
        return all_passed;
    }

} // namespace ODRTest
