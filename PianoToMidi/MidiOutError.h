#pragma once

class MidiOutError : public std::exception
{
public:
#pragma warning(suppress:4514)	// Unreferenced inline function has been removed
	explicit MidiOutError(const char* msg = "Unknown exception") noexcept : errMsg_(msg) {}
	virtual ~MidiOutError() override final = default;
	virtual const char* what() const override final { return errMsg_.c_str(); }
private:
	std::string errMsg_;
};