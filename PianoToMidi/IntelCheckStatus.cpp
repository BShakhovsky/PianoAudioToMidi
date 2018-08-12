#include "stdafx.h"

#ifdef _DEBUG

#include "IntelCheckStatus.h"
#include "CqtError.h"

void CheckIPPresult(const IppStatus status)
{
	using std::string;

	string errMsg;
	switch (status)
	{
	case ippStsNoErr:																		break;
	case ippStsNullPtrErr:			errMsg = "Null pointer: ";								break;
	case ippStsSizeErr:				errMsg = "Length <= 0 (or <= 3, "
													"or dst len < src len + border): ";		break;
	case ippStsDivByZeroErr:		errMsg = "Division by value < min float number: ";		break;
	case ippStsDataTypeErr:			errMsg = "Data type not supported: ";					break;
	case ippStsNotSupportedModeErr:	errMsg = "Comparison mode not supported: ";				break;
	case ippStsDomain:				errMsg = "Argument is out of the function domain "
										"(at least one of the input elements <= 0: ";		break;
	case ippStsSingularity:			errMsg = "Argument is the singularity point "
										"(at least one of the input elements <= 0: ";		break;
	case ippStsThreshNegLevelErr:	errMsg = "Negative complex threshold: ";				break;

	case ippStsIIROrderErr:			errMsg = "IIR filter order <= 0: ";						break;
	case ippStsChannelErr:			errMsg = "IIR filter number of channels <= 0: ";		break;
	case ippStsContextMatchErr:		errMsg = "Incorrect IIR filter state identifier: ";		break;

	case ippStsStepErr:				errMsg = "Step <= 0: ";									break;
	case ippStsNotEvenStepErr:		errMsg = "One of the step values not divisible by 4 "
													"for floating-point images, "
													"or by 2 for short-integer images: ";	break;
	case ippStsMaskSizeErr:			errMsg = "Mask size has field <= 0 or even field: ";	break;
	case ippStsAnchorErr:			errMsg = "Anchor is outside the mask size: ";			break;
	case ippStsBorderErr:			errMsg = "Illegal border type: ";						break;
	case ippStsNumChannelsErr:		errMsg = "Illegal number of channels: ";				break;

	default:						errMsg = "Unknown error: ";
	}
	if (not errMsg.empty()) throw CqtError((errMsg + ippGetStatusString(status)).c_str());
}

void CheckMKLresult(sparse_status_t status)
{
	switch (status)
	{
	case SPARSE_STATUS_SUCCESS:										break;
	case SPARSE_STATUS_NOT_INITIALIZED:		assert(not
		"The routine encountered an empty handle or matrix array");	break;
	case SPARSE_STATUS_ALLOC_FAILED:		assert(not
		"Internal memory allocation failed");						break;
	case SPARSE_STATUS_INVALID_VALUE:		assert(not
		"The input parameters contain an invalid value");			break;
	case SPARSE_STATUS_EXECUTION_FAILED:	assert(not
		"Execution failed");										break;
	case SPARSE_STATUS_INTERNAL_ERROR:		assert(not
		"An error in algorithm implementation occurred");			break;
	case SPARSE_STATUS_NOT_SUPPORTED:		assert(not
		"The requested operation is not supported");				break;
	default:								assert(not
		"Unknown error");
	}
}

#endif