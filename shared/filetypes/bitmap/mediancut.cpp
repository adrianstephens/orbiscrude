/* pngquant.c - quantize the colors in an alphamap down to a specified number
**
** Copyright (C) 1989, 1991 by Jef Poskanzer.
** Copyright (C) 1997, 2000 by Greg Roelofs; based on an idea by
**							Stefan Schneider.
**
** Permission to use, copy, modify, and distribute this software and its
** documentation for any purpose and without fee is hereby granted, provided
** that the above copyright notice appear in all copies and that both that
** copyright notice and this permission notice appear in supporting
** documentation.  This software is provided "as is" without express or
** implied warranty.
*/

/* GRR TO DO:  if all samples are gray and image is opaque and sample depth
would be no bigger than palette and user didn't explicitly
specify a mapfile, switch to grayscale */

#include "bitmap.h"
#include "base/algorithm.h"

using namespace iso;

#define MAXCOLORS  32767	//1048575
#define FS_SCALE   1024 	// Floyd-Steinberg scaling factor

//#define REP_CENTER_BOX
//#define REP_AVERAGE_COLORS
#define REP_AVERAGE_PIXELS

#define WEIGHTR	1.0f
#define WEIGHTG	1.0f
#define WEIGHTB	1.0f
#define WEIGHTA	1.0f

struct HistItem {
	ISO_rgba	col;
	int			value;
};

struct HistLink {
	HistItem	ch;
	HistLink	*next;
};

class HashTable {
	enum {HASH_SIZE = 20023};
	HistLink	*entries[HASH_SIZE];
public:
	HashTable();
	~HashTable();
	HistLink*&	operator[](int i) { return entries[i];}

	static int	HashValue(const ISO_rgba &p)	{ return ((p.r * 33023 + p.g * 30013 + p.b * 27011 + p.a * 24007) & 0x7fffffff) % HASH_SIZE;}

	int			LookUp(const ISO_rgba &col) const;
	void		Add(const ISO_rgba &col, int value);
	HistItem*	Flatten(int maxcols) const;

};

struct box {
	int		ind;
	int		colors;
	int		sum;
};


static HistItem*	MedianCut(HistItem* achv, int colors, int sum, int maxval, int newcolors);
static HistItem*	CreateHistogram(bitmap &bm, int maxcols, int* colsP);
static HashTable*	CreateHashTable(bitmap &bm, int maxcols, int* colsP);

int Quantise(bitmap &bm, int reqcolors, bool floyd) {
	ISO_rgba	*pP;
	int			row, col, limitcol;
	int			ind;
	int			maxval;
	HistItem	*achv, *colmap;
	long		*thisrerr, *nextrerr, *thisgerr, *nextgerr, *thisberr, *nextberr, *thisaerr, *nextaerr, *temperr;
	long		sr, sg, sb, sa, err;
	int			colors, newcolors;
	bool		fs_direction;
	int			cols = bm.Width(), rows = bm.Height();

	bm.Unpalette();
	/*
	** Step 2: attempt to make a histogram of the colors, unclustered.
	** If at first we don't succeed, lower maxval to increase color
	** coherence and try again.  This will eventually terminate, with
	** maxval at worst 15, since 32^3 is approximately MAXCOLORS.
	[GRR POSSIBLE BUG:	what about 32^4 ?]
	 */

	for (maxval = 255; !(achv = CreateHistogram(bm, MAXCOLORS, &colors)); ) {
		int newmaxval = maxval / 2;
		for (row = 0; row < rows; ++row)
			for (col = 0, pP = bm.ScanLine(row); col < cols; ++col, ++pP) {
				pP->r = (pP->r * newmaxval + maxval / 2) / maxval;
				pP->g = (pP->g * newmaxval + maxval / 2) / maxval;
				pP->b = (pP->b * newmaxval + maxval / 2) / maxval;
				pP->a = (pP->a * newmaxval + maxval / 2) / maxval;
			}
		maxval = newmaxval;
	}
	newcolors = min(colors, reqcolors);

	/*
	** Step 3: apply median-cut to histogram, making the new colmap.
	*/
	colmap = MedianCut(achv, colors, rows * cols, maxval, newcolors);
	delete[] achv;


	bm.CreateClut(newcolors);
//	if (bm.HasAlpha())
//		bm.SetFlag(BMF_CLUTALPHA);
	for (int x = 0; x < newcolors; ++x) {
		bm.Clut(x) = ISO_rgba(
			(colmap[x].col.r * 255 + (maxval >> 1)) / maxval,
			(colmap[x].col.g * 255 + (maxval >> 1)) / maxval,
			(colmap[x].col.b * 255 + (maxval >> 1)) / maxval,
			(colmap[x].col.a * 255 + (maxval >> 1)) / maxval
		);
	}

	/*
	** Step 4: map the colors in the image to their closest match in the
	** new colormap, and write 'em out.
	*/
	HashTable	acht;

	if ( floyd ) {
		/* Initialize Floyd-Steinberg error vectors. */
		thisrerr = new long[cols + 2];
		nextrerr = new long[cols + 2];
		thisgerr = new long[cols + 2];
		nextgerr = new long[cols + 2];
		thisberr = new long[cols + 2];
		nextberr = new long[cols + 2];
		thisaerr = new long[cols + 2];
		nextaerr = new long[cols + 2];
		//		  srand( (int) ( time( 0 ) ^ getpid( ) ) );
		for (col = 0; col < cols + 2; ++col) {
			thisrerr[col] = rand() % (FS_SCALE * 2) - FS_SCALE;
			thisgerr[col] = rand() % (FS_SCALE * 2) - FS_SCALE;
			thisberr[col] = rand() % (FS_SCALE * 2) - FS_SCALE;
			thisaerr[col] = rand() % (FS_SCALE * 2) - FS_SCALE;
			/* (random errors in [-1 .. 1]) */
		}
		fs_direction = true;
	}
	for (row = 0; row < rows; ++row) {
		if (floyd)
			for (col = 0; col < cols + 2; ++col)
				nextrerr[col] = nextgerr[col] =	nextberr[col] = nextaerr[col] = 0;

		if (!floyd || fs_direction) {
			col = 0;
			limitcol = cols;
			pP = bm.ScanLine(row);
		} else {
			col = cols - 1;
			limitcol = -1;
			pP = &(bm.ScanLine(row)[col]);
		}
		do {
			if (floyd) {
				// Use Floyd-Steinberg errors to adjust actual color.
				sr = pP->r + thisrerr[col + 1] / FS_SCALE;
				sg = pP->g + thisgerr[col + 1] / FS_SCALE;
				sb = pP->b + thisberr[col + 1] / FS_SCALE;
				sa = pP->a + thisaerr[col + 1] / FS_SCALE;
				if (sr < 0) sr = 0; else if (sr > maxval) sr = maxval;
				if (sg < 0) sg = 0; else if (sg > maxval) sg = maxval;
				if (sb < 0) sb = 0; else if (sb > maxval) sb = maxval;
				if (sa < 0) sa = 0; else if (sa > maxval) sa = maxval;
				*pP = ISO_rgba((uint8)sr, (uint8)sg, (uint8)sb, (uint8)sa);
			}

			/* Check hash table to see if we have already matched this color. */
			ind = acht.LookUp(*pP);
			if (ind == -1) {
				/* No; search colmap for closest match. */
				int	r1, g1, b1, a1, r2, g2, b2, a2;
				int	dist, newdist;

				r1 = pP->r;
				g1 = pP->g;
				b1 = pP->b;
				a1 = pP->a;
				dist = 2000000000;
				for (int i = 0; i < newcolors; ++i ) {
					r2 = colmap[i].col.r;
					g2 = colmap[i].col.g;
					b2 = colmap[i].col.b;
					a2 = colmap[i].col.a;
					newdist = (r1 - r2) * (r1 - r2) + (g1 - g2) * (g1 - g2) + (b1 - b2) * (b1 - b2) + (a1 - a2) * (a1 - a2);
					if (newdist < dist) {
						ind		= i;
						dist	= newdist;
					}
				}
				acht.Add(*pP, ind);
			}

			if (floyd) {
				/* Propagate Floyd-Steinberg error terms. */
				if (fs_direction) {
					err = (sr - (long)colmap[ind].col.r)*FS_SCALE;
					thisrerr[col + 2] += (err * 7) / 16;
					nextrerr[col	] += (err * 3) / 16;
					nextrerr[col + 1] += (err * 5) / 16;
					nextrerr[col + 2] += (err	 ) / 16;
					err = (sg - (long)colmap[ind].col.g)*FS_SCALE;
					thisgerr[col + 2] += (err * 7) / 16;
					nextgerr[col	] += (err * 3) / 16;
					nextgerr[col + 1] += (err * 5) / 16;
					nextgerr[col + 2] += (err	 ) / 16;
					err = (sb - (long)colmap[ind].col.b)*FS_SCALE;
					thisberr[col + 2] += (err * 7) / 16;
					nextberr[col	] += (err * 3) / 16;
					nextberr[col + 1] += (err * 5) / 16;
					nextberr[col + 2] += (err	 ) / 16;
					err = (sa - (long)colmap[ind].col.a)*FS_SCALE;
					thisaerr[col + 2] += (err * 7) / 16;
					nextaerr[col	] += (err * 3) / 16;
					nextaerr[col + 1] += (err * 5) / 16;
					nextaerr[col + 2] += (err	 ) / 16;
				} else {
					err = (sr - (long)colmap[ind].col.r)*FS_SCALE;
					thisrerr[col	] += (err * 7) / 16;
					nextrerr[col + 2] += (err * 3) / 16;
					nextrerr[col + 1] += (err * 5) / 16;
					nextrerr[col	] += (err	 ) / 16;
					err = (sg - (long)colmap[ind].col.g)*FS_SCALE;
					thisgerr[col	] += (err * 7) / 16;
					nextgerr[col + 2] += (err * 3) / 16;
					nextgerr[col + 1] += (err * 5) / 16;
					nextgerr[col	] += (err	 ) / 16;
					err = (sb - (long)colmap[ind].col.b)*FS_SCALE;
					thisberr[col	] += (err * 7) / 16;
					nextberr[col + 2] += (err * 3) / 16;
					nextberr[col + 1] += (err * 5) / 16;
					nextberr[col	] += (err	  ) / 16;
					err = (sa - (long)colmap[ind].col.a)*FS_SCALE;
					thisaerr[col	] += (err * 7) / 16;
					nextaerr[col + 2] += (err * 3) / 16;
					nextaerr[col + 1] += (err * 5) / 16;
					nextaerr[col	] += (err	 ) / 16;
				}
			}

			*pP = ind;

			if (!floyd || fs_direction) {
				++col;
				++pP;
			} else {
				--col;
				--pP;
			}
		} while (col != limitcol);

		if (floyd) {
			temperr		= thisrerr;
			thisrerr	= nextrerr;
			nextrerr	= temperr;
			temperr		= thisgerr;
			thisgerr	= nextgerr;
			nextgerr	= temperr;
			temperr		= thisberr;
			thisberr	= nextberr;
			nextberr	= temperr;
			temperr		= thisaerr;
			thisaerr	= nextaerr;
			nextaerr	= temperr;
			fs_direction = !fs_direction;
		}

	}

	if (floyd) {
		delete[] thisrerr;
		delete[] nextrerr;
		delete[] thisgerr;
		delete[] nextgerr;
		delete[] thisberr;
		delete[] nextberr;
		delete[] thisaerr;
		delete[] nextaerr;
	}
	delete[] colmap;
	return 0;
}


// Based on Paul Heckbert's paper, "Color Image Quantization for Frame Buffer Display," SIGGRAPH 1982 Proceedings, page 297.
static HistItem* MedianCut(HistItem* achv, int colors, int sum, int maxval, int newcolors) {
	box			*bv		= new box[newcolors];
	HistItem	*colmap	= new HistItem[newcolors];
	int			boxes	= 1;

	for (int i = 0; i < newcolors; ++i)
		colmap[i].col = ISO_rgba(0, 0, 0, 0);

	// Set up the initial box.
	bv[0].ind		= 0;
	bv[0].colors	= colors;
	bv[0].sum		= sum;

	// Main loop: split boxes until we have enough.
	while (boxes < newcolors) {
		// Find the first splittable box.
		int		bi;
		for (bi = 0; bi < boxes && bv[bi].colors < 2; ++bi);

		if (bi == boxes)
			break;		  // ran out of colors!

		int	indx	= bv[bi].ind;
		int	clrs	= bv[bi].colors;
		int	sm		= bv[bi].sum;

		// Go through the box finding the minimum and maximum of each component - the boundaries of the box.
		int	minr	= achv[indx].col.r, maxr = minr;
		int	ming	= achv[indx].col.g, maxg = ming;
		int	minb	= achv[indx].col.b, maxb = minb;
		int	mina	= achv[indx].col.a, maxa = mina;

		for (int i = 1; i < clrs; ++i) {
			int	v = achv[indx + i].col.r;
			if (v < minr) minr = v;
			if (v > maxr) maxr = v;
			v = achv[indx + i].col.g;
			if (v < ming) ming = v;
			if (v > maxg) maxg = v;
			v = achv[indx + i].col.b;
			if (v < minb) minb = v;
			if (v > maxb) maxb = v;
			v = achv[indx + i].col.a;
			if (v < mina) mina = v;
			if (v > maxa) maxa = v;
		}

		/*
		** Find the largest dimension, and sort by that component.	I have
		** included two methods for determining the "largest" dimension;
		** first by simply comparing the range in RGB space, and second
		** by transforming into luminosities before the comparison.  You
		** can switch which method is used by switching the commenting on
		** the LARGE_ defines at the beginning of this source file.
		*/

		struct component_compare {
			int	c;
			component_compare(int _c) : c(_c)	{}
			bool operator()(const HistItem &a, const HistItem &b)	{ return ((uint8*)&a.col)[c] < ((uint8*)&b.col)[c]; }
		};

		float	rr = WEIGHTR * (maxr - minr),
				rg = WEIGHTG * (maxg - ming),
				rb = WEIGHTB * (maxb - minb),
				ra = WEIGHTA * (maxa - mina);
		int		c	= ra >= rr && ra >= rg && ra >= rb ? 3
					: ra >= rg && ra >= rb ? 0
					: rg >= rb ? 1
					: 2;
		sort(achv + indx, achv + indx + clrs, component_compare(c));

		// Find the median based on the counts, so that about half the pixels (not colors, pixels) are in each subdivision.
		int halfsum = sm / 2, lowersum = achv[indx].value, med;
		for (med = 1; med < clrs - 1; ++med) {
			if (lowersum >= halfsum)
				break;
			lowersum += achv[indx + med].value;
		}

		// Split the box, and sort to bring the biggest boxes to the top.
		bv[bi].colors		= med;
		bv[bi].sum			= lowersum;

		bv[boxes].ind		= indx + med;
		bv[boxes].colors	= clrs - med;
		bv[boxes].sum		= sm - lowersum;
		++boxes;

		struct sum_compare {
			bool operator()(const box &a, const box &b)	{ return a.sum > b.sum; }
		};

		sort(bv, bv + boxes, sum_compare());
	}

	/*
	** Ok, we've got enough boxes.	Now choose a representative color for
	** each box.  There are a number of possible ways to make this choice.
	** One would be to choose the center of the box; this ignores any structure
	** within the boxes.  Another method would be to average all the colors in
	** the box - this is the method specified in Heckbert's paper.	A third
	** method is to average all the pixels in the box.	You can switch which
	** method is used by switching the commenting on the REP_ defines at
	** the beginning of this source file.
	*/
	for (int bi = 0; bi < boxes; ++bi) {
#ifdef REP_CENTER_BOX
		int indx	= bv[bi].ind;
		int clrs	= bv[bi].colors;
		int	minr	= achv[indx].col.r, maxr = minr;
		int	ming	= achv[indx].col.g, maxg = ming;
		int	minb	= achv[indx].col.b, maxb = minb;
		int	mina	= achv[indx].col.a, maxa = mina;
		for (int i = 1; i < clrs; ++i) {
			int	v = achv[indx + i].col.r;
			minr = min( minr, v );
			maxr = max( maxr, v );
			v = achv[indx + i].col.g;
			ming = min( ming, v );
			maxg = max( maxg, v );
			v = achv[indx + i].col.b;
			minb = min( minb, v );
			maxb = max( maxb, v );
			v = achv[indx + i].col.a;
			mina = min( mina, v );
			maxa = max( maxa, v );
		}
		colmap[bi].col = ISO_rgba((minr + maxr) / 2, (ming + maxg) / 2, (minb + maxb) / 2, (mina + maxa) / 2 );
#endif /*REP_CENTER_BOX*/
#ifdef REP_AVERAGE_COLORS
		int indx = bv[bi].ind;
		int clrs = bv[bi].colors;
		int r = 0, g = 0, b = 0, a = 0;

		for (int i = 0; i < clrs; ++i) {
			r += achv[indx + i].col.r;
			g += achv[indx + i].col.g;
			b += achv[indx + i].col.b;
			a += achv[indx + i].col.a;
		}
		colmap[bi].col = ISO_rgba(r / clrs, g / clrs, b / clrs, a / clrs);
#endif /*REP_AVERAGE_COLORS*/
#ifdef REP_AVERAGE_PIXELS
		int indx = bv[bi].ind;
		int clrs = bv[bi].colors;
		int r = 0, g = 0, b = 0, a = 0, sum = 0;

		for (int i = 0; i < clrs; ++i) {
			r += achv[indx + i].col.r * achv[indx + i].value;
			g += achv[indx + i].col.g * achv[indx + i].value;
			b += achv[indx + i].col.b * achv[indx + i].value;
			a += achv[indx + i].col.a * achv[indx + i].value;
			sum += achv[indx + i].value;
		}
		r = min(r / sum, maxval);
		g = min(g / sum, maxval);
		b = min(b / sum, maxval);
		a = min(a / sum, maxval);
		colmap[bi].col = ISO_rgba((uint8)r, (uint8)g, (uint8)b, (uint8)a);
#endif /*REP_AVERAGE_PIXELS*/
	}
	delete[] bv;

	return colmap;
}

static HashTable* CreateHashTable(bitmap &bm, int maxcols, int *colsP) {
	HashTable	*acht = new HashTable;
	*colsP = 0;

	// Go through the entire image, building a hash table of colors.
	for (int row = 0; row < bm.Height(); ++row) {
		ISO_rgba	*pP = bm.ScanLine(row);
		for (int col = 0; col < bm.Width(); ++col, ++pP) {
			HistLink *achl;
			int	hash = HashTable::HashValue(*pP);
			for (achl = (*acht)[hash]; achl; achl = achl->next)
				if (achl->ch.col == *pP)
					break;
			if (achl)
				achl->ch.value++;
			else {
				if (++(*colsP) > maxcols) {
					delete acht;
					return NULL;
				}
				achl			= new HistLink;
				achl->ch.col	= *pP;
				achl->ch.value	= 1;
				achl->next		= (*acht)[hash];
				(*acht)[hash]	= achl;
			}
		}
	}

	return acht;
}

static HistItem* CreateHistogram(bitmap &bm, int maxcols, int *colsP) {
	HashTable *acht = CreateHashTable(bm, maxcols, colsP);
	if (!acht)
		return NULL;
	HistItem* achv = acht->Flatten(maxcols);
	delete acht;
	return achv;
}

HashTable::HashTable() {
	for (int i = 0; i < HASH_SIZE; ++i )
		entries[i] = NULL;
}

HashTable::~HashTable() {
	HistLink *achl, *achlnext;
	for (int i = 0; i < HASH_SIZE; ++i ) {
		for (achl = entries[i]; achl; achl = achlnext ) {
			achlnext = achl->next;
			delete achl;
		}
	}
}

void HashTable::Add(const ISO_rgba& col, int value) {
	int		hash	= HashTable::HashValue(col);
	HistLink *achl	= new HistLink;

	achl->ch.col	= col;
	achl->ch.value	= value;
	achl->next		= entries[hash];
	entries[hash]	= achl;
}

int HashTable::LookUp(const ISO_rgba& col) const {
	for (HistLink *achl = entries[HashValue(col)]; achl; achl = achl->next)
		if (achl->ch.col == col)
			return achl->ch.value;

	return -1;
}

HistItem* HashTable::Flatten(int maxcols) const {
	HistItem	*achv = new HistItem[maxcols];
	for (int i = 0, j = 0; i < HASH_SIZE; ++i) {
		for (HistLink *achl = entries[i]; achl; achl = achl->next)
			achv[j++] = achl->ch;		// Add the new entry.
	}
	return achv;
}
