#pragma once

#define BORIS_ERROR(NAME) class NAME##Error : public std::exception { public:				\
	__pragma(warning(suppress:4514)) /* Unreferenced inline function has been removed */	\
	explicit NAME##Error(const char* msg = "Unknown exception") noexcept : errMsg_(msg) {}	\
	virtual ~##NAME##Error() override final = default;										\
	virtual const char* what() const override final { return errMsg_.c_str(); }				\
	private: std::string errMsg_; };