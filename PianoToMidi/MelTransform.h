#pragma once

class MelTransform
{
public:
	explicit MelTransform(const std::shared_ptr<AudioLoader>&, size_t rate = 22'050, size_t nMels = 128, float fMin = 0, float fMax = 0, bool htk = false,
		bool norm = true, size_t nFft = 2'048, int hopLen = 512, WIN_FUNC window = WIN_FUNC::HANN, PAD_MODE pad = PAD_MODE::MIRROR, float power = 2);
	~MelTransform();
	
#pragma warning(push)
#pragma warning(disable:4514) // Unreferenced inline function has been removed
	const std::shared_ptr<AlignedVector<float>>& GetMel() const { return mel_; }
	int GetHopLen() const { return hopLen_; }
	const std::string& GetLog() const { return log_; }

	const std::array<size_t, 88> & GetNoteIndices() const
	{
		assert(std::any_of(noteIndices_.cbegin(), noteIndices_.cend(), [](size_t i) { return i != 0; }) and "Note indices have not been calculated yet");
		return noteIndices_;
	}
	const std::array<size_t, 8> & GetOctaveIndices() const
	{
		assert(std::any_of(octaveIndices_.cbegin(), octaveIndices_.cend(), [](size_t i) { return i != 0; }) and "Octave indices have not been calculated yet");
		return octaveIndices_;
	}
#pragma warning(pop)
	void CalcOctaveIndices(bool AnotC = false); // A if true, C by default
private:
	void MelFilters(size_t rate, size_t nMels, float fMin, float fMax, bool htk, bool norm);
	void MelFreqs(size_t nMels, float fMin, float fMax, bool htk);
	void CalcNoteIndices();

	const int hopLen_;
#ifdef _WIN64
	const byte pad_[4]{ 0 };
#endif

	std::shared_ptr<AlignedVector<float>> mel_;
	std::vector<float> fftFreqs_, melFreqs_, melWeights_;
	std::string log_;

	std::array<size_t, 88> noteIndices_;
	std::array<size_t, 8> octaveIndices_;

	const MelTransform& operator=(const MelTransform&) = delete;
};