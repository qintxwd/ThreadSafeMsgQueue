#pragma once

/**
 * @file ThreadSafeMsgQueue.h
 * @brief Thread-Safe Message Queue Framework - All-in-One Header
 * 
 * This is a comprehensive, high-performance, header-only C++ message queue framework
 * designed for inter-module communication in complex systems like SLAM applications.
 * 
 * Key Features:
 * - Type-safe message handling with templates
 * - Priority-based message queuing with deterministic ordering
 * - Thread-safe operations with minimal locking overhead
 * - Batch operations for high-throughput scenarios
 * - Performance monitoring and statistics
 * - Exception-safe operations
 * - Modern C++11/14/17 features
 * 
 * Usage Example:
 * ```cpp
 * // Create a message queue
 * auto queue = std::make_shared<MsgQueue>(1000);
 * 
 * // Create and enqueue a message
 * struct MyData { int value; };
 * auto msg = make_msg<MyData>(0, MyData{42});
 * queue->enqueue(msg);
 * 
 * // Dequeue and process
 * auto received = queue->dequeue();
 * if (auto typed_msg = std::dynamic_pointer_cast<Msg<MyData>>(received)) {
 *     int value = typed_msg->getContent().value;
 * }
 * ```
 * 
 * @author QYH
 * @version 2.0
 * @date 2025
 */

// Include all framework headers
#include "Msg.h"
#include "MsgQueue.h"
#include "SubCallback.h"
#include <string>  // For std::to_string

// Optional: Include the pub-sub system if needed
// #include "PubSub.h"

/**
 * @brief Framework version information
 */
namespace ThreadSafeMsgQueue {
    constexpr int VERSION_MAJOR = 2;
    constexpr int VERSION_MINOR = 0;
    constexpr int VERSION_PATCH = 0;
    
    inline std::string getVersion() {
        return std::to_string(VERSION_MAJOR) + "." + 
               std::to_string(VERSION_MINOR) + "." + 
               std::to_string(VERSION_PATCH);
    }
}

/**
 * @brief Summary of optimizations and ODR compliance fixes:
 * 
 * 1. **ODR (One Definition Rule) Compliance:**
 *    - Fixed global variable issues using inline functions
 *    - Ensured single definition across multiple translation units
 *    - Thread-safe message ID generation with atomic operations
 *    - Eliminated symbol conflicts in header-only design
 * 
 * 2. **Msg.h Optimizations:**
 *    - Added unique message IDs for deterministic ordering
 *    - Implemented perfect forwarding constructors for zero-copy message creation
 *    - Added move semantics and eliminated unnecessary copies
 *    - Added helper functions for convenient message creation
 *    - Made base class non-copyable but movable for performance
 *    - Fixed ODR compliance with inline function design
 * 
 * 3. **MsgQueue.h Optimizations:**
 *    - Added overflow protection with configurable queue size limits
 *    - Implemented batch enqueue/dequeue operations for high throughput
 *    - Added comprehensive performance statistics and monitoring
 *    - Added timeout support for blocking operations
 *    - Improved thread safety with proper locking patterns
 *    - Added queue management functions (size, empty, clear)
 * 
 * 4. **SubCallback.h Optimizations:**
 *    - Added type introspection capabilities
 *    - Improved error handling with boolean return values
 *    - Added callback chaining functionality
 *    - Made callbacks non-copyable but movable
 *    - Added factory functions for easier callback creation
 * 
 * 5. **General Framework Improvements:**
 *    - Consistent C++11/14/17 compatibility
 *    - Exception safety guarantees
 *    - Reduced dynamic allocations through move semantics
 *    - Better performance through atomic operations
 *    - Comprehensive error handling
 *    - Modern C++ best practices throughout
 *    - Complete ODR compliance for header-only deployment
 * 
 * 6. **Performance Characteristics:**
 *    - Enqueue: O(log n) due to priority queue
 *    - Dequeue: O(log n) due to priority queue
 *    - Batch operations: O(k log n) where k is batch size
 *    - Memory overhead: Minimal with efficient shared_ptr usage
 *    - Thread contention: Reduced through careful lock granularity
 * 
 * 7. **Thread Safety:**
 *    - All operations are fully thread-safe
 *    - Lock-free statistics where possible
 *    - Proper memory ordering for atomic operations
 *    - No race conditions in queue operations
 * 
 * 8. **Deployment Options:**
 *    - ✅ Header-Only (Recommended): Simple deployment, ODR compliant
 *    - ⚠️ Dynamic Library: Available if needed for large projects
 *    - ❌ Static Library: Not recommended due to global state issues
 * 
 * 9. **ODR Compliance Verification:**
 *    - Comprehensive test suite in ODRTest.h
 *    - Multi-threading tests for global state consistency
 *    - Cross-translation-unit validation
 *    - Performance benchmarking
 */

