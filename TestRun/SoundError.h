#pragma once

class SoundError : public std::exception
{
public:
#pragma warning(push)
#pragma warning(disable: 4514)	// Unreferenced inline function has been removed
	explicit SoundError(const char* msg = "Unknown exception") noexcept : errMsg_(msg) {}
#pragma warning(pop)
	virtual ~SoundError() override final = default;
	virtual const char* what() const override final { return errMsg_.c_str(); }
private:
	std::string errMsg_;
};