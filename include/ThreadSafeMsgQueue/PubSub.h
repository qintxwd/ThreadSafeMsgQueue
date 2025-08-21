#pragma once

#include "Msg.h"
#include "MsgQueue.h"
#include "SubCallback.h"
#include <unordered_map>
#include <vector>
#include <memory>
#include <mutex>
#include <thread>
#include <atomic>
#include <string>
#include <functional>
#include <algorithm>
#include <chrono>
#include <iostream>

namespace qyh {
namespace ThreadSafeMsgQueue {

/**
 * @brief High-performance thread-safe publish-subscribe system
 * 
 * Features:
 * - Topic-based message routing
 * - Automatic subscriber lifecycle management
 * - Batch processing support
 * - Performance monitoring
 * - Graceful shutdown handling
 */
class PubSubSystem {
public:
    struct Config {
        size_t default_queue_size = 1000;
        size_t worker_thread_count = 1;
        std::chrono::milliseconds processing_timeout{100};
        bool enable_statistics = true;
    };

    struct TopicStatistics {
        std::atomic<uint64_t> messages_published{0};
        std::atomic<uint64_t> messages_processed{0};
        std::atomic<uint64_t> active_subscribers{0};
        std::atomic<uint64_t> total_processing_time_us{0};
        
        // Custom copy operations for atomics
        TopicStatistics() = default;
        TopicStatistics(const TopicStatistics& other) {
            messages_published.store(other.messages_published.load());
            messages_processed.store(other.messages_processed.load());
            active_subscribers.store(other.active_subscribers.load());
            total_processing_time_us.store(other.total_processing_time_us.load());
        }
        
        TopicStatistics& operator=(const TopicStatistics& other) {
            if (this != &other) {
                messages_published.store(other.messages_published.load());
                messages_processed.store(other.messages_processed.load());
                active_subscribers.store(other.active_subscribers.load());
                total_processing_time_us.store(other.total_processing_time_us.load());
            }
            return *this;
        }
    };

    explicit PubSubSystem(const Config& config = Config());
    ~PubSubSystem();

    // Non-copyable, non-movable
    PubSubSystem(const PubSubSystem&) = delete;
    PubSubSystem& operator=(const PubSubSystem&) = delete;
    PubSubSystem(PubSubSystem&&) = delete;
    PubSubSystem& operator=(PubSubSystem&&) = delete;

    /**
     * @brief Start the pub-sub system
     */
    bool start();

    /**
     * @brief Stop the pub-sub system gracefully
     */
    void stop();

    /**
     * @brief Check if the system is running
     */
    bool isRunning() const { return running_.load(std::memory_order_acquire); }

    /**
     * @brief Subscribe to a topic with type-safe callback
     * @param topic Topic name
     * @param callback Function to call when message is received
     * @return Subscription handle for unsubscribing
     */
    template<typename T>
    uint64_t subscribe(const std::string& topic, std::function<void(const MsgPtr<T>&)> callback) {
        std::lock_guard<std::mutex> lock(subscribers_mutex_);
        
        auto sub_callback = std::make_shared<SubCallback<T>>(std::move(callback));
        uint64_t sub_id = next_subscription_id_.fetch_add(1, std::memory_order_relaxed);
        
        auto& topic_subscribers = subscribers_[topic];
        topic_subscribers.emplace_back(sub_id, sub_callback);
        
        // Update statistics
        if (config_.enable_statistics) {
            topic_stats_[topic].active_subscribers.fetch_add(1, std::memory_order_relaxed);
        }
        
        return sub_id;
    }

    /**
     * @brief Unsubscribe from a topic
     * @param topic Topic name
     * @param subscription_id Subscription ID returned by subscribe()
     */
    bool unsubscribe(const std::string& topic, uint64_t subscription_id);

    /**
     * @brief Publish a message to a topic
     * @param topic Topic name
     * @param message Message to publish
     * @param priority Message priority (higher number = higher priority)
     */
    template<typename T>
    bool publish(const std::string& topic, const T& content, int priority = 0) {
        if (!running_.load(std::memory_order_acquire)) {
            return false;
        }

        auto msg = make_msg<T>(priority, content);
        return publishMessage(topic, msg);
    }

    /**
     * @brief Publish a pre-created message to a topic
     */
    bool publishMessage(const std::string& topic, BaseMsgPtr message);

    /**
     * @brief Batch publish multiple messages
     */
    template<typename T>
    size_t publishBatch(const std::string& topic, const std::vector<T>& contents, int priority = 0) {
        if (!running_.load(std::memory_order_acquire)) {
            return 0;
        }

        std::vector<BaseMsgPtr> messages;
        messages.reserve(contents.size());
        
        for (const auto& content : contents) {
            messages.push_back(make_msg<T>(priority, content));
        }
        
        return publishMessageBatch(topic, messages);
    }

    /**
     * @brief Get topic statistics
     */
    TopicStatistics getTopicStatistics(const std::string& topic) const;

    /**
     * @brief Get all topic names
     */
    std::vector<std::string> getTopicNames() const;

    /**
     * @brief Get subscriber count for a topic
     */
    size_t getSubscriberCount(const std::string& topic) const;

    /**
     * @brief Clear all subscribers and queues
     */
    void clear();

private:
    using SubscriberEntry = std::pair<uint64_t, BaseSubCallbackPtr>;
    using SubscriberList = std::vector<SubscriberEntry>;

    Config config_;
    std::atomic<bool> running_{false};
    std::atomic<bool> should_stop_{false};
    std::atomic<uint64_t> next_subscription_id_{1};

    // Topic management
    std::unordered_map<std::string, SubscriberList> subscribers_;
    std::unordered_map<std::string, MsgQueuePtr> topic_queues_;
    mutable std::mutex subscribers_mutex_;

    // Statistics
    mutable std::unordered_map<std::string, TopicStatistics> topic_stats_;
    mutable std::mutex stats_mutex_;

    // Worker threads
    std::vector<std::thread> worker_threads_;

    // Internal methods
    void workerLoop();
    void processTopicQueue(const std::string& topic, MsgQueuePtr queue);
    size_t publishMessageBatch(const std::string& topic, const std::vector<BaseMsgPtr>& messages);
    MsgQueuePtr getOrCreateQueue(const std::string& topic);
    void updateStatistics(const std::string& topic, uint64_t processing_time_us);
};

// Convenience singleton for global access
class GlobalPubSub {
public:
    static PubSubSystem& instance() {
        static PubSubSystem instance;
        return instance;
    }

    template<typename T>
    static uint64_t subscribe(const std::string& topic, std::function<void(const MsgPtr<T>&)> callback) {
        return instance().subscribe<T>(topic, std::move(callback));
    }

    static bool unsubscribe(const std::string& topic, uint64_t subscription_id) {
        return instance().unsubscribe(topic, subscription_id);
    }

    template<typename T>
    static bool publish(const std::string& topic, const T& content, int priority = 0) {
        return instance().publish<T>(topic, content, priority);
    }

    static bool start() {
        return instance().start();
    }

    static void stop() {
        instance().stop();
    }

private:
    GlobalPubSub() = default;
};

// RAII subscription handle for automatic cleanup
template<typename T>
class SubscriptionHandle {
public:
    SubscriptionHandle(PubSubSystem& pubsub, const std::string& topic, uint64_t sub_id)
        : pubsub_(pubsub), topic_(topic), sub_id_(sub_id), valid_(true) {}

    ~SubscriptionHandle() {
        if (valid_) {
            pubsub_.unsubscribe(topic_, sub_id_);
        }
    }

    // Non-copyable but movable
    SubscriptionHandle(const SubscriptionHandle&) = delete;
    SubscriptionHandle& operator=(const SubscriptionHandle&) = delete;

    SubscriptionHandle(SubscriptionHandle&& other) noexcept
        : pubsub_(other.pubsub_), topic_(std::move(other.topic_)), 
          sub_id_(other.sub_id_), valid_(other.valid_) {
        other.valid_ = false;
    }

    SubscriptionHandle& operator=(SubscriptionHandle&& other) noexcept {
        if (this != &other) {
            if (valid_) {
                pubsub_.unsubscribe(topic_, sub_id_);
            }
            pubsub_ = other.pubsub_;
            topic_ = std::move(other.topic_);
            sub_id_ = other.sub_id_;
            valid_ = other.valid_;
            other.valid_ = false;
        }
        return *this;
    }

    void release() {
        valid_ = false;
    }

    bool isValid() const { return valid_; }
    uint64_t getSubscriptionId() const { return sub_id_; }
    const std::string& getTopic() const { return topic_; }

private:
    PubSubSystem& pubsub_;
    std::string topic_;
    uint64_t sub_id_;
    bool valid_;
};

// Helper function to create RAII subscription
template<typename T>
SubscriptionHandle<T> makeSubscription(PubSubSystem& pubsub, const std::string& topic, 
                                     std::function<void(const MsgPtr<T>&)> callback) {
    uint64_t sub_id = pubsub.subscribe<T>(topic, std::move(callback));
    return SubscriptionHandle<T>(pubsub, topic, sub_id);
}

} // namespace ThreadSafeMsgQueue
} // namespace qyh

// Inline implementations for header-only library
namespace qyh {
namespace ThreadSafeMsgQueue {

// PubSubSystem Implementation
inline PubSubSystem::PubSubSystem(const Config& config) 
    : config_(config) {}

inline PubSubSystem::~PubSubSystem() {
    stop();
}

inline bool PubSubSystem::start() {
    std::lock_guard<std::mutex> lock(subscribers_mutex_);
    if (running_.load()) {
        return true; // Already running
    }
    
    should_stop_.store(false);
    running_.store(true);
    
    // Start worker threads
    worker_threads_.reserve(config_.worker_thread_count);
    for (size_t i = 0; i < config_.worker_thread_count; ++i) {
        worker_threads_.emplace_back(&PubSubSystem::workerLoop, this);
    }
    
    return true;
}

inline void PubSubSystem::stop() {
    // Signal threads to stop
    should_stop_.store(true);
    running_.store(false);
    
    // Wait for all worker threads to finish
    for (auto& thread : worker_threads_) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    worker_threads_.clear();
}

inline bool PubSubSystem::unsubscribe(const std::string& topic, uint64_t subscription_id) {
    std::lock_guard<std::mutex> lock(subscribers_mutex_);
    
    auto it = subscribers_.find(topic);
    if (it != subscribers_.end()) {
        auto& subscribers = it->second;
        auto sub_it = std::find_if(subscribers.begin(), subscribers.end(),
            [subscription_id](const SubscriberEntry& entry) {
                return entry.first == subscription_id;
            });
        
        if (sub_it != subscribers.end()) {
            subscribers.erase(sub_it);
            
            // Update statistics
            if (config_.enable_statistics) {
                std::lock_guard<std::mutex> stats_lock(stats_mutex_);
                auto stats_it = topic_stats_.find(topic);
                if (stats_it != topic_stats_.end()) {
                    stats_it->second.active_subscribers.fetch_sub(1, std::memory_order_relaxed);
                }
            }
            return true;
        }
    }
    return false;
}

inline bool PubSubSystem::publishMessage(const std::string& topic, BaseMsgPtr message) {
    if (!running_.load(std::memory_order_acquire)) {
        return false;
    }
    
    auto queue = getOrCreateQueue(topic);
    if (queue && queue->enqueue(message)) {
        // Update statistics
        if (config_.enable_statistics) {
            std::lock_guard<std::mutex> stats_lock(stats_mutex_);
            topic_stats_[topic].messages_published.fetch_add(1, std::memory_order_relaxed);
        }
        return true;
    }
    return false;
}

inline PubSubSystem::TopicStatistics PubSubSystem::getTopicStatistics(const std::string& topic) const {
    std::lock_guard<std::mutex> stats_lock(stats_mutex_);
    auto it = topic_stats_.find(topic);
    if (it != topic_stats_.end()) {
        return it->second;
    }
    return TopicStatistics{};
}

inline std::vector<std::string> PubSubSystem::getTopicNames() const {
    std::lock_guard<std::mutex> lock(subscribers_mutex_);
    std::vector<std::string> topics;
    topics.reserve(subscribers_.size());
    for (const auto& pair : subscribers_) {
        topics.push_back(pair.first);
    }
    return topics;
}

inline size_t PubSubSystem::getSubscriberCount(const std::string& topic) const {
    std::lock_guard<std::mutex> lock(subscribers_mutex_);
    auto it = subscribers_.find(topic);
    return (it != subscribers_.end()) ? it->second.size() : 0;
}

inline void PubSubSystem::clear() {
    std::lock_guard<std::mutex> lock(subscribers_mutex_);
    std::lock_guard<std::mutex> stats_lock(stats_mutex_);
    
    subscribers_.clear();
    topic_queues_.clear();
    topic_stats_.clear();
}

inline void PubSubSystem::workerLoop() {
    while (!should_stop_.load()) {
        bool processed_any = false;
        
        // Process all topic queues
        {
            std::lock_guard<std::mutex> lock(subscribers_mutex_);
            
            for (auto& topic_pair : topic_queues_) {
                const std::string& topic = topic_pair.first;
                auto& queue = topic_pair.second;
                
                if (!queue->empty()) {
                    processTopicQueue(topic, queue);
                    processed_any = true;
                }
            }
        }
        
        // If no messages were processed, sleep briefly
        if (!processed_any) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
}

inline void PubSubSystem::processTopicQueue(const std::string& topic, MsgQueuePtr queue) {
    auto start_time = std::chrono::high_resolution_clock::now();
    
    // Process up to 10 messages per call to avoid blocking too long
    int processed_count = 0;
    const int max_batch = 10;
    
    while (processed_count < max_batch && !queue->empty()) {
        auto message = queue->dequeue();
        if (!message) {
            break;
        }
        
        // Find subscribers for this topic
        auto sub_it = subscribers_.find(topic);
        if (sub_it != subscribers_.end()) {
            // Deliver message to all subscribers
            for (const auto& subscriber_entry : sub_it->second) {
                auto& callback = subscriber_entry.second;
                try {
                    callback->call(message);
                } catch (const std::exception& e) {
                    // Log error but continue processing
                    std::cerr << "Error in subscriber callback for topic '" << topic 
                              << "': " << e.what() << std::endl;
                }
            }
        }
        
        processed_count++;
    }
    
    // Update statistics
    if (config_.enable_statistics && processed_count > 0) {
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
        updateStatistics(topic, duration.count());
        
        std::lock_guard<std::mutex> stats_lock(stats_mutex_);
        topic_stats_[topic].messages_processed.fetch_add(processed_count, std::memory_order_relaxed);
    }
}

inline size_t PubSubSystem::publishMessageBatch(const std::string& topic, const std::vector<BaseMsgPtr>& messages) {
    if (!running_.load(std::memory_order_acquire)) {
        return 0;
    }
    
    auto queue = getOrCreateQueue(topic);
    if (!queue) {
        return 0;
    }
    
    size_t published_count = 0;
    for (const auto& message : messages) {
        if (queue->enqueue(message)) {
            published_count++;
        }
    }
    
    // Update statistics
    if (config_.enable_statistics && published_count > 0) {
        std::lock_guard<std::mutex> stats_lock(stats_mutex_);
        topic_stats_[topic].messages_published.fetch_add(published_count, std::memory_order_relaxed);
    }
    
    return published_count;
}

inline MsgQueuePtr PubSubSystem::getOrCreateQueue(const std::string& topic) {
    // Note: This assumes we're already holding subscribers_mutex_
    auto it = topic_queues_.find(topic);
    if (it != topic_queues_.end()) {
        return it->second;
    }
    
    // Create new queue
    auto queue = std::make_shared<MsgQueue>(config_.default_queue_size);
    topic_queues_[topic] = queue;
    return queue;
}

inline void PubSubSystem::updateStatistics(const std::string& topic, uint64_t processing_time_us) {
    std::lock_guard<std::mutex> stats_lock(stats_mutex_);
    topic_stats_[topic].total_processing_time_us.fetch_add(processing_time_us, std::memory_order_relaxed);
}

} // namespace ThreadSafeMsgQueue
} // namespace qyh
