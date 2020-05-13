#pragma once
#include <memory>
#include <functional>
#include "Msg.h"

class BaseSubCallback;
using BaseSubCallbackPtr = std::shared_ptr<BaseSubCallback>;


class BaseSubCallback : public std::enable_shared_from_this<BaseSubCallback>
{
public:
	BaseSubCallback() {}
	~BaseSubCallback() {}

	virtual void call(const BaseMsgPtr msg)  = 0;

	BaseSubCallbackPtr shared_from_base() {
		return shared_from_this();
	}
private:

};




template<typename T>
class SubCallback : public BaseSubCallback
{
public:
	typedef std::function<void(const MsgPtr<T>)> Callback;

	SubCallback(const Callback &callback_) :
		callback(callback_)
	{
	}
	~SubCallback()
	{
	}
	virtual void call(BaseMsgPtr msg)
	{
		auto mptr = std::dynamic_pointer_cast<Msg<T>>(msg);
		if (mptr) {
			callback(mptr);
		}
	}

private:
	Callback callback;
};

template<typename T>
using SubCallbackPtr = std::shared_ptr<SubCallback<T>>;
