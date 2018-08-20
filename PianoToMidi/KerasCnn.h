#pragma once

class KerasCnn
{
public:
	explicit KerasCnn(const std::string& fileName);
	const std::string& GetLog() const;
	~KerasCnn();

	std::vector<float> Predict2D(const float*, size_t nRows, size_t nColumns) const;
private:
	const std::unique_ptr<struct KerasData> data_;

	KerasCnn(const KerasCnn&) = delete;
	const KerasCnn& operator=(const KerasCnn&) = delete;
};