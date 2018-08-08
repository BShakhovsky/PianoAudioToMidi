#include "stdafx.h"
#include "ConstantQ.h"

using namespace std;

ConstantQ::ConstantQ(const vector<float>& rawAudio, const int rate, const int hopLen,
	const float fMin, const size_t nBins, const int octave, const float filtScale, const float norm,
	const float sparsity, const CQT_WINDOW window, const bool toScale, const bool isPadReflect)
{
	/* The recursive sub-sampling method described by
	Schoerkhuber, Christian, and Anssi Klapuri
	"Constant-Q transform toolbox for music processing."
	7th Sound and Music Computing Conference, Barcelona, Spain. 2010 */

	// How many octaves are we dealing with?
	const auto nOctaves(static_cast<int>(ceil(static_cast<float>(nBins) / octave))),
		nFilters(min(octave, static_cast<int>(nBins)));
	const auto lenOrig(rawAudio.size());

	// First thing, get the freqs of the top octave:
//	const auto freqs = cqt_frequencies(nBins, fMin, octave)[-octave:];
//	fmin_t = np.min(freqs)
//	fmax_t = np.max(freqs)

	// Determine required resampling quality:
//	const auto filterCutoff(fmax_t * (1 + .5 * filters.window_bandwidth(window) / Q));

	const auto nyquist(rate / 2.);
//	const auto resType(filterCutoff < audio.BW_FASTEST * nyquist ? "kaiser_fast" : "kaiser_best");

//	y, sr, hop_length = __early_downsample(y, sr, hop_length, res_type, n_octaves, nyquist, filter_cutoff, scale)

//	cqt_resp = []

//	if (resType != 'kaiser_fast')
	{
		// Do the top octave before resampling to allow for fast resampling
//		fft_basis, n_fft, _ = __cqt_filter_fft(rate, fmin_t, n_filters, octave, filtScale, norm, sparsity, window);

		// Compute the CQT filter response and append it to the stack
//		cqt_resp.append(__cqt_response(y, n_fft, hop_length, fft_basis, pad_mode))

//		fmin_t /= 2
//		fmax_t /= 2
//		n_octaves -= 1

//		filterCutoff = fmax_t * (1 + 0.5 * filters.window_bandwidth(window) / Q);

//		resType = 'kaiser_fast';
	}

	// Make sure our hop is long enough to support the bottom octave:
//	const auto num_twos = __num_two_factors(hop_length);
//	if (num_twos < n_octaves - 1) throw CqtError("Hop length must be a positive integer, ""multiple of 2^{0:d} for {1:d}-octave CQT".format(n_octaves - 1, n_octaves));

	// Now do the recursive bit
//	fft_basis, n_fft, _ = __cqt_filter_fft(rate, fmin_t, n_filters, octave, filtScale, norm, sparsity, window);

//	my_y, my_sr, my_hop = y, sr, hop_length

	// Iterate down the octaves
//	for i in range(n_octaves)
	{
		// Resample (except first time)
//		if (i > 0 and if len(my_y) < 2) throw CqtError("Input signal length={} is too short for ""{:d}-octave CQT".format(len_orig, n_octaves));

//		my_y = audio.resample(my_y, my_sr, my_sr / 2.0, res_type = res_type, scale = True);
		// The re-scale the filters to compensate for downsampling:
//		fft_basis[:] *= np.sqrt(2)

//		my_sr /= 2.0;
//		my_hop /= 2;

		// Compute the cqt filter response and append to the stack
//		cqt_resp.append(__cqt_response(my_y, n_fft, my_hop, fft_basis, pad_mode));
	}

//	C = __trim_stack(cqt_resp, n_bins);

	if (toScale)
	{
//		lengths = filters.constant_q_lengths(sr, fmin, n_bins = n_bins, bins_per_octave = bins_per_octave, window = window, filter_scale = filter_scale);
//		C /= np.sqrt(lengths[:, np.newaxis]);
	}

//	return C;

	UNREFERENCED_PARAMETER(rawAudio);
	UNREFERENCED_PARAMETER(rate);
	UNREFERENCED_PARAMETER(hopLen);
	UNREFERENCED_PARAMETER(fMin);
	UNREFERENCED_PARAMETER(nBins);
	UNREFERENCED_PARAMETER(octave);
	UNREFERENCED_PARAMETER(filtScale);
	UNREFERENCED_PARAMETER(norm);
	UNREFERENCED_PARAMETER(sparsity);
	UNREFERENCED_PARAMETER(window);
	UNREFERENCED_PARAMETER(toScale);
	UNREFERENCED_PARAMETER(isPadReflect);
}

ConstantQ::~ConstantQ()
{
}