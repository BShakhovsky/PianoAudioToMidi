#include "stdafx.h"
#include "AlignedVector.h"
#include "CqtBasis.h"
#include "CqtBasisData.h"

using namespace std;

CqtBasis::CqtBasis(const int rate, const float fMin, const size_t nBins, const int octave,
	const int scale, const int hopLen, const CQT_WINDOW window)
	: data_(make_unique<CqtBasisData>(rate, fMin, nBins, octave, scale, hopLen, window))
{}

#pragma warning(push)
#pragma warning(disable:4710) // Function not inlined
CqtBasis::~CqtBasis() {}
#pragma warning(pop)

void CqtBasis::Calculate(const float sparsity) const { data_->Calculate(sparsity); }

const vector<vector<complex<float>>>& CqtBasis::GetCqtFilters() const { return data_->filts_; }