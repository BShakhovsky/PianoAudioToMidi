#pragma once

class CqtError : public std::exception
{
public:
	explicit CqtError(const char* msg = "Unknown exception") noexcept : errMsg_(msg) {}
	virtual ~CqtError() override final = default;
	virtual const char* what() const override final { return errMsg_.c_str(); }
private:
	std::string errMsg_;
};