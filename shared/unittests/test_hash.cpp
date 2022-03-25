#include "base/hash.h"
#include "base/array.h"
#include "extra/random.h"
#include "base/lock_free_hash.h"
#include "base/strings.h"
#include "jobs.h"

using namespace iso;

dynamic_array<int> random_ints(int n) {
	rng<simple_random>	r(1);
	dynamic_array<int>	a(n);
	shuffle(a, make_int_iterator(0), r);
	return a;
}

struct test_hash {
	test_hash() {
		hash_map<int, int>	h;
		auto	ints = random_ints(1000);

//		for (auto i : ints)
//			h[i] = i * i;

		for (int i = 0; i < 1000; i++) {
			h[ints[i]] = i * i;
			if (i >= 8)
				ISO_ASSERT(h.remove(ints[i-8]));
		}

		ConcurrentJobs	jobs(32);

		hash_map<int, int*, true>	nbhmp;
		parallel_for_block(jobs, ints, [&nbhmp](int &i) {
			nbhmp.put(i, &i);
		});
		for (auto &i : ints) {
			ISO_ALWAYS_ASSERT(nbhmp[i].get() == &i);
		}

		hash_map<int, int, true>	nbhm;

		timer	time;
		for (int i = 0; i < 100; i++) {
			ISO_OUTPUTF("test hash: %i\n", i);
			nbhm.clear();
			parallel_for_block(jobs, ints, [&nbhm](int i) {
				nbhm.put(i, i * i);
			});

			ISO_ALWAYS_ASSERT(nbhm.size() == ints.size());
			parallel_for_block(jobs, ints, [&nbhm](int i) {
				nbhm.remove(i);
			});
			ISO_ALWAYS_ASSERT(nbhm.empty());
		}
	}

} _test_hash;

struct test_atomic_hash {
	test_atomic_hash() {
		auto	ints = random_ints(10000);

		ConcurrentJobs	jobs(32);
		
		atomic_hash_map<int, int*>	nbhmp;
		parallel_for_block(jobs, ints, [&nbhmp](int &i) {
			nbhmp.put(i, &i);
		});
		for (auto &i : ints) {
			ISO_ALWAYS_ASSERT(nbhmp[i].get() == &i);
		}
		
		atomic_hash_map<int, int>	nbhm;

		timer	time;
		for (int i = 0; i < 100; i++) {
			ISO_OUTPUTF("test hash: %i\n", i);
			nbhm.clear();
		#if 1
			parallel_for_block(jobs, ints, [&nbhm](int i) {
				nbhm.put(i, i * i);
			});
		#else
			for (auto i : ints)
				nbhm.put(i, i * i);
		#endif

			ISO_ALWAYS_ASSERT(nbhm.size() == ints.size());
#if 0
			dynamic_bitarray<>	set(num_elements(ints));
			for (auto i : nbhm.snapshot().with_keys()) {
				set.set(i.key);
			}
			ISO_ALWAYS_ASSERT(set.count_clear() == 0);
			for (auto i : ints) {
				//ISO_ALWAYS_ASSERT(nbhm.get(i) == i * i);
				ISO_ALWAYS_ASSERT(nbhm[i] == i * i);
			}
#endif
		#if 1
			parallel_for_block(jobs, ints, [&nbhm](int i) {
				nbhm.remove(i);
			});
		#else
			for (auto i : ints)
				nbhm.remove(i);
		#endif
			ISO_ALWAYS_ASSERT(nbhm.empty());

		}
		ISO_OUTPUTF("test_atomic_hash time:: %f\n", (float)time);
	}
} _test_atomic_hash;