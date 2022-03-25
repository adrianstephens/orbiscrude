#include "grabcut.h"
#include "base/algorithm.h"
#include "base/strings.h"
#include "extra/kd_tree.h"
#include "extra/random.h"
#include "jobs.h"
#include "maths/polygon.h"
#include "maxflow_grid.h"

//#define USE_OPENCV
#define USE_ISOPOD

#ifdef USE_OPENCV

#undef _INC_MATH
#include "opencv2/opencv.hpp"

#pragma comment(lib,"opencv_core310.lib")
#pragma comment(lib,"opencv_imgproc310d.lib")
#endif

//#define USE_KDTREE
#define USE_KMEANS_HAMMERLY
//#define USE_KDBALLS
//#define USE_MAINTAINED_SUMS

using namespace iso;

#define ISO_TRACEF2(...)	trace_accum().formati(__VA_ARGS__)

namespace iso {

template<int N, typename T> inline size_t to_string(char *s, const array_vec<T, N> &v) {
	char	*s0 = s;
	for (int i = 0; i < N; i++) {
		*s++ = i == 0 ? '(' : ',';
		s += to_string(s, v[i]);
	}
	*s++ = ')';
	return s - s0;
}

}

template<class I, class F> void parallel_for_block0(I i, I end, F f, int nt = 64) {
	while (i != end) {
		f(i);
		++i;
	}
}

template<typename T, int N> void reset(const block<T, N> &b) {
	fill(b, 0);
}
template<typename T, int N, int M> void reset(const block<array_vec<T, N>, M> &b) {
	for_each(b, [](array_vec<T,N> &t) { clear(t); });
}
template<typename T> void reset(const dynamic_array<T> &b) {
	memset(b, 0, b.size() * sizeof(T));
}

//typedef constructable<soft_vector<3,double> >	Vec3d;
//typedef constructable<soft_vector<3,float> >	Vec3f;

typedef array_vec<double, 3>	Vec3d;
typedef array_vec<float, 3>		Vec3f;

plane bisect(param(ray3) &r) {
	return plane(r.d, r.centre());
}

struct kd_balls {
	static atomic<int>	hits;
	static atomic<int>	misses;

	const dynamic_array<Vec3f>	&data;
	dynamic_array<sphere>		balls;

	kd_balls(const dynamic_array<Vec3f> &_data, const interval<Vec3f> &ext) : data(_data) {
		//return;
		int	n	= data.size32();
		balls.resize(n);

		plane	*planes = alloc_auto(plane, n - 1 + 6);

		Vec3f	e0 = ext.minimum(), e1 = ext.maximum();
		plane	*ep	= planes + n - 1;
		ep[0] = plane( x_axis,  e0.x);
		ep[1] = plane( y_axis,  e0.y);
		ep[2] = plane( z_axis,  e0.z);

		ep[3] = plane(-x_axis, -e1.x);
		ep[4] = plane(-y_axis, -e1.y);
		ep[5] = plane(-z_axis, -e1.z);

		position3	*testpts = alloc_auto(position3, 100);

		for (int i = 0; i < n; i++) {
			for (int j = 0; j < n; j++) {
				if (i != j)
					planes[j - int(j > i)] = bisect(ray3(position3(data[j]), position3(data[i])));
			}
			halfspace_intersection3	h(make_range_n(make_const(planes), n - 1 + 6));
			auto	pc	= h.points(testpts);

			balls[i] = h.inscribe();
		}
	}
	int		nearest_neighbour(const Vec3f &x, float &_min_dist, int prev) const {
	#if 1
		if (balls[prev].contains((position3)x))
			return prev;
		return iso::nearest_neighbour(data, x, _min_dist) - data.begin();;
	#else
		position3	v = x;
		float	mind = maximum;
		int		mini;
		for (auto &i : data) {
			float	d = len2(position3(i) - v);
			if (d < mind) {
				mind = d;
				mini = &i - data.begin();
			}
		}
		if (balls[prev].contains(v)) {
			++hits;
			ISO_ASSERT(prev == mini);
		} else {
			++misses;
		}
		return mini;
	#endif
	}
};

atomic<int>	kd_balls::hits;
atomic<int>	kd_balls::misses;

//-----------------------------------------------------------------------------
//	k-means
//-----------------------------------------------------------------------------

template<typename T, typename R> static void generateRandomCenter(const interval<block<T, 1> > &box, block<T, 1> &centre, R &rng) {
	int		dims	= box.size();
	float	margin	= 1.f / dims;
	interval<float>	m(-margin, 1 + margin);

	for (int j = 0; j < dims; j++)
		centre[j] = rng.from(box[j] * m);
}

template<int N, typename T, typename R> static void generateRandomCenter(const interval<array_vec<T,N>> &box, array_vec<T,N> &centre, R &rng) {
	float	margin	= 1.f / N;
	interval<float>	m(-margin, 1 + margin);
	centre = rng.from(box * m);
}

template<typename T, typename C, typename R> static void generateCentersRandom(const interval<T> &box, C &&centres, int K, R &rng) {
	for (int k = 0; k < K; k++)
		generateRandomCenter(box, centres[k], rng);
}

// k-means centre initialization using the following algorithm:
// Arthur & Vassilvitskii (2007) k-means++: The Advantages of Careful Seeding

template<typename C, typename V, typename R> static void generateCentersPP(C &data, V &centres, int K, R &rng, int trials) {
	int					N = data.size32();
	dynamic_array<float>	_dist(N * 3);
	float	*dist	= _dist.begin(), *tdist = dist + N, *tdist2 = tdist + N;

	int		i0		= rng.to(N);
	auto	&d0		= data[i0];
	centres[0]		= d0;

	parallel_for_block(int_range(N), [=](int i) {
		dist[i] = dist2(data[i], d0);
	});

	float	sum0	= 0;
	for (size_t i = 0; i < N; i++)
		sum0	+= dist[i];

	for (int k = 1; k < K; k++) {
		float	best_s	= maximum;
		int		best_i	= -1;

		for (int j = 0; j < trials; j++) {
			float	p	= (float)rng * sum0;
			int		ci	= 0;
			while (ci < N - 1 && (p -= dist[ci]) > 0)
				ci++;

			auto	&dc	= data[ci];
			parallel_for_block(int_range(N), [=](int i) {
				tdist2[i] = min(dist2(data[i], dc), dist[i]);
			});

			float	s = 0;
			for (int i = 0; i < N; i++)
				s += tdist2[i];

			if (s < best_s) {
				best_s	= s;
				best_i	= ci;
				swap(tdist, tdist2);
			}
		}
		centres[k]	= data[best_i];
		sum0		= best_s;
		swap(dist, tdist);
	}
}

template<typename C, typename V, typename R> static void generateCentersPP2(C &data, V &centres, int K, R &rng, int trials) {
	int						N = data.size32();
	dynamic_array<float>	dists(N, maximum);
	dynamic_array<int>		chosen_pts(K);

	// choose the first point randomly
	int	i0			= rng.to(N);
	chosen_pts[0]	= i0;
	centres[0]		= data[i0];

	for (int ndx = 1; ndx < K; ++ndx) {
		float sum_distribution = 0;
		// look for the point that is furthest from any center
		float max_dist = 0;
		for (int i = 0; i < N; ++i) {
			float	d2	= dists(data[i], data[i0]);
			if (d2 < dists[i])
				dists[i] = d2;

			if (dists[i] > max_dist)
				max_dist = dists[i];

			sum_distribution += dists[i];
		}

		bool unique = true;
		do {
			// choose a random interval according to the new distribution
			float	r	= sum_distribution * (float)rng;
			float	sum	= dists[0];
			i0 = 0;
			while (sum < r)
				sum += dists[++i0];
			for (int i = 0; unique && i < ndx; ++i)
				unique = i0 != chosen_pts[i];
		} while (!unique);

		chosen_pts[ndx] = i0;
		centres[ndx]	= data[i0];
	}
}


enum KmeansFlags {
	KMEANS_RANDOM_CENTERS		= 0,	// Select random initial centres in each attempt
	KMEANS_PP_CENTERS			= 2,	// Use kmeans++ centre initialization by Arthur and Vassilvitskii
	KMEANS_USE_INITIAL_LABELS	= 1,	// flag to use user-supplied initial centres
};

template<typename C, typename V=typename container_traits<typename T_noref<C>::type>::element>
void kmeans(const C &data, int K, dynamic_array<int> &labels, dynamic_array<V> &centres, int max_count, int flags) {
	/*static*/ iso::rng<simple_random>	rng(0x12345678);
	int		N		= data.size32();
	float	eps		= epsilon;

	labels.resize(N);
	centres.resize(K);

	dynamic_array<int>		centre_counts(K);
	dynamic_array<V>		centre_sums(K);

#ifdef USE_KMEANS_HAMMERLY
	dynamic_array<float>	s(K);					// Half the distance between each centre and its closest other centre.
	dynamic_array<float>	shifts(K);				// Movement of each centre
	dynamic_array<float>	lower(N, 0);			// One upper bound for each point on the distance between that point and its assigned (closest) centre.
	dynamic_array<float>	upper(N, maximum);		// Lower bound for each point on the distance between that point and the centres being tracked for lower bounds
#endif

	interval<V>				box = get_extent(data);

	if (!(flags & KMEANS_USE_INITIAL_LABELS)) {
		if (flags & KMEANS_PP_CENTERS)
			generateCentersPP(data, centres, K, rng, 3);
		else
			generateCentersRandom(box, centres, K, rng);

//		kd_tree<const C&>	kdt(centres, 1);
		kd_tree<const C&>	kdt(centres, 1);
		parallel_for_block(int_range(N), [&](int i) {
			float	d2;
			labels[i]	= kdt.nearest_neighbour(data[i], d2);
		#ifdef USE_KMEANS_HAMMERLY
			upper[i]	= iso::sqrt(d2);
		#endif
		});
	}

	// initialise centres
#ifdef USE_MAINTAINED_SUMS
	reset(centre_sums);
	for (int k = 0; k < K; k++)
		centre_counts[k] = 0;

	for (int i = 0; i < N; i++) {
		int		k	= labels[i];
		centre_sums[k] += data[i];
		centre_counts[k]++;
	}
#endif

	for (int iter = 0;; ) {
	#ifndef USE_MAINTAINED_SUMS
		reset(centre_sums);
		for (int k = 0; k < K; k++)
			centre_counts[k] = 0;

		for (int i = 0; i < N; i++) {
			int		k	= labels[i];
			centre_sums[k] += data[i];
			centre_counts[k]++;
			//float	d2;
			//int	closest = nearest_neighbour(centres, data[i], d2) - centres.begin();
			//ISO_ASSERT(closest==k);
		}
	#endif

		int		max_shift_k		= 0;
		float	max_shift		= 0;
		float	second_shift	= 0;

		for (int k = 0; k < K; k++) {
			if (centre_counts[k] == 0) {
				// if some cluster appeared to be empty then:
				//   1. find the biggest cluster
				//   2. find the farthest from the centre point in the biggest cluster
				//   3. exclude the farthest point from the biggest cluster and form a new 1-point cluster.
				int		max_k		= 0;
				for (int k1 = 1; k1 < K; k1++) {
					if (centre_counts[max_k] < centre_counts[k1])
						max_k = k1;
				}

				float	max_dist	= 0;
				int		max_i		= -1;
				for (int i = 0; i < N; i++) {
					if (labels[i] == max_k) {
						float dist = dist2(data[i], centres[max_k]);
						if (dist > max_dist) {
							max_dist	= dist;
							max_i		= i;
						}
					}
				}

				--centre_counts[max_k];
				centre_counts[k]	= 1;
				labels[max_i]		= k;

				centre_sums[max_k]	-= data[max_i];
				centre_sums[k]		+= data[max_i];
			}

			V		new_centre	= centre_sums[k] / centre_counts[k];
			auto	shift		= dist(centres[k], new_centre);
			if (shift> max_shift) {
				second_shift	= max_shift;
				max_shift		= shift;
				max_shift_k		= k;
			} else if (shift > second_shift) {
				second_shift	= shift;
			}
		#ifdef USE_KMEANS_HAMMERLY
			shifts[k]			= shift;
		#endif
			centres[k]			= new_centre;
		}

		if (++iter == max_count || max_shift <= eps)
			break;

	#ifdef USE_KMEANS_HAMMERLY
		// compute inter-centre squared distances between all pairs
		for (int i = 0; i < K; ++i) {
			float	mind2	= maximum;
			auto	&ci		= centres[i];
			for (int j = 0; j < K; ++j) {
				if (i != j) {
					auto d2 = dist2(ci, centres[j]);
					if (d2 < mind2)
						mind2 = d2;
				}
			}
			s[i] = iso::sqrt(mind2) / 2;
		}

		// loop over all records
		parallel_for_block(int_range(N), [&](int i) {
			int k = labels[i], prev_k = k;

			upper[i] += shifts[k];
			lower[i] -= k == max_shift_k ? second_shift : max_shift;

			// if upper[i] is less than the greater of these two, then we can ignore record i
			auto upper_comparison_bound = max(s[k], lower[i]);

			// first check: if u(x) <= s(c(x)) or u(x) <= lower(x), then ignore x, because its closest centre must still be closest
			if (upper[i] > upper_comparison_bound) {
				// otherwise, compute the real distance between this record and its closest centre, and update upper
				auto u2		= dist2(data[i], centres[k]);
				upper[i]	= iso::sqrt(u2);

				// if (u(x) <= s(c(x))) or (u(x) <= lower(x)), then ignore x
				if (upper[i] > upper_comparison_bound) {
					// now update the lower bound by looking at all other centres
					float	l2 = maximum; // the squared lower bound
					for (int j = 0; j < K; ++j) {
						if (j == k)
							continue;

						auto d2 = dist2(data[i], centres[j]);
						if (d2 < u2) {
							// another centre is closer than the current assignment
							l2	= u2;	// change the lower bound to be the current upper bound (since the current upper bound is the distance to the now-second-closest known centre)
							u2	= d2;	// adjust the upper bound and the current assignment
							k	= j;
						} else if (d2 < l2) {
							// we must reduce the lower bound on the distance to the *second* closest centre to x[i]
							l2	= d2;
						}
					}

					lower[i] = iso::sqrt(l2);
					upper[i] = iso::sqrt(u2);
				#ifdef USE_MAINTAINED_SUMS
					// if the assignment for i has changed, then adjust the counts and locations of each centre's accumulated mass
					if (k != prev_k) {
						--centre_counts[prev_k];
						++centre_counts[k];
						centre_sums[prev_k]	-= data[i];
						centre_sums[k]		+= data[i];
						labels[i] = k;
					}
				#else
					labels[i] = k;
				#endif
				}
			}
		});
	#else
	#ifdef USE_KDBALLS
		kd_balls	kdb(centres, box);
	#elif defined USE_KDTREE
		kd_tree<const C&>	kdt(centres, 1);
	#endif

		// assign labels
		parallel_for_block(int_range(N), [&](int i) {
		#ifdef USE_MAINTAINED_SUMS
			int		prev_k = labels[i];
		#endif
			float	d2;
		#ifdef USE_KDBALLS
			int		k = kdb.nearest_neighbour(data[i], d2, labels[i]);
		#elif defined USE_KDTREE
			int		k = kdt.nearest_neighbour(data[i], d2);
		#else
			int		k = nearest_neighbour(centres, data[i], d2) - centres.begin();
		#endif
		#ifdef USE_MAINTAINED_SUMS
			if (k != prev_k) {
				--centre_counts[prev_k];
				++centre_counts[k];
				centre_sums[prev_k]	-= data[i];
				centre_sums[k]		+= data[i];
				labels[i] = k;
			}
		#else
			labels[i] = k;
		#endif
		});
	#endif
	}
}

template<typename C, typename V=typename container_traits<C>::element>
float kmeans(const C &data, int K, dynamic_array<int> &labels, dynamic_array<V> &centres, int max_count, int attempts, int flags) {
	size_t					N = data.size();
	dynamic_array<int>		best_labels(N);
	float					best_compactness	= maximum;

	for (int a = 0; a < attempts; a++) {
		kmeans(data, K, labels, centres, max_count, flags);

		dynamic_array<float>	dists(N);
		parallel_for_block(int_range(N), [&](int i) {
			dists[i] = dist2(data[i], centres[labels[i]]);
		});

		float	compactness		= 0;
		for (int i = 0; i < N; i++)
			compactness += dists[i];

		if (compactness < best_compactness) {
			best_compactness = compactness;
			swap(labels, best_labels);
		}
		flags &= ~KMEANS_USE_INITIAL_LABELS;
	}
	swap(labels, best_labels);
	return best_compactness;
}


/*
double _kmeans(block<float,2> data, int K, auto_block<int,1> &labels, auto_block<float, 2> &centres, int maxCount, int attempts, int flags) {
	return kmeans(data, K, labels, centres, maxCount, attempts, flags);
}

double _kmeans(block<soft_vector<4,float>,1> data, int K, auto_block<int,1> &labels, auto_block<soft_vector<4,float>, 1> &centres, int maxCount, int attempts, int flags) {
	return kmeans(data, K, labels, centres, maxCount, attempts, flags);
}
*/
//-----------------------------------------------------------------------------
//	GCGraph
//-----------------------------------------------------------------------------

template<class T> class GCGraph {
	typedef array_vec<int16, 2> ID;
	static const int TERMINAL = -1, ORPHAN = -2;

	struct Vtx : e_slink<Vtx> {
		ID		id;		// for debugging only
		//Vtx		*next;	// links active verts during max_flow()
		int		parent;	// node's parent
		int		first;	// first edge (0->none)
		int		step;	// step at which dist was calculated
		int		dist;	// dist to the terminal
		T		weight;
		uint8	t;		// which tree
		Vtx(T _weight) : first(0), weight(_weight) {}
		Vtx() { clear(*this); }
	};
	struct Edge {
		int		dest;	//destination vert
		int		next;	//next edge
		T		weight;
		Edge()	{ clear(*this); }
		Edge(int _dest, int _next, T _weight) : dest(_dest), next(_next), weight(_weight) {}
	};

	struct EdgeIterator {
		typedef forward_iterator_t iterator_category;
		typedef T element, &reference;
		const Edge	*edges;
		int		i;
		EdgeIterator(const Edge *_edges, int _i) : edges(_edges), i(_i) {}
		const T&		operator*()	const { return edges[i].weight; }
		bool			operator==(const EdgeIterator &b) const { return i == b.i; }
		bool			operator!=(const EdgeIterator &b) const { return i != b.i; }
		EdgeIterator&	operator++() { i = edges[i].next; return *this; }
	};

	dynamic_array<Vtx>	verts;
	dynamic_array<Edge> edges;
	dynamic_array<Vtx*> orphans;	//here to avoid re-allocation
//	T					flow;
	int					stats[10];

public:
	GCGraph(uint32 cols, uint32 rows, uint32 edgeCount) {// : flow(0) {
		uint32	vtxCount = cols * rows;
		verts.resize(vtxCount);

		Vtx	*v = verts;
		for (int y = 0; y < rows; y++)
			for (int x = 0; x < cols; x++)
				v++->id.set(x, y);

		edges.reserve(edgeCount + 2);
		edges.resize(2);
		clear(stats);
	}
	bool	inSourceSegment(int i) {
		return verts[i].t == 0;
	}
//	int		addVert(T sourceW, T sinkW)	{
//		verts.push_back(sourceW - sinkW);
//		flow		+= min(sourceW, sinkW);
//		return verts.size32() - 1;
//	}

	int		firstEdge(int v) {
		return verts[v].first;
	}
	int		setEdgeWeights(int e, T w, T revw) {
		edges[e].weight		= w;
		edges[e ^ 1].weight	= revw;
		return edges[e].next;
	}
	int		setEdgeWeights(int e, T w) {
		return setEdgeWeights(e, w, w);
	}

	void	addEdges(int i, int j, T w, T revw) {
		int	e = edges.size32();

		edges.emplace_back(j, verts[i].first, w);
		verts[i].first = e++;

		edges.emplace_back(i, verts[j].first, revw);
		verts[j].first = e++;
	}
	void	addEdges(int i, int j, T w)	{
		addEdges(i, j, w, w);
	}
	void	addEdges(int i, int j) {
		addEdges(i, j, 0, 0);
	}
	void	sortEdgeLists();
	void	setTermWeights(int i, T sourceW, T sinkW) {
//		flow			+= min(sourceW, sinkW);
		verts[i].weight	= sourceW - sinkW;
	}
	T		getVertWeight(int i) const {
		return verts[i].weight;
	}
	range<EdgeIterator>	getEdges(int i) const {
		return range<EdgeIterator>(EdgeIterator(edges, verts[i].first), EdgeIterator(edges, 0));
	}

	/*
	void	addTermWeights(int i, T sourceW, T sinkW) {
		Vtx	&v = verts[i];
		if (v.weight > 0)
			sourceW += v.weight;
		else
			sinkW	-= v.weight;
		flow		+= min(sourceW, sinkW);
		v.weight	= sourceW - sinkW;
	}
	*/

	Vtx	*getDest(int e) {
		return verts + edges[e].dest;
	}
	Vtx	*getSrce(int e) {
		return getDest(e ^ 1);
	}
	typedef e_slist_tail<Vtx> state;
	void	max_flow_init(state &active);
	bool	max_flow_step(state &active, int step);
	void	max_flow();
};

template<class T> void	GCGraph<T>::sortEdgeLists() {
	for (auto &v : verts) {
	#if 1
		int		dirs[8] = {0};
		for (int e = v.first; e; e = edges[e].next) {
			Vtx	&d = verts[edges[e].dest];
			int	dx = d.id.x - v.id.x, dy = d.id.y - v.id.y;

			enum DIR {
				L, UL, U, UR,
				R, DR, D, DL,
			};
			DIR	dir = dy > 0 ? (dx == 0 ? D : dx > 0 ? DR : DL)
					: dy < 0 ? (dx == 0 ? U : dx > 0 ? UR : UL)
					: (dx > 0 ? R : L);
			dirs[dir] = e;
		}
		int	ep = 0;
		for (int i = 0; i < 8; i++) {
			if (int e = dirs[i]) {
				if (ep)
					edges[ep].next = e;
				else
					v.first = e;
				ep = e;
			}
		}
		edges[ep].next = 0;
	#else
		int	ep = 0;
		for (int e = v.first, en; e; e = en) {
			en = edges[e].next;
			edges[e].next = ep;
			ep = e;
		}
		v.first = ep;
	#endif
	}
}

template<class T> void GCGraph<T>::max_flow_init(state &active) {
	// initialize the active queue and the graph vertices
	for (auto &i : verts) {
		i.step = 0;
		if (i.weight != 0) {
			active.push_back(&i);
			i.dist		= 1;
			i.parent	= TERMINAL;
			i.t			= i.weight < 0;
		} else {
			i.parent	= 0;
		}
	}
}

template<class T> bool GCGraph<T>::max_flow_step(state &active, int step) {
	int		connection = -1;

	// grow S & T search trees, find an edge connecting them
	while (!active.empty()) {
		++stats[1];

		Vtx	*v = &active.front();
		ISO_ASSERT(v->parent != ORPHAN);
		if (v->parent) {
			uint8	vt = v->t;
			for (int e = v->first; e != 0; e = edges[e].next) {
				++stats[2];

				if (edges[e ^ vt].weight == 0)
					continue;

				++stats[3];

				Vtx	*u = verts + edges[e].dest;
				if (!u->parent) {
					u->t		= vt;
					u->parent	= e ^ 1;
					u->step		= v->step;
					u->dist		= v->dist + 1;

					// add u to active queue
					if (!u->next)
						active.push_back(u);
					ISO_TRACEF2("Parent %0 to edge %1-%2\n", u->id, getSrce(u->parent)->id, v->id);

				} else if (u->t != vt) {
					connection = e ^ vt;
					break;

				} else if (u->dist > v->dist + 1 && u->step <= v->step) {
					// reassign the parent
					u->parent	= e ^ 1;
					u->step		= v->step;
					u->dist		= v->dist + 1;
					ISO_TRACEF2("Reparent %0 to edge %1-%2\n", u->id, getSrce(u->parent)->id, v->id);
				}
			}
			if (connection > 0)
				break;
		}
		// exclude the vertex from the active list
		active.pop_front();
	}

	if (connection <= 0)
		return false;

	ISO_TRACEF2("Bridge edge %0-%1\n", getSrce(connection)->id, getDest(connection)->id);

	// find the minimum edge weight along the path
	T	min_weight = edges[connection].weight;
	ISO_ASSERT(min_weight > 0);

	// k = 1: source tree, k = 0: destination tree
	for (int k = 1; k >= 0; k--) {
		Vtx	*v;
		int	e;
		for (v = verts + edges[connection ^ k].dest; (e = v->parent) >= 0; v = verts + edges[e].dest) {
			++stats[4];
			min_weight = min(min_weight, edges[e ^ k].weight);
			ISO_ASSERT(min_weight > 0);
		}
		min_weight = min(min_weight, abs(v->weight));
		ISO_ASSERT(min_weight > 0);
	}

	// modify weights of the edges along the path and collect orphans
	edges[connection].weight		-= min_weight;
	edges[connection ^ 1].weight	+= min_weight;
//	flow							+= min_weight;

	// k = 1: source tree, k = 0: destination tree
	for (int k = 1; k >= 0; k--) {
		Vtx	*v;
		int	e;
		for (v = verts + edges[connection ^ k].dest; (e = v->parent) >= 0; v = verts + edges[e].dest) {
			edges[e ^ (k ^ 1)].weight += min_weight;
			if ((edges[e ^ k].weight -= min_weight) == 0) {
				orphans.push_back(v);
				ISO_ASSERT(v->parent);
				v->parent = ORPHAN;
			}
		}

		if ((v->weight += min_weight * (1 - k * 2)) == 0) {
			orphans.push_back(v);
			ISO_ASSERT(v->parent);
			v->parent = ORPHAN;
		}
	}

	// restore the search trees by finding new parents for the orphans
	while (!orphans.empty()) {
		++stats[5];

		static const int int_max = maximum;
		Vtx*	v			= orphans.pop_back_value();
		int		min_dist	= int_max;
		uint8	vt			= v->t;
		int		e0			= 0;

		for (int e = v->first; e != 0; e = edges[e].next) {
			++stats[6];

			if (edges[e ^ (vt ^ 1)].weight == 0)
				continue;

			Vtx	*u = verts + edges[e].dest;
			if (u->t != vt || u->parent == 0)
				continue;

			++stats[7];

			// compute the distance to the tree root
			int	d = 1;
			for (;;) {
				if (u->step == step) {
					d += u->dist;
					break;
				}
				d++;
				int	ej = u->parent;
				if (ej < 0) {
					if (ej == ORPHAN) {
						d		= int_max;
					} else {
						u->step	= step;
						u->dist = 1;
					}
					break;
				}
				u = verts + edges[ej].dest;
			}

			// update the distance
			if (d < int_max) {
				if (d < min_dist) {
					min_dist = d;
					e0		= e;
				}
				for (u = verts + edges[e].dest; u->step != step; u = verts + edges[u->parent].dest) {
					u->step	= step;
					u->dist = --d;
				}
			}
		}

		if ((v->parent = e0) > 0) {
			ISO_TRACEF2("Adopt %0 by edge %1-%2\n", v->id, getSrce(e0)->id, getDest(e0)->id);
			v->step	= step;
			v->dist	= min_dist;

		} else {
			// no parent is found
			v->step = 0;
			for (int e = v->first; e != 0; e = edges[e].next) {
				Vtx	*u = verts + edges[e].dest;
				int	ej = u->parent;
				if (u->t != vt || !ej)
					continue;

				if (edges[e ^ (vt ^ 1)].weight && !u->next)
					active.push_back(u);

				if (ej > 0 && verts + edges[ej].dest == v) {
					orphans.push_back(u);
					u->parent = ORPHAN;
				}
			}
		}
	}
	return true;
}

template<class T> void GCGraph<T>::max_flow() {
	e_slist_tail<Vtx>		active;

	// initialize the active queue and the graph vertices
	max_flow_init(active);

	// run the search-path -> augment-graph -> restore-trees loop
	for (int step = 1; max_flow_step(active, step); ++step)
		;
}

//-----------------------------------------------------------------------------
//	GCGraph2 - implicit edges
//-----------------------------------------------------------------------------

template<class T> class GCGraph2 {
	enum {NONE, SRCE_TREE = 2, DEST_TREE = 3, ORPHAN = 4};
	typedef array_vec<int16, 2> ID;

	struct Vtx : e_slink<Vtx> {
	//	ID		id;		// for debugging only
		T		*parent;
		T		weight;
		int		step;	// step at which dist was calculated
		int16	dist;	// dist to tree root
		uint8	t;		// which tree

		Vtx() { clear(*this); }
	};

	typedef	array<T,8>	Edge;

	dynamic_array<Vtx>	verts;
	dynamic_array<Edge>	edges;
	dynamic_array<Vtx*> orphans;	//here to avoid re-allocation
	int					offsets[8];
	int					opposite_offsets[8];
	int					stats[10];

public:
	enum DIR {
		L, UL, U, UR,
		R, DR, D, DL,
	};
	friend DIR& operator++(DIR &d) { return d = DIR(d + 1); }

	int	get_index(int i) const {
		int	s = offsets[D];
		int	w = s - 1;
		int	y = i / w;
		int	x = i - y * w;
		return (y + 1) * s + (x + 1);
	}
	struct EdgeIterator {
		typedef forward_iterator_t iterator_category;
		typedef T element, &reference;
		const Edge	&e;
		int			i;
		EdgeIterator(const Edge &_e, int _i) : e(_e), i(_i) {}
		const T&	operator*()	const { return e[i]; }
		bool		operator==(const EdgeIterator &b) const { return i == b.i; }
		bool		operator!=(const EdgeIterator &b) const { return i != b.i; }
		EdgeIterator& operator++() { ++i; return *this; }
	};

	GCGraph2(uint32 w, uint32 h) : verts((w + 1) * (h + 2)), edges((w + 1) * (h + 2)) {
		uint32	s	= w + 1;
		offsets[0] = 0	- 1;
		offsets[1] = -s	- 1;
		offsets[2] = -s	+ 0;
		offsets[3] = -s	+ 1;
		offsets[4] = 0	+ 1;
		offsets[5] = s	+ 1;
		offsets[6] = s	+ 0;
		offsets[7] = s	- 1;

		for (int i = 0; i < 8; i++)
			opposite_offsets[i] = offsets[i] * 8 + (i ^ 4) - i;

/*		Vtx	*v = verts;
		for (int y = -1; y < int(h); y++)
			for (int x = -1; x < int(w); x++)
				v++->id.set(x, y);
*/
		clear(stats);
	}

	bool	inSourceSegment(int i) {
		return verts[get_index(i)].t == SRCE_TREE;
	}

	void	setTermWeights(int i, T sourceW, T sinkW) {
		verts[get_index(i)].weight	= sourceW - sinkW;
	}
	void	setEdgeWeights(int i, DIR dir, T w, T revw) {
		i = get_index(i);
		edges[i][dir] = w;

		int	j = i + offsets[dir];
     	edges[j][dir ^ 4] = revw;
	}
	void	setEdgeWeights(int i, DIR dir, T w) {
		setEdgeWeights(i, dir, w, w);
	}

	T*	getEdgeWeight(const Vtx *v, DIR dir, uint8 vt) {
		int	i = v - verts;
		return vt
			? edges[i + offsets[dir]] + (dir ^ 4)
			: edges[i] + dir;
	}
	T*	getOppositeWeight(T *w) {
		intptr_t	x	= w - &edges[0][0];
		return  w + opposite_offsets[x & 7];
	}

	Vtx	*getSrce(const T *w) {
		intptr_t	x	= w - &edges[0][0];
		return verts + (x >> 3);
	}
	Vtx	*getDest(const T *w) {
		intptr_t	x	= w - &edges[0][0];
		return verts + (x >> 3) + offsets[DIR(x & 7)];
	}

	Vtx	*getDest(Vtx *v, DIR dir) {
		return v + offsets[dir];
	}
	T		getVertWeight(int i) const {
		return verts[get_index(i)].weight;
	}
	range<EdgeIterator>	getEdges(int i) const {
		i = get_index(i);
		return range<EdgeIterator>(EdgeIterator(edges[i], 0), EdgeIterator(edges[i], 8));
	}

	typedef e_slist_tail<Vtx> state;
	void	max_flow_init(state &active);
	bool	max_flow_step(state &active, int step);
	void	max_flow();
};

template<class T> void GCGraph2<T>::max_flow_init(state &active) {
	// initialize the active queue and the graph vertices
	for (auto &i : verts) {
		i.step		= 0;
		i.parent	= 0;
		if (i.weight != 0) {
			//last		= last->next = &i;
			active.push_back(&i);
			i.dist		= 1;
			i.t			= i.weight < 0 ? DEST_TREE : SRCE_TREE;
		} else {
			i.t			= NONE;
		}
	}
}

template<class T> bool GCGraph2<T>::max_flow_step(state &active, int step) {
	T		*connection = 0;

	// grow S & T search trees, find an edge connecting them
	while (!active.empty()) {
		++stats[1];

		Vtx	*v = &active.front();
		if (v->t != NONE) {
			uint8	vt	= v->t & 1;

			for (int i = 0; i != 8; ++i) {
				++stats[2];
				DIR	dir = DIR(i);
				T	*e	= getEdgeWeight(v, dir, vt);
				if (*e == 0)
					continue;

				++stats[3];

				Vtx	*u = getDest(v, dir);
				if (u->t == NONE) {
					u->t		= v->t;
					u->parent	= getEdgeWeight(v, dir, 1);
					u->step		= v->step;
					u->dist		= v->dist + 1;
					if (!u->next)
						active.push_back(u);
					ISO_TRACEF2("Parent %0 to edge %1-%2\n", u->id, getSrce(u->parent)->id, v->id);

				} else if ((u->t & 1) != vt) {
					connection	= e;
					break;

				} else if (u->dist > v->dist + 1 && u->step <= v->step) {
					// reassign the parent
					u->parent	= getEdgeWeight(v, dir, 1);
					u->step		= v->step;
					u->dist		= v->dist + 1;
					ISO_TRACEF2("Reparent %0 to edge %1-%2\n", u->id, getSrce(u->parent)->id, v->id);
				}
			}
			if (connection)
				break;
		}
		// exclude the vertex from the active list
		active.pop_front();
	}

	if (!connection)
		return false;

	ISO_TRACEF2("Bridge edge %0-%1\n", getSrce(connection)->id, getDest(connection)->id);

	// find the minimum edge weight along the path
	T	min_weight = *connection;
	ISO_ASSERT(min_weight > 0);

	Vtx		*v;
	T		*e;
	//source tree
	for (v = getSrce(connection); e = v->parent; v = getSrce(e)) {
		e = getOppositeWeight(e);
		min_weight = min(min_weight, *e);
	}

	min_weight = min(min_weight, abs(v->weight));

	//dest tree
	for (v = getDest(connection); e = v->parent; v = getDest(e))
		min_weight = min(min_weight, *e);

	min_weight = min(min_weight, abs(v->weight));
	ISO_ASSERT(min_weight > 0);

	// modify weights of the edges along the path and collect orphans
	*connection						-= min_weight;
	*getOppositeWeight(connection)	+= min_weight;

	//source tree
	for (v = getSrce(connection); e = v->parent; v = getSrce(e)) {
		++stats[4];
		*e += min_weight;
		e = getOppositeWeight(e);
		if ((*e -= min_weight) == 0) {
			orphans.push_back(v);
			v->parent	= 0;
			v->t		|= ORPHAN;
		}
	}
	if ((v->weight -= min_weight) == 0) {
		orphans.push_back(v);
		v->parent	= 0;
		v->t		|= ORPHAN;
	}

	//dest tree
	for (v = getDest(connection); e = v->parent; v = getDest(e)) {
		++stats[4];
		*getOppositeWeight(e) += min_weight;
		if ((*e -= min_weight) == 0) {
			orphans.push_back(v);
			v->parent	= 0;
			v->t		|= ORPHAN;
		}
	}

	//v->weight += min_weight;	// not checking for 0 here - OK? NO!
	if ((v->weight += min_weight) == 0) {
		orphans.push_back(v);
		v->parent	= 0;
		v->t		|= ORPHAN;
	}

	// restore the search trees by finding new parents for the orphans
	while (!orphans.empty()) {
		++stats[5];
		Vtx*	v			= orphans.pop_back_value();
		int		min_dist	= maximum;
		uint8	vt			= v->t & 1;
		T		*e0			= 0;

		for (int i = 0; i != 8; ++i) {
			++stats[6];
			DIR		dir = DIR(i);
			if (*getEdgeWeight(v, dir, vt ^ 1) == 0)
				continue;

			Vtx	*u = getDest(v, dir);
			if (u->t == NONE || (u->t & 1) != vt)
				continue;

			++stats[7];

			// compute the distance to the tree root
			int		d	= 1;
			T		*ej;
			while ((ej = u->parent) && u->step != step) {
				d++;
				u = getDest(ej);
			}

			if (!(u->t & ORPHAN)) {
				if (u->step == step) {
					d += u->dist;
				} else {
					d++;
					u->step	= step;
					u->dist = 1;
				}

				// update the distance
				if (d < min_dist) {
					min_dist	= d;
					e0			= getEdgeWeight(v, dir, 0);
				}
				for (u = getDest(v, dir); u->step != step; u = getDest(u->parent)) {
					u->step	= step;
					u->dist = --d;
				}
			}
		}

		if (v->parent = e0) {
			ISO_TRACEF2("Adopt %0 by edge %1-%2\n", v->id, getSrce(e0)->id, getDest(e0)->id);
			v->step	= step;
			v->dist	= min_dist;
			v->t	&= ~ORPHAN;

		} else {
			// no parent is found
			v->step = 0;
			v->t	= NONE;
			for (int i = 0; i != 8; ++i) {
				DIR		dir = DIR(i);
				Vtx		*u	= getDest(v, dir);
				if (u->t == NONE || (u->t & 1) != vt)
					continue;

				if (*getEdgeWeight(v, dir, vt ^ 1) && !u->next)
					active.push_back(u);

				T		*ej = u->parent;
				if (ej && getDest(ej) == v) {
					orphans.push_back(u);
					u->parent	= 0;
					u->t		|= ORPHAN;
				}
			}
		}
	}
	return true;
}

template<class T> void GCGraph2<T>::max_flow() {
	e_slist_tail<Vtx>		active;

	// initialize the active queue and the graph vertices
	max_flow_init(active);

	// run the search-path -> augment-graph -> restore-trees loop
	for (int step = 1; max_flow_step(active, step); ++step)
		;
}

//-----------------------------------------------------------------------------
//	Gaussian Mixture Model
//-----------------------------------------------------------------------------

template<typename T> struct GaussianAccumulator {
	typedef array_vec<T, 3>	V;

	T		sums[3];
	T		prods[3][3];
	int		num_samples;

	void add(const V &color) {
		sums[0]		+= color.x;
		sums[1]		+= color.y;
		sums[2]		+= color.z;
		prods[0][0] += color.x * color.x;
		prods[0][1] += color.x * color.y;
		prods[0][2] += color.x * color.z;
		prods[1][0] += color.y * color.x;
		prods[1][1] += color.y * color.y;
		prods[1][2] += color.y * color.z;
		prods[2][0] += color.z * color.x;
		prods[2][1] += color.z * color.y;
		prods[2][2] += color.z * color.z;
		num_samples++;
	}

	T get_coeff(int total_samples) const {
		return num_samples ? (T)num_samples / total_samples : 0;
	}

	void get_mean_cov(T *m, T *c) const {
		int	 n	= num_samples;
		m[0]	= sums[0] / n;
		m[1]	= sums[1] / n;
		m[2]	= sums[2] / n;

		c[0]	= prods[0][0] / n - m[0] * m[0];
		c[1]	= prods[0][1] / n - m[0] * m[1];
		c[2]	= prods[0][2] / n - m[0] * m[2];
		c[3]	= prods[1][0] / n - m[1] * m[0];
		c[4]	= prods[1][1] / n - m[1] * m[1];
		c[5]	= prods[1][2] / n - m[1] * m[2];
		c[6]	= prods[2][0] / n - m[2] * m[0];
		c[7]	= prods[2][1] / n - m[2] * m[1];
		c[8]	= prods[2][2] / n - m[2] * m[2];

		T dtrm = c[0] * (c[4] * c[8] - c[5] * c[7]) - c[1] * (c[3] * c[8] - c[5] * c[6]) + c[2] * (c[3] * c[7] - c[4] * c[6]);
		if (dtrm <= (T)epsilon) {
			// Adds the white noise to avoid singular covariance matrix.
			const T variance = T(0.01);
			c[0] += variance;
			c[4] += variance;
			c[8] += variance;
		}
	}
	GaussianAccumulator() { clear(*this); }
};

template<typename T> class GaussianMixture : GaussianModel<T> {
	typedef GaussianModel<T>	B;
	using B::coefs;
	using B::mean;
	using B::cov;
	typedef typename GaussianAccumulator<T>::V V;

	T	inverseCovs[3][3];
	T	covDeterms;

public:
	void	calcInverseCovAndDeterm() {
		if (coefs) {
			T *c	= cov;
			T dtrm = covDeterms = c[0] * (c[4] * c[8] - c[5] * c[7]) - c[1] * (c[3] * c[8] - c[5] * c[6]) + c[2] * (c[3] * c[7] - c[4] * c[6]);

			ISO_ASSERT(dtrm > (T)epsilon);
			inverseCovs[0][0] = (c[4] * c[8] - c[5] * c[7]) / dtrm;
			inverseCovs[1][0] = -(c[3] * c[8] - c[5] * c[6]) / dtrm;
			inverseCovs[2][0] = (c[3] * c[7] - c[4] * c[6]) / dtrm;
			inverseCovs[0][1] = -(c[1] * c[8] - c[2] * c[7]) / dtrm;
			inverseCovs[1][1] = (c[0] * c[8] - c[2] * c[6]) / dtrm;
			inverseCovs[2][1] = -(c[0] * c[7] - c[1] * c[6]) / dtrm;
			inverseCovs[0][2] = (c[1] * c[5] - c[2] * c[4]) / dtrm;
			inverseCovs[1][2] = -(c[0] * c[5] - c[2] * c[3]) / dtrm;
			inverseCovs[2][2] = (c[0] * c[4] - c[1] * c[3]) / dtrm;
		}
	}
	GaussianMixture() {}
	GaussianMixture(const B &_model) : B(_model) {
		calcInverseCovAndDeterm();
	}
	void operator=(const B &_model) {
		B::operator=(_model);
		calcInverseCovAndDeterm();
	}

	T	operator()(const V &color) const {
		if (coefs == 0)
			return 0;
		ISO_ASSERT(covDeterms > (T)epsilon);
		V diff	= color - load<V>(mean);
		T mult = diff[0] * (diff[0] * inverseCovs[0][0] + diff[1] * inverseCovs[1][0] + diff[2] * inverseCovs[2][0])
					+ diff[1] * (diff[0] * inverseCovs[0][1] + diff[1] * inverseCovs[1][1] + diff[2] * inverseCovs[2][1])
					+ diff[2] * (diff[0] * inverseCovs[0][2] + diff[1] * inverseCovs[1][2] + diff[2] * inverseCovs[2][2]);
		return rsqrt(covDeterms) * iso::exp(-0.5 * mult);
	}
	void	update(const GaussianAccumulator<T> &acc, int total_samples) {
		if (coefs = acc.get_coeff(total_samples)) {
			acc.get_mean_cov(mean, cov);
			calcInverseCovAndDeterm();
		}
	}
};

template<typename T, int N> class GaussianMixtureN : array<GaussianMixture<T>, N> {
	typedef typename GaussianAccumulator<T>::V V;
public:
	static const int componentsCount = N;
	typedef array<GaussianModel<T>, N>	Model;

	struct Learning : array<GaussianAccumulator<T>, N> {
		int		total_samples;

		void add(int ci, const V &color) {
			this->t[ci].add(color);
			total_samples++;
		}
		T get_coeff(int ci) const {
			return this->t[ci].get_coeff(total_samples);
		}
		void get_mean_cov(int ci, T *m, T *c) const {
			return this->t[ci].get_mean_cov(m, c);
		}

		Learning() : total_samples(0) {}
	};

	void operator=(const Model& _model) {
		copy(_model, *(array<GaussianMixture<T>, N>*)this);
	}

	T	operator()(const V &color) const {
		T res = 0;
		for (auto &i : *this)
			res += i(color);
		return res;
	}
	int		whichComponent(const V &color) const {
		int		k	= 0;
		T		max	= 0;

		for (auto &i : *this) {
			T	p = i(color);
			if (p > max) {
				k	= this->index_of(i);
				max = p;
			}
		}
		return k;
	}
	void	endLearning(const Learning &learning) {
		for (int i = 0; i < this->size(); ++i)
			this->t[i].update(learning[i], learning.total_samples);
	}
};

//-----------------------------------------------------------------------------
//	"GrabCut — Interactive Foreground Extraction using Iterated Graph Cuts"
//	Carsten Rother, Vladimir Kolmogorov, Andrew Blake.
//-----------------------------------------------------------------------------

static inline bool is_fg(uint8 a)		{ return a != GC_BGD && a != GC_PR_BGD; }
static inline bool is_unsure(uint8 a)	{ return a == GC_PR_BGD || a == GC_PR_FGD; }

typedef GaussianMixtureN<double, 5>	GMM;

//#define USE_GC	1	//graph
//#define USE_GC	2	//graph2
//#define USE_GC	3	//both
#define USE_GC	4		//maxflow_grid

#if USE_GC==3
bool validate(int rows, int cols, const GCGraph<double> &graph1, const GCGraph<double>::state &state1, const GCGraph2<double> &graph2, const GCGraph2<double>::state &state2) {
	ISO_ASSERT(state1.size() == state2.size());
	auto	i1 = state1.begin();
	for (auto &v2 : state2) {
		if (i1->id != v2.id || i1->weight != v2.weight || i1->step != v2.step || i1->dist != v2.dist)
			return false;
		++i1;
	}
	for (int y = 1; y < rows - 1; y++) {
		for (int x = 1, v = y * cols + x; x < cols - 1; x++, v++) {
			if (graph1.getVertWeight(v) != graph2.getVertWeight(v))
				return false;
			auto	e1	= graph1.getEdges(v);
			int		n1	= e1.size32();
			auto	i1	= e1.begin();
			for (auto &e2 : graph2.getEdges(v)) {
				if (*i1 != e2)
					return false;
				++i1;
			}
		}
	}
	return true;
}
#endif

void iso::grabCut(const block<rgbx8, 2> &img, GMM::Model &bgdModel, GMM::Model &fgdModel, int rectx, int recty, int rectw, int recth, int iterCount, int mode) {
#ifdef USE_OPENCV
	cv::Mat cvmask;
	cv::Mat bgModel,fgModel;
	cv::Mat img2(img.size<2>(), img.size<1>(), CV_8UC4, img[0], img.pitch());

	cv::cvtColor(img2 , img2 , CV_RGBA2RGB);

	cv::grabCut(img2, cvmask, cv::rect(rectx, recty, rectw, recth),  bgModel,fgModel, iterCount,  cv::GC_INIT_WITH_RECT);

#ifndef USE_ISOPOD
	for (int y = 0; y < img.size<2>(); y++)
		for (int x = 0; x < img.size<1>(); x++)
			img[y][x].w.i = cvmask.at<uint8>(y, x);
#endif
#endif

#ifdef USE_ISOPOD
	CriticalSection	cs;
	auto			with(cs);
	timer			time;

	GMM		gmm[2];

	gmm[0] = bgdModel;
	gmm[1] = fgdModel;

	int		cols		= img.size<1>();
	int		rows		= img.size<2>();
	int		num_edges	= 4 * cols * rows - 3 * (cols + rows) + 2;

	block<uint8, 3> mask = make_block((uint8*)&img[0][0], 4, cols, rows).sub<1>(3, 1);

	if (mode != GC_EVAL) {
		if (!(mode & GC_INIT_WITH_MASK))
			fill(mask, GC_BGD);

		if (mode & GC_INIT_WITH_RECT)
			fill(mask.sub<2>(rectx, rectw).sub<3>(recty, recth), GC_PR_FGD);

		//Initialize GMM background and foreground models using kmeans algorithm.
		const int	kMeansItCount	= 10;
		const int	kMeansType		= KMEANS_PP_CENTERS;

		dynamic_array<Vec3f>	samples[2];
		for (int y = 0; y < rows; y++) {
			for (int x = 0; x < cols; x++)
				samples[is_fg(mask[y][x][0])].push_back(img[y][x]);
		}
		ISO_ASSERT(!samples[0].empty() && !samples[1].empty());

		//ISO_TRACEF("time-1 ") << time << '\n';

		parallel_for_block0(0, 2, [&](int j) {
			dynamic_array<int>		labels;
			dynamic_array<Vec3f>	centres;
			kmeans(samples[j], GMM::componentsCount, labels, centres, kMeansItCount, kMeansType);
			GMM::Learning	learning;
			int				*lab = labels;
			for (auto &i : samples[j])
				learning.add(*lab++, to<double>(i));
			gmm[j].endLearning(learning);
		}, 2);

		//ISO_TRACEF("time0 ") << time << '\n';
	}
	if (iterCount <= 0)
		return;

	// calculate beta - parameter of GrabCut algorithm: beta = 1/(2*avg(sqr(||color[i] - color[j]||)))
	double	total_dist2 = 0;
	for (int y = 0; y < rows; y++) {
		for (int x = 0; x < cols; x++) {
			float3	color = to<float>(img[y][x]);
			if (x > 0)
				total_dist2 += dist2(color, (float3)img[y][x - 1]);
			if (y > 0 && x > 0)
				total_dist2 += dist2(color, (float3)img[y - 1][x - 1]);
			if (y > 0)
				total_dist2 += dist2(color, (float3)img[y - 1][x]);
			if (y > 0 && x < cols - 1)
				total_dist2 += dist2(color, (float3)img[y - 1][x + 1]);
		}
	}
	//ISO_TRACEF("time1 ") << time << '\n';

	const double beta		= total_dist2 <= (double)epsilon ? 0 : 1 / (2 * total_dist2 / num_edges);
	const double gamma		= 50;
	const double lambda		= 9 * gamma;

	// calculate weights
	struct Weight {
		double	left;
		double	upleft;
		double	up;
		double	upright;
	};
	auto_block<Weight, 2> weights	= make_auto_block<Weight>(cols, rows);

	const double gamma_rsqrt2	= gamma * rsqrt2;

	parallel_for_block(int_range(rows), [&](int y) {
		for (int x = 0; x < cols; x++) {
			Vec3f	color	= to<float>(img[y][x]);
			Weight&	weight	= weights[y][x];
//			weight.left		= x > 0					? gamma			* exp(-beta * dist2(color, img[y][x - 1]))		: 0;
//			weight.upleft	= x > 0 && y > 0		? gamma_rsqrt2	* exp(-beta * dist2(color, img[y - 1][x - 1]))	: 0;
//			weight.up		= y > 0					? gamma			* exp(-beta * dist2(color, img[y - 1][x]))		: 0;
//			weight.upright	= x < cols - 1 && y > 0	? gamma_rsqrt2	* exp(-beta * dist2(color, img[y - 1][x + 1]))	: 0;
		}
	}, rows);

//	ISO_TRACEF("time2 ") << time << '\n';

#if USE_GC & 1
	// construct GCGraph
	GCGraph<double> graph(cols, rows, 2 * num_edges);
	for (int y = 0, v = 0; y < rows; y++) {
		for (int x = 0; x < cols; x++, v++) {
			// set edges
			if (x > 0)
				graph.addEdges(v, v - 1);
			if (x > 0 && y > 0)
				graph.addEdges(v, v - cols - 1);
			if (y > 0)
				graph.addEdges(v, v - cols);
			if (x < cols - 1 && y > 0)
				graph.addEdges(v, v - cols + 1);
		}
	}
	graph.sortEdgeLists();
#endif
#if USE_GC & 2
	GCGraph2<double> graph2(cols, rows);
#endif

#if USE_GC & 4
	GridGraph_2D<double, double, double, 8> graph3(cols, rows);
#endif

//	ISO_TRACEF("time3 ") << time << '\n';

	auto_block<int, 2> indices	= make_auto_block<int>(cols, rows);

	for (int i = 0; i < iterCount; i++) {
		// assign GMMs components
		parallel_for_block(int_range(rows), [&](int y) {
			for (int x = 0; x < cols; x++) {
				bool	fg	= is_fg(mask[y][x][0]);
				indices[y][x] = gmm[fg].whichComponent(to<double>(img[y][x]));
			}
		});

//		ISO_TRACEF("time4 ") << time << '\n';

		// learn GMMs parameters
		GMM::Learning	learning[2];
		for (int y = 0; y < rows; y++) {
			for (int x = 0; x < cols; x++) {
				bool	fg	= is_fg(mask[y][x][0]);
				int		ci	= indices[y][x];//gmm[fg].whichComponent(img[y][x]);
				learning[fg].add(ci, to<double>(img[y][x]));
			}
		}
		gmm[0].endLearning(learning[0]);
		gmm[1].endLearning(learning[1]);

//		ISO_TRACEF("time5 ") << time << '\n';


	#if USE_GC == 3
		int	validate_frequency	= 100000;
		int	next_validate		= 0;

		for (bool redo = true; redo;) {
			redo = false;
	#endif
			// set GCGraph weights
		#if USE_GC & 4
			graph3.reset();
		#endif
			parallel_for_block(int_range(rows), [&](int y) {
				for (int x = 0, v = y * cols; x < cols; x++, v++) {
					// get t-weights
					double	w0 = 0, w1 = 0;
					if (x > 0 && x < cols - 1 && y > 0 && y < rows - 1) {
						switch (mask[y][x][0]) {
							case GC_PR_BGD:
							case GC_PR_FGD: {
								Vec3d	color	= to<double>(img[y][x]);
								w0 = -log(gmm[0](color));
								w1 = -log(gmm[1](color));
								break;
							}
							case GC_BGD:
								w1 = lambda;
								break;
							default:
								w0 = lambda;
								break;
						}
					}

					// set n-weights
					Weight&	w	= weights[y][x];
				#if USE_GC & 1
					graph.setTermWeights(v, w0, w1);
					int		e	= graph.firstEdge(v);
					if (x > 0)
						e = graph.setEdgeWeights(e, w.left);
					if (x > 0 && y > 0)
						e = graph.setEdgeWeights(e, w.upleft);
					if (y > 0)
						e = graph.setEdgeWeights(e, w.up);
					if (x < cols - 1 && y > 0)
						e = graph.setEdgeWeights(e, w.upright);
				#endif
				#if USE_GC & 2
					graph2.setTermWeights(v, w0, w1);
					if (x > 0)
						graph2.setEdgeWeights(v, graph2.L, w.left);
					if (x > 0 && y > 0)
						graph2.setEdgeWeights(v, graph2.UL, w.upleft);
					if (y > 0)
						graph2.setEdgeWeights(v, graph2.U, w.up);
					if (x < cols - 1 && y > 0)
						graph2.setEdgeWeights(v, graph2.UR, w.upright);
				#endif
				#if USE_GC & 4
					v = graph3.node_id(x, y);
					graph3.set_terminal_cap(v, w0, w1);
					if (x > 0)
						graph3.setEdgeWeights(v, graph3.LE, w.left);
					if (x > 0 && y > 0)
						graph3.setEdgeWeights(v, graph3.LL, w.upleft);
					if (y > 0)
						graph3.setEdgeWeights(v, graph3.EL, w.up);
					if (x < cols - 1 && y > 0)
						graph3.setEdgeWeights(v, graph3.GL, w.upright);
				#endif
				}
			});

//			ISO_TRACEF("time6 ") << time << '\n';

	#if USE_GC == 3
			// estimate segmentation using max flow algorithm
			GCGraph<double>::state	state1;
			GCGraph2<double>::state	state2;
			graph.max_flow_init(state1);
			graph2.max_flow_init(state2);

			for (int step = 0, done = 0; done != 3; ++step) {
				if (next_validate == 0) {
					if (!validate(rows, cols, graph, state1, graph2, state2)) {
						next_validate = step - (validate_frequency + 1) / 2;
						validate_frequency /= 2;
						ISO_ASSERT(validate_frequency);
						redo = true;
						break;
					}
					next_validate = validate_frequency;
				}
				--next_validate;

				if (!(done & 1) && !graph.max_flow_step(state1, step))
					done |= 1;
				if (!(done & 2) && !graph2.max_flow_step(state2, step))
					done |= 2;
			}

		}
	#elif USE_GC == 1
		graph.max_flow();
	#elif USE_GC == 2
		graph2.max_flow();
	#else
		graph3.compute_maxflow();
	#endif
//		ISO_TRACEF("time7 ") << time << '\n';

		for (int y = 0, v = 0; y < rows; y++) {
		#if USE_GC == 4
			v = 0;
		#endif
			for (const auto d : mask[y]) {
				if (is_unsure(d[0]))
				#if USE_GC == 3
					d[0] = (mode & GC_GRAPH2 ? graph2.inSourceSegment(v) : graph.inSourceSegment(v)) ? GC_PR_FGD : GC_PR_BGD;
				#elif USE_GC == 1
					d[0] = graph.inSourceSegment(v) ? GC_PR_FGD : GC_PR_BGD;
				#elif USE_GC == 2
					d[0] = graph2.inSourceSegment(v) ? GC_PR_FGD : GC_PR_BGD;
				#else
					d[0] = graph3.get_segment(graph3.node_id(v, y)) ? GC_PR_BGD : GC_PR_FGD;
				#endif
				++v;
			}
		}

//		ISO_TRACEF("time8 ") << time << '\n';

	}

#ifdef USE_OPENCV
	int	diff = 0;
	for (int y = 0; y < img.size<2>(); y++)
		for (int x = 0; x < img.size<1>(); x++)
			diff += int(img[y][x].w.i != cvmask.at<uint8>(y, x));
	ISO_TRACEF("difference = %f\n", float(diff) / (img.size<2>() * img.size<2>()));
#endif

#endif
}

void iso::grabCut(const block<rgbx8, 2> &img, int x, int y, int w, int h, int iterCount) {
	GMM::Model	model;
	grabCut(img, model, model, x, y, w, h, iterCount, GC_INIT_WITH_RECT);
}


