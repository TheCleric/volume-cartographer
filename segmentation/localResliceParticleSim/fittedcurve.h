#pragma once

#ifndef _VOLCART_SEGMENTATION_FITTED_CURVE_H_
#define _VOLCART_SEGMENTATION_FITTED_CURVE_H_

#include "spline.h"
#include <vector>
#include <cassert>

namespace volcart {

namespace segmentation {

template <typename Scalar>
class FittedCurve {
private:	
	CubicSpline<Scalar> spline_;
	size_t npoints_;
	std::vector<Scalar> resampledPoints_;

public:
	enum class DerivativeMethod
	{
		CENTRAL_DIFFERENCE,
		FIVE_POINT_STENCIL
	};

	using ScalarVector = std::vector<Scalar>;

	FittedCurve() = default;

	FittedCurve(const ScalarVector& xs, const ScalarVector& ys) :
		spline_(xs, ys),
   		npoints_(xs.size())	{}

	FittedCurve(const ScalarVector& xs, const ScalarVector& ys, size_t npoints) :
		spline_(xs, ys),
		npoints_(npoints)
	{
		resample(npoints);
	}

	const decltype(spline_)& spline() const { return spline_; }

	std::vector<Pixel> resampledPoints() const
	{
		std::vector<Pixel> rs;
		rs.reserve(npoints_);
		std::transform(resampledPoints_.begin(), resampledPoints_.end(),
				std::back_inserter(rs),
				[=](Scalar x) { return spline_.eval(x); });
		return rs;
	}

	Pixel eval(Scalar t) const { return spline_.eval(t); }

	void resample(size_t npoints)
	{
		resampledPoints_.clear();
		resampledPoints_.resize(npoints, 0.0);

		// New interval in t-space
		Scalar newTInterval = 1.0 / npoints;

		// Calculate new knot positions in t-space
		Scalar sum = 0;
		for (size_t i = 0; i < npoints && sum <= 1; ++i) {
			resampledPoints_[i] = sum;
			sum += newTInterval;
		}
	}
	
	std::vector<Scalar> deriv() const
	{
		std::vector<Scalar> ps;
		ps.reserve(npoints_);
		for (uint32_t i = 0; i < npoints_; ++i) {
			if (i == 0) {
				ps.push_back(derivForewardDifference(0)(1));
			} else if (i == 1) {
				ps.push_back(derivCentralDifference(1)(1));
			} else if (i == npoints_ - 2) {
				ps.push_back(derivCentralDifference(npoints_-2)(1));
			} else if (i == npoints_ - 1) {
				ps.push_back(derivBackwardDifference(npoints_-1)(1));
			} else {
				ps.push_back(derivFivePointStencil(i)(1));
			}
		}
		return ps;
	}

	Pixel derivCentralDifference(uint32_t index, uint32_t hstep=1) const
	{
		assert(index >= 1 && index <= resampledPoints_.size()-1 &&
				"index must not be an endpoint\n");
		Scalar before = resampledPoints_[index - hstep];
		Scalar after = resampledPoints_[index + hstep];
		return spline_.eval(after) - spline_.eval(before);
	}

	Pixel derivBackwardDifference(uint32_t index, uint32_t hstep=1) const
	{
		assert(index >= 1 && "index must not be first point\n");
		Scalar current = resampledPoints_[index];
		Scalar before  = resampledPoints_[index - hstep];
		return spline_.eval(current) - spline_.eval(before);
	}

	Pixel derivForewardDifference(uint32_t index, uint32_t hstep=1) const
	{
		assert(index <= resampledPoints_.size()-1 && "index must not be last point\n");
		Scalar current = resampledPoints_[index];
		Scalar after  = resampledPoints_[index + hstep];
		return spline_.eval(after) - spline_.eval(current);
	}

	Pixel derivFivePointStencil(uint32_t index, uint32_t hstep=1) const
	{
		assert(index >= 2 && index <= resampledPoints_.size()-2 &&
				"index must not be first/last two points\n");
		Scalar before2 = resampledPoints_[index - (2 * hstep)];
		Scalar before1 = resampledPoints_[index - hstep];
		Scalar after1  = resampledPoints_[index + hstep];
		Scalar after2  = resampledPoints_[index + (2 * hstep)];
		return -spline_.eval(after2) +
			   8 * spline_.eval(after1) -
			   8 * spline_.eval(before1) +
			   spline_.eval(before2);
	}

};

}

}

#endif
