#include "stdafx.h"

#include "AlignedVector.h"
#include "EnumFuncs.h"
#include "AudioLoader.h"

#include "MelTransform.h"
#include "MelError.h"

#include "ShortTimeFourier.h"
#include "IntelCheckStatus.h"

using namespace std;

MelTransform::MelTransform(const shared_ptr<AudioLoader>& audio, const size_t rate, const size_t nMels, const float fMin, const float fMax,
	const bool htk, const bool norm, const size_t nFft, const int hopLen, const WIN_FUNC window, const PAD_MODE pad, const float power)
	: hopLen_(hopLen),
	mel_(make_shared<AlignedVector<float>>())
{
	assert(power > 0 and "Power must be positive (e.g. 1 for energy, 2 for power, etc.)");
	assert(audio->GetBytesPerSample() == sizeof(float) and "Raw audio data is assumed to be in float-format before calculating MEL-spectrogram");
	ShortTimeFourier stft(nFft, window, pad);
	stft.RealForward(reinterpret_cast<float*>(audio->GetRawData()), audio->GetNumSamples(), hopLen);

	AlignedVector<float> stftData(stft.GetSTFT().size());
	CHECK_IPP_RESULT(ippsMagnitude_32fc(reinterpret_cast<const Ipp32fc*>(stft.GetSTFT().data()), stftData.data(), static_cast<int>(stftData.size())));

	if (power != 1) CHECK_IPP_RESULT(power == 2 ? ippsSqr_32f(stftData.data(), stftData.data(), static_cast<int>(stftData.size()))
		: ippsPowx_32f_A11(stftData.data(), power, stftData.data(), static_cast<Ipp32s>(stftData.size())));

	assert(fftFreqs_.empty() and "Fft frequencies should not have been calculated until here");
	fftFreqs_.resize(1 + nFft / 2); // Center freqs of each FFT bin
//	const auto delta(static_cast<float>(rate / 2. / (fftFreqs_.size() - 1)));
//	for (auto iter(next(fftFreqs_.begin())); iter != fftFreqs_.cend(); ++iter)* iter = *prev(iter) + delta;
	for (size_t i(0); i < fftFreqs_.size(); ++i) fftFreqs_.at(i) = static_cast<float>(i * (rate / 2.) / (fftFreqs_.size() - 1));

	MelFilters(rate, nMels, fMin, fMax, htk, norm);
	mel_->resize(nMels * stft.GetNumFrames());
	cblas_sgemm(CblasRowMajor, CblasNoTrans, CblasNoTrans, static_cast<int>(nMels), static_cast<int>(stft.GetNumFrames()), static_cast<int>(1 + nFft / 2),
		1, melWeights_.data(), static_cast<int>(1 + nFft / 2),
		stftData.data(), static_cast<int>(stft.GetNumFrames()), 0, mel_->data(), static_cast<int>(stft.GetNumFrames()));
	fftFreqs_.clear();
	assert(not melWeights_.empty() and "Mel filters should have already been calculated");
	melWeights_.clear();

	MKL_Simatcopy('R', 'T', nMels, stft.GetNumFrames(),
		1, mel_->data(), stft.GetNumFrames(), nMels);

	CalcNoteIndices();
	assert(not melFreqs_.empty() and "Mel frequencies should have already been calculated");
	melFreqs_.clear();
}
MelTransform::~MelTransform() {}; // C4710 Function not inlined

float HzToMel(const float freq, const bool htk)
{
	if (htk) return 2'595 * log10(1 + freq / 700);

	const auto fSp(200.f / 3), minLogHz(1'000.f);					// Linear region scale, and beginning of log region (Hz)
	return freq < minLogHz ? freq / fSp								// Linear region
		: minLogHz / fSp + log(freq / minLogHz) / (log(6.4f) / 27);	// Beginning of log region (Mels) + Log / Log region step size
}

void MelTransform::MelFreqs(const size_t nMels, const float fMin, const float fMax, const bool htk)
{
	/*	The mel scale is a quasi-logarithmic function of acoustic frequency designed such that
		perceptually similar pitch intervals(e.g.octaves) appear equal in width over the full hearing range.

		Because the definition of the mel scale is conditioned by a finite number of subjective psychoaoustical experiments,
		several implementations coexist in the audio signal processing literature
			(Umesh, S., Cohen, L., &Nelson, D.Fitting the mel scale.
			In Proc.International Conference on Acoustics, Speech, and Signal Processing
			(ICASSP), vol. 1, pp. 217 - 220, 1998).

		By default, librosa replicates the behavior of the well-established MATLAB Auditory Toolbox of Slaney
			(Slaney, M.Auditory Toolbox: A MATLAB Toolbox for Auditory Modeling Work.
			Technical Report, version 2, Interval Research Corporation, 1998).
		According to this default implementation, the conversion from Hertz to mel is linear below 1 kHz and logarithmic above 1 kHz.

		Another available implementation replicates the Hidden Markov Toolkit (HTK) according to the following formula:
			mel = 2595 * log10(1 + f / 700)
			(Young, S., Evermann, G., Gales, M., Hain, T., Kershaw, D., Liu, X.,
			Moore, G., Odell, J., Ollason, D., Povey, D., Valtchev, V., &Woodland, P.
			The HTK book, version 3.4.Cambridge University, March 2009). */


	const auto melMin(HzToMel(fMin, htk)), melMax(HzToMel(fMax, htk));		// Uniformly spaced 'center freqs' of mel bands:

	assert(melFreqs_.empty() and "Mel frequencies have been calculated twice");
	melFreqs_.resize(nMels);
	for (size_t i(0); i < melFreqs_.size(); ++i) melFreqs_.at(i) = static_cast<float>(melMin + i * (melMax - melMin) / (melFreqs_.size() - 1));

	// Mel to Hz:
	if (htk) transform(melFreqs_.cbegin(), melFreqs_.cend(), melFreqs_.begin(), [](const float m) { return 700 * (pow(10, m / 2'595) - 1); });
	else
	{
		const auto fSp(200. / 3), minLogHz(1'000.), minLogMel(minLogHz / fSp);	// Linear region scale, beginning of log region (Hz), same (Mels)
		transform(melFreqs_.cbegin(), melFreqs_.cend(), melFreqs_.begin(), [fSp, minLogMel, minLogHz](const float m)
			{
				return static_cast<float>(m < minLogMel ? fSp * m				// Linear region
					: minLogHz * exp(log(6.4) / 27 * (m - minLogMel)));			// MelsStart * exp(Log region step size * (m - melsStart))
			});
	}
}

void MelTransform::MelFilters(const size_t rate, const size_t nMels, const float fMin, const float fMax, const bool htk, const bool norm)
{
	// Filterbank matrix to combine FFT bins into Mel-frequency bins

	using placeholders::_1;

	MelFreqs(nMels + 2, fMin, fMax ? fMax : rate / 2.f, htk);	// 'Center freqs' of mel bands - uniformly spaced between limits
	assert(not melFreqs_.empty() and "Mel frequencies should have already been calculated");
	auto fDiff(melFreqs_);
	adjacent_difference(melFreqs_.cbegin(), melFreqs_.cend(), fDiff.begin());

	assert(not fftFreqs_.empty() and "Fft frequencies should have already been calculated");
	vector<vector<float>> ramps(melFreqs_.size(), vector<float>(fftFreqs_.size()));
	for (size_t i(0); i < ramps.size(); ++i) transform(fftFreqs_.cbegin(), fftFreqs_.cend(), ramps.at(i).begin(), bind(minus<float>(), melFreqs_.at(i), _1));

	assert(melWeights_.empty() and "Mel filters have been calculated twice");
	melWeights_.resize(nMels * fftFreqs_.size());
	for (size_t i(0); i < nMels; ++i)
	{
		auto lower(ramps.at(i)), upper(ramps.at(i + 2)); // Lower & upper slopes for all bins, then intersect them with each other and zero:
		transform(lower.cbegin(), lower.cend(), upper.cbegin(), melWeights_.begin() + static_cast<ptrdiff_t>(i * fftFreqs_.size()),
			[&fDiff, i](const float lwr, const float upr) { return max(0.f, min(-lwr / fDiff.at(i + 1), upr / fDiff.at(i + 2))); });
	}

	if (norm)
	{
		// Slaney-style mel is scaled to be approx constant energy per channel
		// Divide the triangular mel weights by the width of the mel band (area normalization)
		vector<float> eNorm(nMels);
		transform(melFreqs_.cbegin() + 2, melFreqs_.cend(), melFreqs_.cbegin(), eNorm.begin(), [](const float m2, const float m1) { return 2 / (m2 - m1); });
		for (size_t i(0); i < nMels; ++i) transform(melWeights_.cbegin() + static_cast<ptrdiff_t>(i * fftFreqs_.size()), melWeights_.cbegin() + static_cast<ptrdiff_t>((i + 1) * fftFreqs_.size()),
			melWeights_.begin() + static_cast<ptrdiff_t>(i * fftFreqs_.size()), bind(multiplies<float>(), eNorm.at(i), _1));
	}
	// Otherwise, leave all the triangles aiming for a peak value of 1.0

	// Only check weights if melFreqs.at(0) is positive
	for (size_t i(0); i < nMels; ++i) if (melFreqs_.at(i) != 0 and *max_element(melWeights_.cbegin() + static_cast<ptrdiff_t>(i * fftFreqs_.size()),
		melWeights_.cbegin() + static_cast<ptrdiff_t>((i + 1) * fftFreqs_.size())) <= 0)
	{
		log_ += "Empty filters detected in mel frequency basis.\n";
		log_ += "Some channels will produce empty responses.\n";
		log_ += "Try increasing your sampling rate (and fmax) or reducing n_mels.\n\n";
		break;
	}
}


void MelTransform::CalcNoteIndices()
{
	assert(all_of(noteIndices_.cbegin(), noteIndices_.cend(), [](size_t i) { return i == 0; }) and "Note indices have been calculated twice");
	assert(all_of(octaveIndices_.cbegin(), octaveIndices_.cend(), [](size_t i) { return i == 0; }) and "Octave indices have been calculated before note indices");
	assert(not melFreqs_.empty() and "Fft frequencies should have already been calculated");

	auto rightIter(melFreqs_.cend());
	for (auto i(noteIndices_.size()); i; --i)
	{
		const auto freq(440 * pow(2.f, (static_cast<float>(i) - 1 + 21 - 69) / 12)); // Note + 21 --> Midi --> Hz
		rightIter = upper_bound(melFreqs_.cbegin(), rightIter, freq);
		
		if (rightIter == melFreqs_.cbegin()) break; // all other indices left as zeros

		noteIndices_.at(i - 1) = static_cast<size_t>(rightIter - melFreqs_.cbegin()) - (freq - *prev(rightIter) < *rightIter - freq ? 1 : 0);
	}

	Sleep(10);
}

void MelTransform::CalcOctaveIndices(const bool AnotC)
{
	assert(all_of(octaveIndices_.cbegin(), octaveIndices_.cend(), [](size_t i) { return i == 0; }) and "Octave indices have been calculated twice");
	assert(any_of(noteIndices_.cbegin(), noteIndices_.cend(), [](size_t i) { return i != 0; }) and "Note indices have not been calculated yet");
	for (size_t i(0); i < octaveIndices_.size(); ++i) octaveIndices_.at(i) = noteIndices_.at(12 * i + (AnotC ? 0 : 3));
}