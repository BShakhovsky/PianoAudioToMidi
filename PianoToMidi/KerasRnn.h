#pragma once

class KerasRnn
{
public:
	explicit KerasRnn(const std::string& fileName);
	const std::string& GetLog() const;
	~KerasRnn();

	std::vector<float> Predict2D(const float*, size_t nRows, size_t nColumns) const;
	std::vector<float> PredictMulti(const float* mels, size_t nRows, size_t nColumns, const float* onsets, const float* offsets, size_t nOnOffColumns) const;
private:
	const std::unique_ptr<struct KerasData> data_;

	KerasRnn(const KerasRnn&) = delete;
	const KerasRnn& operator=(const KerasRnn&) = delete;
};