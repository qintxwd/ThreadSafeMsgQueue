#include "ThreadSafeMsgQueue/ThreadSafeMsgQueue.h"
#include <iostream>
#include <vector>
#include <atomic>

using namespace qyh::ThreadSafeMsgQueue;

struct CallbackTestData {
    int value;
    std::string text;
    
    CallbackTestData(int v, const std::string& t) : value(v), text(t) {}
};

class CallbackTest {
public:
    bool runAllTests() {
        std::cout << "=== ThreadSafeMsgQueue Callback System Test ===" << std::endl;
        
        bool all_passed = true;
        
        all_passed &= testBasicCallback();
        all_passed &= testTypeSafety();
        all_passed &= testCallbackChaining();
        all_passed &= testExceptionHandling();
        all_passed &= testCallbackFactories();
        
        std::cout << "\n=== Callback Test Summary ===" << std::endl;
        if (all_passed) {
            std::cout << "✅ ALL CALLBACK TESTS PASSED!" << std::endl;
        } else {
            std::cout << "❌ Some callback tests failed!" << std::endl;
        }
        
        return all_passed;
    }

private:
    bool testBasicCallback() {
        std::cout << "\n1. Testing Basic Callback..." << std::endl;
        
        bool test_passed = true;
        std::atomic<bool> callback_called{false};
        std::atomic<int> received_value{-1};
        
        // Create callback
        auto callback = makeCallback<CallbackTestData>([&](const MsgPtr<CallbackTestData>& msg) {
            received_value.store(msg->getContent().value);
            callback_called.store(true);
        });
        
        // Create message
        auto msg = make_msg<CallbackTestData>(1, CallbackTestData(42, "test"));
        
        // Call callback
        bool result = callback->call(msg);
        
        if (!result) {
            std::cout << "❌ Callback should return true for correct type" << std::endl;
            test_passed = false;
        }
        
        if (!callback_called.load()) {
            std::cout << "❌ Callback was not called" << std::endl;
            test_passed = false;
        }
        
        if (received_value.load() != 42) {
            std::cout << "❌ Callback received wrong value: " << received_value.load() << std::endl;
            test_passed = false;
        }
        
        if (test_passed) {
            std::cout << "✅ Basic callback test passed" << std::endl;
        }
        
        return test_passed;
    }
    
    bool testTypeSafety() {
        std::cout << "\n2. Testing Type Safety..." << std::endl;
        
        bool test_passed = true;
        std::atomic<bool> callback_called{false};
        
        // Create callback for specific type
        auto callback = makeCallback<CallbackTestData>([&](const MsgPtr<CallbackTestData>& msg) {
            callback_called.store(true);
        });
        
        // Create message of different type
        struct WrongType { int x; };
        auto wrong_msg = make_msg<WrongType>(1, WrongType{123});
        
        // Try to call callback with wrong type
        bool result = callback->call(wrong_msg);
        
        if (result) {
            std::cout << "❌ Callback should return false for wrong type" << std::endl;
            test_passed = false;
        }
        
        if (callback_called.load()) {
            std::cout << "❌ Callback should not be called for wrong type" << std::endl;
            test_passed = false;
        }
        
        // Test type checking
        if (!callback->canHandle<CallbackTestData>()) {
            std::cout << "❌ Callback should handle correct type" << std::endl;
            test_passed = false;
        }
        
        if (callback->canHandle<WrongType>()) {
            std::cout << "❌ Callback should not handle wrong type" << std::endl;
            test_passed = false;
        }
        
        if (test_passed) {
            std::cout << "✅ Type safety test passed" << std::endl;
        }
        
        return test_passed;
    }
    
    bool testCallbackChaining() {
        std::cout << "\n3. Testing Callback Chaining..." << std::endl;
        
        bool test_passed = true;
        std::atomic<int> total_calls{0};
        std::vector<int> call_order;
        std::mutex order_mutex;
        
        // Create callback chain
        CallbackChain<CallbackTestData> chain;
        
        // Add multiple callbacks
        for (int i = 0; i < 5; ++i) {
            chain.addCallback([&, i](const MsgPtr<CallbackTestData>& msg) {
                total_calls.fetch_add(1);
                std::lock_guard<std::mutex> lock(order_mutex);
                call_order.push_back(i);
            });
        }
        
        if (chain.size() != 5) {
            std::cout << "❌ Chain size incorrect: " << chain.size() << std::endl;
            test_passed = false;
        }
        
        if (chain.empty()) {
            std::cout << "❌ Chain should not be empty" << std::endl;
            test_passed = false;
        }
        
        // Create and call with message
        auto msg = make_msg<CallbackTestData>(1, CallbackTestData(1, "chain_test"));
        chain.call(msg);
        
        if (total_calls.load() != 5) {
            std::cout << "❌ Not all callbacks called: " << total_calls.load() << "/5" << std::endl;
            test_passed = false;
        }
        
        if (call_order.size() != 5) {
            std::cout << "❌ Call order recording failed" << std::endl;
            test_passed = false;
        }
        
        // Test chain clearing
        chain.clear();
        if (!chain.empty() || chain.size() != 0) {
            std::cout << "❌ Chain clear failed" << std::endl;
            test_passed = false;
        }
        
        if (test_passed) {
            std::cout << "✅ Callback chaining test passed" << std::endl;
        }
        
        return test_passed;
    }
    
    bool testExceptionHandling() {
        std::cout << "\n4. Testing Exception Handling..." << std::endl;
        
        bool test_passed = true;
        
        // Create callback that throws exception
        auto throwing_callback = makeCallback<CallbackTestData>([](const MsgPtr<CallbackTestData>& msg) {
            throw std::runtime_error("Test exception");
        });
        
        // Create message
        auto msg = make_msg<CallbackTestData>(1, CallbackTestData(1, "exception_test"));
        
        // Call callback - should handle exception gracefully
        bool result = throwing_callback->call(msg);
        
        if (result) {
            std::cout << "❌ Callback should return false when exception is thrown" << std::endl;
            test_passed = false;
        }
        
        // Test that system continues to work after exception
        std::atomic<bool> normal_callback_called{false};
        auto normal_callback = makeCallback<CallbackTestData>([&](const MsgPtr<CallbackTestData>& msg) {
            normal_callback_called.store(true);
        });
        
        result = normal_callback->call(msg);
        if (!result || !normal_callback_called.load()) {
            std::cout << "❌ Normal callback should work after exception" << std::endl;
            test_passed = false;
        }
        
        if (test_passed) {
            std::cout << "✅ Exception handling test passed" << std::endl;
        }
        
        return test_passed;
    }
    
    bool testCallbackFactories() {
        std::cout << "\n5. Testing Callback Factories..." << std::endl;
        
        bool test_passed = true;
        std::atomic<int> lambda_calls{0};
        std::atomic<int> function_calls{0};
        
        // Test lambda callback
        auto lambda_callback = makeCallback<CallbackTestData>([&](const MsgPtr<CallbackTestData>& msg) {
            lambda_calls.fetch_add(1);
        });
        
        // Test function callback
        auto function_callback = makeCallback<CallbackTestData>(
            std::function<void(const MsgPtr<CallbackTestData>&)>([&](const MsgPtr<CallbackTestData>& msg) {
                function_calls.fetch_add(1);
            })
        );
        
        // Test message
        auto msg = make_msg<CallbackTestData>(1, CallbackTestData(1, "factory_test"));
        
        // Call both callbacks
        bool lambda_result = lambda_callback->call(msg);
        bool function_result = function_callback->call(msg);
        
        if (!lambda_result) {
            std::cout << "❌ Lambda callback failed" << std::endl;
            test_passed = false;
        }
        
        if (!function_result) {
            std::cout << "❌ Function callback failed" << std::endl;
            test_passed = false;
        }
        
        if (lambda_calls.load() != 1) {
            std::cout << "❌ Lambda callback not called" << std::endl;
            test_passed = false;
        }
        
        if (function_calls.load() != 1) {
            std::cout << "❌ Function callback not called" << std::endl;
            test_passed = false;
        }
        
        // Test type info
        const std::type_info& lambda_type = lambda_callback->getMessageType();
        const std::type_info& function_type = function_callback->getMessageType();
        
        if (lambda_type != typeid(CallbackTestData)) {
            std::cout << "❌ Lambda callback type info incorrect" << std::endl;
            test_passed = false;
        }
        
        if (function_type != typeid(CallbackTestData)) {
            std::cout << "❌ Function callback type info incorrect" << std::endl;
            test_passed = false;
        }
        
        if (test_passed) {
            std::cout << "✅ Callback factories test passed" << std::endl;
        }
        
        return test_passed;
    }
    
private:
    std::mutex order_mutex;
};

int main() {
    CallbackTest test;
    bool result = test.runAllTests();
    
    return result ? 0 : 1;
}
