
#include "ThreadSafeMsgQueue.h"
#include <sstream>

template<typename T>
void onMsgSub(MsgPtr<T> msg) {
	std::cout << "thread id[" << std::this_thread::get_id() << "] subscribe "<< typeid(T).name() <<" msg=" << msg->getContent() << std::endl;
}

template<typename T>
void testPublish(ThreadSafeMsgQueuePtr tfmq, std::string topic,T t)
{
	std::cout << "thread id[" << std::this_thread::get_id() << "] public topic " << topic <<" "<<typeid(T).name()<<" msg="<<t<<std::endl;
	MsgPtr<T> mp(new Msg<T>(t));
	tfmq->publish<T>(topic, mp);
}


void testThreadPublishInt(ThreadSafeMsgQueuePtr tfmq, std::string topic)
{
	for (int i=0;;++i)
	{
		testPublish<int>(tfmq, topic, i);
		std::this_thread::sleep_for(std::chrono::milliseconds(20));
	}
}

void testThreadPublishDouble(ThreadSafeMsgQueuePtr tfmq, std::string topic)
{
	for (int i = 0;; ++i)
	{
		testPublish<double>(tfmq, topic, 0.1*i);
		std::this_thread::sleep_for(std::chrono::milliseconds(20));
	}
}

void testThreadPublishString(ThreadSafeMsgQueuePtr tfmq, std::string topic)
{
	for (int i = 0;; ++i)
	{
		std::stringstream ss;
		ss<<"str_" << i;
		testPublish<std::string>(tfmq, topic, ss.str());
		std::this_thread::sleep_for(std::chrono::milliseconds(20));
	}
}

template<typename T>
void testThreadSubscribe(ThreadSafeMsgQueuePtr tfmq,std::string topic)
{
	std::cout << "thread id[" << std::this_thread::get_id() << "] sub topic " << topic;
	tfmq->subscribe<T>(topic, onMsgSub<T>);
	tfmq->run();
}


void main()
{
	ThreadSafeMsgQueuePtr tfmq = ThreadSafeMsgQueue::getInstance();
	
	std::vector<std::thread> threads;
	//topic: topic_a
	//类型:string
	//订阅者：10个  
	//发布者：10个
	for (int i = 0; i < 10; ++i)
	{
		threads.push_back(std::thread(std::bind(testThreadSubscribe<std::string>, tfmq,"topic_a")));
		threads.push_back(std::thread(std::bind(testThreadPublishString, tfmq, "topic_a")));
	}

	//topic: topic_b
	//订阅者：3个   分别订阅int类型 double类型 string类型
	//发布者：15个  分3组，每5个分别发布int类型 double类型 string类型
	threads.push_back(std::thread(std::bind(testThreadSubscribe<std::string>, tfmq, "topic_b")));
	threads.push_back(std::thread(std::bind(testThreadSubscribe<int>, tfmq, "topic_b")));
	threads.push_back(std::thread(std::bind(testThreadSubscribe<double>, tfmq, "topic_b")));
	for (int i = 0; i < 5; ++i)
	{
		threads.push_back(std::thread(std::bind(testThreadPublishString, tfmq, "topic_b")));
	}
	for (int i = 0; i < 5; ++i)
	{
		threads.push_back(std::thread(std::bind(testThreadPublishDouble, tfmq, "topic_b")));
	}
	for (int i = 0; i < 5; ++i)
	{
		threads.push_back(std::thread(std::bind(testThreadPublishInt, tfmq, "topic_b")));
	}
	

	for (int i = 0; i < threads.size(); ++i)
		threads[i].join();
	


}