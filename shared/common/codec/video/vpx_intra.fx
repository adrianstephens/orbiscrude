#include "common.fxh"
#include "vpx.fxh"

//-----------------------------------------------------------------------------
//	INTRA-PREDICTION
//-----------------------------------------------------------------------------

#define TexType	RWTexture2D<uint>

#ifdef PLAT_PS4
uint left(TexType tex, int2 uv, int y) {
	return tex[uv + int2(-1, y)];
}
uint above(TexType tex, int2 uv, int x) {
	return tex[uv + int2(x, -1)];
}
#else
uint left(TexType tex, int2 uv, int y) {
	return tex[int2(uv.x / 4 - 1, uv.y + y)] >> 24;
}
uint above(TexType tex, int2 uv, int x) {
	return tex[int2((uv.x + x) / 4, uv.y - 1)] >> ((x & 3) * 8);
}
#endif

int avg2(int a, int b)			{ return (a + b + 1) >> 1; }
int avg3(int a, int b, int c)	{ return (a + 2 * b + c + 2) >> 2; }

void add_clamp_pixel(TexType dest, int2 uv, uint x) {
	dest[uv] = clamp(int(dest[uv] + x) - 128, 0, 255);
}

def_enum(PREDICTION_MODE)
	PRED_DC			= 0,	// Average of above and left pixels
	PRED_V			= 1,	// Vertical
	PRED_H			= 2,	// Horizontal
	PRED_D45		= 3,	// Directional 45  deg = round(arctan(1/1) * 180/pi)
	PRED_D135		= 4,	// Directional 135 deg = 180 - 45
	PRED_D117		= 5,	// Directional 117 deg = 180 - 63
	PRED_D153		= 6,	// Directional 153 deg = 180 - 27
	PRED_D207		= 7,	// Directional 207 deg = 180 + 27
	PRED_D63		= 8,	// Directional 63  deg = round(arctan(2/1) * 180/pi)
	PRED_TM			= 9,	// True-motion

	DC_NO_L			= 10,
	DC_NO_A			= 11,
	DC_127			= 12,
	DC_129			= 13,
	D45_NO_R		= 14,
	D135_NO_L		= 15,
	D135_NO_A		= 16,
	D117_NO_L		= 17,
	D117_NO_A		= 18,
	D153_NO_L		= 19,
	D153_NO_A		= 20,
	D63_NO_R		= 21
end_enum

int intraDC(int size, int2 uv, int2 duv, TexType dest) {
#ifdef PLAT_PS4
	int	a	= above(dest, uv, duv.x);
	int	b	= left(dest, uv, duv.x);
	a += QuadSwizzle(a, 1u, 0u, 3u, 2u);
	b += QuadSwizzle(b, 1u, 0u, 3u, 2u);

	a += QuadSwizzle(a, 2u, 3u, 0u, 1u);
	b += QuadSwizzle(b, 2u, 3u, 0u, 1u);

	if (size > 4) {
		a += LaneSwizzle(a, 31u, 0u, 4u);
		b += LaneSwizzle(b, 31u, 0u, 4u);
		if (size > 8) {
			a += LaneSwizzle(a, 31u, 0u, 8u);
			b += LaneSwizzle(b, 31u, 0u, 8u);
			if (size > 16) {
				a += LaneSwizzle(a, 31u, 0u, 16u);
				b += LaneSwizzle(b, 31u, 0u, 16u);
			}
		}
	}
	return (a + b + size) / (size * 2);
#else
	int	t = 0;
	for (int i = 0; i < size; i++)
		t += above(dest, uv, i) + left(dest, uv, i);
	return (t + size) / (size * 2);
#endif
}

int intraDC_NO_L(int size, int2 uv, int2 duv, TexType dest) {
#ifdef PLAT_PS4
	int	a	= above(dest, uv, duv.x);
	a += QuadSwizzle(a, 1u, 0u, 3u, 2u);
	a += QuadSwizzle(a, 2u, 3u, 0u, 1u);

	if (size > 4) {
		a += LaneSwizzle(a, 31u, 0u, 4u);
		if (size > 8) {
			a += LaneSwizzle(a, 31u, 0u, 8u);
			if (size > 16)
				a += LaneSwizzle(a, 31u, 0u, 16u);
		}
	}
	return (a + size / 2) / size;
#else
	int	t = 0;
	for (int i = 0; i < size; i++)
		t += above(dest, uv, i);
	return (t + size / 2) / size;
#endif
}

int intraDC_NO_A(int size, int2 uv, int2 duv, TexType dest) {
#ifdef PLAT_PS4
	int	a	= left(dest, uv, duv.x);
	a += QuadSwizzle(a, 1u, 0u, 3u, 2u);
	a += QuadSwizzle(a, 2u, 3u, 0u, 1u);

	if (size > 4) {
		a += LaneSwizzle(a, 31u, 0u, 4u);
		if (size > 8) {
			a += LaneSwizzle(a, 31u, 0u, 8u);
			if (size > 16)
				a += LaneSwizzle(a, 31u, 0u, 16u);
		}
	}
	return (a + size / 2) / size;
#else
	int	t = 0;
	for (int i = 0; i < size; i++)
		t += left(dest, uv, i);
	return (t + size / 2) / size;
#endif
}

int intraDC_127(int size, int2 uv, int2 duv, TexType dest) {
	return 127;
}

int intraDC_129(int size, int2 uv, int2 duv, TexType dest) {
	return 129;
}

int intraV(int size, int2 uv, int2 duv, TexType dest) {
	return above(dest, uv, duv.x);
}

int intraH(int size, int2 uv, int2 duv, TexType dest) {
	return left(dest, uv, duv.y);
}

int intraD45(int size, int2 uv, int2 duv, TexType dest) {
	int	u	= duv.x + duv.y;
	return u == (size - 1) + (size - 1)
		? above(dest, uv, u + 1)
		: avg3(above(dest, uv, u), above(dest, uv, u + 1), above(dest, uv, u + 2));
}
int intraD45_NO_R(int size, int2 uv, int2 duv, TexType dest) {
	int	u	= duv.x + duv.y;
	int	m	= size - 1;
	return avg3(above(dest, uv, min(u, m)), above(dest, uv, min(u + 1, m)), above(dest, uv, min(u + 2, m)));
}

int intraD135(int size, int2 uv, int2 duv, TexType dest) {
	if (duv.y <= duv.x) {
		int	u = duv.x - duv.y;
		return avg3(dest[uv + (u == 0 ? int2(-1, 0) : int2(u - 2, -1))], above(dest, uv, u - 1), above(dest, uv, u));
	} else {
		int	v = duv.y - duv.x;
		return avg3(left(dest, uv, v - 2), left(dest, uv, v - 1), left(dest, uv, v));
	}
}
int intraD135_NO_L(int size, int2 uv, int2 duv, TexType dest) {
	if (duv.y <= duv.x) {
		int	u = duv.x - duv.y;
		return avg3(u < 2 ? 129 : above(dest, uv, u - 2), u < 1 ? 129 : above(dest, uv, u - 1), above(dest, uv, u));
	} else {
		return 129;
	}
}
int intraD135_NO_A(int size, int2 uv, int2 duv, TexType dest) {
	if (duv.y <= duv.x) {
		int	u = duv.x - duv.y;
		return avg3(u == 0 ? left(dest, uv, 0) : 127, 127, 127);
	} else {
		int	v = duv.y - duv.x;
		return avg3(v < 2 ? 127 : left(dest, uv, v - 2), v < 1 ? 127 : left(dest, uv, v - 1), left(dest, uv, v));
	}
}
int intraD117(int size, int2 uv, int2 duv, TexType dest) {
	if (duv.y <= duv.x * 2) {
		int	u = duv.x - duv.y / 2;
		int	a = above(dest, uv, u - 1), b = above(dest, uv, u);
		return (duv.y & 1) == 0
			? avg2(a, b)
			: avg3(above(dest, uv, u - 2), a, b);
	} else {
		int	v = duv.y - duv.x * 2;
		return avg3(dest[uv + (v == 1 ? int2(0, -1) : int2(-1, v - 3))], left(dest, uv, v - 2), left(dest, uv, v - 1));
	}
}
int intraD117_NO_L(int size, int2 uv, int2 duv, TexType dest) {
	if (duv.y <= duv.x * 2) {
		int	u = duv.x - duv.y / 2;
		int	a = u < 1 ? 129 : above(dest, uv, u - 1), b = above(dest, uv, u);
		return (duv.y & 1) == 0
			? avg2(a, b)
			: avg3(u < 2 ? 129 : above(dest, uv, u - 2), a, b);
	} else {
		int	v = duv.y - duv.x * 2;
		return avg3(v == 1 ? above(dest, uv, 0) : 129, 129, 129);
	}
}
int intraD117_NO_A(int size, int2 uv, int2 duv, TexType dest) {
	if (duv.y <= duv.x * 2) {
		return 127;
	} else {
		int	v = duv.y - duv.x * 2;
		return avg3(v < 3 ? 127 : left(dest, uv, v - 3), v < 2 ? 127 : left(dest, uv, v - 2), v < 1 ? 127 : left(dest, uv, v - 1));
	}
}
int intraD153(int size, int2 uv, int2 duv, TexType dest) {
	if (duv.y < duv.x / 2) {
		int	u = duv.x - duv.y * 2;
		return avg3(above(dest, uv, u - 3), above(dest, uv, u - 2), above(dest, uv, u - 1));
	} else {
		int	v = duv.y - duv.x / 2;
		int	a = left(dest, uv, v - 1), b = left(dest, uv, v);
		return (duv.x & 1) == 0
			? avg2(a, b)
			: avg3(dest[uv + (v == 0 ? int2(0, -1) : int2(-1, v - 2))], a, b);
	}
}
int intraD153_NO_L(int size, int2 uv, int2 duv, TexType dest) {
	if (duv.y <= duv.x / 2) {
		int	u = duv.x - duv.y * 2;
		return avg3(u < 3 ? 129 : above(dest, uv, u - 3), u < 2 ? 129 : above(dest, uv, u - 2), u < 1 ? 129 : above(dest, uv, u - 1));
	} else {
		return 129;
	}
}
int intraD153_NO_A(int size, int2 uv, int2 duv, TexType dest) {
	if (duv.y < duv.x / 2) {
		return 127;
	} else {
		int	v = duv.y - duv.x / 2;
		int	a = v < 1 ? 127 : left(dest, uv, v - 1), b = left(dest, uv, v);
		return (duv.x & 1) == 0
			? avg2(a, b)
			: avg3(v < 2 ? 127 : left(dest, uv, v - 2), a, b);
	}
}
int intraD207(int size, int2 uv, int2 duv, TexType dest) {
	int	v	= duv.y + duv.x / 2;
	int	m	= size - 1;
	int	a	= left(dest, uv, min(v, m)), b = left(dest, uv, min(v + 1, m));
	return (duv.x & 1) == 0
		? avg2(a, b)
		: avg3(a, b, left(dest, uv, min(v + 2, m)));
}

int intraD63(int size, int2 uv, int2 duv, TexType dest) {
	int	u	= duv.x + duv.y / 2;
	int	m	= size + 3;
	int	a	= above(dest, uv, min(u, m)), b = above(dest, uv, min(u + 1, m));
	return (duv.y & 1) == 0
		? avg2(a, b)
		: avg3(a, b, above(dest, uv, min(u + 2, m)));
}
int intraD63_NO_R(int size, int2 uv, int2 duv, TexType dest) {
	int	u	= duv.x + duv.y / 2;
	int	m	= size - 1;
	int	a	= above(dest, uv, min(u, m)), b = above(dest, uv, min(u + 1, m));
	return (duv.y & 1) == 0
		? avg2(a, b)
		: avg3(a, b, above(dest, uv, min(u + 2, m)));
}

int intraTM(int size, int2 uv, int2 duv, TexType dest) {
	return clamp(left(dest, uv, duv.y) + above(dest, uv, duv.x) - above(dest, uv, -1), 0, 0xff);
}

int intra(int size, int mode, int2 uv, int2 duv, TexType dest) {
#ifdef PLAT_PS4
	switch (mode) {
		case PRED_DC:		return intraDC		(size, uv, duv, dest);
		case PRED_V:		return intraV		(size, uv, duv, dest);
		case PRED_H:		return intraH		(size, uv, duv, dest);
		case PRED_D45:		return intraD45		(size, uv, duv, dest);
		case PRED_D135:		return intraD135	(size, uv, duv, dest);
		case PRED_D117:		return intraD117	(size, uv, duv, dest);
		case PRED_D153:		return intraD153	(size, uv, duv, dest);
		case PRED_D207:		return intraD207	(size, uv, duv, dest);
		case PRED_D63:		return intraD63		(size, uv, duv, dest);
		case PRED_TM:		return intraTM		(size, uv, duv, dest);
		case DC_NO_L:		return intraDC_NO_L	(size, uv, duv, dest);
		case DC_NO_A:		return intraDC_NO_A	(size, uv, duv, dest);
		case DC_127:		return intraDC_127	(size, uv, duv, dest);
		case DC_129:		return intraDC_129	(size, uv, duv, dest);
		case D45_NO_R:		return intraD45_NO_R(size, uv, duv, dest);
		case D135_NO_L:		return intraD135_NO_L(size, uv, duv, dest);
		case D135_NO_A:		return intraD135_NO_A(size, uv, duv, dest);
		case D117_NO_L:		return intraD117_NO_L(size, uv, duv, dest);
		case D117_NO_A:		return intraD117_NO_A(size, uv, duv, dest);
		case D153_NO_L:		return intraD153_NO_L(size, uv, duv, dest);
		case D153_NO_A:		return intraD153_NO_A(size, uv, duv, dest);
		case D63_NO_R:		return intraD63_NO_R(size, uv, duv, dest);
	}
#else
	return mode;
#endif
}

int		get_pred_mode(uint info)	{ return info & 63; }
int2	get_pred_uv(uint info)		{ return int2(BitFieldExtract(info, 6u, 13u), BitFieldExtract(info, 19u, 13u)); }

#ifdef PLAT_PS4

struct PREDbuffers {
	RegularBuffer<uint>		info;
	TexType		dest;
};

COMPUTE_SHADER(4,4,1)
void intra4(int2 threadid : GROUP_THREAD_ID, uint groupid : GROUP_ID, PREDbuffers buffers : S_SRT_DATA) {
	uint	info	= buffers.info[groupid];
	int2	uv		= get_pred_uv(info);
	add_clamp_pixel(buffers.dest, uv + threadid, intra(4, get_pred_mode(info), uv, threadid, buffers.dest));
}

COMPUTE_SHADER(8,8,1)
void intra8(int2 threadid : GROUP_THREAD_ID, uint groupid : GROUP_ID, PREDbuffers buffers : S_SRT_DATA) {
	uint	info	= buffers.info[groupid];
	int2	uv		= get_pred_uv(info);
	add_clamp_pixel(buffers.dest, uv + threadid, intra(8, get_pred_mode(info), uv, threadid, buffers.dest));
}

COMPUTE_SHADER(16,16,1)
void intra16(int2 threadid : GROUP_THREAD_ID, uint groupid : GROUP_ID, PREDbuffers buffers : S_SRT_DATA) {
	uint	info	= buffers.info[groupid];
	int2	uv		= get_pred_uv(info);
	add_clamp_pixel(buffers.dest, uv + threadid, intra(16, get_pred_mode(info), uv, threadid, buffers.dest));
}

COMPUTE_SHADER(32,32,1)
void intra32(int2 threadid : GROUP_THREAD_ID, uint groupid : GROUP_ID, PREDbuffers buffers : S_SRT_DATA) {
	uint	info	= buffers.info[groupid];
	int2	uv		= get_pred_uv(info);
	add_clamp_pixel(buffers.dest, uv + threadid, intra(32, get_pred_mode(info), uv, threadid, buffers.dest));
}
#else

RegularBuffer<uint>		buffers_info;
TexType		buffers_dest;

COMPUTE_SHADER(4,4,1)
void intra4(int2 threadid : GROUP_THREAD_ID, uint groupid : GROUP_ID) {
	uint	info	= buffers_info[groupid];
	int2	uv		= get_pred_uv(info);
	add_clamp_pixel(buffers_dest, uv + threadid, intra(4, get_pred_mode(info), uv, threadid, buffers_dest));
}

COMPUTE_SHADER(8,8,1)
void intra8(int2 threadid : GROUP_THREAD_ID, uint groupid : GROUP_ID) {
	uint	info	= buffers_info[groupid];
	int2	uv		= get_pred_uv(info);
	add_clamp_pixel(buffers_dest, uv + threadid, intra(8, get_pred_mode(info), uv, threadid, buffers_dest));
}

COMPUTE_SHADER(16,16,1)
void intra16(int2 threadid : GROUP_THREAD_ID, uint groupid : GROUP_ID) {
	uint	info	= buffers_info[groupid];
	int2	uv		= get_pred_uv(info);
	add_clamp_pixel(buffers_dest, uv + threadid, intra(16, get_pred_mode(info), uv, threadid, buffers_dest));
}

COMPUTE_SHADER(32,32,1)
void intra32(int2 threadid : GROUP_THREAD_ID, uint groupid : GROUP_ID) {
	uint	info	= buffers_info[groupid];
	int2	uv		= get_pred_uv(info);
	add_clamp_pixel(buffers_dest, uv + threadid, intra(32, get_pred_mode(info), uv, threadid, buffers_dest));
}

#endif

technique intra_predictions {
	pass p0 { SET_CS(intra4); }
	pass p1 { SET_CS(intra8); }
	pass p2 { SET_CS(intra16); }
	pass p3 { SET_CS(intra32); }
};

#ifdef PLAT_PS4

struct PREDbuffers2 {
	RegularBuffer<uint2>	info;
	TexType		dest;
};

COMPUTE_SHADER(4,4,1)
void intra4_2(int2 threadid : GROUP_THREAD_ID, uint groupid : GROUP_ID, PREDbuffers2 buffers : S_SRT_DATA) {
	uint	info	= buffers.info[groupid].x;
	int2	uv		= get_pred_uv(info);
	add_clamp_pixel(buffers.dest, uv + threadid, intra(4, get_pred_mode(info), uv, threadid, buffers.dest));
}

COMPUTE_SHADER(8,8,1)
void intra8_2(int2 threadid : GROUP_THREAD_ID, uint groupid : GROUP_ID, PREDbuffers2 buffers : S_SRT_DATA) {
	uint	info	= buffers.info[groupid].x;
	int2	uv		= get_pred_uv(info);
	add_clamp_pixel(buffers.dest, uv + threadid, intra(8, get_pred_mode(info), uv, threadid, buffers.dest));
}

COMPUTE_SHADER(16,16,1)
void intra16_2(int2 threadid : GROUP_THREAD_ID, uint groupid : GROUP_ID, PREDbuffers2 buffers : S_SRT_DATA) {
	uint	info	= buffers.info[groupid].x;
	int2	uv		= get_pred_uv(info);
	add_clamp_pixel(buffers.dest, uv + threadid, intra(16, get_pred_mode(info), uv, threadid, buffers.dest));
}

COMPUTE_SHADER(32,32,1)
void intra32_2(int2 threadid : GROUP_THREAD_ID, uint groupid : GROUP_ID, PREDbuffers2 buffers : S_SRT_DATA) {
	uint	info	= buffers.info[groupid].x;
	int2	uv		= get_pred_uv(info);
	add_clamp_pixel(buffers.dest, uv + threadid, intra(32, get_pred_mode(info), uv, threadid, buffers.dest));
}

#else

RegularBuffer<uint2>	buffers_info2;

COMPUTE_SHADER(4,4,1)
void intra4_2(int2 threadid : GROUP_THREAD_ID, uint groupid : GROUP_ID) {
	uint	info	= buffers_info2[groupid].x;
	int2	uv		= get_pred_uv(info);
	add_clamp_pixel(buffers_dest, uv + threadid, intra(4, get_pred_mode(info), uv, threadid, buffers_dest));
}

COMPUTE_SHADER(8,8,1)
void intra8_2(int2 threadid : GROUP_THREAD_ID, uint groupid : GROUP_ID) {
	uint	info	= buffers_info2[groupid].x;
	int2	uv		= get_pred_uv(info);
	add_clamp_pixel(buffers_dest, uv + threadid, intra(8, get_pred_mode(info), uv, threadid, buffers_dest));
}

COMPUTE_SHADER(16,16,1)
void intra16_2(int2 threadid : GROUP_THREAD_ID, uint groupid : GROUP_ID) {
	uint	info	= buffers_info2[groupid].x;
	int2	uv		= get_pred_uv(info);
	add_clamp_pixel(buffers_dest, uv + threadid, intra(16, get_pred_mode(info), uv, threadid, buffers_dest));
}

COMPUTE_SHADER(32,32,1)
void intra32_2(int2 threadid : GROUP_THREAD_ID, uint groupid : GROUP_ID) {
	uint	info	= buffers_info2[groupid].x;
	int2	uv		= get_pred_uv(info);
	add_clamp_pixel(buffers_dest, uv + threadid, intra(32, get_pred_mode(info), uv, threadid, buffers_dest));
}
#endif

technique intra_predictions2 {
	pass p0 { SET_CS(intra4_2); }
	pass p1 { SET_CS(intra8_2); }
	pass p2 { SET_CS(intra16_2); }
	pass p3 { SET_CS(intra32_2); }
};
