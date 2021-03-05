#pragma once

class KerasRnn
{
public:
	explicit KerasRnn(const std::string& fileName);
	const std::string& GetLog() const;
	~KerasRnn();

	fdeep::float_vec Predict2D(const float*, size_t nRows, size_t nColumns) const;
	fdeep::float_vec PredictMulti(const float* mels, size_t nRows, size_t nColumns, const float* onsets, const float* offsets, size_t nOnOffColumns) const;
private:
	const std::unique_ptr<struct KerasData> data_;

	KerasRnn(const KerasRnn&) = delete;
	const KerasRnn& operator=(const KerasRnn&) = delete;
};