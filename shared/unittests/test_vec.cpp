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


		vec<uint8,3>	tv3;
		uint8			tb[16] = {1,2,3,};
		load(tv3, tb);

		load_s<decltype(tv3)>::load(tv3, tb);

		tv3 = tv3 + tv3;

		vec<uint32,9>	tv{1,2,3,4,5,6,7,8,9};
		uint32			ta[16] = {};
		store(tv, ta);

		auto	m = maskedi<0x55>(tv);
		store(m, ta);
	}

} _test_vec;
