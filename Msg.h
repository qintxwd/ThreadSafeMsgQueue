#pragma once
#include "BaseMsg.h"
#include <string>


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