#include "linq.h"
#include "base/tuple.h"

using namespace iso;
using namespace linq;

//-----------------------------------------------------------------------------
//	test
//-----------------------------------------------------------------------------
#if 0
namespace iso {
template<typename F, typename... P> struct deferred : deferred_mixin<deferred<F,P...>> {
	F			func;
	tuple<P...>	params;
	deferred(F &&f, P&& ...p) : func(move(f)), params(forward<P>(p)...) {}

	template<typename...X, size_t...J>	auto f(const index_list<J...>&, X&&... x) const {
		return func(params.get<J>()(forward<X>(x)...)...);
	}
	template<typename...X>	auto operator()(X&&... x) const {
		return f(make_index_list<sizeof...(P)>(), forward<X>(x)...);
	}
};
}
#endif
struct test_linq {
	test_linq() {
		tuple<>	test;

		ISO_OUTPUTF("test_linq\n");

		dynamic_array<int>	x = int_range<int>(100);

		auto	w2	= where([](int v) { return v % 3 == 0; })
					| where([](int v) { return v < 50; });

		auto	x2 = x | w2 | to_array();
//		auto	x0 = from(x) | transform([](int i) { return i / 10; }) | to_array();
		auto	x0 = from(x)
			| transform(
				op_param<0>() / scalar(10)
			)
			| to_array();
		/*
		ISO_OUTPUTF("test1\n");
		for (auto i : from(x) | where([](int v) { return v % 3 == 0; })) {
			ISO_TRACEF("i=") << i << '\n';
		}

		ISO_OUTPUTF("test2\n");
		for (auto i : from(x)
			| where([](int v) { return v % 3 == 0; })
			| skip(4)
			| where([](int v) { return v < 50; })
			| transform([](int v) { return v * 10; })
//			)
		) {
			ISO_TRACEF("i=") << i << '\n';
		}
		*/
		struct entry1 {
			string	a, b;
		};
		struct entry2 {
			string	a;
			int		b;
		};

		entry1	table1[] = {
			{"a",	"one"},
			{"b",	"two"},
			{"c",	"three"},
			{"d",	"four"},
			{"e",	"five"},
			{"f",	"six"},
			{"g",	"seven"},
		};
		entry2	table2[] = {
			{"g",	7},
			{"a",	1},
			{"b",	2},
			{"e",	5},
//			{"c",	3},
			{"d",	4},
			{"f",	6},
		};

//		auto	y = make_array(make_deferred(table1, field(&entry1::a)) == make_deferred(table2, field(&entry2::a)));
//		auto	y = make_array(make_deferred(table1, field(&entry1::a)) == make_deferred(table2, field(&entry2::a)));

		auto cat =
			from(table1) | transform([](const entry1 &i) { return i.a; })
			| concat(
				from(table2) | transform([](const entry2 &i) { return i.a; })
			)
			| first_or_default();

		auto j = to_array(from(table1) | join(table2,
			make_field(&entry1::a),										//[](const entry1 &i) { return i.a; },
			make_field(&entry2::a),										//[](const entry2 &i) { return i.a; },
			/*(op_param<1>()[make_field(&entry2::b)] + scalar(10)) / scalar(2)*/	[](const entry1 &a, const entry2 &b) { return b.b; }
		));

		ISO_OUTPUTF("test3\n");
//			dynamic_array<int>	x0 = transform(int_range<int>(100), [](int i) { return i / 10; });
		for (auto i : from(x0)) {// | distinct()) {
//		for (auto i : x0) {
			//	for (auto i : x | where([](int v) { return v % 3 == 0; })) {
			ISO_TRACEF("i=") << i << '\n';
		}
		auto	av = from(x0) | distinct() | average();

		ISO_OUTPUTF("test4\n");

		//for (auto i : x | take_while([](int i) { return i < 10; })) {
		for (auto i : from(x) | where([](int v) { return v % 3 == 0; }) | order_by([](int a, int b) { return a > b; })) {
		//for (auto i : from(x) | where([](int v) { return v % 3 == 0; }) | order_by([](int a, int b) { return a / 10 > b / 10; }) | then_by([](int a, int b) { return a % 10 < b % 10; })) {
			ISO_TRACEF("i=") << i << '\n';
		}

		//int	t = at_or_default(from(x) | where([](int v) { return v % 7 == 0; }), 7);
	}
} _test_linq;
