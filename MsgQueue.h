#pragma once
#include "Msg.h"
#include <mutex>
#include <condition_variable>
#include <queue>

class MsgQueue;
using MsgQueuePtr = std::shared_ptr<MsgQueue>;

class MsgQueue : public std::enable_shared_from_this<MsgQueue>
{
public:
	MsgQueue()
	{
	}
	~MsgQueue()
	{
	}

	void enqueue(BaseMsgPtr msg)
	{
		std::lock_guard<std::mutex> lg(mtx);
		typedef std::chrono::time_point<std::chrono::system_clock, std::chrono::microseconds> MicroClock_Type;
        MicroClock_Type now = std::chrono::time_point_cast<std::chrono::microseconds>(std::chrono::system_clock::now());
        msg->settimestamp(now.time_since_epoch().count());
		msg_queue_.push(msg);
		cv.notify_all();
	}

	BaseMsgPtr dequeue()
	{
		std::lock_guard<std::mutex> lg(mtx);
		if (msg_queue_.empty())
			return nullptr;
		auto result = msg_queue_.top();
		msg_queue_.pop();
		return result;
	}

	BaseMsgPtr dequeue_block()
	{
		std::unique_lock<std::mutex> lg(mtx);
		cv.wait(lg, [&] { return !msg_queue_.empty(); });
		auto result = msg_queue_.top();
		msg_queue_.pop();
		return result;
	}

private:
	std::priority_queue<BaseMsgPtr, std::vector<BaseMsgPtr>, BaseMsgPtrCompareLess> msg_queue_;
	std::mutex mtx;
	std::condition_variable cv;
};
