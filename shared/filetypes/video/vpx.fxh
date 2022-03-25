//-----------------------------------------------------------------------------
//	helpers
//-----------------------------------------------------------------------------

#define PACK4n(a,b,c,d)								((a) & 0xf) | (((b) & 0xf)<<4) | (((c) & 0xf)<<8) | (((d) & 0xf)<<12)
#define PACK8n(a,b,c,d,e,f,g,h)						PACK4n(a,b,c,d) | ((PACK4n(e,f,g,h))<<16)

#ifdef PLAT_PS4
typedef ulong	nibble16;
#define PACK16n(a,b,c,d,e,f,g,h,i,j,k,l,m,n,o,p)	uint(PACK8n(a,b,c,d,e,f,g,h)) | (ulong(PACK8n(i,j,k,l,m,n,o,p))<<32)
#else
typedef uint2	nibble16;
#define PACK16n(a,b,c,d,e,f,g,h,i,j,k,l,m,n,o,p)	uint2(PACK8n(a,b,c,d,e,f,g,h), PACK8n(i,j,k,l,m,n,o,p))
#endif

#define PACK4b(a,b,c,d)								((a) & 0xff) | (((b) & 0xff)<<8) | (((c) & 0xff)<<16) | (((d) & 0xff)<<24)
#define PACK8b(a,b,c,d,e,f,g,h)						PACK4b(a,b,c,d), PACK4b(e,f,g,h)
#define PACK16b(a,b,c,d,e,f,g,h,i,j,k,l,m,n,o,p)	PACK8b(a,b,c,d,e,f,g,h), PACK8b(i,j,k,l,m,n,o,p)

#define PACK4bo(O, a,b,c,d)							PACK4b(a + O, b + O, c + O, d + O)
#define PACK8bo(O, a,b,c,d,e,f,g,h)					PACK4bo(O, a,b,c,d), PACK4bo(O, e,f,g,h)
#define PACK16bo(O, a,b,c,d,e,f,g,h,i,j,k,l,m,n,o,p) PACK8bo(O, a,b,c,d,e,f,g,h), PACK8bo(O, i,j,k,l,m,n,o,p)

#define PACK2w(a,b)									a|(b<<16)
#define PACK4w(a,b,c,d)								PACK2w(a, b), PACK2w(c, d)
#define PACK8w(a,b,c,d,e,f,g,h)						{PACK4w(a,b,c,d), PACK4w(e,f,g,h)}
#define PACK16w(a,b,c,d,e,f,g,h,i,j,k,l,m,n,o,p)	PACK8w(a,b,c,d,e,f,g,h), PACK8w(i,j,k,l,m,n,o,p)

int2 unpack2i(uint x) {
	return int2(x << 16, x) >> 16;
}
int4 unpack4i(uint v) {
	return int4(v << 24, v << 16, v << 8, v) >> 24;
}
uint2 unpack2u(uint x) {
	return uint2(x << 16, x) >> 16;
}
uint4 unpack4u(uint v) {
	return uint4(v << 24, v << 16, v << 8, v) >> 24;
}

