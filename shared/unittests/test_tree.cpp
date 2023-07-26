#include "base/tree.h"
#include "base/strings.h"
#include "base/array.h"
#include "extra/random.h"
#include "events.h"

using namespace iso;

struct test_tree : Handles2<test_tree, AppEvent> {
	struct intnode : e_treenode<intnode, false> {
		int	i;
		intnode(int i) : i(i) {}
		operator int() const { return i; }
	};
	struct rb_intnode : e_rbnode<rb_intnode, false> {
		int	i;
		rb_intnode(int i) : i(i) {}
		operator int() const { return i; }
	};
	struct rb_floatnode : e_rbnode<rb_floatnode, false> {
		float	i;
		rb_floatnode(float i) : i(i) {}
		operator float() const { return i; }
	};
	struct rb_floatnodep : e_rbnode<rb_floatnodep, true> {
		float	i;
		rb_floatnodep(float i) : i(i) {}
		operator float() const { return i; }
	};

	void	operator()(AppEvent *ev) {
		if (ev->state != AppEvent::BEGIN)
			return;

		auto	less2 = [](int a, int b) { return a < b; };

		e_tree<intnode>						u0;
		e_tree<intnode, decltype(less2)&>	u1(less2);

		map<int,int>						m;
		map<int,int, less, true>			mp;
		multimap<int,int>					mm;
		multimap<int,int,less,true>			mmp;

		set<int>							s;
		multiset<int>						sm;

		ISO_ASSERT(sizeof(u0) == 8);
		ISO_ASSERT(sizeof(u1) == 16);

		random = 42;

		dynamic_array<int>		ints(1000);
		random.fill(ints, 1000);
		for (auto &j : ints) {
			auto	i = ints.index_of(j);
			u0.insert(new intnode(j));
			u1.insert(new intnode(j));
			m.put(j, i);			//ISO_ASSERT(validate(m));
			mp.put(j, i);			//ISO_ASSERT(validate(mp));
			mm.put(j, i);
			mmp.put(j, i);

			s.insert(j);
			sm.insert(j);

		}

		ISO_ASSERT(validate(u0));
		ISO_ASSERT(validate(u1));
		ISO_ASSERT(validate(m));
		ISO_ASSERT(validate(mp));
		ISO_ASSERT(validate(mm));
		ISO_ASSERT(validate(mmp));
		ISO_ASSERT(validate(s));
		ISO_ASSERT(validate(sm));

		for (auto j : ints) {
			u0.remove(u0.find(j));
			u1.remove(u1.find(j));
			m.remove(m.find(j));	//ISO_ASSERT(validate(m));
			mp.remove(mp.find(j));	//ISO_ASSERT(validate(mp));
			mm.remove(mm.find(j));	//ISO_ASSERT(validate(mm));
			mmp.remove(mmp.find(j));//ISO_ASSERT(validate(mmp));
		}

		ISO_ASSERT(validate(u0));
		ISO_ASSERT(validate(u1));
		ISO_ASSERT(validate(m));
		ISO_ASSERT(validate(mp));
		ISO_ASSERT(validate(mm));
		ISO_ASSERT(validate(mmp));

		ISO_ASSERT((s & s) == s);
		ISO_ASSERT((s | s) == s);
		ISO_ASSERT((s ^ s) == none);
		ISO_ASSERT((s - s).empty());

		ISO_ASSERT((sm * sm) == sm);
		ISO_ASSERT((sm | sm) == sm);
		ISO_ASSERT((sm - sm) == none);
		ISO_ASSERT((sm + sm - sm) == sm);

		interval_tree<float, int>	it1, it2;
		dynamic_array<float>	floats(1000);
		random.fill(floats);
		int	x = 0;
		for (auto &i : make_split_range<2>(floats))
			it1.insert({i[0], i[1]}, x++);

		x = 0;
		for (auto i : floats)
			it2.insert({i, i+.1f}, x++);

		x = 0;
		while (it2) {
			//auto	r = ;
			it2.remove(it2.root());
			++x;
		}

	#if 1
		timer	time;

		dynamic_array<float>	nums(10000000);
		random.fill(nums);

		dynamic_array<rb_floatnode>	nodes = nums;
		e_rbtree<rb_floatnode>			e;
		time.reset();
		for (auto &i : nodes) {
			e.insert(&i);
		}
		ISO_OUTPUTF("tree1 insert time:: %f\n", (float)time);
		ISO_ASSERT(validate(e));

		time.reset();
		for (auto i : nums)
			e.remove(e.find(i));
		ISO_OUTPUTF("tree1 remove time:: %f\n", (float)time);
		ISO_ASSERT(validate(e));

		dynamic_array<rb_floatnodep>	nodesp = nums;
		e_rbtree<rb_floatnodep>			ep;
		time.reset();
		for (auto &i : nodesp)
			ep.insert(&i);

		ISO_OUTPUTF("tree2p insert time:: %f\n", (float)time);
		ISO_ASSERT(validate(ep));

		time.reset();
		for (auto i : nums)
			ep.remove(ep.find(i));
		ISO_OUTPUTF("tree2p remove time:: %f\n", (float)time);
		ISO_ASSERT(validate(ep));

	#endif
	}
} _test_tree;
