#include "base/algorithm.h"
#include "base/bits.h"
#include "base/maths.h"
#include "base/vector.h"
#include "extra/random.h"
#include "utilities.h"
#include "model_utils.h"
#include "extra/kd_tree.h"

namespace iso {

struct CacheVertex {
	uint32	ntris:8, offset:24;

	void	add(uint32 *adjacency, int t) {
		adjacency[offset + ntris++] = t;
	}
	bool	remove(uint32 *adjacency, int t) {
		adjacency += offset;
		for (int j = 0; j < ntris; j++) {
			if (adjacency[j] == t) {
				adjacency[j] = adjacency[--ntris];
				return true;
			}
		}
		return false;
	}
	range<const uint32*>	adjacents(const uint32 *adjacency) {
		return make_range_n(adjacency + offset, ntris);
	}
	CacheVertex() : ntris(0) {}
};

template<typename T, typename I> static void FindAdjacentTriangles(range<T*> verts, uint32 *adjacency, range<const I*> indices) {
	// First scan over the vertex data, count the total number of occurrances of each vertex
	for (auto i : indices)
		verts[i].ntris++;

	// Count the triangle array offset for each vertex, initialize the rest of the data
	uint32	offset = 0;
	for (auto &i : verts) {
		i.offset	= offset;
		offset		+= i.ntris;
		i.ntris		= 0;
	}

	// Fill the vertex data structures with indices to the triangles using each vertex
	int	j = 0;
	for (auto i : indices)
		verts[i].add(adjacency, j++ / 3);
}

//template<typename I> static void FindAdjacentTriangles(uint32 *adjacency, range<const I*> indices, uint32 nverts) {
//	temp_array<CacheVertex>	verts(nverts);
//	FindAdjacentTriangles(make_rangec(verts), adjacency, indices);
//}

//-------------------
// The return value is the total cost of pushing the triangle, in terms of FIFO misses

static uint32 PushTriangle(uint32 *fifo, uint32 fifo_size, uint32 i0, uint32 i1, uint32 i2) {
	uint32 *p0 = 0, *p1 = 0, *p2 = 0;

	for (uint32 *p = fifo, *end = p + fifo_size; p < end; p++) {
		const uint32 i = *p;
		if (i == i0)
			p0 = p;
		if (i == i1)
			p1 = p;
		if (i == i2)
			p2 = p;
	}

	uint32 pushes = 0;
	if (!p0)
		fifo[fifo_size + pushes++] = i0;
	if (!p1)
		fifo[fifo_size + pushes++] = i1;
	if (!p2)
		fifo[fifo_size + pushes++] = i2;

	return pushes;
}

static inline uint32 PushTriangle(uint32 *fifo, uint32 fifo_size, const uint32 *indices) {
	return PushTriangle(fifo, fifo_size, indices[0], indices[1], indices[2]);
}

//-------------------

struct VertexCacheOptimizerS {
	struct Params {
		float k1, k2, k3;
		Params(float k1, float k2, float k3) : k1(k1), k2(k2), k3(k3) {}
		float Score(uint32 pushes, uint32 adjacent, float distance) const { return k1 * pushes - k2 * adjacent + k3 * distance; }
	};

	temp_array<CacheVertex>	verts;
	temp_array<uint32>		adjacency;
	uint32			fifo[1000];

	uint32			PushFocusVertex(range<const uint32*> adjacent, const uint32 *indices, uint32 **out, uint32 fifo_size);
	float			ScoreVertex(uint32 index, const uint32 *indices, uint32 fifo_size, const Params &params);

	VertexCacheOptimizerS(range<const uint32*> indices, uint32 nverts, uint32 *out, uint32 _fifoSize, const Params &params);
};

uint32 VertexCacheOptimizerS::PushFocusVertex(range<const uint32*> adjacent, const uint32 *indices, uint32 **out, uint32 fifo_size) {
	temp_dynamic_array<uint32> adjacent2 = adjacent;

	// this loop simply finds the cheapest triangle to push into the triangle list and does it until it runs out of triangles
	static const uint32 max_uint32	= maximum;
	uint32	totalPush = 0;
	while (!adjacent2.empty()) {
		uint32*	best_index	= nullptr;
		uint32	best_score	= max_uint32;

		for (auto &t : adjacent2) {
			uint32 score = PushTriangle(fifo + totalPush, fifo_size, indices + t * 3);
			if (score < best_score) {
				best_score	= score;
				best_index	= &t;
			}
		}
		// push the best face onto the fifo
		auto	t	= *best_index;
		auto	tri	= indices + t * 3;
		totalPush	+= PushTriangle(fifo + totalPush, fifo_size, tri);

		if (out) {
			for (int i = 0; i < 3; i++) {
				int		v = tri[i];
				*(*out)++ = v;
				verts[v].remove(adjacency, t);
			}
		}

		// delete the best_triangle from the array
		adjacent2.erase_unordered(best_index);
	}

	return totalPush;
}

float VertexCacheOptimizerS::ScoreVertex(uint32 index, const uint32 *indices, uint32 fifo_size, const Params &params) {
	uint32	nadj = verts[index].ntris;

	// skip any vertex that is black
	if (nadj == 0)
		return maximum;

	// count up the number of pushes adding this focus vertex takes (C1)
	uint32 pushes	= PushFocusVertex(make_range_n(adjacency + verts[index].offset, nadj), indices, NULL, fifo_size);

	// we know this vertex turned black now, let's find out where it is in the buffer (C3)
	uint32 distance = 0;
	for (uint32 *p = fifo + pushes; p < fifo + pushes + fifo_size; p++, distance++) {
		if (*p == index)
			break;
	}

	// calculate the final score for this vertex
	return params.Score(pushes, nadj, float(distance) / fifo_size);
}

VertexCacheOptimizerS::VertexCacheOptimizerS(range<const uint32*> indices, uint32 nverts, uint32 *out, uint32 fifo_size, const Params &params) : verts(nverts), adjacency(indices.size()) {
	temp_array<uint32>	nonBlackIndexes(indices.size32());
	temp_array<uint32>	potentialBlackIndexes(indices.size32());

	FindAdjacentTriangles(make_rangec(verts), adjacency.begin(), indices);

	memset(fifo, 0xff, sizeof(fifo));		// initialize the fifo to unmatchable indexes

	auto	pindices = indices.begin();
	rcopy(nonBlackIndexes, pindices);

	// This loop continues until all indexes are black (completely used)
	sort(nonBlackIndexes);
	uint32 numIndexesLeft = uint32(unique(nonBlackIndexes.begin(), nonBlackIndexes.end()) - nonBlackIndexes.begin());
	static const uint32	max_uint32	= maximum;

	while (numIndexesLeft) {
		// Look through the FIFO and take the vertex with the lowest cost
		uint32	bestIndex	= max_uint32;
		float	best_score	= maximum;

		for (const uint32 *p = fifo; p < fifo + fifo_size; p++) {
			const uint32 index = *p;
			if (index != max_uint32) {
				float const score = ScoreVertex(index, pindices, fifo_size, params);
				if (score < best_score) {
					bestIndex	= index;
					best_score	= score;
				}
			}
		}

		// If no vertex is decidedly best, pick the lowest-degree vertex out of the non-black verts
		if (bestIndex == max_uint32) {
			// count the faces left on each vertex, and pick the one with the fewest faces
			uint32 lowestDegree = maximum;
			for (uint32 i = 0; i < numIndexesLeft; i++) {
				const uint32	index	= nonBlackIndexes[i];
				const uint32	nadj	= verts[index].ntris;

				if (nadj < lowestDegree) {
					// this can only fail if there's a bug that does not properly remove black vertexes from the nonBlackIndexes array
					ISO_ASSERT(nadj != 0);
					lowestDegree	= nadj;
					bestIndex		= index;
				}
			}
		}

		// there must be a vertex to add by now
		ISO_ASSERT(bestIndex != max_uint32);

		// add the vertex and all its related triangles
		auto	adjacent	= verts[bestIndex].adjacents(adjacency);

		// copy out the vertex indexes that may be going black
		uint32			numPotentialBlackIndexes = 0;
		for (auto t : adjacent) {
			potentialBlackIndexes[numPotentialBlackIndexes++] = pindices[t * 3 + 0];
			potentialBlackIndexes[numPotentialBlackIndexes++] = pindices[t * 3 + 1];
			potentialBlackIndexes[numPotentialBlackIndexes++] = pindices[t * 3 + 2];
		}

		// actually push the fan of faces now
		uint32	pushes	= PushFocusVertex(adjacent, pindices, &out, fifo_size);

		// run through all the vertexes related to the faces related to this bestVertex
		sort(potentialBlackIndexes.slice_to(numPotentialBlackIndexes));
		for (uint32 *i = potentialBlackIndexes, *e = unique(potentialBlackIndexes.slice_to(numPotentialBlackIndexes)); i != e; ++i) {
			const uint32	v	= *i;
			if (verts[v].ntris == 0) {
				// remove it from the nonBlackIndexes array
				uint32 *ptr = lower_bound(nonBlackIndexes.begin(), nonBlackIndexes.begin() + numIndexesLeft, v);
				ISO_ASSERT(ptr != nonBlackIndexes + numIndexesLeft && *ptr == v);
				//memmove(ptr, ptr + 1, sizeof(uint32) * (nonBlackIndexes + numIndexesLeft - (ptr + 1)));
				erase_array(ptr, ptr + 1, nonBlackIndexes + numIndexesLeft);
				numIndexesLeft--;
			}
		}

		// move the fifo back to the start region again, so we don't overrun our static buffer
		for (uint32 i = 0; i < fifo_size; i++)
			fifo[i] = fifo[i + pushes];
	}

}

// Interleave the index buffer so each VGT has locality
static void Alternate(range<uint32*> out, uint32 alternatePrim) {
	uint32	num_tris		= out.size32() / 3;
	uint32	numIndexSets	= (num_tris + alternatePrim - 1) / alternatePrim;

	// The last index 'set' has to be at the end. This means that for even sets, vgt0 works on the top half, and for odd sets vgt0 works on the bigger bottom half
	uint32 vgt0	= 0;
	uint32 vgt1	= (numIndexSets / 2) * alternatePrim;

	if (numIndexSets & 1)
		swap(vgt0, vgt1);

	temp_array<uint32> buffer(out.size());
	for (uint32 i = 0; i < num_tris;) {
		uint32	n = min(num_tris - i, alternatePrim);
		memcpy(buffer + i * 3, out.begin() + vgt0 * 3, sizeof(uint32) * 3 * n);
		i		+= n;
		vgt0	+= n;

		if (n = min(num_tris - i, alternatePrim)) {
			memcpy(buffer + i * 3, out.begin() + vgt1 * 3, sizeof(uint32) * 3 * n);
			i		+= n;
			vgt1	+= n;
		}
	}

	memcpy(out.begin(), buffer, sizeof(uint32) * num_tris * 3);
}

//-------------------

// This function measures the cost of cache misses when rendering a triangle list. Lower costs are faster (and thus preferable).
uint32 GetCacheCost(range<const uint32*> indices, uint32 fifo_size, uint32 alternatePrim) {
	// initialize the fifos to unused values (one fifo per shader engine)
	uint32 sizeFifo			= fifo_size + 3;
	uint32 *fifos			= alloc_auto(uint32, 2 * sizeFifo);
	uint32 totalCosts[2]	= {0};

	memset(fifos, 0xff, sizeof(uint32) * 2 * sizeFifo);

	for (uint32 t = 0, nt = indices.size() / 3; t < nt; t++) {
		uint32	se		= (t / alternatePrim) & 1;
		uint32 *fifo	= fifos + se * sizeFifo;
		uint32	pushes	= PushTriangle(fifo, fifo_size, indices.begin() + t * 3);
		totalCosts[se] += pushes;

		// copy back the fifo now so it starts at the beginning of fifo[]
		for(uint32 i = 0; i < sizeFifo - pushes; i++)
			fifo[i] = fifo[i + pushes];

		for(uint32 i = sizeFifo - pushes; i < sizeFifo; i++)
			fifo[i] = maximum;
	}
	return totalCosts[0] + totalCosts[1];
}

//------------------

uint32 VertexCacheOptimizerHillclimber(range<const uint32*> indices, uint32 nverts, uint32 *out, uint32 hillClimberIterations, uint32 fifo_size, uint32 alternatePrim) {
	if (out == indices.begin()) {
		temp_array<uint32>	temp(indices.size());
		uint32	cost	= VertexCacheOptimizerHillclimber(indices, nverts, temp, hillClimberIterations, fifo_size, alternatePrim);
		copy(temp, out);
		return cost;
	}

	VertexCacheOptimizerS::Params best(1, 0.5f, 1.3f);
	auto	out2	= make_range_n(out, indices.size());

	// initially run the default constants and measure it as a baseline promise to the partitioner
	VertexCacheOptimizerS(indices, nverts, out, fifo_size, best);
	Alternate(out2, alternatePrim);
	uint32	best_score = GetCacheCost(out2, fifo_size, alternatePrim);

	if (hillClimberIterations) {
		rng<simple_random>	random;

		for (uint32 runs = 0; runs < hillClimberIterations; runs++) {
			// permute the constants a bit
			float	d = float(hillClimberIterations - runs) / hillClimberIterations;
			VertexCacheOptimizerS::Params	params(
				best.k1 + random.from(-d, d),
				best.k2 + random.from(-d, d),
				best.k3 + random.from(-d, d)
			);

			// re-run the optimizer
			VertexCacheOptimizerS(indices, nverts, out, fifo_size, params);
			Alternate(out2, alternatePrim);
			uint32 score = GetCacheCost(out2, fifo_size, alternatePrim);

			// only consider an ordering that is as good or better than a previous one, and is not LARGER when generating the index table
			if (score < best_score) {
				best		= params;
				best_score	= score;
			}
		}

		// run the best one through and remember it
		VertexCacheOptimizerS(indices, nverts, out, fifo_size, best);
		Alternate(out2, alternatePrim);
		best_score = GetCacheCost(out2, fifo_size, alternatePrim);
	}
	return best_score;
}

//-----------------------------------------------------------------------------
// Tom Forsyth's cache optimiser (uses a LRU cache)
//-----------------------------------------------------------------------------

struct VertexCacheOptimizerForsythParams {
	uint32	cache_size;
	float	cache_score[32];
	float	valence_score[32];

	VertexCacheOptimizerForsythParams(uint32 cache_size, float last_tri_score = 0.75f, float decay_power = 1.5f, float valence_scale = 2, float valence_power = 0.5f);

	float	Score(uint32 adjacent, int cache_pos) const {
		return	adjacent == 0 ? 0
			:	(cache_pos < 0 ? 0 : cache_score[cache_pos]) + (adjacent < num_elements(valence_score) ? valence_score[adjacent] : 0);
	}
};

VertexCacheOptimizerForsythParams::VertexCacheOptimizerForsythParams(uint32 cache_size, float last_tri_score, float decay_power, float valence_scale, float valence_power) : cache_size(cache_size) {
	for (int i = 0; i < num_elements(cache_score); i++)
		cache_score[i] = i < 3 ? last_tri_score : pow(1 - float(i - 3) / (cache_size - 3), decay_power);

	// Bonus points for having a low number of tris still to use the vert, so we get rid of lone verts quickly
	for (int i = 1; i < num_elements(valence_score); i++)
		valence_score[i] = valence_scale * pow(float(i), -valence_power);
}

void VertexCacheOptimizerForsyth(range<const uint32*> indices, uint32 nverts, uint32 *out, const VertexCacheOptimizerForsythParams &params) {
	struct Vertex : CacheVertex {
		int8	cache_pos	= -1;
		float	last_score;

		float	Score(const VertexCacheOptimizerForsythParams &params) const {
			return params.Score(ntris, cache_pos);
		}
		void	updateScore(const VertexCacheOptimizerForsythParams &params, uint32 *adjacency, float *score) {
			float newScore	= Score(params);
			float diff		= newScore - last_score;
			for (int j = 0; j < ntris; j++)
				score[adjacency[offset + j]] += diff;
			last_score = newScore;
		}
	};

	uint32	cache_size	= params.cache_size;

	temp_array<int>		cache(cache_size + 3, -1);
	temp_array<Vertex>	verts(nverts);
	temp_array<uint32>	adjacency(indices.size());
	
	FindAdjacentTriangles(make_rangec(verts), adjacency, indices);

	// Initialize the score for all vertices
	for (auto &i : verts)
		i.last_score = i.Score(params);

	// Initialize the score for all triangles
	temp_array<float>	score = transformc<3>(indices, [&](uint32 v0, uint32 v1, uint32 v2)->float {
		return verts[v0].last_score + verts[v1].last_score + verts[v2].last_score;
	});

	for (;;) {
		// Find the best triangle
		int best_triangle	= -1;
		int best_score		= -1;
		for (auto &i : score) {
			if (i > best_score) {
				best_score		= i;
				best_triangle	= score.index_of(i);
			}
		}
		if (best_triangle < 0)
			break;

		// Output the currently best triangle, as long as there are triangles left to output
		while (best_triangle >= 0) {
			// Mark the triangle as added
			score[best_triangle] = -1;

			for (int i = 0; i < 3; i++) {
				// Update this vertex
				int v = indices[3 * best_triangle + i];
				*out++	= v;

				// Check the current cache position, if it is in the cache
				int endpos = verts[v].cache_pos;
				if (endpos < 0)
					endpos = cache_size + i;

				// Move all cache entries from the previous position in the cache to the new target position (i) one step backwards
				for (int j = endpos; j > i; j--) {
					int	x = cache[j] = cache[j - 1];
					// If this cache slot contains a real vertex, update its cache tag
					if (x >= 0)
						verts[x].cache_pos = j;
				}
				// Insert the current vertex into its new target slot
				cache[i]			= v;
				verts[v].cache_pos	= i;
				verts[v].remove(adjacency, best_triangle);
			}

			// Update the scores of all verts & triangles in the cache
			for (int i = 0; i < cache_size + 3; i++) {
				int v = cache[i];
				if (v < 0)
					break;
				if (i >= cache_size) {
					// vertex has been pushed outside of the actual cache
					verts[v].cache_pos	= -1;
					cache[i]			= -1;
				}
				verts[v].updateScore(params, adjacency, score);
			}

			// Find the best triangle referenced by vertices in the cache
			best_triangle	= -1;
			best_score		= -1;
			for (int i = 0; i < cache_size; i++) {
				int v = cache[i];
				if (v < 0)
					break;
				for (int j = 0; j < verts[v].ntris; j++) {
					int t = adjacency[verts[v].offset + j];
					if (score[t] > best_score) {
						best_triangle	= t;
						best_score		= score[t];
					}
				}
			}
		}
	}
}

void VertexCacheOptimizerForsyth(range<const uint32*> indices, uint32 nverts, uint32 *out, uint32 cache_size) {
	if (out != indices.begin())
		return VertexCacheOptimizerForsyth(indices, nverts, out, VertexCacheOptimizerForsythParams(cache_size));

	temp_array<uint32>	temp(indices.size());
	VertexCacheOptimizerForsyth(indices, nverts, temp, VertexCacheOptimizerForsythParams(cache_size));
	copy(temp, out);
}

//-----------------------------------------------------------------------------
// Meshletize
//-----------------------------------------------------------------------------

// tries to add a triangle to this meshlet
template<typename T> void InlineMeshlet::add(const T *tri) {
	uint32	indices[3] = {~0, ~0, ~0};

	for (uint32 j = 0; j < 3; ++j) {
		for (auto &i : unique_indices) {
			if (i == tri[j]) {
				indices[j] = unique_indices.index_of(i);
				break;
			}
		}
		if (!~indices[j]) {
			indices[j] = unique_indices.size32();
			unique_indices.push_back(tri[j]);
		}
	}

	// Add the new primitive
	primitives.push_back({indices[0], indices[1], indices[2]});
}

//
// Strongly influenced by https://github.com/zeux/meshoptimizer - Thanks amigo!
//

InlineMeshlet::CullData InlineMeshlet::ComputeCullData(const range<stride_iterator<const float3p>> &verts) const {
	position3	vertices[256];
	float3		normals[256];

	// get vertices
	auto	v = &vertices[0];
	for (auto &i : unique_indices)
		*v++ = position3(verts[i]);

	// Generate primitive normals
	auto	n = &normals[0];
	for (auto &i : primitives)
		*n++ = normalise(GetNormal(vertices[i.i0], vertices[i.i1], vertices[i.i2]));

	// Calculate spatial bounds
	auto bound = sphere::bound_ritter(make_range(&vertices[0], v));

	// Calculate the normal cone
	// 1. Normalized center point of minimum bounding sphere of unit normals == conic axis
	float3 axis = normalise(sphere::bound_ritter(make_range((const position3*)normals, (const position3*)n)).centre().v);

	// 2. Calculate dot product of all normals to conic axis, selecting minimum
	float mind = 1;
	for (auto &i : make_range(&normals[0], n))
		mind = min(mind, dot(axis, i));

	float maxt = 0;
	if (mind < .1f) {
		// Degenerate cone
		axis	= zero;
		mind	= 0;

	} else {
		// Find the point on center-t*axis ray that lies in negative half-space of all triangles
		auto	c = bound.centre();
		for (auto &i : primitives) {
			auto	n	= normals[primitives.index_of(i)];
			float	dc	= dot(c - vertices[i.i0], n);
			float	dn	= dot(axis, n);
			maxt = max(maxt, dc / dn);
		}
	}

	return {bound, axis, sqrt(1 - square(mind)), maxt};
}

struct TriInfo {
	float3		centroid;
	float3		normal;
	TriInfo(const triangle3 t) :  centroid(t.centre()), normal(t.normal()) {}
};

template<typename T> dynamic_array<InlineMeshlet> Meshletize(uint32 max_verts, uint32 max_prims, range<const T*> indices, range<stride_iterator<const float3p>> verts) {
	const uint32 num_tris = indices.size32() / 3;

	dynamic_array<TriInfo>	tri_info = transformc<3>(indices, [&](int i0, int i1, int i2) {
		return triangle3(position3(verts[i0]), position3(verts[i1]), position3(verts[i2]));
	});

	auto	kd	= make_kd_tree(make_field_container(tri_info, centroid));

	temp_array<CacheVertex>				vert_adjacency(verts.size32());
	temp_array<uint32>					adjacency(indices.size());
	FindAdjacentTriangles(make_rangec(vert_adjacency), adjacency, indices);

	dynamic_bitarray<>					checklist(num_tris, false);
	dynamic_array<position3>			positions;
	dynamic_array<float3>				normals;
	dynamic_array<pair<uint32,float>>	candidates;
	hash_set<uint32>					candidateCheck;

	dynamic_array<InlineMeshlet>		output;

	for (uint32 t = 0; t != num_tris; t = checklist.next(t, false)) {
		candidates.emplace_back(t, 0);
		candidateCheck.insert(t);
		
		auto	&curr = output.push_back();

		for (;;) {
			uint32	index;
			if (candidates.empty()) {
				float	dist;
				auto	i = kd.nearest_neighbour(sphere::bound_ritter(positions).centre(), dist);
				if (i < 0)
					break;
				index = i;
			} else {
				index = candidates.pop_back_value().a;
			}

			// try to add triangle to meshlet
			auto	tri				= indices.begin() + index * 3;
			auto	num_indices		= curr.num_new(tri);
			if (curr.unique_indices.size() + num_indices <= max_verts) {
				curr.add(tri);
				checklist[index] = true;		// mark as added
				kd.find(index).remove();

				// determine whether we need to move to the next meshlet
				if (curr.primitives.size() == max_prims)
					break;

				// add positions & normal
				if (num_indices)
					for (auto i : curr.unique_indices.slice(-(int)num_indices))
						positions.push_back(verts[i]);
				normals.push_back(normalise(GetNormal(position3(verts[tri[0]]), position3(verts[tri[1]]), position3(verts[tri[2]]))));

				// add all applicable adjacent triangles to candidate list
			#if 1
				for (uint32 i = 0; i < 3; ++i) {
					uint32	v	= indices[index * 3 + i];
					for (auto adj : vert_adjacency[v].adjacents(adjacency)) {
						if (adj != -1 && !checklist[adj] && candidateCheck.count(adj) == 0) {
							candidates.emplace_back(adj, 0);
							candidateCheck.insert(adj);
						}
					}
				}
			#endif
				if (candidates.size() > 1) {
					// order candidates by score
					auto	bound	= sphere::bound_ritter(positions);
					float3	normal	= normalise(sphere::bound_ritter(element_cast<const position3>(normals)).centre().v);
					for (auto &i : candidates) {
						auto	tri			= indices.begin() + i.a * 3;

						// Computes a candidacy score based on spatial locality, orientational coherence, and vertex re-use within a meshlet
						// Vertex reuse
						float	reuseScore	= curr.num_new(tri);

						// Distance from centre point
						float	max2 = zero;
						for (uint32 i = 0; i < 3; ++i)
							max2 = max(max2, len2(bound.centre() - position3(verts[tri[i]])));

						float	locScore	= log2(max2 / bound.radius2() + one);

						// Angle between normal and meshlet cone axis
						float3	n			= normalise(GetNormal(position3(verts[tri[0]]), position3(verts[tri[1]]), position3(verts[tri[2]])));
						float	oriScore	= (one - dot(n, normal)) / 2;

						i.b = reuseScore + locScore + oriScore;
					}
					sort(candidates, [](auto &a, auto &b) { return a.b > b.b; });
				}

			} else if (candidates.empty()) {
				break;
			}
		}

		positions.clear();
		normals.clear();
		candidates.clear();
		candidateCheck.clear();
	}

	return output;
}

template dynamic_array<InlineMeshlet> Meshletize<uint32>(uint32 max_verts, uint32 max_prims, range<const uint32*> indices, range<stride_iterator<const float3p>> verts);
template dynamic_array<InlineMeshlet> Meshletize<uint16>(uint32 max_verts, uint32 max_prims, range<const uint16*> indices, range<stride_iterator<const float3p>> verts);


}  // namespace iso
