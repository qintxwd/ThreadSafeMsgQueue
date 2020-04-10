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
