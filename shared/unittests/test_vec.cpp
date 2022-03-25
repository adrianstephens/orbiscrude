#include "base/vector.h"

using namespace iso;

struct test_vec {
	double	d;
	test_vec() {
		double4	x{7.,2.,3.,4.}, y{10.,20.,30.,40.};
		d = dot(x.yzw, y.xwy);
		auto	s2 = sort(x.xy);
		auto	s3 = sort(x.xyz);
		auto	s4 = sort(x);

	}

} _test_vec;
