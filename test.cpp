
#include "ThreadSafeMsgQueue.h"
#include <sstream>
#include <functional>

template <typename T>
void onMsgSub(MsgPtr<T> msg)
{
	std::cout << "thread id[" << std::this_thread::get_id() << "] subscribe callback "
			  << " msg= [" << msg->getContent() << "]" << std::endl;
}

template <typename T>
void testPublish(ThreadSafeMsgQueuePtr tfmq, std::string topic, T t, int priority = 0)
{
	std::cout << "thread id[" << std::this_thread::get_id() << "] public topic [" << topic << "] msg=[" << t << "]" << std::endl;
	MsgPtr<T> mp(new Msg<T>(t, priority));
	tfmq->publish<T>(topic, mp);
}

void testThreadPublishInt(ThreadSafeMsgQueuePtr tfmq, std::string topic)
{
	for (int i = 0;; ++i)
	{
		testPublish<int>(tfmq, topic, i);
		std::this_thread::sleep_for(std::chrono::milliseconds(20));
	}
}

void testThreadPublishDouble(ThreadSafeMsgQueuePtr tfmq, std::string topic)
{
	for (int i = 0;; ++i)
	{
		testPublish<double>(tfmq, topic, 0.1 * i);
		std::this_thread::sleep_for(std::chrono::milliseconds(20));
	}
}

void testThreadPublishString(ThreadSafeMsgQueuePtr tfmq, std::string topic)
{
	for (int i = 0;; ++i)
	{
		std::stringstream ss;
		ss << "str_" << i;
		testPublish<std::string>(tfmq, topic, ss.str());
		std::this_thread::sleep_for(std::chrono::milliseconds(20));
	}
}

void testThreadPublishPriorityString(ThreadSafeMsgQueuePtr tfmq, std::string topic)
{
	std::srand(std::time(0));
	for (int i = 0;; ++i)
	{
		std::stringstream ss;
		ss << "str_" << i;
		testPublish<std::string>(tfmq, topic, ss.str(), i);
		std::this_thread::sleep_for(std::chrono::milliseconds(20));
	}
}

template <typename T>
void testThreadSubscribe(ThreadSafeMsgQueuePtr tfmq, std::string topic)
{
	tfmq->subscribe<T>(topic, onMsgSub<T>);
	tfmq->run();
}

int main()
{
	ThreadSafeMsgQueuePtr tfmq = ThreadSafeMsgQueue::getInstance();

	std::vector<std::thread> threads;

	//test priority
	threads.push_back(std::thread(std::bind(testThreadPublishPriorityString, tfmq, "topic_a")));
	std::this_thread::sleep_for(std::chrono::seconds(2));
	threads.push_back(std::thread(std::bind(testThreadSubscribe<std::string>, tfmq, "topic_a")));

	//test thread safe
	// for (int i = 0; i < 10; ++i)
	// {
	// 	threads.push_back(std::thread(std::bind(testThreadSubscribe<std::string>, tfmq, "topic_a")));
	// 	threads.push_back(std::thread(std::bind(testThreadPublishString, tfmq, "topic_a")));
	// }

	//test different payload
	// threads.push_back(std::thread(std::bind(testThreadSubscribe<std::string>, tfmq, "topic_b")));
	// threads.push_back(std::thread(std::bind(testThreadSubscribe<int>, tfmq, "topic_b")));
	// threads.push_back(std::thread(std::bind(testThreadSubscribe<double>, tfmq, "topic_b")));
	// for (int i = 0; i < 5; ++i)
	// {
	// 	threads.push_back(std::thread(std::bind(testThreadPublishString, tfmq, "topic_b")));
	// }
	// for (int i = 0; i < 5; ++i)
	// {
	// 	threads.push_back(std::thread(std::bind(testThreadPublishDouble, tfmq, "topic_b")));
	// }
	// for (int i = 0; i < 5; ++i)
	// {
	// 	threads.push_back(std::thread(std::bind(testThreadPublishInt, tfmq, "topic_b")));
	// }

	for (int i = 0; i < threads.size(); ++i)
		threads[i].join();

	return 0;
}