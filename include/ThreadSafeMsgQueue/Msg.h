#pragma once

#include <cstdint>  // For int64_t
#include <memory>
#include <atomic>
#include <type_traits>

namespace qyh {
namespace ThreadSafeMsgQueue {

// Thread-safe global message ID generator with ODR-safe implementation
namespace detail {
    // Use a function-local static with external linkage to ensure single instance
    // This technique ensures ODR compliance in header-only libraries
    inline std::atomic<uint64_t>& getGlobalMsgIdCounter() {
        // The 'inline' keyword here ensures that even if this function is included
        // in multiple translation units, there will be only ONE instance of the
        // static variable across the entire program
        static std::atomic<uint64_t> global_msg_id_counter{0};
        return global_msg_id_counter;
    }
    
    // Wrapper function for better API
    inline uint64_t getNextMsgId() {
        return getGlobalMsgIdCounter().fetch_add(1, std::memory_order_relaxed);
    }
}

class BaseMsg;
using BaseMsgPtr = std::shared_ptr<BaseMsg>;

class BaseMsg : public std::enable_shared_from_this<BaseMsg>
{
public:
  BaseMsg(int _priority = 0) noexcept
    : priority(_priority)
    , timestamp(0)
    , msg_id_(detail::getNextMsgId())
  {
  }
  virtual ~BaseMsg() = default;

  // Non-copyable but movable for performance
  BaseMsg(const BaseMsg&) = delete;
  BaseMsg& operator=(const BaseMsg&) = delete;
  BaseMsg(BaseMsg&&) = default;
  BaseMsg& operator=(BaseMsg&&) = default;

  BaseMsgPtr shared_from_base()
  {
    return shared_from_this();
  }

  // Made const and noexcept with tie-breaking using message ID
  bool operator<(const BaseMsg &other) const noexcept
  {
    if (priority != other.priority)
      return priority < other.priority;
    if (timestamp != other.timestamp)
      return timestamp > other.timestamp;  // Older messages (smaller timestamp) have higher priority
    return msg_id_ > other.msg_id_;  // Use message ID as final tie-breaker
  }

  // Renamed and made noexcept
  void setTimestamp(int64_t _timestamp) noexcept
  {
    timestamp = _timestamp;
  }

  int getPriority() const noexcept { return priority; }
  int64_t getTimestamp() const noexcept { return timestamp; }
  uint64_t getMessageId() const noexcept { return msg_id_; }

protected:
  int priority;
  int64_t timestamp;
  uint64_t msg_id_;  // Unique message ID for deterministic ordering
};

struct BaseMsgPtrCompareLess
{
  // Made const-correct and noexcept
  bool operator()(const BaseMsgPtr &a, const BaseMsgPtr &b) const noexcept
  {
    return (*a) < (*b);
  }
};

template <typename MSG_CONTENT_TYPE>
class Msg : public BaseMsg
{
public:
  // Perfect forwarding constructor for optimal performance
  template<typename... Args>
  explicit Msg(int _priority, Args&&... args) noexcept(std::is_nothrow_constructible_v<MSG_CONTENT_TYPE, Args...>)
    : BaseMsg(_priority)
    , content(std::forward<Args>(args)...)
  {
  }

  // Convenience constructor with default priority
  template<typename... Args>
  explicit Msg(Args&&... args) noexcept(std::is_nothrow_constructible_v<MSG_CONTENT_TYPE, Args...>)
    : BaseMsg(0)
    , content(std::forward<Args>(args)...)
  {
  }

  ~Msg() override = default;

  // Performance: Return const reference to avoid copy
  const MSG_CONTENT_TYPE &getContent() const noexcept
  {
    return content;
  }

  // Allow mutable access when needed
  MSG_CONTENT_TYPE &getContent() noexcept
  {
    return content;
  }

  // Move accessor for one-time consumption
  MSG_CONTENT_TYPE moveContent() noexcept(std::is_nothrow_move_constructible_v<MSG_CONTENT_TYPE>)
  {
    return std::move(content);
  }

protected:
  MSG_CONTENT_TYPE content;
};

template <typename T>
using MsgPtr = std::shared_ptr<Msg<T>>;

// Helper function to create messages with perfect forwarding
template<typename T, typename... Args>
MsgPtr<T> make_msg(int priority, Args&&... args)
{
  return std::make_shared<Msg<T>>(priority, std::forward<Args>(args)...);
}

template<typename T, typename... Args>
MsgPtr<T> make_msg(Args&&... args)
{
  return std::make_shared<Msg<T>>(std::forward<Args>(args)...);
}

} // namespace ThreadSafeMsgQueue
} // namespace qyh