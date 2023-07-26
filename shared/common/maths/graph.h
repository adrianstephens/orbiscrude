#ifndef GRAPH_H
#define GRAPH_H

#include "base/defs.h"
#include "base/hash.h"
#include "base/list.h"
#include "base/algorithm.h"
#include "comp_geom.h"

namespace iso {

//template<typename N>	decltype(auto)	outgoing(const N &n);
//template<typename N>	decltype(auto)	incoming(const N &n);

//template<typename N> struct default_inout {
//	static decltype(auto)	outgoing(const N &n)	{ using iso::outgoing; return outgoing(n); }
//	static decltype(auto)	incoming(const N &n)	{ using iso::incoming; return incoming(n); }
//};

//-----------------------------------------------------------------------------
//	graph_edges
//-----------------------------------------------------------------------------

template<class E> struct edge: slink_base<edge<E>>, welded_half<edge<E>, true> {
	E			e;
	edge(edge *f)				: welded_half<edge, true>(f) {}
	edge(edge *f, const E &e)	: welded_half<edge, true>(f), e(e) {}
};

template<typename E> struct link_payload<edge<E>> {
	static	E	*get(edge<E> *p)	{ return &p->e; }
};

template<class E> struct graph_edges0 {
	typedef iso::edge<E>			edge;
	typedef	slist_base<edge>		edges_t;
	typedef typename edge::pair_t	edge_pair_t;

	edges_t outgoing, incoming;

	friend edge_pair_t *add(graph_edges0 *from, graph_edges0 *to) {
		auto	*pair	= new edge_pair_t;
		from->outgoing.push_front(&pair->h0);
		to->incoming.push_front(&pair->h1);
		return pair;
	}
	friend edge_pair_t *add(graph_edges0 *from, graph_edges0 *to, const E &a, const E &b) {
		auto	*pair	= new edge_pair_t(a, b);
		from->outgoing.push_front(&pair->h0);
		to->incoming.push_front(&pair->h1);
		return pair;
	}

	friend bool remove(edge *e, graph_edges0 *from, graph_edges0 *to) {
		if (auto prev = from->outgoing.prev(e)) {
			prev->unlink_next();
			if (auto prev = to->incoming.prev(e->flip())) {
				prev->unlink_next();
				delete e->get_pair();
				return true;
			}
		}
		return false;
	}

	~graph_edges0() {
		while (!outgoing.empty())
			outgoing.pop_front();
		while (!incoming.empty())
			incoming.pop_front();
	}

	friend edges_t&			outgoing(graph_edges0 &p)		{ return p.outgoing; }
	friend edges_t&			incoming(graph_edges0 &p)		{ return p.incoming; }
	friend const edges_t&	outgoing(const graph_edges0 &p)	{ return p.outgoing; }
	friend const edges_t&	incoming(const graph_edges0 &p)	{ return p.incoming; }
};

template<typename T> auto& outgoing(const _not<T> &p) { return incoming(p.t); }
template<typename T> auto& incoming(const _not<T> &p) { return outgoing(p.t); }


template<class N> struct graph_edges : graph_edges0<N*> {
	typedef graph_edges0<N*>	B;
	using typename B::edges_t;
	using B::outgoing;
	using B::incoming;

	~graph_edges() {
		while (!outgoing.empty()) {
			auto	j = outgoing.begin();
			if (!remove(j.link(), this, *j))
				break;
		}
		while (!incoming.empty()) {
			auto	j = incoming.begin();
			if (!remove(j.link(), this, *j))
				break;
		}
	}

	void	add_edge(N *to) {
		add(this, to, to, static_cast<N*>(this));
	}
	bool	remove_edge(N *to) {
		if (auto e = find_check(outgoing, to))
			return remove(e.link(), this, to);
		return false;
	}

	void	merge(edges_t &dest, edges_t &srce) {
		while (!srce.empty()) {
			auto i			= srce.pop_front();
			i->flip()->e	= static_cast<N*>(this);
			dest.push_front(i);
		}
	}
	void merge(N *srce) {
		remove_edge(srce);
		merge(outgoing, srce->outgoing);
		merge(incoming, srce->incoming);
	}
};


template<typename N> struct default_inout {
	static decltype(auto)	outgoing(const N &n)	{ using iso::outgoing; return outgoing(n); }
	static decltype(auto)	incoming(const N &n)	{ using iso::incoming; return incoming(n); }
};


//-----------------------------------------------------------------------------
//	graph
//-----------------------------------------------------------------------------

template<typename T> struct graph_node : graph_edges<graph_node<T> > {
	T	t;
	graph_node(const T &t) : t(t) {}
};

template<typename T, typename N = graph_node<T>, typename NN = dynamic_array<N*>> struct graph {
	typedef N		node_t;
	NN		nodes;

	graph() {}

	graph(T *p, size_t n) {
		nodes.reserve(n);
		while (n--)
			add_node(*p++);
	}
	template<typename I> graph(I begin, I end) {
		nodes.reserve(distance(begin, end));
		while (begin != end) {
			add_node(*begin);
			++begin;
		}
	}

	template<typename C> graph(C &&c) : nodes(c) {
		nodes.reserve(num_elements(c));
		for (auto &i : c)
			add_node(i);
	}

	node_t *add_node(const T &t) {
		return nodes.push_back(new node_t(t));
	}

	size_t num_vertices() const {
		return nodes.size();
	}
};

//-----------------------------------------------------------------------------
//	union_find
//	efficient technique for	tracking equivalence classes as	pairs of elements are incrementally	unified	into the same class.
//	Uses path compression but without weight-balancing -> worst case O(nlogn), good case O(n)
//-----------------------------------------------------------------------------
template<typename T> class union_find : hash_map<T, T> {
	bool	irep(T e, T &rep) {
		if (T *p = check(e)) {
			T	r	= *p;
			T	t	= e;

			while (r !=	t)
				r = get(t = r);

			while (e !=	r) {
				swap(t = r, get(e));
				e = t;
			}
			rep = r;
			return true;
		}
		return false;
	}
public:

	// put these two elements in the same class; returns true if were different
	bool unify(T e1, T e2) {
		if (e1 == e2)
			return false;

		T	r1, r2;
		if (!irep(e1, r1))
			put(r1 = e1, e1);

		if (!irep(e2, r2))
			put(r2 = e2, e2);

		if (r1 == r2)
			return false;

		put(r1, r2);
		return true;
	}
	// are two elements	in the same	equivalence	class?
	bool equal(T e1, T e2) {
		T	r1, r2;
		return	e1 == e2
			||	(irep(e1, r1) && irep(e2, r2) && r1 == r2);
	}
	// only	valid until	next unify()
	T	representative(T e) {
		T	r;
		return irep(e, r) ? r : e;
	}
};

#if 1
//-----------------------------------------------------------------------------
// Kruskal MinimumSpanningTree:  O(e log(e))	(Prim's algorithm is recommended when e=~n^2)
// Given a graph 'unconnected' consisting solely of nodes, computes the minimum spanning tree 'undirected' over the nodes under the cost metric f
//-----------------------------------------------------------------------------
template<typename G, typename F> bool kruskal_mst(G &&nodes, F&& f) {
	typedef element_t<G>	T;

	struct Edge {
		T		v1, v2;
		float	w;
		Edge(const T &v1, const T &v2, float w) : v1(v1), v2(v2), w(w) {}
	};

	priority_queue<dynamic_array<Edge>> edges;
	
	for (auto &v1 : nodes) {
		for (auto &v2 : outgoing(v1)) {
			if (g.index_of(v2) <= g.index_of(v1))
				edges.container().emplace_back(v1, v2, f(v1, v2));
		}
	}


	//sort(edges, [](const Edge &a, const Edge &b) { return a.w < b.w; });
	union_find<T>	uf;
	size_t			added 			= 0;
	size_t			num_vertices	= num_elements(unconnected);

	edges.make_heap();
	for (auto &i : edges) {
		if (uf.unify(i.v1, i.v2)) {
			i.v1.add_edge(i.v2);
			if (++added == num_vertices - 1)
				return true;
		}
	}
	return false;
}

// Returns an undirected graph that is the MST of undirected, or empty if g is not connected
template<typename G, typename F> G kruskal_mst(G &undirected, F&& f) {
	G	unconnected = undirected;

	if (!kruskal_mst(undirected, unconnected, f))
		unconnected.clear();
	return unconnected;
}
#endif


//-----------------------------------------------------------------------------
// Prim MinimumSpanningTree
// Returns a undirected graph that is the minimum spanning tree of the full graph between the num points, where the cost metric between two points v1 and v2 is f(v1, v2)
// Prim's algorithm, complexity O(n^2)
//-----------------------------------------------------------------------------

template<typename F> auto prim_mst(int num, F&& f) {
	float	inf = 1e38f;
	dynamic_array<float>	lowcost(num);
	dynamic_array<int>		closest(num);

	for (int i = 1; i < num; i++) {
		lowcost[i] = f(0, i);
		closest[i] = 0;
	}

	dynamic_array<graph_node<int>>	g(int_range(num));

	for (int i = 1; i < num; i++) {
		auto	minj	= argmin(slice(lowcost, 1));
		*minj			= inf;

		int		mini	= lowcost.index_of(minj);
		g[mini].add_edge(&g[closest[mini]]);

		for (int j = 1; i < num; i++) {
			if (lowcost[j] != inf) {
				float pnd = f(mini, j);
				if (pnd < lowcost[j]) {
					lowcost[j] = pnd;
					closest[j] = mini;
				}
			}
		}
	}
	return g;
}

//-----------------------------------------------------------------------------
// Kahn's algorithm
//-----------------------------------------------------------------------------

template<typename G> auto kahn_sort(G &&graph) {
	typedef element_t<G>	N;
	dynamic_array<N*>		L;	// list that will contain the sorted elements
	dynamic_array<N*>		S;	// Set of all nodes with no incoming edge

	for (auto &i : graph) {
		if (outgoing(i).empty())
			S.push_back(i);
	}

	while (!S.empty()) {
		auto	n = S.pop_back_value();		// remove a node n from S
		L.push_back(n);						// add n to tail of L
		while (!incoming(n).empty()) {		// for each node m with an edge e from n to m do
			auto	m = incoming(n).front();
			m->remove_edge(n);				// remove edge e from the graph
			//	if m has no other incoming edges then
			if (outgoing(m).empty())
				S.push_back(m);
		}
	}

	return L;
}

//-----------------------------------------------------------------------------
//shortest paths
//-----------------------------------------------------------------------------

template<typename G, typename F> auto shortest_paths(const G &nodes, F &&f) {
	typedef element_t<G>	N;
	size_t	n = num_elements(nodes);
	dynamic_array<float>	d(n, 0.f);		// shortest path distances
	dynamic_array<N*>		P(n, nullptr);	// predecessors

	//Loop over the vertices u as ordered in V, [starting from s]:
	for (auto &u : nodes) {
		//For each vertex v directly following u (i.e., there exists an edge from u to v)
		for (auto& v : outgoing(u)) {
			auto	w = f(u, v);			// Let w be the weight of the edge from u to v
			if (d[v] > d[u] + w) {			// Relax the edge
				d[v] = d[u] + w;
				P[v] = u;
			}
		}
	}

	return P;
}

//-----------------------------------------------------------------------------
// Colour a graph (heuristically since optimal is NP-hard)
//-----------------------------------------------------------------------------

template<typename G, typename T> int graph_colour(const G &nodes, hash_map<T, int> &colours) {
	int num_colors = 0;

	colours.clear();
	for (auto &vv : nodes) {
		if (colours.check(vv))
			continue;

		colours[vv] = 0;

		dynamic_array_de<T> queue;
		queue.push_back(vv);
		while (!queue.empty()) {
			T v = queue.front();
			queue.pop_front();
			ISO_ASSERT(colours[v] == 0);

			hash_set<int> setcol;
			for (const T& v2 : outgoing(v)) {
				if (T *col2 = colours.check(v2)) {
					if (*col2)
						setcol.insert(*col2);
				} else {
					queue.push_back(v2);
				}
			}

			int col = 1;
			while (setcol.count(col))
				col++;
			colours[v] = col;

			if (col > num_colors)
				num_colors = col;
		}
	}
	return num_colors;
}

//-----------------------------------------------------------------------------
//	postorder - container adapter for postorder traversal
//-----------------------------------------------------------------------------

template<class N, typename IO = default_inout<N>, class S = small_set<const N*, 8>> struct postorder : IO {
	typedef noref_t<decltype(outgoing(declval<const N>()))>	C;
	typedef iterator_t<C>					I;
public:
	struct entry : range<I> {
		const N	*n;
		entry(range<I> r, const N *n) : range<I>(r), n(n) {}
	};
	dynamic_array<entry>	stack;
	S						visited;

	void traverse_child() {
		while (!stack.back().empty()) {
			N *n = stack.back().pop_front_value();
			if (!visited.check_insert(n))
				stack.emplace_back(IO::outgoing(*n), n);
		}
	}
	size_t next() {
		stack.pop_back();
		if (!stack.empty())
			traverse_child();
		return stack.size();
	}
	const N* get() const {
		return stack.back().n;
	}

	struct iterator {
		postorder	&p;
		size_t		depth;
		iterator(postorder &p, size_t depth) : p(p), depth(depth) {}
		bool operator==(const iterator &b)	const	{ return depth == b.depth; }
		bool operator!=(const iterator &b)	const	{ return !(*this == b); }
		const N*	operator*()				const	{ return p.get(); }
		const N*	operator->()			const	{ return p.get(); }
		iterator&	operator++()					{ depth = p.next(); return *this; }
	};

	postorder(const N *root, const IO& inout = {}) : IO(inout) {
		visited.insert(root);
		stack.emplace_back(IO::outgoing(*root), root);
		traverse_child();
	}

	iterator	begin() const { return iterator(*unconst(this), stack.size()); }
	iterator	end()	const { return iterator(*unconst(this), 0); }
};

template<class N> auto make_postorder(N *t) {
	return postorder<N>(t);
}
template<class N, typename IO> auto make_postorder(N *t, const IO& inout) {
	return postorder<N, IO>(t, inout);
}

/*
template<class N> void assign_graph_orders(N *n, uint32 &k) {
	n->order = 1;
	for (auto i = n->edges; i; ++i) {
		if (i->order == 0)
			assign_graph_orders(*i, k);
	}
	n->order = ++k;
}
template<class N> void assign_graph_orders(const dynamic_array<N*> &nodes) {
	for (auto i : nodes)
		i->order = 0;
	uint32		k = 0;
	for (auto i : nodes) {
		if (i->order == 0)
			assign_graph_orders(*i, k);
	}
}
template<class N> void topologic_sort(const dynamic_array<N*> &nodes, dynamic_array<N*> &sorted) {
	assign_graph_orders(nodes);
	uint32	n = nodes.size();
	for (auto i : nodes)
		sorted[n - i->order] = i;
}

template<class N> bool test_acyclic(const dynamic_array<N*> &nodes) {
	assign_graph_orders(nodes);
	for (auto i : nodes) {
		uint32	iorder = i->order;
		for (auto j = i->edges; j; ++j) {
			if (iorder >= j->order)
				return false;
		}
	}
	return true;
}

template<class N> void dijkstra(const dynamic_array<N*> &nodes) {
	priority_queue<N*, dynamic_array<N*> >	q;

	struct node {
		float	dist;
		N		*node;
		int		potnode;
	};

}
*/
//-----------------------------------------------------------------------------
//	DominatorTree
//	Based on: "A Simple, Fast Dominance Algorithm" by Cooper, Harvey and Kennedy
//-----------------------------------------------------------------------------

template<typename N, typename IO = default_inout<N>> class DominatorTree : public IO {
public:
	struct Info {
		const N					*node;
		int						post_order;
		Info					*idom;	// Immediate dominator (i.e. parent of the tree)
		dynamic_array<Info*>	doms;	// immediately dominated blocks (i.e. children of the tree)
		dynamic_array<Info*>	preds;	// predecessor blocks (in CFG)

		Info(const N *node) : node(node), idom(0) {}

		friend const dynamic_array<Info*>& outgoing(const Info &t) { return t.doms; }
	};

	dynamic_array<Info>			pre_order;
	dynamic_array<Info*>		post_order;
	hash_map<const N*, Info*>	info_map;
	Info*						info_root;

private:
	static bool _dominates(const Info *a, const Info *b) {
	#if 0
		return a->pre_order <= b->pre_order && a->post_order >= b->post_order;
	#else
		if (a->post_order >= b->post_order) {
			for (const Info *i = b; i; i = i->idom) {
				if (i == a)
					return true;
			}
		}
		return false;
	#endif
	}

	static Info *intersect(Info *info1, Info *info2) {
		if (info1) {
			while (info1 != info2) {
				while (info1->post_order < info2->post_order) {
					info1 = info1->idom;
					if (!info1)
						return info2;
				}
				while (info2->post_order < info1->post_order) {
					info2 = info2->idom;
					if (!info2)
						return info1;
				}
			}
		}
		return info2;
	}

	void	init_info(const N *n, Info *info) {
		for (auto &i : IO::outgoing(*n)) {
			Info *info2	= info_map[i];
			if (!info2) {
				info2		= &pre_order.emplace_back(i);
				info_map[i]	= info2;
				init_info(i, info2);
			}
			info2->preds.push_back(info);
		}
		info->post_order = post_order.size32();
		post_order.push_back(info);
	}

	void create(const N *root) {
		size_t n0 = num_elements(make_postorder(root, *(IO*)this));
		pre_order.reserve(n0);
		info_map[root]	= info_root = &pre_order.emplace_back(root);
		init_info(root, info_root);
		ISO_ASSERT(n0 == post_order.size());

		info_root->idom	= info_root;
		for (bool changed = true; changed;) {
			changed = false;
			// Iterate over the list in reverse order, i.e., forward on CFG edges
			for (auto &i : reversed(post_order)) {
				Info *new_idom = 0;

				for (auto &p : i->preds)
					new_idom = intersect(new_idom, p);

				// Check if the idom value has changed
				if (new_idom && new_idom != i->idom) {
					i->idom = new_idom;
					changed = true;
				}
			}
		}

		info_root->idom	= 0;

		// make tree child links
		for (auto i : post_order) {
			if (Info *idom = i->idom)
				idom->doms.push_back(i);
		}
	}

public:
	DominatorTree(const N *root, IO&& inout = {}) : IO(move(inout)) {
		create(root);
	}

	void	recreate(const N *root) {
		pre_order.clear();
		post_order.clear();
		info_map.clear();
		create(root);
	}

	const Info	*get_info(const N *n) const {
		auto	*p = info_map.check(n);
		return p ? *p : 0;
	}
	static const N	*get_node(const Info *i) {
		return i ? i->node : 0;
	}

	const Info	*operator[](const N *n) const {
		return get_info(n);
	}

	bool is_acyclic() const {
		for (auto i : post_order) {
			uint32	iorder = i->post_order;
			for (auto j : IO::outgoing(*i->node)) {
				if (iorder >= get_info(j)->post_order)
					return false;
			}
		}
		return true;
	}

	static bool dominates(const Info *a, const Info *b) {
		return !b || a == b || (a && _dominates(a, b));
	}
	static bool properly_dominates(const Info *a, const Info *b) {
		return a && b && a != b && _dominates(a, b);
	}
	bool dominates(const N *a, const N *b) const {
		return a == b || dominates(get_info(a), get_info(b));
	}
	bool properly_dominates(const N *a, const N *b) {
		return a != b && dominates(get_info(a), get_info(b));
	}

	const Info *nearest_common_dominator(const Info *a, const Info *b) const {
		if (a == info_root || b == info_root)
			return info_root;

		for (const Info *i = a; i; i = i->idom) {
			if (_dominates(i, b))
				return i;
		}
		return 0;
	}
	const N *nearest_common_dominator(const N *a, const N *b) const {
		return get_node(nearest_common_dominator(get_info(a), get_info(b)));
	}

	const N *idom(const N *a) const {
		return get_node(get_info(a)->idom);
	}
	
	bool single_entering_block(Info *n) const {
		return count(n->preds, [this, n](Info *i) { return !dominates(n, i); }) == 1;
	}
	bool single_entering_block(const N *n) const {
		return single_entering_block(get_info(n));
	}

	auto dominated_nodes(N *head) const {
		hash_set_with_key<const N*>		result;
		if (const Info *i = get_info(head)) {
			dynamic_array<const Info*>	work_list;
			work_list.push_back(i);

			while (!work_list.empty()) {
				const Info	*i	= work_list.pop_back_retref();
				for (auto &j : i->doms) {
					result.insert(j->node);
					work_list.push_back(j);
				}
			}
		}
		return result;
	}

	auto dominance_frontier(const N *head) const {
		hash_set_with_key<const N*>		result;
		if (const Info *i0 = get_info(head)) {
			dynamic_array<const Info*>	work_list;
			work_list.push_back(i0);

			while (!work_list.empty()) {
				const Info	*i	= work_list.pop_back_retref();
				for (auto &j : i->doms)
					work_list.push_back(j);

				for (auto &j : IO::outgoing(*i->node)) {
					if (!dominates(i0, get_info(j)))
						result.insert(j);
				}
			}
		}
		return result;
	}

	auto back_edges(const N *head) const {
		hash_set_with_key<const N*>		result;
		if (const Info *i = get_info(head)) {
			for (auto &j : i->preds) {
				if (dominates(i, j))
					result.insert(j->node);
			}
		}
		return result;
	}
	auto back_edges2(const N *head) const {
		hash_set_with_key<const N*>		result;
		for (auto &j : IO::incoming(*head)) {
			if (dominates(head, j))
				result.insert(j);
		}
		return result;
	}
};

}

#endif
