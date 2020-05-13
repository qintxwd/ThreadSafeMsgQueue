// ThreadSafeMsgQueue.h: 标准系统包含文件的包含文件
// 或项目特定的包含文件。

#pragma once

#include <iostream>
#include <map>
#include <list>
#include <thread>
#include <chrono>
#include "MsgQueue.h"
#include "SubCallback.h"

class ThreadSafeMsgQueue;
using ThreadSafeMsgQueuePtr = std::shared_ptr< ThreadSafeMsgQueue>;

class ThreadSafeMsgQueue
{
public:
	static ThreadSafeMsgQueuePtr getInstance()
	{
		static ThreadSafeMsgQueuePtr instance_ptr = nullptr;
		if (instance_ptr == nullptr) {
			instance_ptr.reset(new ThreadSafeMsgQueue());
		}
		return instance_ptr;
	}
	~ThreadSafeMsgQueue()
	{

	}

	template<typename MSG_TYPE>
	void publish(std::string topic, MsgPtr<MSG_TYPE> msg_ptr)
	{
		std::lock_guard<std::mutex> lg(mtx);
		if (msg_queues.find(topic) == msg_queues.end()) {
			msg_queues[topic].reset(new MsgQueue());
		}
		msg_queues[topic]->enqueue(msg_ptr->shared_from_base());
	}

	template<typename MSG_TYPE>
	void subscribe(std::string topic, std::function<void(const MsgPtr<MSG_TYPE> msg)> callback)
	{
		std::lock_guard<std::mutex> lg(mtx);
		SubCallbackPtr<MSG_TYPE> callback_ptr(new SubCallback<MSG_TYPE>(callback));
		msg_callbacks[topic].push_back(callback_ptr);
	}

	void run()
	{
		while (true) {
			if (runOnce())
			{
				continue;
			}else
			{
				std::this_thread::sleep_for(std::chrono::milliseconds(20));
			}
		}
	}

	bool runOnce()
	{
		bool busy = false;
		std::lock_guard<std::mutex> lg(mtx);
		for (auto itr = msg_queues.begin(); itr != msg_queues.end(); ++itr)
		{
			BaseMsgPtr msg_ptr = itr->second->dequeue();
			if (msg_ptr) {
				busy = true;
				if (msg_callbacks.find(itr->first) != msg_callbacks.end()) {
					for (auto pos = msg_callbacks[itr->first].begin(); pos != msg_callbacks[itr->first].end(); ++pos)
					{						
						(*pos)->call(msg_ptr);
					}
				}
			}
		}
		return busy;
	}

private:
	ThreadSafeMsgQueue()
	{
		MsgQueuePtr default_queue(new MsgQueue());
		msg_queues[""] = default_queue;
	}

private:
	std::mutex mtx;
	std::map<std::string, MsgQueuePtr> msg_queues;
	std::map<std::string, std::list<BaseSubCallbackPtr> > msg_callbacks;
};

