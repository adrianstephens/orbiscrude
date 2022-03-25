#include "base/algorithm.h"
#include "base/bits.h"
#include "base/maths.h"
#include "utilities.h"

namespace iso {

struct CacheVertex {
	uint32	ntris:8, offset:24;

	void	add(uint32 *adjacency, int t) {
		adjacency[offset + ntris++] = t;
	}
	void	remove(uint32 *adjacency, int t) {
		adjacency += offset;
		for (int j = 0; j < ntris; j++) {
			if (adjacency[j] == t) {
				adjacency[j] = adjacency[--ntris];
				break;
			}
		}
	}
	CacheVertex() : ntris(0) {}
};

template<typename T> static void FindAdjacentTriangles(T *verts, uint32 nverts, uint32 *adjacency, const uint32 *indices, uint32 ntris) {
	// First scan over the vertex data, count the total number of occurrances of each vertex
	for (int i = 0; i < ntris * 3; i++)
		verts[indices[i]].ntris++;

	// Count the triangle array offset for each vertex, initialize the rest of the data
	int sum = 0;
	for (int i = 0; i < nverts; i++) {
		verts[i].offset	= sum;
		sum += verts[i].ntris;
		verts[i].ntris	= 0;
	}

	// Fill the vertex data structures with indices to the triangles using each vertex
	for (int i = 0; i < ntris; i++) {
		for (int j = 0; j < 3; j++)
			verts[indices[3 * i + j]].add(adjacency, i);
	}
}

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

	uint32			PushFocusVertex(const uint32 *adjacent, uint32 nadj, const uint32 *indices, uint32 **out, uint32 fifo_size);
	float			ScoreVertex(uint32 index, const uint32 *indices, uint32 fifo_size, const Params &params);

	VertexCacheOptimizerS(const uint32 *indices, uint32 ntris, uint32 nverts, uint32 *out, uint32 _fifoSize, const Params &params);
};

uint32 VertexCacheOptimizerS::PushFocusVertex(const uint32 *adjacent, uint32 nadj, const uint32 *indices, uint32 **out, uint32 fifo_size) {
	uint32 *adjacent2 = alloc_auto(uint32, nadj);

	for (uint32 i = 0; i < nadj; i++)
		adjacent2[i] = adjacent[i];

	// this loop simply finds the cheapest triangle to push into the triangle list and does it until it runs out of triangles
	static const uint32 max_uint32	= maximum;
	uint32	totalPush = 0;
	while (nadj > 0) {
		uint32 bestScore	= max_uint32;
		uint32 bestIndex	= max_uint32;

		for (uint32 i = 0; i < nadj; i++) {
			const uint32 t = adjacent2[i];
			uint32 triangleScore = PushTriangle(fifo + totalPush, fifo_size, indices + t * 3);
			if (triangleScore < bestScore) {
				bestScore	= triangleScore;
				bestIndex	= i;
			}
		}
		ISO_ASSERT(bestIndex != max_uint32);
		uint32 t	= adjacent2[bestIndex];

		// push the best face onto the fifo
		totalPush	+= PushTriangle(fifo + totalPush, fifo_size, indices + t * 3);

		if (out) {
			for (int i = 0; i < 3; i++) {
				int		v = indices[t * 3 + i];
				*(*out)++ = v;
				verts[v].remove(adjacency, t);
			}
		}

		// delete the bestTriangle from the array
		adjacent2[bestIndex] = adjacent2[--nadj];
	}

	return totalPush;
}

float VertexCacheOptimizerS::ScoreVertex(uint32 index, const uint32 *indices, uint32 fifo_size, const Params &params) {
	uint32	nadj = verts[index].ntris;

	// skip any vertex that is black
	if (nadj == 0)
		return maximum;

	// count up the number of pushes adding this focus vertex takes (C1)
	uint32 pushes	= PushFocusVertex(adjacency + verts[index].offset, nadj, indices, NULL, fifo_size);

	// we know this vertex turned black now, let's find out where it is in the buffer (C3)
	uint32 distance = 0;
	for (uint32 *p = fifo + pushes; p < fifo + pushes + fifo_size; p++, distance++) {
		if (*p == index)
			break;
	}

	// calculate the final score for this vertex
	return params.Score(pushes, nadj, float(distance) / fifo_size);
}

VertexCacheOptimizerS::VertexCacheOptimizerS(const uint32 *indices, uint32 ntris, uint32 nverts, uint32 *out, uint32 fifo_size, const Params &params) : verts(nverts), adjacency(ntris * 3) {
	const uint32		nindices			= ntris * 3;
	temp_array<uint32>	nonBlackIndexes(nindices);
	temp_array<uint32>	potentialBlackIndexes(nindices);

	FindAdjacentTriangles(verts.begin(), nverts, adjacency.begin(), indices, ntris);

	memset(fifo, 0xff, sizeof(fifo));		// initialize the fifo to unmatchable indexes

	rcopy(nonBlackIndexes, indices);

	// This loop continues until all indexes are black (completely used)
	sort(nonBlackIndexes);
	uint32 numIndexesLeft = uint32(unique(nonBlackIndexes.begin(), nonBlackIndexes.end()) - nonBlackIndexes.begin());
	static const uint32	max_uint32	= maximum;

	while (numIndexesLeft) {
		// Look through the FIFO and take the vertex with the lowest cost
		uint32	bestIndex	= max_uint32;
		float	bestScore	= maximum;

		for (const uint32 *p = fifo; p < fifo + fifo_size; p++) {
			const uint32 index = *p;
			if (index != max_uint32) {
				float const score = ScoreVertex(index, indices, fifo_size, params);
				if (score < bestScore) {
					bestIndex = index;
					bestScore = score;
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
		const uint32	*adjacent	= adjacency + verts[bestIndex].offset;
		uint32			nadj		= verts[bestIndex].ntris;

		// copy out the vertex indexes that may be going black
		uint32			numPotentialBlackIndexes = 0;
		for (uint32 i = 0; i < nadj; i++) {
			const uint32 t = adjacent[i];
			potentialBlackIndexes[numPotentialBlackIndexes++] = indices[t * 3 + 0];
			potentialBlackIndexes[numPotentialBlackIndexes++] = indices[t * 3 + 1];
			potentialBlackIndexes[numPotentialBlackIndexes++] = indices[t * 3 + 2];
		}

		// actually push the fan of faces now
		uint32	pushes	= PushFocusVertex(adjacent, nadj, indices, &out, fifo_size);

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
static void Alternate(uint32 *out, uint32 ntris, uint32 alternatePrim) {
	uint32 numIndexSets		= (ntris + alternatePrim - 1) / alternatePrim;

	// The last index 'set' has to be at the end. This means that for even sets, vgt0 works on the top half, and for odd sets vgt0 works on the bigger bottom half
	uint32 vgt0	= 0;
	uint32 vgt1	= (numIndexSets / 2) * alternatePrim;

	if (numIndexSets & 1)
		swap(vgt0, vgt1);

	temp_array<uint32> buffer(ntris * 3);
	for (uint32 i = 0; i < ntris;) {
		uint32	n = min(ntris - i, alternatePrim);
		memcpy(buffer + i * 3, out + vgt0 * 3, sizeof(uint32) * 3 * n);
		i		+= n;
		vgt0	+= n;

		if (n = min(ntris - i, alternatePrim)) {
			memcpy(buffer + i * 3, out + vgt1 * 3, sizeof(uint32) * 3 * n);
			i		+= n;
			vgt1	+= n;
		}
	}

	memcpy(out, buffer, sizeof(uint32) * ntris * 3);
}

//-------------------

// This function measures the cost of cache misses when rendering a triangle list. Lower costs are faster (and thus preferable).
uint32 GetCacheCost(const uint32 *indices, uint32 ntris, uint32 fifo_size, uint32 alternatePrim) {
	// initialize the fifos to unused values (one fifo per shader engine)
	uint32 sizeFifo			= fifo_size + 3;
	uint32 *fifos			= alloc_auto(uint32, 2 * sizeFifo);
	uint32 totalCosts[2]	= {0};

	memset(fifos, 0xff, sizeof(uint32) * 2 * sizeFifo);

	for (uint32 t = 0; t < ntris; t++) {
		uint32	se		= (t / alternatePrim) & 1;
		uint32 *fifo	= fifos + se * sizeFifo;
		uint32	pushes	= PushTriangle(fifo, fifo_size, indices + t * 3);
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

uint32 VertexCacheOptimizerHillclimber(const uint32 *indices, uint32 ntris, uint32 nverts, uint32 *out, uint32 hillClimberIterations, uint32 fifo_size, uint32 alternatePrim) {
	if (out == indices) {
		temp_array<uint32>	temp(ntris * 3);
		uint32	cost	= VertexCacheOptimizerHillclimber(indices, ntris, nverts, temp, hillClimberIterations, fifo_size, alternatePrim);
		copy(temp, out);
		return cost;
	}

	VertexCacheOptimizerS::Params best(1, 0.5f, 1.3f);

	// initially run the default constants and measure it as a baseline promise to the partitioner
	VertexCacheOptimizerS(indices, ntris, nverts, out, fifo_size, best);
	Alternate(out, ntris, alternatePrim);
	uint32	bestScore = GetCacheCost(out, ntris, fifo_size, alternatePrim);

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
			VertexCacheOptimizerS(indices, ntris, nverts, out, fifo_size, params);
			Alternate(out, ntris, alternatePrim);
			uint32 score = GetCacheCost(out, ntris, fifo_size, alternatePrim);

			// only consider an ordering that is as good or better than a previous one, and is not LARGER when generating the index table
			if (score < bestScore) {
				best		= params;
				bestScore	= score;
			}
		}

		// run the best one through and remember it
		VertexCacheOptimizerS(indices, ntris, nverts, out, fifo_size, best);
		Alternate(out, ntris, alternatePrim);
		bestScore = GetCacheCost(out, ntris, fifo_size, alternatePrim);
	}
	return bestScore;
}

//-----------------------------------------------------------------------------
// Tom Forsyth's cache optimiser (uses a LRU cache)
//-----------------------------------------------------------------------------

struct VertexCacheOptimizerForsythParams {
	uint32 cache_size;
	float cacheScore[32];
	float valenceScore[32];

	VertexCacheOptimizerForsythParams(uint32 cache_size, float last_tri_score = 0.75f, float decay_power = 1.5f, float valence_scale = 2, float valence_power = 0.5f);

	float	Score(uint32 adjacent, int cachePos) const {
		return	adjacent == 0 ? 0
			:	(cachePos < 0 ? 0 : cacheScore[cachePos]) + (adjacent < num_elements(valenceScore) ? valenceScore[adjacent] : 0);
	}
};

VertexCacheOptimizerForsythParams::VertexCacheOptimizerForsythParams(uint32 cache_size, float last_tri_score, float decay_power, float valence_scale, float valence_power) : cache_size(cache_size) {
	for (int i = 0; i < num_elements(cacheScore); i++)
		cacheScore[i] = i < 3 ? last_tri_score : pow(1 - float(i - 3) / (cache_size - 3), decay_power);

	// Bonus points for having a low number of tris still to use the vert, so we get rid of lone verts quickly
	for (int i = 1; i < num_elements(valenceScore); i++)
		valenceScore[i] = valence_scale * pow(float(i), -valence_power);
}

void VertexCacheOptimizerForsyth(const uint32* indices, uint32 ntris, uint32 nverts, uint32 *out, const VertexCacheOptimizerForsythParams &params) {
	struct Vertex : CacheVertex {
		int8	cachePos;
		float	lastScore;

		float	Score(const VertexCacheOptimizerForsythParams &params) const {
			return params.Score(ntris, cachePos);
		}
		void	updateScore(const VertexCacheOptimizerForsythParams &params, uint32 *adjacency, float *score) {
			float newScore	= Score(params);
			float diff		= newScore - lastScore;
			for (int j = 0; j < ntris; j++)
				score[adjacency[offset + j]] += diff;
			lastScore = newScore;
		}

		Vertex() : cachePos(-1) {}
	};

	uint32	cache_size	= params.cache_size;
	temp_array<Vertex>	verts(nverts);
	temp_array<uint32>	adjacency(3 * ntris);
	FindAdjacentTriangles(verts.begin(), nverts, adjacency, indices, ntris);

	// Initialize the score for all vertices
	for (auto &i : verts)
		i.lastScore = i.Score(params);

	// Initialize the score for all triangles
	temp_array<float>	score = transformc(make_range_n(indices, ntris * 3), [&](uint32 v0, uint32 v1, uint32 v2)->float {
		return verts[v0].lastScore + verts[v1].lastScore + verts[v2].lastScore;
	});

	// Initialize the cache
	temp_array<int> cache(cache_size + 3, -1);

	// Find the best triangle
	int bestTriangle	= -1;
	int bestScore		= -1;
	for (int i = 0; i < ntris; i++) {
		if (score[i] > bestScore) {
			bestScore		= score[i];
			bestTriangle	= i;
		}
	}

	// Output the currently best triangle, as long as there are triangles left to output
	while (bestTriangle >= 0) {
		// Mark the triangle as added
		score[bestTriangle] = -1;

		for (int i = 0; i < 3; i++) {
			// Update this vertex
			int v = indices[3 * bestTriangle + i];
			*out++	= v;

			// Check the current cache position, if it is in the cache
			int endpos = verts[v].cachePos;
			if (endpos < 0)
				endpos = cache_size + i;

			// Move all cache entries from the previous position in the cache to the new target position (i) one step backwards
			for (int j = endpos; j > i; j--) {
				int	x = cache[j] = cache[j - 1];
				// If this cache slot contains a real vertex, update its cache tag
				if (x >= 0)
					verts[x].cachePos = j;
			}
			// Insert the current vertex into its new target slot
			cache[i]			= v;
			verts[v].cachePos	= i;
			verts[v].remove(adjacency, bestTriangle);
		}

		// Update the scores of all verts & triangles in the cache
		for (int i = 0; i < cache_size + 3; i++) {
			int v = cache[i];
			if (v < 0)
				break;
			if (i >= cache_size) {
				// vertex has been pushed outside of the actual cache
				verts[v].cachePos	= -1;
				cache[i]			= -1;
			}
			verts[v].updateScore(params, adjacency, score);
		}

		// Find the best triangle referenced by vertices in the cache
		bestTriangle	= -1;
		bestScore		= -1;
		for (int i = 0; i < cache_size; i++) {
			int v = cache[i];
			if (v < 0)
				break;
			for (int j = 0; j < verts[v].ntris; j++) {
				int t = adjacency[verts[v].offset + j];
				if (score[t] > bestScore) {
					bestTriangle	= t;
					bestScore		= score[t];
				}
			}
		}
		// If no active triangle was found at all, scan the whole list of triangles
		if (bestTriangle < 0) {
			for (int i = 0; i < ntris; i++) {
				if (score[i] > bestScore) {
					bestScore		= score[i];
					bestTriangle	= i;
				}
			}
		}
	}
}

void VertexCacheOptimizerForsyth(const uint32* indices, uint32 ntris, uint32 nverts, uint32 *out, uint32 cache_size) {
	if (out != indices)
		return VertexCacheOptimizerForsyth(indices, ntris, nverts, out, VertexCacheOptimizerForsythParams(cache_size));

	temp_array<uint32>	temp(ntris * 3);
	VertexCacheOptimizerForsyth(indices, ntris, nverts, temp, VertexCacheOptimizerForsythParams(cache_size));
	copy(temp, out);
}

} // namespace iso

