#include "stdafx.h"
#include "ShortTimeFourier.h"
#include "StftData.h"

using namespace std;

ShortTimeFourier::ShortTimeFourier(const vector<float>& rawAudio,
	const int frameLen, const int hopLen, const STFT_WINDOW window, const bool isPadReflected)
	: data_(make_unique<StftData>(rawAudio, frameLen, hopLen, window, isPadReflected))
{}

#pragma warning(push)
#pragma warning(disable:4710) // Function not inlined
ShortTimeFourier::~ShortTimeFourier() {}
#pragma warning(pop)

vector<vector<complex<float>>> ShortTimeFourier::RealForward() const
{
	// Vertical stride = 1 sample, horizontal stride = hop length, the end may get truncated:
	vector<vector<complex<float>>> result(1 + (
		data_->paddedBuff_.size() - data_->frameLen_) / data_->hopLen_,
		// FFT will write here half + 1 complex numbers:
		vector<complex<float>>(1ull + data_->frameLen_ / 2));

	for (size_t i(0); i < result.size(); ++i)
	{
		// Temporarily window the time series into buffer with different type (complex instead of float)
		// FFT will overwrite result on top:
		CopyMemory(result.at(i).data(), data_->paddedBuff_.data()
			+ static_cast<ptrdiff_t>(i) * data_->hopLen_,
			// The last two floats (one complex) is currently empty:
			data_->frameLen_ * sizeof data_->paddedBuff_.front());

		data_->window_->multiplyWithWindowingTable(reinterpret_cast<float*>(
			result.at(i).data()), result.at(i).size() * 2);
		// Conjugate to match phase from DPWE code:
		data_->fft_.performRealOnlyForwardTransform(reinterpret_cast<float*>(result.at(i).data()), true);
	}

	return move(result);
}