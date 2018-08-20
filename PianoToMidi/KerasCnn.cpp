#include "stdafx.h"
#include "KerasCnn.h"
#include "KerasError.h"

using namespace std;
using namespace fdeep;

struct KerasData
{
	unique_ptr<model> cnn;
	string log;

	KerasData() = default;
	~KerasData();
private:
	KerasData(const KerasData&) = delete;
	const KerasData& operator=(const KerasData&) = delete;
};
KerasData::~KerasData() {} // 4710 Function not inlined

KerasCnn::KerasCnn(const string& fileName)
	: data_(make_unique<KerasData>())
{
	try
	{
		data_->cnn = make_unique<model>(load_model(fileName,
#ifdef _DEBUG
			true,
#else
			false,
#endif
			[this](const string& msg) { data_->log += msg; }
		));
	}
	catch (const runtime_error& e) { throw KerasError(e.what()); }
}

#pragma warning(suppress:4710 4711) // Function selected for automatic inline expansion and not inlined :)
KerasCnn::~KerasCnn() {}

const string& KerasCnn::GetLog() const { return data_->log; }

vector<float> KerasCnn::Predict2D(const float* tensIn, const size_t nRows, const size_t nCols) const
{
	using internal::float_vec;

	float_vec tensTrans(tensIn, tensIn + nRows * nCols);
	MKL_Simatcopy('R', 'T', nRows, nCols, 1, tensTrans.data(), nCols, nRows);
	try
	{
		const auto result = data_->cnn->predict({ tensor3(shape3(nCols, 1, nRows), move(tensTrans)) });
		assert(result.size() == 1 and "Result is 2D-vector instead of 1D");
		return move(*result.front().as_vector());
	}
	catch (const runtime_error& e) { throw KerasError(e.what()); }
}