/*************************************************************************
 * Author: Keith Schwarz (htiek@cs.stanford.edu)
 * An implementation of the fast inplace merge algorithm described by Huang and Langston in "Practical In-Place Merging," Communications of the ACM, Volume 31, Issue 3, March 1988, pages 348-352.
 *
 * The algorithm works by splitting the input into sqrt(n) blocks of size sqrt(n), choosing one block to use as a buffer, and then merging the other blocks together with the buffer as scratch space.  The main complexity
 * of the algorithm comes from the case where the input is not a perfect square in length.  When this happens, the input needs to be heavily preprocessed into a sequence of blocks that are of suitable size.
 *
 * First, we compute the size of each block to be s = ceil(sqrt(n)).  Next, we locate the largest s elements of the input, which are some combination of the last elements of the two
 * sequences to be merged.  The paper refers to the group of largest elements in the first sequence as A and the group in the second sequence as B.
 * Next, a group of elements C is chosen which directly precedes A and has size equal to B, and a group of elements D is chosen which directly precedes B and has size K - |B| mod s, where K is the length of either list.
 * This causes the second list, with B and D removed, to have a perfect multiple of s elements.  A group F is then chosen at the front of the first list with size K mod s, again to make the first list have a size multiple of s.
 * The input now looks like this:
 *
 * F s s s ... s s C A s s s ... s D B
 *
 * Here, each s refers to an arbitrary block of size s.  We ned swap regions C and B, which have the same size.  This puts B and A next to each other, and since they were chosen to be the largest elements of the entire
 * sequence, we will treat them together as a buffer block, referred to by Z.  This is seen here:
 *
 * F s s s ... s s Z s s s ... s D B
 *
 * Everything here except for F, D, and B are in their proper place.  Without any optimizations, we could now correctly sort all of these elements in-place in O(n) as follows.  First we apply the subroutine described
 * below to sort all of the s blocks together, giving us the following sequence:
 *
 * F [all s blocks in sorted order] Z D B
 *
 * Since |F| < s and |Z| = s, we could then merge F and the sorted sequence using Z as a buffer in O(n) time, then repeat this process to merge C and B into the sequence as well.  However, the paper details several
 * optimizations which yield an O(n) algorithm with much smaller runtime as follows.  Recall that the elements are ordered as follows:
 *
 * F s s s ... s s Z s s s ... s D B
 *
 * We begin by merging together the elements of D and B together using Z as a temporary buffer.  This produces a new group E, as seen here:
 *
 * F s s s ... s s Z s s s ... s E
 *
 * At this point we slightly diverge from the original text.  Let the first block of the second range be G, and then swap that block with the first actual block of the first list (H):
 *
 * F G s s ... s s Z H s ... s E
 *
 * Notice that F and G have the smallest elements from the range, so if we merge the two of them together, the first |F| elements of the result are the smallest elements of the entire range.  We merge them together
 * using the buffer as scratch space, yielding two new groups F' and G', as seen here:
 *
 * F' G' s s ... s s Z H s ... s E
 *
 * Finally, exchange G' and H to get
 *
 * F' H s s ... s s Z G' s ... s E
 *
 * Now, the only parts of this setup that aren't perfect multiples of s are F', which we don't need to touch (it's already in the right place), and E.  Once we've run the subroutine described below, we can merge E into the
 * sequence quite easily; we'll see how later.
 *
 * The core of the algorithm is a block merging step in which all of the size-s blocks are merged into sorted order, using a special buffer block as scratch space.  This buffer must consist of the largest elements of the
 * sequence for reasons that will become clearer later on.  Thus the input to this subroutine looks like this:
 *
 * s s s ... s Z s ... s s
 *
 * Where Z is the buffer block.  Note that because the input was initially two sorted sequences, each of these blocks is sorted in ascending order.
 *
 * The first step of this algorithm is to swap the buffer to the front of the sequence in O(s) time, as seen here:
 *
 * Z s s s ... s s
 *
 * Next, we sort the remaining blocks in ascending order by their last element.
 * To do so, we use naive selection sort.  Selection sorting O(s) blocks will make O(s^2) = O(n) comparisons, and will perform a total of O(s) block swaps, each of which takes O(s) time, for a total of O(s^2) = O(n) swap time.
 *
 * Finally, we begin the merge.  Starting at the leftmost block, we scan for the largest increasing sequence we can find by picking a string of blocks where each block's last element is less than the next block's first element.
 * This gives us an O(s) way of locating increasing sequences, since each block is internally sorted.  Once we have located this sequence, we merge it in with the next block, using the buffer as temporary space.  We stop
 * the merge as soon as the last element of the first sequence is moved into the correct place.  Since the blocks are stored in sorted ascending order, the merge will terminate with the buffer elements moved up past the first
 * sequence and before some remaining portion of the second sequence.  This process terminates when the buffer comes directly before a nondecreasing sequence.  When this happens, we use the rotate algorithm to push the
 * buffer past the last elements.  This gives us a sequence of sorted blocks followed by an unsorted block containing the largest elements of the sequence, which are all in the correct region of the input.  We can then
 * sort the buffer to finish the merge.
 *
 * Once we've finished this step, we have everything in place except for the elements in E.  We can fix this easily as follows.  First, swap |E| of the smallest elements from the sorted sequence and E.  We now have this setup:
 *
 * E {...sorted sequence...} {|E| of the smallest elements}
 *
 * Now, do a backwards merge of E and the sorted sequence in a fashion that moves the last |E| elements to the front of the array, then sort the first E elements in-place.  Voila!  Everything is sorted!
 */

#include "base/algorithm.h"

namespace iso {

// merges using swaps to buffer for auxiliary storage space
// [begin, mid) must fit in the buffer, though [mid, end) need not
template<typename I, typename C> void inplace_buffered_merge(I begin, I mid, I end, I buffer, C comp) {
	// Move the first elements into the buffer
	swap_ranges(begin, mid, buffer);

	const I end1 = buffer + (mid - begin);
	for (I i1 = buffer, i2 = mid, out = begin; i1 != end1; ++out) {
		if (comp(*i1, *i2)) {
			swap(*i1, *out);
			++i1;
		} else {
			swap(*i2, *out);
			if (++i2 == end) {
				swap_ranges(i1, end1, ++out);
				break;
			}
		}
	}
}

// merges such that the resulting sequence looks like [merged-elements] [new-buffer] [unconsumed second sequence]
template<typename I, typename C> I inplace_moving_merge(I begin, I mid, I end, I buffer, C comp) {
	// Keep writing until we exhaust the first sequence (we won't exhaust the second because its last element is bigger than the last element of the first sequence)
	for (I i1 = begin, i2 = mid; i1 != mid; ++buffer) {
		// Move the smaller element forward
		if (!comp(*i2, *i1)) {
			swap(*i1, *buffer);
			++i1;
		} else {
			swap(*i2, *buffer);
			++i2;
		}
	}
	return buffer;
}


template<typename I, typename C = less> void inplace_merge(I begin, I mid, I end, C comp = C()) {
	const size_t	size1 		= distance(begin, mid);
	const size_t	size2 		= distance(mid, end);
	const size_t 	blockSize	= isqrt(size1 + size2);
	
	if (size1 < blockSize) {
		block_merge(begin, mid, end, comp);

	} else if (size2 < blockSize) {
		block_merge(make_reverse_iterator(end - 1),make_reverse_iterator(mid - 1),make_reverse_iterator(begin - 1), flip(comp));

	} else {
		// get groups A and B
		// A is the group of largest elements from the first sequence
		// B is the group of largest elements from the second sequence
		I	a(mid), b(end);
		for (size_t i = 0; i < blockSize; ++i) {
			if (comp(*(a - 1), *(b - 1)))
				--b;
			else
				--a;
		}

		// get groups C and D
		// C directly precedes A and has size equal to B;
		// D directly precedes B and has size (listSize - |B| mod blockSize)
		// F s s s ... s s C A s s s ... s D B
		size_t	s2 	= distance(b, end);
		I 		c 	= a - s2;
		I 		d 	= b - ((size2 - s2) % blockSize);
		
		// swap regions C and B, which have the same size, putting B and A next to each other; BA becomes our buffer to get:
		// F s s s ... s s BA s s s ... s D C
		swap_ranges(c, a, b);

		// merge D and C together using the buffer as scratch
		inplace_buffered_merge(d, b, end, c, comp);

		// this will be where our buffer starts eventually
		I 		buffer 	= begin;

		// if first list is not a multiple of blockSize, we merge the extra elements with the smallest elements from the second list
		if (const size_t f = size1 % blockSize) {
			buffer += f;
			// swap the first block from the second list and the second block from the first list, so the smallest elements from both lists are adjacent
			swap_ranges(buffer, buffer + blockSize, mid);
			// merge F and this block using the buffer as scratch
			inplace_buffered_merge(begin, buffer, buffer + blockSize, c, comp);
			// move the blocks back
			swap_ranges(buffer, buffer + blockSize, mid);
		}

		// MAIN ALGORITHM
		
		// swap the old buffer at c with the new buffer to get:
		// F BA s s s ... s DC
		swap_ranges(buffer, buffer + blockSize, c);

		// sort all of the blocks in ascending order by their last elements to get:
		// F[all s blocks in sorted order]BA DC
		for (I i = buffer + blockSize; i != d; i += blockSize) {
			I min = i;
			for (I curr = i + blockSize; curr != d; curr += blockSize) {
				if (comp(*(curr + blockSize - 1), *(min + blockSize - 1))							// if this is outright smaller...
				|| (!comp(*(min + blockSize - 1), *(curr + blockSize - 1)) && comp(*curr, *min))	// or it's equal but has a smaller first element...
				)
					min = curr;	// pick it
			}
			// swap into place
			if (min != i)
				swap_ranges(i, i + blockSize, min);
		}
		
		// scan over the blocks, looking for increasing sequences that can be merged
		bool	merged	= true;
		for (I block = buffer + blockSize; merged; ) {
			merged = false;
			for (I i = block + blockSize; i != d; i += blockSize) {
				if (comp(*i, *(i - 1))) {
					// do a buffer-moving merge of the range up through the start of the last block with the last block and remember where the buffer ended up getting repositioned
					buffer 	= inplace_moving_merge(buffer + blockSize, i, i + blockSize, buffer, comp);
					block 	= i;
					merged	= true;
					break;
				}
			}
		}

		// we're now sorted up to D
		
		// sort using an in-place sort, then rotate it through the rest of the range to finish up the sequence
		heap_make(buffer, buffer + blockSize, comp);
		heap_sort(buffer, buffer + blockSize, comp);
		rotate(buffer, buffer + blockSize, d);

		// merge D into this resulting sequence:
		
		// swap D and the smallest elements of the range, doing an in-place merge
		swap_ranges(d, end, begin);
		inplace_buffered_merge(begin, begin + distance(d, end), d, d, comp);

		// sort the buffer using heapsort
		heap_make(d, end, comp);
		heap_sort(d, end, comp);

		// rotate the buffer to the front
		rotate(begin, d, end);
	}
}

#ifdef TEST
#include "base/array.h"
#include "utilities.h"

struct test_inplace_merge {
	dynamic_array<float>	a;
	rng<simple_random> random;
	
	static bool int_sort(float a, float b) { return int(a) > int(b); }

	test_inplace_merge() : random(42) {
//		a = transformc(int_range(100), [this](int) { return random.to(1000); });
//		a.append(transformc(int_range(100), [this](int) { return random.to(1000); }));
		a = transformc(int_range(100), [this](int i) { return random.to(10); });
		a.append(transformc(int_range(100), [this](int i) { return random.to(10); }));
		sort(sub_range(a, 0, 100), int_sort);
		sort(sub_range(a, 100), int_sort);
		for (auto &i : a) i += a.index_of(i) / 200.f;

//		heap_make(a, int_sort);
//		heap_sort(a, int_sort);

		inplace_merge(a.begin(), a.begin() + 100, a.end(), int_sort);
		ISO_ASSERT(check_sorted(a, int_sort));
	}
	
} _test_timsort;
#endif


} // namespace iso
