#pragma once

struct CqtBasisData
{
	std::vector<std::vector<std::complex<float>>> filts_;

	explicit CqtBasisData(int rate, float fMin, size_t nBins, int octave,
		int scale, int hopLen, CqtBasis::CQT_WINDOW window);
	void Calculate(float sparsity);
private:
	void CalcFrequencies(float fMin, int octave, int scale);
	void CalcLengths();

	void CalcBufferPointers(size_t binIndex);
	void CalcTimeDomainFilter();
	void MultiplyWindow() const;
	void Normalize() const;
	void FilterFft();


	const int rate_;
	const CqtBasis::CQT_WINDOW window_;

	const float Q_;
#ifdef _WIN64
	const byte pad1[4] = { 0 };
#elif not defined _WIN32
#	error Either 32 or 64 bit Windows should be defined
#endif
	std::vector<float> freqs_, lens_; // Fractional lengths of each filter

	size_t nFft_;
	std::unique_ptr<juce::dsp::FFT> fft_;

	size_t bInd_, offset_;
	Ipp32fc* buff_;
	int size_;
#ifdef _WIN64
	const byte pad2[4] = { 0 };
#elif not defined _WIN32
#	error Either 32 or 64 bit Windows should be defined
#endif

	CqtBasisData(const CqtBasisData&) = delete;
	CqtBasisData operator=(const CqtBasisData&) = delete;
};