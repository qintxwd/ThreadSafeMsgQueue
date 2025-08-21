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
