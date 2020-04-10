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
		msg_queue_.push(msg);
		cv.notify_all();
	}

	BaseMsgPtr dequeue()
	{
		std::lock_guard<std::mutex> lg(mtx);
		if (msg_queue_.empty())return nullptr;
		auto result = msg_queue_.front();
		msg_queue_.pop();
		return result;
	}

	BaseMsgPtr dequeue_block()
	{
		std::unique_lock<std::mutex> lg(mtx);
		cv.wait(lg, [&] {return !msg_queue_.empty(); });
		auto result = msg_queue_.front();
		msg_queue_.pop();
		return result;
	}


private:
	std::queue<BaseMsgPtr> msg_queue_;
	std::mutex mtx;
	std::condition_variable cv;
};
