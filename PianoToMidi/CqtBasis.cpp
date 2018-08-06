#include "stdafx.h"
#include "CqtBasis.h"
#include "CqtBasisData.h"

using namespace std;

CqtBasis::CqtBasis(const int rate, const float fMin, const size_t nBins, const int octave,
	const int scale, const int hopLen, const CQT_WINDOW window)
	: data_(make_unique<CqtBasisData>(rate, fMin, nBins, octave, scale, hopLen, window))
{}

CqtBasis::~CqtBasis() {}

void CqtBasis::Calculate(const float sparsity) const { data_->Calculate(sparsity); }

const vector<vector<complex<float>>>& CqtBasis::GetCqtFilters() const { return data_->filts_; }