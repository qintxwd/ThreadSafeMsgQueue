#pragma once

#include <memory>

class BaseMsg;
using BaseMsgPtr = std::shared_ptr<BaseMsg>;

class BaseMsg : public std::enable_shared_from_this<BaseMsg>
{
public:
	BaseMsg() {}
	virtual ~BaseMsg() {}
	BaseMsgPtr shared_from_base() {
		return shared_from_this();
	}
};

template<typename MSG_CONTENT_TYPE>
class Msg : public BaseMsg
{
public:
	explicit Msg(const MSG_CONTENT_TYPE& content_) : BaseMsg(),
		content(content_)
	{
	}
	Msg(const Msg& msg) :content(msg.content) {
	}
	~Msg() {
	}
	MSG_CONTENT_TYPE getContent() { return content; }
protected:
	MSG_CONTENT_TYPE content;
};


template<typename T>
using MsgPtr = std::shared_ptr<Msg<T>>;