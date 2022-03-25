#include "sparse.h"
#include "allocators/allocator.h"

namespace iso {

//-----------------------------------------------------------------------------
//	permutations
//-----------------------------------------------------------------------------

void sparse_layout_vector::permute(const int *p) {
	if (p) {
		int	*temp	= alloc_auto(int, nz);
		for (int j = 0; j < nz; j++)
			temp[j]	= i[j];

		for (int j = 0; j < nz; j++)
			i[j]	= p[temp[j]];
	}
}

void sparse_layout_vector::inv_permute(const int *p) {
	if (p) {
		int	*temp	= alloc_auto(int, nz);
		for (int j = 0; j < nz; j++)
			temp[j]	= i[j];

		for (int j = 0; j < nz; j++)
			i[p[j]] = temp[j];
	}
}

int sparse_layout_vector::add_pattern(int *mask, int mark, int *out) {
	int	nz = 0;
	for (int j = 0; j < nz; j++) {
		int	k = i[j];
		if (mask[k] < mark) {
			mask[k]		= mark;
			out[nz++]	= k;	// add k to pattern
		}
	}
	return nz;
}

void invert_permute(const int *s, int *d, int n) {
	for (int i = 0; i < n; i++)
		d[s[i]] = i;
}

// C = A(p,q) where p and q are permutations of 0..m-1 and 0..n-1
sparse_layout sparse_layout::permute(const int *pinv, const int *q) const {
	sparse_layout	B(m, n, nz());
	int		*ia	= this->ia(),	*ja = this->ja();
	int		*ib	= B.ia(),		*jb = B.ja();

	int	nz = 0;
	for (int i = 0; i < n;i++) {
		ib[i] = nz;
		int	i2 = q ? q[i] : i;
		for (int j = ia[i2], j1 = ia[i2 + 1]; j < j1; j++)
			jb[nz++] = pinv ? pinv[ja[j]] : ja[j];
	}
	ib[n] = nz;
	return B;
}

//-----------------------------------------------------------------------------
//	sparse_layout
//-----------------------------------------------------------------------------

sparse_layout_coords &sparse_layout_coords::compact() {
	sort(*this);
	resize(unique(begin(), end()) - begin());
	return *this;
}

sparse_layout::sparse_layout(sparse_layout_coords &coords) {
	coords.compact();
	if (coords.size() == 0)
		return;

	sparse_layout::create(coords.m, coords.n, int(coords.size()));

	int	c	= 0, p = 0;
	int	*ia	= this->ia(), *ja = this->ja();

	for (sparse_coord *i = coords.begin(), *e = coords.end(); i != e; ++i) {
		while (c <= i->c)
			ia[c++] = p++;
		*ja++	= i->r;
	}
	while (c <= coords.n)
		ia[c++] = p;

	categorise();
}

int sparse_layout::categorise() {
	if (m != n)
		return 0;

	int		*t		= alloc_auto(int, n);
	for (int i = 0; i < n; i++)
		t[i] = -1;

	int		*ia		= this->ia(), *ja = this->ja();
	bool	upper	= true, lower = true, diagonal = true, symmetrical = true;
	bool	has_diagonal = false;

	for (int i = 0, j = 0; i < n; i++) {
		for (int j1 = ia[i + 1]; j < j1; j++) {
			int	k = ja[j];
			if (k == i) {
				has_diagonal = true;
			} else {
				diagonal = false;
				if (k < i) {
					lower	= false;
					t[k]	= i;
				} else {
					upper = false;
					if (t[i] != k)
						symmetrical = false;
				}
			}
		}
	}
	flags =	(symmetrical		? PATTERN_SYMMETRIC	: 0)
		|	(diagonal			? DIAGONAL			: 0)
		|	(lower				? LOWER				: 0)
		|	(upper				? UPPER				: 0)
		|	(has_diagonal		? HAS_DIAGONAL		: 0);

	return upper ? 1 : lower ? -1 : 0;
}

sparse_layout get_transpose(const sparse_layout &A) {
	int		m	= A.m, n = A.n;
	sparse_layout	B(n, m, A.nz());
	int		*ia	= A.ia(), *ja = A.ja();
	int		*ib	= B.ia(), *jb = B.ja();

	for (int i = 0; i <= m; i++)
		ib[i] = 0;

	for (int i = 0, j = 0; i < n; i++) {
		for (int j1 = ia[i + 1]; j < j1; j++)
			ib[ja[j] + 1]++;
	}

	for (int i = 0; i < A.m; i++)
		ib[i + 1] += ib[i];

	for (int i = 0, j = 0; i < n; i++) {
		for (int j1 = ia[i + 1]; j < j1; j++)
			jb[ib[ja[j]]]	= i;
	}

	for (int i = m; i > 0; i--)
		ib[i] = ib[i - 1];
	ib[0] = 0;

	B.flags	= A.flags;
	B.flags.swap(sparse_layout::LOWER, sparse_layout::UPPER);
	return B;
}

sparse_layout operator+(const sparse_layout &A, const sparse_layout &B) {
	int		mmax	= max(A.m, B.m);
	int		na		= A.n, nb = B.n, nmax = max(na, nb);
	int		nzmax	= A.nz() + B.nz();// just assume that no entries overlaps for speed

	sparse_layout	C(mmax, nmax, nzmax);
	int				*ia	= A.ia(), *ja = A.ja();
	int				*ib	= B.ia(), *jb = B.ja();
	int				*ic	= C.ia(), *jc = C.ja();

	int		*mask	= alloc_auto(int, mmax);
	for (int i = 0; i < mmax; i++)
		mask[i] = -1;

	int		nz		= 0;
	for (int i = 0; i < nmax; i++) {
		int	start = ic[i] = nz;
		if (i < na) {
			for (int j = ia[i], j1 = ia[i + 1]; j < j1; j++) {
				jc[nz] = ja[j];
				mask[ja[j]] = nz++;
			}
		}
		if (i < nb) {
			for (int j = ib[i], j1 = ib[i + 1]; j < j1; j++) {
				if (mask[jb[j]] < start)
					jc[nz++] = jb[j];
			}
		}
	}
	ic[nmax] = nz;
	return C;
}

sparse_layout operator*(const sparse_layout &A, const sparse_layout &B) {
	if (A.n != B.m)
		return sparse_layout();

	int		m	= A.m;
	int		n	= B.n;

	int		*mask	= alloc_auto(int, m);
	for (int i = 0; i < m; i++)
		mask[i] = -1;

	int		*ia	= A.ia(), *ja = A.ja();
	int		*ib	= B.ia(), *jb = B.ja();
	int		nz = 0;
	for (int i = 0, j = 0; i < n; i++) {
		for (int j1 = ib[i + 1]; j < j1; j++) {
			for (int k = ia[jb[j]], k1 = ia[jb[j] + 1]; k < k1; k++) {
				if (mask[ja[k]] != -i - 2) {
					mask[ja[k]] = -i - 2;
					nz++;
				}
			}
		}
	}

	sparse_layout	C(m, n, nz);
	int	*ic	= C.ia(), *jc = C.ja();

	nz = 0;
	for (int i = 0, j = 0; i < n; i++) {
		ic[i] = nz;
		for (int j1 = ib[i + 1]; j < j1; j++) {
			for (int k = ia[jb[j]], k1 = ia[jb[j] + 1]; k < k1; k++) {
				int	t = ja[k];
				if (mask[t] < ic[i]) {
					mask[t] = nz;
					jc[nz]	= t;
					nz++;
				}
			}
		}
	}
	ic[n] = nz;
	return C;
}

//-----------------------------------------------------------------------------
//	reach
//-----------------------------------------------------------------------------

inline int flip(int i)		{ return -i - 2; }
inline int unflip(int i)	{ return i < 0 ? flip(i) : i; }

struct reach_workspace {
	int	*xi, *stack;
	static size_t	size(int n)	{ return 2 * n * sizeof(int); }

	reach_workspace(linear_allocator &a, int n) {
		xi		= a.alloc<int>(n);
		stack	= a.alloc<int>(n);
	}

	int	dfs(const sparse_layout &G, int j, int top, const int *pinv);
	int reach(const sparse_layout &G, const sparse_layout_vector &b, const int *pinv);
	int	ereach(const sparse_layout &A, int k, const int *parent);
};

// depth-first-search of the graph of a matrix, starting at node j
int reach_workspace::dfs(const sparse_layout &G, int j, int top, const int *pinv) {
	int		*ig = G.ia(), *jg = G.ja();
	xi[0]		= j;					// initialize the recursion stack
	for (int head = 0; head >= 0;) {
		int	j		= xi[head];				// get j from the top of the recursion stack
		int	jnew	= pinv ? pinv[j] : j;
		if (ig[j] >= 0) {
			ig[j] = flip(ig[j]);			// mark node j as visited
			stack[head] = jnew < 0 ? 0 : unflip(ig[jnew]);
		}
		bool	done	= true;				// node j done if no unvisited neighbors
		int		p2		= jnew < 0 ? 0 : unflip(ig[jnew + 1]);
		for (int p = stack[head]; p < p2; p++) {	// examine all neighbors of j
			int	i = jg[p];							// consider neighbor node i
			if (ig[i] >= 0) {			// skip visited node i
				stack[head]	= p;		// pause depth-first search of node j
				xi[++head]	= i;		// start dfs at node i
				done		= false;	// node j is not done
				break;					// break, to start dfs (i)
			}
		}
		if (done) {					// depth-first search at node j is done
			--head;					// remove j from the recursion stack
			xi[--top] = j;			// and place in the output stack
		}
	}
	return top;
}

// xi[top...n-1] = nodes reachable from graph of G*P' via nodes in B(:,k)
// xi[n...2n-1] used as workspace
int reach_workspace::reach(const sparse_layout &G, const sparse_layout_vector &b, const int *pinv) {
	int		*ig = G.ia();
	int		*jb = b.i;
	int		n	= G.n;

	int		top	= n;
	for (int p = 0; p < b.nz; p++) {
		if (ig[jb[p]] >= 0)		// start a dfs at unmarked node i
			top = dfs(G, jb[p], top, pinv);
	}
	for (int p = top; p < n; p++)
		ig[xi[p]] = flip(ig[xi[p]]);		// restore G
	return top;
}

// find nonzero pattern of Cholesky L(k,1:k-1) using etree and triu(A(:,k))
int	reach_workspace::ereach(const sparse_layout &A, int k, const int *parent) {
	int	n	= A.n;
	int	*ia = A.ia(), *ja = A.ja();

	xi[k] = flip(xi[k]);				// mark node k as visited

	int	top = n;
	for (int p = ia[k]; p < ia[k + 1]; p++) {
		int	i = ja[p];
		if (i <= k) {					// only use upper triangular part of A
			int	len = 0;
			while (xi[i] >= 0) {		// traverse up etree
				stack[len++] = i;		// L(k,i) is nonzero
				xi[i]	= flip(xi[i]);	// mark i as visited
				i		= parent[i];
			}
			while (len > 0)
				stack[--top] = stack[--len];	// push path onto stack
		}
	}

	for (int p = top; p < n; p++)
		xi[stack[p]] = flip(xi[stack[p]]);		// unmark all nodes

	xi[k] = flip(xi[k]);	// unmark node k
	return top;				// s[top..n-1] contains pattern of L(k,:)
}
//-----------------------------------------------------------------------------
//	elimination tree
//-----------------------------------------------------------------------------

struct list_array {
	int				*head, *next;
	static size_t	size(int m, int n)	{ return (m + n) * sizeof(int); }

	list_array(linear_allocator &a, int n, int m) {
		head	= a.alloc<int>(n);
		next	= a.alloc<int>(m);
		clear_lists(n);
	}
	void	clear_lists(int n) {
		for (int i = 0; i < n; i++)
			head[i] = next[i] = -1;
	}
	void	add_head(int i, int j) {
		next[i] = head[j];
		head[j] = i;
	}
};

struct postorder_ws : list_array {
	int				*stack;
	static size_t	size(int n) { return list_array::size(n, n) + n * sizeof(int); }

	postorder_ws(linear_allocator a, int n) : list_array(a, n, n) {
		stack	= a.alloc<int>(n);
	}
	void	post_order(int *post, const int *parent, int n);
};

void postorder_ws::post_order(int *post, const int *parent, int n) {
	for (int j = 0, k = 0; j < n; ++j) {
		if (parent[j] == -1) {
			stack[0] = j;
			for (int *sp = stack; sp >= stack; ) {
				int	p = *sp;
				int	i = head[p];
				if (i == -1) {
					--sp;
					post[k++] = p;
				} else {
					head[p] = next[i];
					*++sp = i;
				}
			}
		}
	}
}

ref_array<int> post_order(const int *parent, int n) {
	postorder_ws		ws(alloca(postorder_ws::size(n)), n);
	for (int j = n - 1; j >= 0; j--) {
		if (parent[j] != -1)
			ws.add_head(j, parent[j]); // add j to list of its parent
	}

	ref_array<int>	post;
	ws.post_order(post, parent, n);
	return post;
}

ref_array<int> elimination_tree(const sparse_layout &A, bool ata) {
	int		n		= A.n;
	int		*ancest	= alloc_auto(int, n);
	int		*ia		= A.ia(), *ja = A.ja();

	int		*prev	= 0;
	if (ata) {
		prev = alloc_auto(int, A.m);
		for (int k = 0; k < A.m; k++)
			prev[k] = -1;
	}

	ref_array<int>	etree(n);
	int		*parent	= etree;

	for (int i = 0, j = 0; i < n; i++) {
		parent[i]	= -1;
		ancest[i]	= -1;
		for (int j1 = ia[i + 1]; j < j1; j++) {
			for (int k = ata ? prev[ja[j]] : ja[j], ni; k != -1 && k < i; k = ni) {
				ni			= ancest[k];
				ancest[k]	= i;	// path compression
				if (ni == -1)
					parent[k] = i;	// no anc., parent is i
			}
			if (ata)
				prev[ja[j]] = i;
		}
	}
	return etree;
}

//-----------------------------------------------------------------------------
// approximate degree ordering
//-----------------------------------------------------------------------------

struct amd_workspace : list_array {
	int	*len, *nv, *elen, *degree, *w, *hhead, *last;

	static size_t	size(int n)	{ return list_array::size(n, n) + 7 * n * sizeof(int); }

	amd_workspace(linear_allocator a, int n) : list_array(a, n, n) {
		len		= a.alloc<int>(n);
		nv		= a.alloc<int>(n);
		elen	= a.alloc<int>(n);
		degree	= a.alloc<int>(n);
		w		= a.alloc<int>(n);
		hhead	= a.alloc<int>(n);
		last	= a.alloc<int>(n);

		for (int i = 0; i < n; i++) {
			last[i]		= -1;
			hhead[i]	= -1;		// hash list i is empty
		}
	}

	int	wclear(int mark, int lemax, int n) {
		if (mark < 2 || mark + lemax < 0) {
			for (int k = 0; k < n; k++)
				if (w[k] != 0)
					w[k] = 1;
			mark = 2;
		}
		return mark;		// at this point, w[0..n-1] < mark holds
	}

	void	add2(int i, int j) {
		if (head[j] != -1)
			last[head[j]] = i;
		add_head(i, j);
	}
	int		remove2(int i) {
		if (next[i] != -1)
			last[next[i]] = -1;
		return next[i];
	}
};

// AMD: approximate minimum degree ordering
ref_array<int> amd(sparse_layout &C, int dense) {
	int		n		= C.n;
	int		nzc		= C.nz();
	int		nzmax	= nzc + nzc / 5 + 2 * n;// add elbow room to C

	if (dense == 0)
		dense = n / 2;

	C.reserve(nzc, nzmax);
	int		*ic		= C.ia(), *jc = C.ja();

	amd_workspace	ws(alloca(amd_workspace::size(n + 1)), n + 1);

	// --- Initialize quotient graph ---
	for (int i = 0; i < n; i++)
		ws.len[i] = ic[i + 1] - ic[i];
	ws.len[n] = 0;
	copy_n(ws.len, ws.degree, n + 1);

	ws.elen[n]		= -2;					// n is a dead element
	ws.w[n]			= 0;					// n is a dead element
	ws.nv[n]		= 1;
	int		mark	= ws.wclear(0, 0, n);	// clear w

	// --- Initialize degree lists ---
	int	nel = 0;
	for (int i = 0; i < n; i++) {
		int	d = ws.degree[i];
		if (d == 0) {						// node i is empty
			ws.elen[i]	= -2;				// element i is dead
			ws.w[i]		= 0;
			ic[i]		= -1;				// i is a root of assembly tree
			nel++;
		} else if (d > dense) {				// node i is dense
			ws.elen[i]	= -1;				// node i is dead
			//ws.w[i]	= 1;				// node i is alive
			ws.nv[i]	= 0;				// absorb i into element n
			ws.nv[n]++;
			ic[i]		= flip(n);
			nel++;
		} else {
			ws.add2(i, d);					// put node i in degree list d
			ws.elen[i]	= 0;				// Ek of node i is empty
			ws.w[i]		= 1;				// node i is alive
			ws.nv[i]	= 1;
		}
	}

	int mindeg	= 0;
	int	lemax	= 0;
	while (nel < n) {
		// --- Select node of minimum approximate degree ---
		int	k = -1;
		while (mindeg < n && (k = ws.head[mindeg]) == -1)
			++mindeg;

		ws.head[mindeg] = ws.remove2(k);	// remove k from degree list

		int	elenk	= ws.elen[k];			// elenk = |Ek|
		int	nvk		= ws.nv[k];				// # of nodes k represents
		nel += nvk;							// nv[k] nodes of A eliminated

		// --- Garbage collection ---
		if (elenk > 0 && nzc + mindeg >= nzmax) {
			for (int j = 0; j < n; j++) {
				int	p = ic[j];
				if (p >= 0) {				// j is a live node or element
					ic[j] = jc[p];			// save first entry of object
					jc[p] = flip(j);		// first entry is now flip(j)
				}
			}
			int q = 0, p = 0;
			while (p < nzc) {				// scan all of memory
				int	j = flip(jc[p++]);
				if (j >= 0) {				// found object j
					jc[q] = ic[j];			// restore first entry of object
					ic[j] = q++;			// new pointer to object j
					for (int k3 = 0; k3 < ws.len[j] - 1; k3++)
						jc[q++] = ic[p++];
				}
			}
			nzc = q;						// jc[cnz...nzmax-1] now free
		}

		// --- Construct new element ---
		int	dk		= 0;
		ws.nv[k]	= -nvk;					// flag k as in Lk
		int	p		= ic[k];
		int	pk1		= elenk == 0 ? p : nzc;	// do in place if elen[k] == 0
		int	pk2		= pk1;
		for (int k1 = 1; k1 <= elenk + 1; k1++) {
			int	e, pj, ln;
			if (k1 > elenk) {
				e	= k;					// search the nodes in k
				pj	= p;					// list of nodes starts at jc[pj]
				ln	= ws.len[k] - elenk;	// length of list of nodes in k
			} else {
				e	= jc[p++];				// search the nodes in e
				pj	= ic[e];
				ln	= ws.len[e];			// length of list of nodes in e
			}

			for (int k2 = 1; k2 <= ln; k2++) {
				int	i	= jc[pj++];
				int	nvi	= ws.nv[i];
				if (nvi > 0) {
					dk			+= nvi;		// degree[Lk] += size of node i
					ws.nv[i]	= -nvi;		// negate nv[i] to denote i in Lk
					jc[pk2++]	= i;		// place i in Lk

					int	x = ws.remove2(i);	// remove i from degree list
					if (ws.last[i] != -1)
						ws.next[ws.last[i]] = x;
					else
						ws.head[ws.degree[i]] = x;
				}
			}
			if (e != k) {
				ic[e]	= flip(k);			// absorb e into k
				ws.w[e]	= 0;				// e is now a dead element
			}
		}
		if (elenk != 0)
			nzc = pk2;						// jc[cnz...nzmax] is free

		ws.degree[k]	= dk;				// external degree of k - |Lk\i|
		ic[k]			= pk1;				// element k is in jc[pk1..pk2-1]
		ws.len[k]		= pk2 - pk1;
		ws.elen[k]		= -2;				// k is now an element

		// --- Find set differences ---
		mark = ws.wclear(mark, lemax, n);	// clear w if necessary
		for (int pk = pk1; pk < pk2; pk++) {	// scan 1: find |Le\Lk|
			int i	= jc[pk];
			int	eln	= ws.elen[i];
			if (eln > 0) {						// skip if elen[i] empty
				int	nvi		= -ws.nv[i];			// nv[i] was negated
				int	wnvi	= mark - nvi;
				for (int p = ic[i]; p <= ic[i] + eln - 1; p++) {	// scan Ei
					int	e = jc[p];
					if (ws.w[e] >= mark)
						ws.w[e] -= nvi;			// decrement |Le\Lk|
					else if (ws.w[e] != 0)		// ensure e is a live element
						ws.w[e] = ws.degree[e] + wnvi;	// 1st time e seen in scan 1
				}
			}
		}

		// --- Degree update ---
		for (int pk = pk1; pk < pk2; pk++) {	// scan2: degree update
			int	i	= jc[pk];					// consider node i in Lk
			int	p1	= ic[i];
			int	p2	= p1 + ws.elen[i] - 1;
			int	pn	= p1;
			int	h	= 0, d = 0;
			for (int p = p1; p <= p2; p++) {	// scan Ei
				int	e = jc[p];
				if (ws.w[e] != 0) {				// e is an unabsorbed element
					int	dext = ws.w[e] - mark;	// dext = |Le\Lk|
					if (dext > 0) {
						d		+= dext;		// sum up the set differences
						jc[pn++] = e;			// keep e in Ei
						h		+= e;			// compute the hash of node i
					} else {
						ic[e]	= flip(k);		// aggressive absorb. e->k
						ws.w[e]	= 0;			// e is a dead element
					}
				}
			}
			ws.elen[i] = pn - p1 + 1;			// elen[i] = |Ei|
			int	p3 = pn;
			int	p4 = p1 + ws.len[i];
			for (int p = p2 + 1; p < p4; p++) {	// prune edges in Ai
				int	j	= jc[p];
				int	nvj = ws.nv[j];
				if (nvj > 0) {				// node j dead or in Lk
					d		+= nvj;			// degree(i) += |j|
					jc[pn++] = j;			// place j in node list of i
					h		+= j;			// compute hash for node i
				}
			}
			if (d == 0) {					// check for mass elimination
				ic[i]		= flip(k);	// absorb i into k
				int	nvi		= -ws.nv[i];
				dk			-= nvi;			// |Lk| -= |i|
				nvk			+= nvi;			// |k| += nv[i]
				nel			+= nvi;
				ws.nv[i]	= 0;
				ws.elen[i]	= -1;			// node i is dead
			} else {
				ws.degree[i] = min(ws.degree[i], d);	// update degree(i)
				jc[pn]		= jc[p3];		// move first node to end
				jc[p3]		= jc[p1];		// move 1st el. to end of Ei
				jc[p1]		= k;			// add k as 1st element in of Ei
				ws.len[i]	= pn - p1 + 1;	// new len of adj. list of node i
				h			= abs(h) % n;	// finalize hash of i
				ws.next[i]	= ws.hhead[h];	// place i in hash bucket
				ws.hhead[h]	= i;
				ws.last[i]	= h;			// save hash of i in last[i]
			}
		}

		ws.degree[k] = dk;					// finalize |Lk|
		lemax	= max(lemax, dk);
		mark	= ws.wclear(mark + lemax, lemax, n);		// clear w

		// --- Supernode detection ---
		for (int pk = pk1; pk < pk2; pk++) {
			int	i = jc[pk];
			if (ws.nv[i] < 0) {			// skip if i is dead
				int	h = ws.last[i];		// scan hash bucket of node i
				i = ws.hhead[h];
				ws.hhead[h] = -1;		// hash bucket will be empty
				for (; i != -1 && ws.next[i] != -1; i = ws.next[i], mark++) {
					int	ln	= ws.len[i];
					int	eln	= ws.elen[i];
					for (int p = ic[i] + 1; p <= ic[i] + ln - 1; p++)
						ws.w[jc[p]] = mark;
					int	jlast = i;
					for (int j = ws.next[i]; j != -1; ) {	// compare i with all j
						bool	ok = ws.len[j] == ln && ws.elen[j] == eln;
						for (int p = ic[j] + 1; ok && p <= ic[j] + ln - 1; p++)
							ok = ws.w[jc[p]] == mark;
						if (ok) {						// i and j are identical
							ic[j]		= flip(i);	// absorb j into i
							ws.nv[i]	+= ws.nv[j];
							ws.nv[j]	= 0;
							ws.elen[j]	= -1;			// node j is dead
							j			= ws.next[j];	// delete j from hash bucket
							ws.next[jlast] = j;
						} else {
							jlast		= j;			// j and i are different
							j			= ws.next[j];
						}
					}
				}
			}
		}

		// --- Finalize new element---
		for (int p = pk1, pk = pk1; pk < pk2; pk++) {	// finalize Lk
			int	i	= jc[pk];
			int	nvi = -ws.nv[i];
			if (nvi > 0) {						// skip if i is dead
				ws.nv[i]	= nvi;				// restore nv[i]
				int	d		= min(ws.degree[i] + dk - nvi, n - nel - nvi);	// compute external degree(i)
				ws.add2(i, d);					// put i back in degree list
				ws.last[i]	= -1;
				mindeg		= min(mindeg, d);	// find new minimum degree
				ws.degree[i] = d;
				jc[p++]		= i;				// place i in Lk
			}
		}
		ws.nv[k] = nvk;						// # nodes absorbed into k
		if ((ws.len[k] = p-pk1) == 0) {		// length of adj list of element k
			ic[k]	= -1;					// k is a root of the tree
			ws.w[k]	= 0;					// k is now a dead element
		}
		if (elenk != 0)
			nzc		= p;					// free unused space in Lk
	}

	// --- Postordering ---
	for (int i = 0; i < n; i++)
		ic[i] = flip(ic[i]);				// fix assembly tree

	ws.clear_lists(n + 1);

	for (int j = n; j >= 0; j--) {			// place unordered nodes in lists
		if (ws.nv[j] <= 0)					// skip if j is an element
			ws.add_head(j, ic[j]);			// place j in list of its parent
	}
	for (int e = n; e >= 0; e--) {			// place elements in lists
		if (ws.nv[e] > 0 && ic[e] != -1)	// skip unless e is an element
			ws.add_head(e, ic[e]);			// place e in list of its parent
	}

	ref_array<int>	post(n + 1);
	((postorder_ws&)ws).post_order(post, ic, n + 1);
	return post;
}
ref_array<int> amd(sparse_layout &&C, int dense) {
	return amd(C, dense);
}

//-----------------------------------------------------------------------------
//	column counts
//-----------------------------------------------------------------------------

// consider A(i,j), node j in ith row subtree and return lca(jprev,j)

struct node_array {
	int	*ancestor;
	int	*maxfirst;
	int	*prevleaf;
	int	*first;

	static size_t	size(int n)	{ return n * 4 * sizeof(int); }

	node_array(void *p, int n) {
		ancestor	= (int*)p;
		maxfirst	= ancestor + n;
		prevleaf	= maxfirst + n;
		first		= prevleaf + n;

		for (int i = 0; i < n; i++) {
			ancestor[i] = i;
			maxfirst[i] = prevleaf[i] = first[i] = -1;
		}
	}
	int		check_leaf(int i, int j, int *jleaf);
	void	update_count(int i, int j, int *counts);
};

int node_array::check_leaf(int i, int j, int *jleaf) {
	*jleaf = 0;
	if (i <= j || first[j] <= maxfirst[i])
		return -1;	// j not a leaf

	maxfirst[i] = first[j];	// update max first[j] seen so far

	int	jprev = prevleaf[i];	// jprev = previous leaf of ith subtree
	prevleaf[i] = j;

	if (jprev == -1) {
		*jleaf = 1;	// 1st leaf, q = root of ith subtree
		return i;
	}
	*jleaf = 2;		// subsequent leaf

	int	q = jprev;
	while (q != ancestor[q])
		q = ancestor[q];

	for (int s = jprev, p; s != q; s = p) {
		p = ancestor[s];	// path compression
		ancestor[s] = q;
	}
	return q;	// q = least common ancester (jprev,j)
}

void node_array::update_count(int i, int j, int *counts) {
	int	jleaf;
	int q = check_leaf(i, j, &jleaf);
	if (jleaf >= 1)
		counts[j]++;	// A(i,j) is in skeleton
	if (jleaf == 2)
		counts[q]--;	// account for overlap in q
}

// column counts of LL' = A or A'A, given parent & post ordering

ref_array<int> col_counts(const sparse_layout &A, const int *parent, const int *post, bool ata) {
	int				m		= A.m, n = A.n;
	node_array		nodes(alloca(node_array::size(n)), n);
	ref_array<int>	counts(n);

	for (int i = 0; i < n; i++) {
		int		j	= post[i];
		counts[j]	= int(nodes.first[j] == -1);
		while (j != -1 && nodes.first[j] == -1) {
			nodes.first[j] = i;
			j = parent[j];
		}
	}

	sparse_layout	AT		= transpose(A);
	int				*it		= AT.ia();
	int				*jt		= AT.ja();

	if (ata) {
		int	*head = 0, *next = 0, *ipost = 0;
		head	= alloc_auto(int, n + 1);
		next	= alloc_auto(int, m);
		ipost	= alloc_auto(int, n);
		invert_permute(post, ipost, n);	// invert post

		for (int i = 0; i < m; i++) {
			int	k = n;
			for (int p = it[i]; p < it[i + 1]; p++)
				k = min(k, ipost[jt[p]]);

			next[i] = head[k];	// place row i in linked list k
			head[k] = i;
		}

		for (int i = 0; i < n; i++) {
			int j = post[i];			// j is the ith node in postordered etree
			if (parent[j] != -1)
				counts[parent[j]]--;	// j is not a root

			for (int j = head[i]; j != -1; j = next[j]) {
				for (int k = it[j]; k < it[j + 1]; k++)
					nodes.update_count(jt[k], j, counts);
			}

			if (parent[j] != -1)
				nodes.ancestor[j] = parent[j];
		}

	} else {
		for (int i = 0; i < n; i++) {
			int j = post[i];			// j is the ith node in postordered etree
			if (parent[j] != -1)
				counts[parent[j]]--;	// j is not a root

			for (int k = it[j]; k < it[j + 1]; k++)
				nodes.update_count(jt[k], j, counts);

			if (parent[j] != -1)
				nodes.ancestor[j] = parent[j];
		}
	}

	for (int j = 0; j < n; j++) {	// sum up delta's of each child
		if (parent[j] != -1)
			counts[parent[j]] += counts[j];
	}
	return counts;
}

//-----------------------------------------------------------------------------
//	symmetric permute
//-----------------------------------------------------------------------------

// C = A(p,p) where A and C are symmetric the upper part stored; pinv not p
int symm_perm_cols(const sparse_layout &A, int *w, const int *pinv) {
	int	n = A.n;

	for (int i = 0; i < n; i++)
		w[i] = 0;

	int	*ia	= A.ia(), *ja = A.ja();

	// count entries in each column of C
	for (int i = 0; i < n; i++) {
		int	i2 = pinv ? pinv[i] : i;			// column i of A is column i2 of C
		for (int j = ia[i], j1 = ia[i + 1]; j < j1; j++) {
			int	k = ja[j];
			if (k <= i) {						// skip lower triangular part of A
				int k2 = pinv ? pinv[k] : k;	// row k of A is row k2 of C
				w[max(k2, i2)]++;				// column count of C
			}
		}
	}

	// compute column pointers of C
	int	nz = 0;
	for (int i = 0; i < n; i++)
		w[i] = (nz += w[i]);
	return nz;
}

// C = A(p,p) where A and C are symmetric the upper part stored; pinv not p
sparse_layout sparse_layout::permute_symmetric(const int *pinv) const {
	int	*w = alloc_auto(int, n);
	int	nz = symm_perm_cols(*this, w, pinv);

	sparse_layout	C(n, n, nz);
	int	*ia	= this->ia(), *ja = this->ja();
	int	*ic	= C.ia(), *jc = C.ja();

	for (int i = 0; i < n; i++)
		ic[i] = w[i];
	ic[n] = nz;

	for (int i = 0; i < n; i++) {
		int	i2 = pinv ? pinv[i] : i;			// column j of A is column j2 of C
		for (int j = ia[i], j1 = ia[i + 1]; j < j1; j++) {
			int	k = ja[j];
			if (k <= i) {						// skip lower triangular part of A
				int k2	= pinv ? pinv[k] : k;	// row k of A is row k2 of C
				int	q	= w[max(k2, i2)]++;
				jc[q]	= min(k2, i2);
			}
		}
	}
	return C;
}

//-----------------------------------------------------------------------------
//	sparse_symbolic
//-----------------------------------------------------------------------------

struct queue_array : list_array {
	int		*tail, *nque;

	static size_t	size(int m, int n)	{ return list_array::size(m, n) + 2 * n * sizeof(int); }	// next 'only' needs m entries?

	queue_array(linear_allocator a, int n) : list_array(a, n, n) {
		tail	= a.alloc<int>(n);
		nque	= a.alloc<int>(n);
		for (int i = 0; i < n; i++) {
			tail[i] = -1;
			nque[i]	= 0;
		}
	}
	void	link(int i, int j)	{
		if (nque[j]++ == 0)
			tail[j] = 1;
		add_head(i, j);
	}
	void	move(int i, int j)	{
		if (nque[j] == 0)
			tail[j] = tail[i];
		next[tail[i]]= head[j];
		nque[j]		+= nque[i];
	}
};

void sparse_symbolic::init(const sparse_layout &A) {
	parent	= elimination_tree(A, true);
	int	m	= A.m, n = A.n;
	m2		= m;

// get leftmost
	leftmost.create(m);
	for (int i = 0; i < m; i++)
		leftmost[i] = -1;

	int		*ia		= A.ia(), *ja = A.ja();
	for (int k = n - 1; k >= 0; k--) {
		for (int p = ia[k]; p < ia[k + 1]; p++)
			leftmost[ja[p]] = k;
	}

// get pinv
	queue_array	q(alloca(queue_array::size(m, n)), n);

	pinv.create(m + n);
	for (int i = m - 1; i >= 0; i--) {	// scan rows in reverse order
		pinv[i] = -1;					// row i is not yet ordered
		int	k = leftmost[i];
		if (k != -1)
			q.link(i, k);
	}

	lnz	= 0;
	for (int k = 0; k < n; k++) {		// find row permutation and nnz(V)
		lnz++;							// count V(k,k) as nonzero */

		int		i	= q.head[k];		// remove row i from queue k */
		if (i < 0)
			i = m2++;					// add A fictitious row
		pinv[i] = k;

		if (--q.nque[k] > 0) {
			lnz			+= q.nque[k];	// nque[k] is nnz (V(k+1:m,k))
			int		p	= parent[k];
			if (p != -1) {				// move all rows to parent of k
				q.move(k, p);
				q.head[p] = q.next[i];
			}
		}
	}
	for (int i = 0, k = n; i < m; i++) {
		if (pinv[i] < 0)
			pinv[i] = k++;
	}

// get cp
	cp	= col_counts(A, parent, post_order(parent, n), true);	// col counts chol(C'*C)
	unz	= 0;
	for (int k = 0; k < n; k++)
		unz += cp[k];
}

// --- Construct matrix C ---
// p = amd(A+A') if symmetric is true, or amd(A'A) otherwise
sparse_layout amd_makec(const sparse_layout &A, sparse_symbolic::ORDER order, int dense) {
	sparse_layout	C;
	sparse_layout	AT	= transpose(A);
	int				m	= A.m, n = A.n;

	if (order == sparse_symbolic::CHOL_ORDER && n == m) {
		C = A + AT;	// C = A + A'

	} else if (order == sparse_symbolic::LU_ORDER) {
		// C = A'A with no dense rows
		int		*ia	= AT.ia(), *ja = AT.ja();
		int		nz	= 0;
		for (int i = 0, j = 0; i < m; i++) {
			ia[i] = nz;
			int	j1 = ia[i + 1];
			if (j1 - j <= dense) {	// skip dense col j
				for (; j < j1; j++)
					ja[nz++] = ja[j];
			}
			j = j1;
		}
		ia[m] = nz;
		C = AT * transpose(AT);
	} else {
		// C = A'A
		C = AT * A;
	}

	C.filter(!is_diagonal());
	return C;
}

sparse_symbolic sparse_symbolic::QR(const sparse_layout &A, ORDER order) {
	sparse_symbolic	s;
	int	dense	= clamp(10 * sqrt(A.n), 16, A.n - 2);		// find dense threshold
	s.q			= amd(amd_makec(A, order, dense), dense);	// fill-reducing ordering
	s.init(order ? A.permute(NULL, s.q) : A);
	return s;
}

sparse_symbolic sparse_symbolic::LU(const sparse_layout &A, ORDER order) {
	sparse_symbolic	s;
	int	dense	= clamp(10 * sqrt(A.n), 16, A.n - 2);		// find dense threshold
	s.q			= amd(amd_makec(A, order, dense), dense);	// fill-reducing ordering
	s.lnz		= s.unz	= 4 * A.nz() + A.n;					// guess nz(L) and nz(U)
	return s;
}

//-----------------------------------------------------------------------------
//	LU
//-----------------------------------------------------------------------------

template<typename T> struct spsolve_workspace : reach_workspace {
	T	*x;
	static size_t	size(int n)	{ return reach_workspace::size(n) + n * sizeof(T); }

	spsolve_workspace(linear_allocator a, int n) : reach_workspace(a, n) {
		x	= a.alloc<T>(n);
		for (int i = 0; i < n; i++)
			x[i] = 0;
	}

	int spsolve(const sparse_matrix<T> &G, const sparse_vector<T> &b, const int *pinv, bool lower);
};

// solve Gx=b(:,k), where G is either upper (lo=0) or lower (lo=1) triangular
template<typename T> int spsolve_workspace<T>::spsolve(const sparse_matrix<T> &G, const sparse_vector<T> &b, const int *pinv, bool lower) {
	int		n	= G.n;
	int		*ig = G.ia(), *jg = G.ja();
	int		*jb = b.i;
	int		top = reach(G, b, pinv);					// xi[top..n-1]=Reach(B(:,k))

	for (int i = top; i < n; i++)
		x[xi[i]] = 0;

	T		*pb = b.v;
	for (int j = 0, j1 = b.nz; j < j1; j++)
		x[jb[j]] = pb[j]; // scatter B

	T		*pg = G.pa();
	for (int i = top; i < n; i++) {
		int	j = xi[i];										// x(j) is nonzero
		int	J = pinv ? pinv[j] : j;							// j maps to col J of G
		if (J >= 0) {										// column J is empty
			x[j] /= pg[lower ? ig[J] : ig[J + 1] - 1];		// x(j) /= G(j,j)
			for (int p = ig[J] + int(lower), q = ig[J + 1] + int(lower) - 1; p < q; p++)
				x[jg[p]] -= pg[p] * x[j];					// x(i) -= G(i,j) * x(j) */
		}
	}
	return top;
}

//[L,U,pinv]=lu(A,[q lnz unz]). lnz and unz can be guess
template<typename T> sparse_numeric<T> sparse_numeric<T>::LU(const sparse_matrix<T> &A, const sparse_symbolic &S, T tol) {
	int		n		= A.n;
	int		lnz_max	= S.lnz;
	int		unz_max	= S.unz;

	sparse_numeric<T>	N;
	N.L.create(n, n, lnz_max);
	N.U.create(n, n, unz_max);
	N.pinv.create(n);

	spsolve_workspace<T>	ws(alloca(spsolve_workspace<T>::size(n)), n);
	int		*il		= N.L.ia(), *iu = N.U.ia();
	int		*jl		= N.L.ja(), *ju = N.U.ja();
	T		*pl		= N.L.pa(), *pu = N.U.pa();

	for (int i = 0; i < n; i++)
		N.pinv[i] = -1;			// no rows pivotal yet

	for (int i = 0; i <= n; i++)
		il[i] = 0;				// no cols of L yet

	int	lnz = 0, unz = 0;
	for (int k = 0; k < n; k++) {		// compute L(:,k) and U(:,k)
		// --- Triangular solve ---
		il[k] = lnz;				// L(:,k) starts here
		iu[k] = unz;				// U(:,k) starts here

		if (lnz + n > lnz_max) {
			N.L.reserve(lnz, lnz_max = lnz_max * 2 + n);
			il	= N.L.ia();
			jl	= N.L.ja();
			pl	= N.L.pa();
		}
		if (unz + n > unz_max) {
			N.U.reserve(unz, unz_max = unz_max * 2 + n);
			iu = N.U.ia();
			ju = N.U.ja();
			pu = N.U.pa();
		}

		int	col = S.q ? S.q[k] : k;
		int	top = ws.spsolve(N.L, A[col], N.pinv, true);	// x = L\A(:,col)

		// --- Find pivot ---
		int	ipiv	= -1;
		int	a		= -1;
		for (int p = top; p < n; p++) {
			int	i = ws.xi[p];		// x(i) is nonzero
			if (N.pinv[i] < 0) {	// row i is not yet pivotal
				int	t = abs(ws.x[i]);
				if (t > a) {
					a		= t;	// largest pivot candidate so far
					ipiv	= i;
				}
			} else {				// x(i) is the entry U(pinv[i],k)
				ju[unz]		= N.pinv[i];
				pu[unz++]	= ws.x[i];
			}
		}

//		if (ipiv == -1 || a <= 0)
//			return (cs_ndone (N, NULL, xi, x, 0));

		if (N.pinv[col] < 0 && abs(ws.x[col]) >= a * tol)
			ipiv = col;

		// --- Divide by pivot ---
		T	pivot		= ws.x[ipiv];		// the chosen pivot
		ju[unz]			= k;				// last entry in U(:,k) is U(k,k)
		pu[unz++]		= pivot;
		N.pinv[ipiv]	= k;				// ipiv is the kth pivot row
		jl[lnz]			= ipiv;				// first entry in L(:,k) is L(k,k) = 1
		pl[lnz++]		= 1;

		for (int p = top; p < n; p++) {		// L(k+1:n,k) = x / pivot
			int	i = ws.xi[p];
			if (N.pinv[i] < 0) {				// x(i) is an entry in L(:,k)
				jl[lnz]		= i;				// save unpermuted row in L
				pl[lnz++]	= ws.x[i] / pivot;	// scale pivot column
			}
			ws.x[i] = 0;						// x[0..n-1] = 0 for next k
		}
	}
	// --- Finalize L and U ---
	il[n] = lnz;
	iu[n] = unz;
	for (int p = 0; p < lnz; p++)
		jl[p] = N.pinv[jl[p]];

	return N;
}

//-----------------------------------------------------------------------------
//	QR
//-----------------------------------------------------------------------------

// sparse QR factorization[V,beta,pinv,R] = qr (A)
template<typename T> sparse_numeric<T> sparse_numeric<T>::QR(const sparse_matrix<T> &A, const sparse_symbolic &S) {
	int					n = A.n, m2 = S.m2;
	sparse_numeric<T>	N;
	sparse_matrix<T>	&V = N.L;
	sparse_matrix<T>	&R = N.U;

	V.create(m2, n, S.lnz);
	R.create(m2, n, S.unz);
	N.B.create(n);

	int	*w = alloc_auto(int, m2 + n);
	T	*x = alloc_auto(T, m2);
	int	*s	= w + m2;

	for (int i = 0; i < m2; i++)
		w[i] = -1;
	for (int i = 0; i < m2; i++)
		x[i] = 0;

	int		*ia = A.ia(), *iv = V.ia(), *ir = R.ia();
	int		*ja = A.jb(), *jv = V.ja(), *jr = R.ja();
	T		*pa = A.pa(), *pv = V.pa(), *pr = R.pa();

	int	rnz = 0, vnz = 0;
	for (int k = 0; k < n; k++) {			// compute V and R
		int		p1	= vnz;
		ir[k]		= rnz;					// R(:,k) starts here
		iv[k]		= vnz;					// V(:,k) starts here
		w[k]		= k;					// add V(k,k) to pattern of V
		jv[vnz++]	= k;
		int	top		= n;
		int	col		= S.q ? S.q[k] : k;
		for (int p = ia[col]; p < ia[col + 1]; ia++) {	// find R(:,k) pattern
			int	len	= 0;
			for (int i = S.leftmost[ja[p]]; w[i] != k; i = S.parent[i]) {// traverse up to k
				s[len++]	= i;
				w[i]		= k;
			}
			while (len > 0)
				s[--top] = s[--len];		// push path on stack
			int	i = S.pinv[ja[p]];			// i = permuted row of A(:,col)
			x[i] = pa[p];					// x (i) = A(:,col)
			if (i > k && w[i] < k) {		// pattern of V(:,k) = x (k+1:m)
				jv[vnz++]	= i;			// add i to pattern of V(:,k)
				w[i]		= k;
			}
		}
		for (int p = top; p < n; p++) {		// for each i in pattern of R(:,k)
			int	i = s[p];					// R(i,k) is nonzero
			applyhh(V[i], x, N.B[i]);		// apply (V(i),Beta(i)) to x
			jr[rnz]		= i;				// R(i,k) = x(i)
			pr[rnz++]	= x[i];
			x[i]		= 0;
			if (S.parent[i] == k)
				vnz += V[i].add_pattern(w, k, jv + vnz);
		}
		for (int p = p1; p < vnz; p++) {	// gather V(:,k) = x
			pv[p]		= x[jv[p]];
			x[jv[p]]	= 0;
		}
		jr[rnz]		= k;					// R(k,k) = norm (x)
		ir[rnz++]	= house(pv + p1, N.B[k], vnz - p1);		//[v,beta]=house(x)
	}
	ir[n]	= rnz;
	iv[n]	= vnz;
	return N;
}

//-----------------------------------------------------------------------------
//	CHOL
//-----------------------------------------------------------------------------

template<typename T> struct chol_workspace : reach_workspace {
	T	*x;
	static size_t	size(int n)	{ return reach_workspace::size(n) + n * sizeof(T); }

	chol_workspace(linear_allocator a, int n) : reach_workspace(a, n) {
		x	= a.alloc<T>(n);
		for (int i = 0; i < n; i++)
			x[i] = 0;
	}

};

// L = chol (A,[pinv parent cp]), pinv is optional
template<typename T> sparse_numeric<T> sparse_numeric<T>::CHOL(const sparse_matrix<T> &A, const sparse_symbolic &S) {
	sparse_numeric<T>	N;
	int					n = A.n;

	chol_workspace<T>	ws(alloca(chol_workspace<T>::size(n)), n);

	sparse_layout		C = S.pinv ? A.permute_symmetric(S.pinv) : A;
	N.L.create(n, n, S.cp[n]);

	int		*ic = C.ia(), *il = N.L.ia(), *jc = C.ja(), *jl = N.L.ja();
	T		*pc	= C.pa(), *pl = N.L.pa();

	for (int i = 0; i < n; i++)
		il[i] = ws.xi[i] = S.cp[i];

	for (int k = 0; k < n; k++) {		// compute L(k,:) for L*L' = C

		// --- Nonzero pattern of L(k,:) ---
		int	top = ws.ereach(C, k, S.parent);		// find pattern of L(k,:)
		ws.x[k] = 0;								// x (0:k) is now zero
		for (int p = ic[k]; p < ic[k + 1]; p++) {	// x = full(triu(C(:,k)))
			if (jc[p] <= k)
				ws.x[jc[p]] = pc[p];
		}
		T	d	= ws.x[k];					// d = C(k,k)
		ws.x[k]	= 0;						// clear x for k+1st iteration

		// --- Triangular solve ---
		for (; top < n; top++) {			// solve L(0:k-1,0:k-1) * x = C(:,k)
			int		i	= ws.stack[top];		// s[top..n-1] is pattern of L(k,:)
			auto	lki	= ws.x[i] / lp[il[i]];	// L(k,i) = x (i) / L(i,i)
			ws.x[i]	= 0;					// clear x for k+1st iteration
			for (int p = il[i] + 1; p < ws.xi[i]; p++)
				ws.x[jl[p]] -= lp[p] * lki;
			d -= lki * lki;					// d = d - L(k,i)*L(k,i)
			p = ws.xi[i]++;
			jl[p] = k;						// store L(k,i) in column i
			lp[p] = lki;
		}
		// --- Compute L(k,k) ---
		if (d <= 0)//return;	not pos def
			break;

		int	p	= ws.xi[k]++;
		jl[p]	= k;		// store L(k,k) = sqrt (d) in column k
		lp[p]	= sqrt(d);
	}
	il[n] = S.cp[n];
	return N;
}

//-----------------------------------------------------------------------------
//	LDL
//-----------------------------------------------------------------------------

sparse_layout ldl_symbolic(const sparse_layout &A, const ref_array<int> &P, ref_array<int> &Pinv, ref_array<int> &parent) {
	int		n		= A.rows();
	int		*Ap		= A.ia();
	int		*Ai		= A.ja();
	int		*flag	= alloc_auto(int, n);
	int		*nz		= alloc_auto(int, n);

	// compute Pinv, the inverse of P
	invert_permute(P, Pinv, n);

	for (int k = 0; k < n; k++) {
		// L(k,:) pattern: all nodes reachable in etree from nz in A(0:k-1,k)
		parent[k]	= -1;			// parent of k is not yet known
		flag[k]		= k;			// mark node k as visited
		nz[k]		= 0;			// count of nonzeros in column k of L
		int		kk	= P[k];			// kth permuted column

		for (int p = Ap[kk], p2 = Ap[kk + 1]; p < p2; p++) {
			// A (i,k) is nonzero (original or permuted A)
			int i = Pinv[Ai[p]];
			if (i < k) {
				// follow path from i to root of etree, stop at flagged node
				for (; flag[i] != k; i = parent[i]) {
					// find parent of i if not yet determined
					if (parent[i] == -1)
						parent[i] = k;
					nz[i]++;				// L(k,i) is nonzero
					flag[i] = k;			// mark i as visited
				}
			}
		}
	}

	int	nz_total = 0;
	for (int k = 0; k < n; k++)
		nz_total += nz[k];

	sparse_layout	out(n, n, nz_total);
	int				*ia = out.ia();
	nz_total = 0;
	for (int k = 0; k < n; k++) {
		ia[k] = nz_total;
		nz_total += nz[k];
	}
	ia[n] = nz_total;
	return out;
}

}// namespace iso

