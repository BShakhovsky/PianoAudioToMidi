#include "stdafx.h"
#include "KerasRnn.h"
#include "KerasError.h"

using namespace std;
using namespace fdeep;

struct KerasData
{
	unique_ptr<model> rnn;
	string log;

	KerasData() = default;
	~KerasData();
private:
	KerasData(const KerasData&) = delete;
	const KerasData& operator=(const KerasData&) = delete;
};
KerasData::~KerasData() {} // 4710 Function not inlined

KerasRnn::KerasRnn(const string& fileName)
	: data_(make_unique<KerasData>())
{
	try
	{
		data_->rnn = make_unique<model>(load_model(fileName,
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
KerasRnn::~KerasRnn() {}

const string& KerasRnn::GetLog() const { return data_->log; }

vector<float> KerasRnn::Predict2D(const float* tensIn, const size_t nRows, const size_t nCols) const
{
	float_vec tensTrans(tensIn, tensIn + nRows * nCols);
	try
	{
		const auto result = data_->rnn->predict({ tensor5(shape5(1, 1, 1, nRows, nCols), move(tensTrans)) });
		assert(result.size() == 1 and "Result is 2D-vector instead of 1D");
		return move(*result.front().as_vector());
	}
	catch (const runtime_error& e) { throw KerasError(e.what()); }
}

vector<float> KerasRnn::PredictMulti(const float* mels, const size_t nRows, const size_t nCols, const float* ons, const float* offs, const size_t nOnOffCols) const
{
	try
	{
		const auto result(data_->rnn->predict_multi({ tensor5s({
			tensor5(shape5(1, 1, 1, nRows, nOnOffCols),	move(float_vec(ons,  ons  + nRows * nOnOffCols))),
			tensor5(shape5(1, 1, 1, nRows, nCols),		move(float_vec(mels, mels + nRows * nCols))),
			tensor5(shape5(1, 1, 1, nRows, nOnOffCols),	move(float_vec(offs, offs + nRows * nOnOffCols))) }) }, true));
		assert(result.size() == 1 and "Result is 2D-vector instead of 1D");
		return move(*result.front().front().as_vector());
	}
	catch (const runtime_error& e) { throw KerasError(e.what()); }
}