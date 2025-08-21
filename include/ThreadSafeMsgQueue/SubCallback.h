#pragma once
#include "Msg.h"
#include <functional>
#include <memory>
#include <type_traits>
#include <vector>

namespace qyh {
namespace ThreadSafeMsgQueue {

class BaseSubCallback;
using BaseSubCallbackPtr = std::shared_ptr<BaseSubCallback>;

/**
 * @brief Base class for type-erased callbacks
 */
class BaseSubCallback : public std::enable_shared_from_this<BaseSubCallback>
{
public:
  BaseSubCallback() = default;
  virtual ~BaseSubCallback() = default;

  // Non-copyable but movable
  BaseSubCallback(const BaseSubCallback&) = delete;
  BaseSubCallback& operator=(const BaseSubCallback&) = delete;
  BaseSubCallback(BaseSubCallback&&) = default;
  BaseSubCallback& operator=(BaseSubCallback&&) = default;

  /**
   * @brief Type-erased callback invocation
   * @param msg Message to process
   * @return true if message was processed successfully
   */
  virtual bool call(const BaseMsgPtr &msg) = 0;

  /**
   * @brief Get the type info of the expected message type
   */
  virtual const std::type_info& getMessageType() const = 0;

  BaseSubCallbackPtr shared_from_base()
  {
    return shared_from_this();
  }
};

/**
 * @brief Type-safe callback wrapper
 */
template <typename T>
class SubCallback : public BaseSubCallback
{
public:
  using Callback = std::function<void(const MsgPtr<T> &)>;

  explicit SubCallback(Callback callback)
    : callback_(std::move(callback))
  {
    static_assert(!std::is_void_v<T>, "Message content type cannot be void");
  }

  ~SubCallback() override = default;

  bool call(const BaseMsgPtr &msg) override
  {
    // Use dynamic_pointer_cast for safety - allows for type checking
    if (auto typed_msg = std::dynamic_pointer_cast<Msg<T>>(msg)) {
      try {
        callback_(typed_msg);
        return true;
      } catch (...) {
        // Log error or handle exception as needed
        return false;
      }
    }
    return false;  // Wrong message type
  }

  const std::type_info& getMessageType() const override {
    return typeid(T);
  }

  /**
   * @brief Check if this callback can handle the given message type
   */
  template<typename U>
  bool canHandle() const {
    return std::is_same_v<T, U>;
  }

private:
  Callback callback_;
};

template <typename T>
using SubCallbackPtr = std::shared_ptr<SubCallback<T>>;

/**
 * @brief Factory function for creating callbacks with perfect forwarding
 */
template<typename T, typename Func>
SubCallbackPtr<T> makeCallback(Func&& func) {
  return std::make_shared<SubCallback<T>>(std::forward<Func>(func));
}

/**
 * @brief Utility class for chaining callbacks
 */
template<typename T>
class CallbackChain {
public:
  void addCallback(SubCallbackPtr<T> callback) {
    callbacks_.push_back(std::move(callback));
  }

  template<typename Func>
  void addCallback(Func&& func) {
    callbacks_.push_back(makeCallback<T>(std::forward<Func>(func)));
  }

  void call(const MsgPtr<T>& msg) {
    for (auto& callback : callbacks_) {
      if (callback) {
        callback->call(msg);
      }
    }
  }

  size_t size() const { return callbacks_.size(); }
  bool empty() const { return callbacks_.empty(); }
  void clear() { callbacks_.clear(); }

private:
  std::vector<SubCallbackPtr<T>> callbacks_;
};

} // namespace ThreadSafeMsgQueue
} // namespace qyh
