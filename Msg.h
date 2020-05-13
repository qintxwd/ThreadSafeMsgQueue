#pragma once

#include <memory>

class BaseMsg;
using BaseMsgPtr = std::shared_ptr<BaseMsg>;

class BaseMsg : public std::enable_shared_from_this<BaseMsg>
{
public:
	BaseMsg(int _priority = 0) : priority(_priority), timestamp(0) {}
	virtual ~BaseMsg() {}
	BaseMsgPtr shared_from_base()
	{
		return shared_from_this();
	}

	bool operator<(const BaseMsg &other)
	{
		if (priority == other.priority)
			return timestamp > other.timestamp;
		return priority < other.priority;
	}

	void settimestamp(int64_t _timestamp)
	{
		timestamp = _timestamp;
	}

protected:
	int priority;
	int64_t timestamp;
};

struct BaseMsgPtrCompareLess
{
	bool operator()(BaseMsgPtr a, BaseMsgPtr b)
	{
		return (*a) < (*b);
	}
};

template <typename MSG_CONTENT_TYPE>
class Msg : public BaseMsg
{
public:
	explicit Msg(const MSG_CONTENT_TYPE &content_, int _priority = 0) : BaseMsg(_priority),
																		content(content_)
	{
	}
	Msg(const Msg &msg) : content(msg.content)
	{
	}
	~Msg()
	{
	}
	MSG_CONTENT_TYPE getContent() { return content; }

protected:
	MSG_CONTENT_TYPE content;
};

template <typename T>
using MsgPtr = std::shared_ptr<Msg<T>>;