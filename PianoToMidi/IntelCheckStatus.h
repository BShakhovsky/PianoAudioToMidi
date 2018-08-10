#pragma once

#ifdef _DEBUG
	void CheckIPPresult(IppStatus status);
	void CheckMKLresult(sparse_status_t status);

#	define CHECK_IPP_RESULT(STATUS) CheckIPPresult(STATUS)
#	define CHECK_MKL_RESULT(STATUS) CheckMKLresult(STATUS)
#elif defined NDEBUG
#	define CHECK_IPP_RESULT(STATUS) STATUS
#	define CHECK_MKL_RESULT(STATUS) STATUS
#else
#	error Not debug, not release, then what is it?
#endif