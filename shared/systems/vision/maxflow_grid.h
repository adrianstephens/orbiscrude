#ifndef MAXFLOW_GRID_H
#define MAXFLOW_GRID_H

#include "base/defs.h"

namespace iso {

struct mem_pool {
	void			*start;
	uint8	*tail;

	void* alloc(size_t size) {
		void* ptr = tail;
		tail += (size + 63) & ~63;
		return ptr;
	}
	template<typename T> T* alloc(size_t size) {
		return (T*)alloc(size * sizeof(T));
	}
	template<typename T> static size_t req(size_t size) {
		return (size * sizeof(T) + 63) & ~63;
	}
	template<typename T> static void clear(T *p, size_t size) {
		memset(p, 0, size * sizeof(T));
	}
	mem_pool() : start(0), tail(0) {}
	~mem_pool() { free(start); }

	bool	init(void *_start) {
		start	= _start;
		tail	= (uint8*)((uintptr_t(start) + 63) & ~63);
		return start != 0;
	}
	bool	init(size_t size) {
		return init(malloc(size));
	}
};

}

void *operator new(size_t s, iso::mem_pool &pool)	{ return pool.alloc(s); }
void *operator new[](size_t s, iso::mem_pool &pool)	{ return pool.alloc(s); }
void operator delete(void*, iso::mem_pool &pool)		{}
void operator delete[](void*, iso::mem_pool &pool)	{}

namespace iso {

template<typename T> struct Queue {
	T*		next;
	T		head;
	T		tail;

	Queue() {}
	Queue(T *p, T _head = 0, T _tail = 0) : next(p), head(_head), tail(_tail) {}
	void	init(T *p, int i = 0)	{ next = p; clear(i); }
	void	clear(int i = 0)		{ head = next[i] = 0; tail = i; }
	bool	empty()					{ return head == 0; }
	T		front()					{ return head; }

	void	quick_push_back(T v)	{ next[tail] = v; next[v] = tail = v; }
	void	push_back(T v)			{ quick_push_back(v); if (!head) head = v; }
	void	check_push_back(T v)	{ if (next[v] == 0) quick_push_back(v); }

	void	push_front(T v)			{ next[v] = head; head = v; }
	T		pop_front()				{ const T t = head; head = next[t]; next[t] = 0; if (t == head) head = tail = 0; return t; }

	void	rewind(int i = 0)		{ head = next[i]; }
	T		get_next(int i)			{ return next[i] == i ? 0 : next[i]; }
};

//-----------------------------------------------------------------------------
//	GridGraph
//-----------------------------------------------------------------------------

template<typename E, typename D, bool SAT> class GridGraph_state {
public:
	typedef typename D::Edge Edge;
	enum Label		{LABEL_F, LABEL_S, LABEL_T};
	uint8*			_flags;
	E*				rc[D::N];

	Label	label(int v)				const	{ return (Label)(_flags[v] & 3); }
	void	set_label(int v, Label l)			{ _flags[v] = (_flags[v] & ~3) | l; }
	bool	not_sat(int v, Edge e)		const	{ return !!(_flags[v] & ((1 << 2) << e)); }
	void	set_sat(int v, Edge e)		const	{ _flags[v] &= ~((1 << 2) << e); }
	void	clear_sat(int v, Edge e)	const	{ _flags[v] |= (1 << 2) << e; }
	void	mask_sat(int v, uint8 m)	const	{ _flags[v] &= ~m << 2; }

	E		get_flow(int v, Edge e)	const	{
		return rc[e][v];
	}
	void	set_flow(int v, Edge e, E x) const {
		if ((rc[e][v] = x))
			clear_sat(v, e);
	}
	void	add_flow(int v, Edge e, E x) const {
		rc[e][v]	+= x;
		clear_sat(v, e);
	}
	bool	sub_flow(int v, Edge e, E x) const {
		if (rc[e][v] -= x)
			return false;
		set_sat(v, e);
		return true;
	}
};


template<typename E, typename D> class GridGraph_state<E, D, false> {
public:
	typedef typename D::Edge Edge;
	enum Label		{LABEL_F, LABEL_S, LABEL_T};
	uint8*			_flags;
	E*				rc[D::N];

	Label	label(int v)				const	{ return (Label)_flags[v]; }
	void	set_label(int v, Label l)			{ _flags[v] = l; }
	bool	not_sat(int v, Edge e)		const	{ return rc[e][v] != 0; }
	void	set_sat(int v, Edge e)		const	{}
	void	clear_sat(int v, Edge e)	const	{}

		E		get_flow(int v, Edge e)	const	{
		return rc[e][v];
	}
	void	set_flow(int v, Edge e, E x) const {
		if ((rc[e][v] = x))
			clear_sat(v, e);
	}
	void	add_flow(int v, Edge e, E x) const {
		rc[e][v]	+= x;
		clear_sat(v, e);
	}
	bool	sub_flow(int v, Edge e, E x) const {
		if (rc[e][v] -= x)
			return false;
		set_sat(v, e);
		return true;
	}
};

template<typename T, typename E, typename F, typename D, bool SAT> class GridGraph_base : public GridGraph_state<E, D, SAT>, public D {
protected:
	typedef GridGraph_state<E, D, SAT>	B;
	using typename D::Edge;
	using typename D::Edges;
	enum		{ NONE = D::N, TERMINAL = D::N + 1 };

	F			MAXFLOW;

	uint8*		_parent;
	int*		_parent_id;
	T*			_rc_st;
	int*		_dist;
	int*		_timestamp;

	T		set_terminal_cap0(int v, T cap_s, T cap_t) {
		B::_flags[v]	= cap_s == cap_t ? B::LABEL_F : cap_s > cap_t ? B::LABEL_S : B::LABEL_T;
		_parent[v]	= cap_s == cap_t ? NONE : TERMINAL;
		_rc_st[v]	= abs(cap_s - cap_t);
		if (_dist)
			_dist[v] = cap_s == cap_t ? 0 : 1;
		return min(cap_s, cap_t);
	}

	bool	find_origin(const int v, const int TIME);
	int		find_origin_dist(int v, const int TIME);
	void	activate(Queue<int> &active, int v);
	bool	grow(Queue<int> &active, int &vs, int &vt, Edge &e);

	template<typename Q> E		augment(Q &orphans, const int vs, const int vt, const Edge e);
	template<typename Q> void	adopt(Queue<int> &active, Q &orphans, Q &orphans2, Q &free_nodes, const int TIME);
	template<typename Q> void	adopt_dist(Queue<int> &active, Q &orphans, Q &orphans2, Q &free_nodes, const int TIME);

	Edge	parent(int v)			const		{ return (Edge)_parent[v]; }
	int		parent_id(int v)		const		{ return _parent_id[v]; }
	int		timestamp(int v)		const		{ return _timestamp[v]; }
	T		rc_st(int v)			const		{ return _rc_st[v]; }

	void	set_parent(int v, int p, Edge e)	{ _parent_id[v] = p; _parent[v] = e; }
	void	no_parent(int v)					{ _parent[v] = NONE; }
	void	set_timestamp(int v, int t)			{ _timestamp[v] = t; }
	T&		rc_st(int v)						{ return _rc_st[v]; }
	int&	dist(int v)							{ return _dist[v]; }

public:
	GridGraph_base() : MAXFLOW(0), _dist(0) {}
	int			get_segment(int v)	const { return B::label(v) == B::LABEL_T; }
	F			get_flow()			const { return MAXFLOW; }
};

template<typename T, typename E, typename F, typename D, bool SAT>
void GridGraph_base<T, E, F, D, SAT>::activate(Queue<int> &active, int v) {
	const int lv = B::label(v);
	if (lv != B::LABEL_F) {
		const Edges	N_ID = D::get_edges(v);
		for (Edge i = Edge(0); i < D::N; ++i) {
			const int v2 = N_ID[i];
			if (lv == B::LABEL_S) {
				if (B::not_sat(v, i) && B::label(v2) != lv) {
					active.check_push_back(v);
					break;
				}
			} else {
				if (B::not_sat(v2, !i) && B::label(v2) == B::LABEL_F) {
					active.check_push_back(v);
					break;
				}
			}
		}
	}
}

template<typename T, typename E, typename F, typename D, bool SAT>
bool GridGraph_base<T, E, F, D, SAT>::grow(Queue<int> &active, int &vs, int &vt, Edge &e) {
	while (const int v = active.front()) {
		const int lv		= B::label(v);
		const Edges	N_ID	= D::get_edges(v);

		if (lv == B::LABEL_S) {
			for (Edge i = Edge(0); i < D::N; ++i) {
				const int v2 = N_ID[i];
				if (B::not_sat(v, i)) {
					if (B::label(v2) == B::LABEL_T) {
						vs	= v;
						vt	= v2;
						e	= !i;
						return true;
					}
					if (B::label(v2) == B::LABEL_F) {
						B::set_label(v2, B::LABEL_S);
						active.check_push_back(v2);
						set_parent(v2, v, !i);
					}
				}
			}
		} else if (lv == B::LABEL_T) {
			for (Edge i = Edge(0); i < D::N; ++i) {
				const int v2 = N_ID[i];
				if (B::not_sat(v2, !i)) {
					if (B::label(v2) == B::LABEL_S) {
						vt	= v;
						vs	= v2;
						e	= i;
						return true;
					}
					if (B::label(v2) == B::LABEL_F) {
						B::set_label(v2, B::LABEL_T);
						active.check_push_back(v2);
						set_parent(v2, v, !i);
					}
				}
			}
		}

		active.pop_front();
	}

	return false;
}

template<typename T, typename E, typename F, typename D, bool SAT>
bool GridGraph_base<T, E, F, D, SAT>::find_origin(const int v0, const int TIME) {
	for (int v = v0; ; v = parent_id(v)) {
		if (timestamp(v) == TIME) {
			for (v = v0; timestamp(v) != TIME; v = parent_id(v))
				set_timestamp(v, TIME);
			return true;
		}

		if (parent(v) == NONE)
			return false;

		if (parent(v) == TERMINAL) {
			for (v = v0; parent(v) != TERMINAL; v = parent_id(v))
				set_timestamp(v, TIME);
			set_timestamp(v, TIME);
			return true;
		}
	}
}

template<typename T, typename E, typename F, typename D, bool SAT> template<typename Q>
E GridGraph_base<T, E, F, D, SAT>::augment(Q &orphans, const int vs, const int vt, const Edge e) {
	int	v;
	E	minrf = B::get_flow(vs, !e);

	//find_minrf_s
	for (v = vs; parent(v) != TERMINAL; v = parent_id(v))
		minrf	= min(minrf, B::get_flow(parent_id(v), !parent(v)));
	minrf = min(minrf, rc_st(v));

	//find_minrf_t
	for (v = vt; parent(v) != TERMINAL; v = parent_id(v))
		minrf	= min(minrf, B::get_flow(v, parent(v)));
	minrf = min(minrf, rc_st(v));

	B::sub_flow(vs, !e, minrf);
	B::add_flow(vt, e, minrf);

	//aug_s
	for (v = vs; parent(v) != TERMINAL; v = parent_id(v)) {
		Edge	p = (Edge)parent(v);
		B::add_flow(v, p, minrf);

		if (B::sub_flow(parent_id(v), !p, minrf)) {
			no_parent(v);
			orphans.push_front(v);
		}
	}

	if ((rc_st(v) -= minrf) == 0) {
		no_parent(v);
		orphans.push_front(v);
	}

	//aug_t
	for (v = vt; parent(v) != TERMINAL; v = parent_id(v)) {
		Edge	p = (Edge)parent(v);
		B::add_flow(parent_id(v), !p, minrf);

		if (B::sub_flow(v, p, minrf)) {
			no_parent(v);
			orphans.push_front(v);
		}
	}

	if ((rc_st(v) -= minrf) == 0) {
		no_parent(v);
		orphans.push_front(v);
	}

	return minrf;
}

template<typename T, typename E, typename F, typename D, bool SAT> template<typename Q>
void GridGraph_base<T, E, F, D, SAT>::adopt(Queue<int> &active, Q &orphans, Q &orphans2, Q &free_nodes, const int TIME) {
	while (!orphans.empty() || !orphans2.empty()) {
		const int v			= !orphans2.empty() ? orphans2.pop_front() : orphans.pop_front();
		const Edges	N_ID	= D::get_edges(v);
		const int lv		= B::label(v);

		if (lv == B::LABEL_S) {
			for (Edge i = Edge(0); i < D::N; ++i) {
				const int v2 = N_ID[i];
				if (B::not_sat(v2, !i) && B::label(v2) == B::LABEL_S && find_origin(v2, TIME)) {
					set_timestamp(v, TIME);
					set_parent(v, v2, i);
					goto next;
				}
			}

		} else if (lv == B::LABEL_T) {
			for (Edge i = Edge(0); i < D::N; ++i) {
				const int v2 = N_ID[i];
				if (B::not_sat(v, i) && B::label(v2) == B::LABEL_T && find_origin(v2, TIME)) {
					set_timestamp(v, TIME);
					set_parent(v, v2, i);
					goto next;
				}
			}
		}

		B::set_label(v, B::LABEL_F);
		free_nodes.push_back(v);

		for (Edge i = Edge(0); i < D::N; ++i) {
			const int v2 = N_ID[i];
			if (B::label(v2) == lv && parent(v2) == !i) {
				no_parent(v2);
				orphans2.push_back(v2);
			}
		}
	next:
		;
	}

	while (!free_nodes.empty()) {
		const int v			= free_nodes.pop_front();
		const Edges	N_ID	= D::get_edges(v);

		for (Edge i = Edge(0); i < D::N; ++i) {
			const int v2 = N_ID[i];
			if (B::not_sat(v2, !i) && B::label(v2) == B::LABEL_S)
				active.check_push_back(v2);
			if (B::not_sat(v, i) && B::label(v2) == B::LABEL_T)
				active.check_push_back(v2);
		}
	}
}

template<typename T, typename E, typename F, typename D, bool SAT>
int GridGraph_base<T, E, F, D, SAT>::find_origin_dist(int v, const int TIME) {
	const int start_v = v;

	for (int d = 1; ; v = parent_id(v), d++) {
		if (timestamp(v) == TIME) {
			int	d2 = (d += dist(v));
			for (v = start_v; timestamp(v) != TIME; v = parent_id(v)) {
				dist(v)			= d2--;
				timestamp(v)	= TIME;
			}
			return d;
		}

		if (parent(v) == NONE)
			return 0x7fffffff;

		if (parent(v) == TERMINAL) {
			int	d2 = d;
			for (v = start_v; parent(v) != TERMINAL; v = parent_id(v)) {
				dist(v)			= d2--;
				timestamp(v)	= TIME;
			}
			timestamp(v)	= TIME;
			dist(v)			= 1;
			return d;
		}
	}
}

template<typename T, typename E, typename F, typename D, bool SAT> template<typename Q>
void GridGraph_base<T, E, F, D, SAT>::adopt_dist(Queue<int> &active, Q &orphans, Q &orphans2, Q &free_nodes, const int TIME) {
	while (!orphans.empty() || !orphans2.empty()) {
		const int v			= !orphans2.empty() ? orphans2.pop_front() : orphans.pop_front();
		const Edges	N_ID	= B::get_edges(v);
		const int lv		= B::label(v);

		int		min_d	= 0x7fffffff;
		int		min_i	= -1;

		if (lv == B::LABEL_S) {
			for (int i = 0; i < D::N; i++) {
				const int N = N_ID[i];
				if (B::not_sat(N, i ^ 1) && B::label(N) == B::LABEL_S) {
					const int d = find_origin_dist(N, TIME);
					if (d < min_d) {
						min_i	= i;
						min_d	= d;
					}
				}
			}

		} else if (lv == B::LABEL_T) {
			for (int i = 0; i < D::N; i++) {
				const int N = N_ID[i];
				if (B::not_sat(v, i) && B::label(N) == B::LABEL_T) {
					const int d = find_origin_dist(N, TIME);
					if (d < min_d) {
						min_i	= i;
						min_d	= d;
					}
				}
			}
		}

		if (min_i != -1) {
			dist(v)			= min_d + 1;
			timestamp(v)	= TIME;
			parent(v)		= min_i;
			parent_id(v)	= N_ID[min_i];

		} else {
			set_label(v, B::LABEL_F);
			free_nodes.push_back(v);

			for (int i = 0; i < D::N; i++) {
				const int N = N_ID[i];
				if (B::label(N) == lv && parent(N) == (i ^ 1)) {
					parent(N) = NONE;
					orphans2.push_back(N);
				}
			}
		}
	}

	while (!free_nodes.empty()) {
		const int v			= free_nodes.pop_front();
		const Edges	N_ID	= B::get_edges(v);

		for (int i = 0; i < D::N; i++) {
			const int N = N_ID[i];
			if (B::not_sat(N, i ^ 1) && B::label(N) == B::LABEL_S)
				active.check_push_back(N);
			if (B::not_sat(v, i) && B::label(N) == B::LABEL_T)
				active.check_push_back(N);
		}
	}
}


template<typename T> struct FQueue {
	T*		buffer;
	T*		head;
	T*		tail;

	FQueue() : buffer(0) {}
	void	init(T *_buffer, int i = 0)	{ buffer = _buffer; clear(i); }
	void	clear(int i = 0)			{ head = tail = buffer + i;	}
	bool	empty()						{ return head == tail;	}
	void	push_back(T item)			{ *tail++ = item;	}
	T		pop_back()					{ return *--tail;	}
	void	push_front(T item)			{ *--head = item;	}
	T		pop_front()					{ return *head++;	}
	T		front()						{ return *head;	}
};

template<typename T, typename E, typename F, typename D> class GridGraph : public GridGraph_base<T, E, F, D, false> {
	typedef GridGraph_base<T, E, F, D, false>	B;
protected:
	mem_pool	pool;

	Queue<int>		active;
	FQueue<int>		orphans;
	FQueue<int>		orphans2;
	FQueue<int>		free_nodes;

	void	init(int N);
	void	reset(int N) {
		memset(B::_parent, B::NONE, N);
		pool.clear(B::_parent_id, N);
		pool.clear(B::_flags, N);
		pool.clear(B::_timestamp, N);
		pool.clear(B::_rc_st, N);

		for (int i = 0; i < D::N; i++)
			pool.clear(B::rc[i], N);

		B::MAXFLOW	= 0;
	}

public:
	void		set_terminal_cap(int v, T cap_s, T cap_t) {
		B::MAXFLOW += B::set_terminal_cap0(v, cap_s, cap_t);
		if (cap_s != cap_t)
			free_nodes.push_back(v);
	}
	bool		bad_alloc()			const { return !pool; }
};

template<typename T, typename E, typename F, typename D>
void GridGraph<T, E, F, D>::init(int N) {
	size_t mem_pool_size = 64
		+ pool.req<uint8>	(N)		// flags
		+ pool.req<uint8>	(N)		// parent
		+ pool.req<int>		(N)		// parent_id
		+ pool.req<int>		(N)		// dist
		+ pool.req<int>		(N)		// timestamp
		+ pool.req<T>		(N)		// rc_st
		+ pool.req<E>		(N) * D::N	// rc[D::N]
		+ pool.req<int>		(N)		// orphans
		+ pool.req<int>		(N)		// orphans2
		+ pool.req<int>		(N)		// free_nodes
		+ pool.req<int>		(N)		// active
	;

	if (!pool.init(calloc(mem_pool_size, 1)))
		return;

	B::_flags		= pool.alloc<uint8>(N);
	B::_parent		= pool.alloc<uint8>(N);
	B::_parent_id	= pool.alloc<int>(N);
	B::_dist		= 0;//pool.alloc<int>(N);
	B::_timestamp	= pool.alloc<int>(N);
	B::_rc_st		= pool.alloc<T>(N);

	for (int i = 0; i < D::N; i++)
		B::rc[i] = pool.alloc<E>(N);

	orphans.init(pool.alloc<int>(N), N);
	orphans2.init(pool.alloc<int>(N));
	free_nodes.init(pool.alloc<int>(N));
	active.init(pool.alloc<int>(N));

	reset(N);
}

//-----------------------------------------------------------------------------
//	2D - N edges
//-----------------------------------------------------------------------------

struct Directions2D_linear_base {
	int		W, H;
	int	set_size(int w, int h) {
		W	= (w + 2 + 7) & ~7;
		H	= (h + 2 + 7) & ~7;
		return W * H;
	}
	int	node_id(int x, int y) const {
		++x;
		++y;
		return x + y * W;
	}
	pair<int,int>	node_coord(int v) const {
		return pair<int,int>(v % W - 1, v / W - 1);
	}
};

struct Directions2D_8x8_base {
	int		W, H;
	int		WB;
	int		YOFS;
	int	set_size(int w, int h) {
		W	= (w + 2 + 7) & ~7;
		H	= (h + 2 + 7) & ~7;
		WB	= W / 8;
		YOFS = (WB - 1) * 64 + 8;
		return W * H;
	}
	int	node_id(int x, int y) const {
		++x;
		++y;
		return (((x >> 3) + (y >> 3) * WB) << 6) + (x & 7) + ((y & 7) << 3);
	}
	pair<int,int>	node_coord(int v) const {
		int	b = v / 64, xb = b % WB, yb = b / WB;
		return pair<int,int>((xb << 3) + (v & 7) - 1, (yb << 3) + ((v >> 3) & 7) - 1);
	}
};

template<int N> struct Directions2D;

#if 0
template<> struct Directions2D<4> : Directions2D_linear_base {
	enum Edge { LE, GE, EL, EG };
	struct Edges {
		int	N_LE, N_GE, N_EL, N_EG;
		Edges(int v, const int YOFS) :
			N_LE(v - 1),
			N_GE(v + 1),
			N_EL(v - YOFS),
			N_EG(v + YOFS)
		{}
		int	operator[](int i) const { return (&N_LE)[i]; }
	};
	Edges			get_edges(int v)	{
		return Edges(v, W);
	}
	static int		get_dir(int dx, int dy) {
		static const uint8 table[3][3] = {
			{ 255,  EL,	255},
			{  LE, 255,	 GE},
			{ 255,  EG,	255}
		};
		return table[dy + 1][dx + 1];
	}
};
#else
template<> struct Directions2D<4> : Directions2D_8x8_base {
	enum Edge { LE, GE, EL, EG, N };

	friend Edge&	operator++(Edge& e)	{ return e = Edge(e + 1); }
	friend Edge		operator!(Edge e)	{ return Edge(e ^ 1); }

	struct Edges {
		int	N_LE, N_GE, N_EL, N_EG;
		Edges(int v, const int YOFS) :
			N_LE( v & 0x07 ? v - 1 : v - 64 + 7),
			N_GE(~v & 0x07 ? v + 1 : v + 64 - 7),
			N_EL( v & 0x38 ? v - 8 : v - YOFS),
			N_EG(~v & 0x38 ? v + 8 : v + YOFS)
		{}
		int	operator[](Edge i) const { return (&N_LE)[i]; }
	};
	Edges			get_edges(int v)	{
		return Edges(v, YOFS);
	}
	static int		get_dir(int dx, int dy) {
		static const uint8 table[3][3] = {
			{ 255,  EL,	255},
			{  LE, 255,	 GE},
			{ 255,  EG,	255}
		};
		return table[dy + 1][dx + 1];
	}
};
#endif

template<> struct Directions2D<8> : Directions2D_8x8_base {
	enum Edge { LE, GE, EL, EG, LL, GG, LG, GL, N };

	friend Edge&	operator++(Edge& e)	{ return e = Edge(e + 1); }
	friend Edge		operator!(Edge e)	{ return Edge(e ^ 1); }

	struct Edges {
		int N_LE, N_GE, N_EL, N_EG, N_LL, N_GG, N_LG, N_GL;
		Edges(int v, const int YOFS) :
			N_LE( v & 0x07 ? v - 1 : v - 57),
			N_GE(~v & 0x07 ? v + 1 : v + 57),
			N_EL( v & 0x38 ? v - 8 : v - YOFS),
			N_EG(~v & 0x38 ? v + 8 : v + YOFS),
			N_LL(N_LE + N_EL - v),
			N_GG(N_GE + N_EG - v),
			N_LG(N_LE + N_EG - v),
			N_GL(N_GE + N_EL - v)
		{}
		int	operator[](Edge i) const { return (&N_LE)[i]; }
	};
	Edges			get_edges(int v)	{ return Edges(v, YOFS); }
	static int		get_dir(int dx, int dy) {
		static const uint8 table[3][3] = {
			{ LL,  EL, GL },
			{ LE, 255, GE },
			{ LG,  EG, GG }
		};
		return table[dy + 1][dx + 1];
	}
};

template<typename T, typename E, typename F, int C> class GridGraph_2D : public GridGraph<T, E, F, Directions2D<C> > {
	typedef GridGraph<T, E, F, Directions2D<C> > B;
	using typename B::Edge;
	const int		ow, oh;
public:
	GridGraph_2D(int w, int h) : ow(w), oh(h) {
		B::init(this->set_size(w, h));
	}
	void	reset() {
		B::reset(B::W * B::H);
	}

	void	set_neighbor_cap(int v, int dx, int dy, E cap) {
		set_flow(v, B::get_dir(dx, dy), cap);
	}

	void	setEdgeWeights(int v, Edge e, E w, E revw) {
		B::set_flow(v, e, w);
		B::set_flow(B::get_edges(v)[e], !e, revw);
	}
	void	setEdgeWeights(int v, Edge e, E w) {
		setEdgeWeights(v, e, w, w);
	}

	template<typename T1, typename E1> void set_caps(const T1* cap_s, const T1* cap_t, const E1* cap_edges[C]);
	void	compute_maxflow();
};

template<typename T, typename E, typename F, int C> template<typename T1, typename E1>
void GridGraph_2D<T, E, F, C>::set_caps(const T1* cap_s, const T1* cap_t, const E1* cap_edges[C]) {
	B::reset(B::W * B::H);

	for (int i = 0, y = 0; y < oh; y++) {
		for (int x = 0; x < ow; x++, i++) {
			const int v = B::node_id(x, y);
			B::MAXFLOW += set_terminal_cap0(v, cap_s[i], cap_t[i]);
			for (Edge j = Edge(0); j < C; ++j)
				set_flow(v, j, cap_edges[j][i]);
		}
	}
}

template<typename T, typename E, typename F, int C>
void GridGraph_2D<T, E, F, C>::compute_maxflow() {
	B::active.clear();
	for (int v = 0, N = B::W * B::H; v < N; v++)
		B::activate(B::active, v);
	B::active.rewind();

	int		vs, vt;
	Edge	e;
	for (int TIME = 1; B::grow(B::active, vs, vt, e); ++TIME) {
		B::MAXFLOW += B::augment(B::orphans, vs, vt, e);
		B::orphans2.clear();
		B::free_nodes.clear();
		B::adopt(B::active, B::orphans, B::orphans2, B::free_nodes, TIME);
	}
}

//-----------------------------------------------------------------------------
//	3D - N edges
//-----------------------------------------------------------------------------

struct Directions3D_base {
	int		W, H, D;
	int		WB, WBHB;
	int		YOFS, ZOFS;

	int	set_size(int w, int h, int d) {
		W	= ((w + 2 + 3) & ~3);
		H	= ((h + 2 + 3) & ~3);
		D	= ((d + 2 + 3) & ~3);

		WB		= W / 4;
		WBHB	= WB * (H / 4);
		YOFS	= WBHB * 64 - 3 * 4;
		ZOFS	= WBHB * 64 - 3 * (4 * 4);

		return W * D * H;
	}
	int	node_id(int x, int y, int z) const {
		++x;
		++y;
		++z;
		return (((x >> 2) + ((y >> 2) * WB) + ((z >> 2) * WBHB)) << 6) + ((x & 3) + ((y & 3) << 2) + ((z & 3) << 4));
	}
};

template<int N> struct Directions3D;

template<> struct Directions3D<6> : Directions3D_base {
	enum Edge { LEE, GEE, ELE, EGE, EEL, EEG, N };
	struct Edges {
		int	N_LEE, N_GEE, N_ELE, N_EGE, N_EEL, N_EEG;
		Edges(int v, const int YOFS, const int ZOFS) :
			N_LEE( v & 0x03 ? v - 1   : v - 61  ),
			N_GEE(~v & 0x03 ? v + 1   : v + 61  ),
			N_ELE( v & 0x0C ? v - 4   : v - YOFS),
			N_EGE(~v & 0x0C ? v + 4   : v + YOFS),
			N_EEL( v & 0x30 ? v - 16  : v - ZOFS),
			N_EEG(~v & 0x30 ? v + 16  : v + ZOFS)
		{}
		int	operator[](Edge i) const { return (&N_LEE)[i]; }
	};
	Edges			get_edges(int v)	{ return Edges(v, YOFS, ZOFS); }
	static int		get_dir(int dx, int dy, int dz) {
		static const uint8 table[3][3][3] = { {
			{255,255,255},
			{255,EEL,255},
			{255,255,255}
		}, {
			{255,ELE,255},
			{LEE,255,GEE},
			{255,EGE,255}
		}, {
			{255,255,255},
			{255,EEG,255},
			{255,255,255}
		} };
		return table[dz + 1][dy + 1][dx + 1];
	}
};

template<> struct Directions3D<26> : Directions3D_base {
	enum Edge {
		LEE, GEE, ELE, EGE, EEL, EEG,
		LLE, GGE, LGE, GLE,
		LEL, GEG, LEG, GEL,
		ELL, EGG, ELG, EGL,
		LLL, GGG, LLG, GGL, LGL, GLG, LGG, GLL,
		N,
	};
	struct Edges {
		int	N_LEE, N_GEE, N_ELE, N_EGE, N_EEL, N_EEG;
		int	N_LLE, N_GGE, N_LGE, N_GLE;
		int	N_LEL, N_GEG, N_LEG, N_GEL;
		int	N_ELL, N_EGG, N_ELG, N_EGL;
		int	N_LLL, N_GGG, N_LLG, N_GGL, N_LGL, N_GLG, N_LGG, N_GLL;
		Edges(int v, const int YOFS, const int ZOFS) :
			N_LEE( v & 0x03 ? v - 1   : v - 61  ),
			N_GEE(~v & 0x03 ? v + 1   : v + 61  ),
			N_ELE( v & 0x0C ? v - 4   : v - YOFS),
			N_EGE(~v & 0x0C ? v + 4   : v + YOFS),
			N_EEL( v & 0x30 ? v - 16  : v - ZOFS),
			N_EEG(~v & 0x30 ? v + 16  : v + ZOFS),

			N_LLE(N_LEE + N_ELE - v),
			N_GGE(N_GEE + N_EGE - v),
			N_LGE(N_LEE + N_EGE - v),
			N_GLE(N_GEE + N_ELE - v),

			N_LEL(N_LEE + N_EEL - v),
			N_GEG(N_GEE + N_EEG - v),
			N_LEG(N_LEE + N_EEG - v),
			N_GEL(N_GEE + N_EEL - v),

			N_ELL(N_ELE + N_EEL - v),
			N_EGG(N_EGE + N_EEG - v),
			N_ELG(N_ELE + N_EEG - v),
			N_EGL(N_EGE + N_EEL - v),

			N_LLL(N_LEE + N_ELE + N_EEL - v - v),
			N_GGG(N_GEE + N_EGE + N_EEG - v - v),
			N_LLG(N_LEE + N_ELE + N_EEG - v - v),
			N_GGL(N_GEE + N_EGE + N_EEL - v - v),
			N_LGL(N_LEE + N_EGE + N_EEL - v - v),
			N_GLG(N_GEE + N_ELE + N_EEG - v - v),
			N_LGG(N_LEE + N_EGE + N_EEG - v - v),
			N_GLL(N_GEE + N_ELE + N_EEL - v - v)
		{}
		int	operator[](Edge i) const { return (&N_LEE)[i]; }
	};
	Edges			get_edges(int v)	{ return Edges(v, YOFS, ZOFS); }
	static int		get_dir(int dx, int dy, int dz) {
		static const uint8 table[3][3][3] = { {
			{LLL,ELL,GLL},
			{LEL,EEL,GEL},
			{LGL,EGL,GGL}
		}, {
			{LLE,ELE,GLE},
			{LEE,255,GEE},
			{LGE,EGE,GGE}
		}, {
			{LLG,ELG,GLG},
			{LEG,EEG,GEG},
			{LGG,EGG,GGG}
		} };
		return table[dz + 1][dy + 1][dx + 1];
	}
};

template<typename T, typename E, typename F, int C> class GridGraph_3D :  public GridGraph<T, E, F, Directions3D<C> > {
	typedef GridGraph<T, E, F, Directions2D<C> > B;
	using typename B::Edge;
	const int		ow, oh, od;
public:
	GridGraph_3D(int w, int h, int d) : ow(w), oh(h), od(d) {
		this->init(this->set_size(w, d, h));
	}
	void set_neighbor_cap(int v, int dx, int dy, int dz, E cap) {
		this->set_flow(v, this->get_dir(dx, dy, dz), cap);
	}
	template<typename T1, typename E1> void set_caps(const T1* cap_s, const T1* cap_t, const E1* cap_edges[C]);
	void	compute_maxflow();
};

template<typename T, typename E, typename F, int C> template<typename T1, typename E1>
void GridGraph_3D<T, E, F, C>::set_caps(const T1* cap_s, const T1* cap_t, const E1* cap_edges[C]) {
	reset(B::W * B::H * B::D);

	for (int i = 0, z = 0; z < od; z++) {
		for (int y = 0; y < oh; y++) {
			for (int x = 0; x < ow; x++, i++) {
				const int v = B::node_id(x, y);
				B::MAXFLOW += set_terminal_cap0(v, cap_s[i], cap_t[i]);
				for (int j = 0; j < C; j++)
					this->set_flow(v, j, cap_edges[j][i]);
			}
		}
	}
}

template<typename T, typename E, typename F, int C>
void GridGraph_3D<T, E, F, C>::compute_maxflow() {
	B::active.clear();
	for (int v = 0, N = B::W * B::H * B::D; v < N; v++)
		B::activate(B::active, v);
	B::active.rewind();

	int		vs, vt;
	Edge	e;
	for (int TIME = 1; B::grow(vs, vt, e); ++TIME) {
		B::MAXFLOW += B::augment(B::orphans, vs, vt, e);
		B::orphans2.clear();
		B::free_nodes.clear();
		adopt(B::active, B::orphans, B::orphans2, B::free_nodes, TIME);
	}
}
} // namespace iso
#endif // MAXFLOW_GRID_H
