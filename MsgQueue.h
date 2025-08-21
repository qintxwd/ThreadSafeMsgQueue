#pragma once
#include "Msg.h"
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <queue>
#include <vector>
#include <atomic>

class MsgQueue;
using MsgQueuePtr = std::shared_ptr<MsgQueue>;

class MsgQueue : public std::enable_shared_from_this<MsgQueue>
{
public:
  struct Statistics {
    std::atomic<uint64_t> total_enqueued{0};
    std::atomic<uint64_t> total_dequeued{0};
    std::atomic<uint64_t> current_size{0};
    std::atomic<uint64_t> peak_size{0};
    std::atomic<uint64_t> total_wait_time_us{0};  // Total waiting time in microseconds
    std::atomic<uint64_t> wait_count{0};
    
    // Custom copy operations for atomics
    Statistics() = default;
    Statistics(const Statistics& other) {
      total_enqueued.store(other.total_enqueued.load());
      total_dequeued.store(other.total_dequeued.load());
      current_size.store(other.current_size.load());
      peak_size.store(other.peak_size.load());
      total_wait_time_us.store(other.total_wait_time_us.load());
      wait_count.store(other.wait_count.load());
    }
    Statistics& operator=(const Statistics& other) {
      if (this != &other) {
        total_enqueued.store(other.total_enqueued.load());
        total_dequeued.store(other.total_dequeued.load());
        current_size.store(other.current_size.load());
        peak_size.store(other.peak_size.load());
        total_wait_time_us.store(other.total_wait_time_us.load());
        wait_count.store(other.wait_count.load());
      }
      return *this;
    }
  };

  explicit MsgQueue(size_t max_size = std::numeric_limits<size_t>::max()) 
    : max_size_(max_size) {}
  ~MsgQueue() = default;

  // Explicitly delete copy/move operations
  MsgQueue(const MsgQueue &) = delete;
  MsgQueue &operator=(const MsgQueue &) = delete;
  MsgQueue(MsgQueue &&) = delete;
  MsgQueue &operator=(MsgQueue &&) = delete;

  // Enhanced enqueue with overflow protection
  bool enqueue(BaseMsgPtr msg)
  {
    if (!msg) return false;
    
    std::lock_guard<std::mutex> lg(mtx_);
    
    // Check for overflow
    if (msg_queue_.size() >= max_size_) {
      return false;  // Queue is full
    }
    
    auto now = std::chrono::time_point_cast<std::chrono::microseconds>(std::chrono::system_clock::now());
    msg->setTimestamp(now.time_since_epoch().count());
    msg_queue_.push(std::move(msg));
    
    // Update statistics
    stats_.total_enqueued.fetch_add(1, std::memory_order_relaxed);
    auto current_size = ++stats_.current_size;
    
    // Update peak size atomically
    uint64_t expected_peak = stats_.peak_size.load(std::memory_order_relaxed);
    while (current_size > expected_peak && 
           !stats_.peak_size.compare_exchange_weak(expected_peak, current_size, std::memory_order_relaxed)) {
      // Retry if another thread updated peak_size
    }
    
    cv_.notify_one();
    return true;
  }

  // Batch enqueue for better performance
  size_t enqueue_batch(const std::vector<BaseMsgPtr>& messages)
  {
    if (messages.empty()) return 0;
    
    std::lock_guard<std::mutex> lg(mtx_);
    size_t enqueued_count = 0;
    auto now = std::chrono::time_point_cast<std::chrono::microseconds>(std::chrono::system_clock::now());
    auto timestamp = now.time_since_epoch().count();
    
    for (const auto& msg : messages) {
      if (!msg || msg_queue_.size() >= max_size_) {
        break;  // Stop if queue is full or invalid message
      }
      
      msg->setTimestamp(timestamp);
      msg_queue_.push(msg);
      ++enqueued_count;
    }
    
    if (enqueued_count > 0) {
      stats_.total_enqueued.fetch_add(enqueued_count, std::memory_order_relaxed);
      auto current_size = stats_.current_size.fetch_add(enqueued_count, std::memory_order_relaxed) + enqueued_count;
      
      // Update peak size
      uint64_t expected_peak = stats_.peak_size.load(std::memory_order_relaxed);
      while (current_size > expected_peak && 
             !stats_.peak_size.compare_exchange_weak(expected_peak, current_size, std::memory_order_relaxed)) {
      }
      
      cv_.notify_all();
    }
    
    return enqueued_count;
  }

  // Non-blocking dequeue with statistics
  BaseMsgPtr dequeue()
  {
    std::lock_guard<std::mutex> lg(mtx_);
    if (msg_queue_.empty()) {
      return nullptr;
    }
    BaseMsgPtr result = msg_queue_.top();
    msg_queue_.pop();
    
    // Update statistics
    stats_.total_dequeued.fetch_add(1, std::memory_order_relaxed);
    stats_.current_size.fetch_sub(1, std::memory_order_relaxed);
    
    return result;
  }

  // Blocking dequeue with timeout support
  BaseMsgPtr dequeue_block(std::chrono::milliseconds timeout = std::chrono::milliseconds::max())
  {
    std::unique_lock<std::mutex> lg(mtx_);
    auto start_time = std::chrono::high_resolution_clock::now();
    
    bool wait_result;
    if (timeout == std::chrono::milliseconds::max()) {
      cv_.wait(lg, [&] { return !msg_queue_.empty(); });
      wait_result = true;
    } else {
      wait_result = cv_.wait_for(lg, timeout, [&] { return !msg_queue_.empty(); });
    }
    
    if (!wait_result) {
      return nullptr;  // Timeout
    }
    
    BaseMsgPtr result = msg_queue_.top();
    msg_queue_.pop();
    
    // Update statistics
    auto end_time = std::chrono::high_resolution_clock::now();
    auto wait_time = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
    
    stats_.total_dequeued.fetch_add(1, std::memory_order_relaxed);
    stats_.current_size.fetch_sub(1, std::memory_order_relaxed);
    stats_.total_wait_time_us.fetch_add(wait_time.count(), std::memory_order_relaxed);
    stats_.wait_count.fetch_add(1, std::memory_order_relaxed);
    
    return result;
  }

  // Batch dequeue for better performance
  size_t dequeue_batch(std::vector<BaseMsgPtr>& messages, size_t max_count)
  {
    std::lock_guard<std::mutex> lg(mtx_);
    messages.clear();
    messages.reserve(std::min(max_count, msg_queue_.size()));
    
    size_t dequeued_count = 0;
    while (dequeued_count < max_count && !msg_queue_.empty()) {
      messages.push_back(msg_queue_.top());
      msg_queue_.pop();
      ++dequeued_count;
    }
    
    if (dequeued_count > 0) {
      stats_.total_dequeued.fetch_add(dequeued_count, std::memory_order_relaxed);
      stats_.current_size.fetch_sub(dequeued_count, std::memory_order_relaxed);
    }
    
    return dequeued_count;
  }

  // Queue management
  size_t size() const {
    std::lock_guard<std::mutex> lg(mtx_);
    return msg_queue_.size();
  }
  
  bool empty() const {
    std::lock_guard<std::mutex> lg(mtx_);
    return msg_queue_.empty();
  }
  
  void clear() {
    std::lock_guard<std::mutex> lg(mtx_);
    while (!msg_queue_.empty()) {
      msg_queue_.pop();
    }
    stats_.current_size.store(0, std::memory_order_relaxed);
  }

  // Statistics access
  Statistics getStatistics() const {
    return stats_;  // Atomic members are copied safely
  }
  
  void resetStatistics() {
    stats_.total_enqueued.store(0, std::memory_order_relaxed);
    stats_.total_dequeued.store(0, std::memory_order_relaxed);
    stats_.peak_size.store(stats_.current_size.load(std::memory_order_relaxed), std::memory_order_relaxed);
    stats_.total_wait_time_us.store(0, std::memory_order_relaxed);
    stats_.wait_count.store(0, std::memory_order_relaxed);
  }

private:
  std::priority_queue<BaseMsgPtr, std::vector<BaseMsgPtr>, BaseMsgPtrCompareLess> msg_queue_;
  mutable std::mutex mtx_;
  std::condition_variable cv_;
  size_t max_size_{std::numeric_limits<size_t>::max()};
  mutable Statistics stats_;
};
