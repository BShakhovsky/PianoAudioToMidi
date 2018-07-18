#pragma once

class FFmpegError : public std::exception
{
public:
	explicit FFmpegError(const char* msg = "Unknown exception") noexcept;
	virtual ~FFmpegError() override final = default;
	virtual const char* what() const override final { return errMsg_.c_str(); }
private:
	std::string errMsg_;
};