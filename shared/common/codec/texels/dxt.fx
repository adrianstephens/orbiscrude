//#include "common.fxh"

#ifdef USE_DX11

#define technique technique11
#define SET_CS(f)		SetComputeShader(CompileShader(cs_5_0, f()))

Texture2D			SrcTexture		: register(t0);
RWTexture2D<uint4>	DestTexture		: register(u0);
RWTexture2D<uint2>	DestTextureDXT1	: register(u0);

#elif defined PLAT_METAL

#define SET_CS(f)		ComputeShader	= compile cs_metal f()

Texture2D<float>	SrcTexture;
RWTexture2D<uint>	DestTexture;

#endif




//-----------------------------------------------------------------------------
//	common
//-----------------------------------------------------------------------------

void Swap(inout float3 a, inout float3 b) {
	float3 t = a;
	a = b;
	b = t;
}

void Swap(inout float a, inout float b) {
	float t = a;
	a = b;
	b = t;
}

void ReadBlock(out float3 texels[16], float2 pos) {
	texels[0]		= SrcTexture[pos * 4 + uint2(0, 0)].rgb;
	texels[1]		= SrcTexture[pos * 4 + uint2(1, 0)].rgb;
	texels[2]		= SrcTexture[pos * 4 + uint2(2, 0)].rgb;
	texels[3]		= SrcTexture[pos * 4 + uint2(3, 0)].rgb;
	texels[4]		= SrcTexture[pos * 4 + uint2(0, 1)].rgb;
	texels[5]		= SrcTexture[pos * 4 + uint2(1, 1)].rgb;
	texels[6]		= SrcTexture[pos * 4 + uint2(2, 1)].rgb;
	texels[7]		= SrcTexture[pos * 4 + uint2(3, 1)].rgb;
	texels[8]		= SrcTexture[pos * 4 + uint2(0, 2)].rgb;
	texels[9]		= SrcTexture[pos * 4 + uint2(1, 2)].rgb;
	texels[10]		= SrcTexture[pos * 4 + uint2(2, 2)].rgb;
	texels[11]		= SrcTexture[pos * 4 + uint2(3, 2)].rgb;
	texels[12]		= SrcTexture[pos * 4 + uint2(0, 3)].rgb;
	texels[13]		= SrcTexture[pos * 4 + uint2(1, 3)].rgb;
	texels[14]		= SrcTexture[pos * 4 + uint2(2, 3)].rgb;
	texels[15]		= SrcTexture[pos * 4 + uint2(3, 3)].rgb;
}

void ReadBlock(out float4 texels[16], float2 pos) {
	texels[0]		= SrcTexture[pos * 4 + uint2(0, 0)];
	texels[1]		= SrcTexture[pos * 4 + uint2(1, 0)];
	texels[2]		= SrcTexture[pos * 4 + uint2(2, 0)];
	texels[3]		= SrcTexture[pos * 4 + uint2(3, 0)];
	texels[4]		= SrcTexture[pos * 4 + uint2(0, 1)];
	texels[5]		= SrcTexture[pos * 4 + uint2(1, 1)];
	texels[6]		= SrcTexture[pos * 4 + uint2(2, 1)];
	texels[7]		= SrcTexture[pos * 4 + uint2(3, 1)];
	texels[8]		= SrcTexture[pos * 4 + uint2(0, 2)];
	texels[9]		= SrcTexture[pos * 4 + uint2(1, 2)];
	texels[10]		= SrcTexture[pos * 4 + uint2(2, 2)];
	texels[11]		= SrcTexture[pos * 4 + uint2(3, 2)];
	texels[12]		= SrcTexture[pos * 4 + uint2(0, 3)];
	texels[13]		= SrcTexture[pos * 4 + uint2(1, 3)];
	texels[14]		= SrcTexture[pos * 4 + uint2(2, 3)];
	texels[15]		= SrcTexture[pos * 4 + uint2(3, 3)];
}


void GetBlockExtent(float3 texels[16], out float3 blockMin, out float3 blockMax) {
	blockMin = texels[0];
	blockMax = texels[0];
	for (uint i = 1; i < 16; ++i) {
		blockMin = min(blockMin, texels[i]);
		blockMax = max(blockMax, texels[i]);
	}
}

void GetBlockExtent(float4 texels[16], out float4 blockMin, out float4 blockMax) {
	blockMin = texels[0];
	blockMax = texels[0];
	for (uint i = 1; i < 16; ++i) {
		blockMin = min(blockMin, texels[i]);
		blockMax = max(blockMax, texels[i]);
	}
}

float dist2(float2 c0, float2 c1) { return dot(c0 - c1, c0 - c1); }
float dist2(float3 c0, float3 c1) { return dot(c0 - c1, c0 - c1); }
float dist2(float4 c0, float4 c1) { return dot(c0 - c1, c0 - c1); }

void InsetBBox(in out float3 blockMin, in out float3 blockMax, int q) {
	float3 inset = (blockMax - blockMin - (q / (255.0 * 2))) / q;
	blockMin = saturate(blockMin + inset);
	blockMax = saturate(blockMax - inset);
}
void InsetBBox(in out float blockMin, in out float blockMax, int q) {
	float inset = (blockMax - blockMin - (q / (255.0 * 2))) / q;
	blockMin = saturate(blockMin + inset);
	blockMax = saturate(blockMax - inset);
}
void InsetBBox(in out float2 blockMin, in out float2 blockMax, int q) {
	float2 inset = (blockMax - blockMin - (q / (255.0 * 2))) / q;
	blockMin = saturate(blockMin + inset);
	blockMax = saturate(blockMax - inset);
}

//-----------------------------------------------------------------------------
//	BC6
//-----------------------------------------------------------------------------
#define QUALITY

float CalcMSLE(float3 a, float3 b) {
	float3 err = log2((b + 1) / (a + 1));
	return dot(err, err);
}

uint PatternFixupID(uint i) {
	return	((0xcd1a0000 >> i) & 1) ? 2
		:	((0x32640000 >> i) & 1) ? 8
		:	15;
}

static const uint patterns[32] = {
	0xcccc, 0x8888, 0xeeee, 0xecc8, 0xc880, 0xfeec, 0xFEC8, 0xEC80, 
	0xC800, 0xFFEC, 0xfe80, 0xE800, 0xFFE8, 0xFF00, 0xFFF0, 0xF000, 
	0xF710, 0x008E, 0x7100, 0x08CE, 0x008C, 0x7310, 0x3100, 0x8CCE, 
	0x088C, 0x3110, 0x6666, 0x366C, 0x17E8, 0x0FF0, 0x718E, 0x399C, 
};
/*
uint Pattern(uint p, uint i) {
	uint p2	= p / 2;
	uint enc =  p2 == 0		? 0x8888cccc
			:	p2 == 1		? 0xecc8eeee
			:	p2 == 2		? 0xfeecc880
			:	p2 == 3		? 0xEC80FEC8
			:	p2 == 4		? 0xFFECC800
			:	p2 == 5		? 0xE800FE80
			:	p2 == 6		? 0xFF00FFE8
			:	p2 == 7		? 0xF000FFF0
			:	p2 == 8		? 0x008EF710
			:	p2 == 9		? 0x08CE7100
			:	p2 == 10	? 0x7310008C
			:	p2 == 11	? 0x8CCE3100
			:	p2 == 12	? 0x3110088C
			:	p2 == 13	? 0x366C6666
			:	p2 == 14	? 0x0FF017E8
			:	p2 == 15	? 0x399C718E
			:	0;

	enc = p & 1 ? enc >> 16 : enc;
	return (enc >> i) & 1;
}
*/

float3 QuantizeF16(float3 x, int bits) {
	return f32tof16(x) * (1 << bits) / 0x7c00;
}

float3 UnquantizeF16(float3 x, int bits) {
	return (x * 65536 + 0x8000) /  (1 << bits);
}

float3 QuantizeF16Delta(float3 x, float3 x0, int bits, int delta_bits) {
	int m = 1 << (delta_bits - 1);
	return clamp(floor(QuantizeF16(x, bits) - x0), -m, m);
}

float3 FinishUnquantizeF16(float3 endpoint0Unq, float3 endpoint1Unq, int weight) {
	float3 comp = (endpoint0Unq * (64 - weight) + endpoint1Unq * weight + 32) * 31 / 4096;
	return f16tof32(uint3(comp));
}

int GetWeight3(int i) {
	return (i * 128 + 7) /  14;
}

int GetWeight4(int i) {
	return (i * 128 + 15) / 30;
}

uint ComputeIndex3(float texelPos, float endPoint0Pos, float endPoint1Pos) {
	float r = (texelPos - endPoint0Pos) / (endPoint1Pos - endPoint0Pos);
	return (uint)clamp(r * 6.98182 + 0.00909 + 0.5, 0.0, 7.0);
}

uint ComputeIndex4(float texelPos, float endPoint0Pos, float endPoint1Pos) {
	float r = (texelPos - endPoint0Pos) / (endPoint1Pos - endPoint0Pos);
	return (uint)clamp(r * 14.93333 + 0.03333 + 0.5, 0.0, 15.0);
}

uint3 SignExtend(float3 v, int bits) {
	uint	sign	= 1 << (bits - 1);
	uint	mask	= sign - 1;
	return (uint3(v) & mask) | (v < 0 ? sign : uint(0));
}

uint3 MaybeSignExtend(float3 v, int bits, bool extend) {
	return extend ? SignExtend(v, bits) : uint3(v);
}

struct EndPoints {
	float3	q0;
	float3	q1;
	float3	uq0;
	float3	uq1;
};

struct EndPoints2 {
	EndPoints	endpoints[2];
};

void init(out EndPoints endpoints, float3 blockMin, float3 blockMax, int bits, int delta_bits) {
	endpoints.q0	= floor(QuantizeF16(blockMin, bits));
	endpoints.q1	= QuantizeF16Delta(blockMax, blockMax, bits, delta_bits);
	endpoints.uq0	= UnquantizeF16(endpoints.q0, 10);
	endpoints.uq1	= UnquantizeF16(endpoints.q0 + endpoints.q1, bits);
}

void init(out EndPoints2 x, float3 block0Min, float3 block0Max, float3 block1Min, float3 block1Max, int bits, int delta_bits) {
	init(x.endpoints[0], block0Min, block0Max, bits, delta_bits);
	init(x.endpoints[1], block1Min, block1Max, bits, delta_bits);
}

float CalcMSLE(in EndPoints e, float3 a, int weight) {
	return CalcMSLE(a, FinishUnquantizeF16(e.uq0, e.uq1, weight));
}

float CalcMSLE(in EndPoints2 e, float3 a, int weight, int paletteID) {
	return CalcMSLE(e.endpoints[paletteID], a, weight);
}

void EncodeP1(out uint4 block, out float blockMSLE, float3 texels[16], bool sign) {
	/* bit sizes available:
	6,6,6		(no delta)
	10,10,10	(no delta)
	11.9
	12.8
	16.4
	*/
	// compute endpoints (min/max RGB bbox)
	float3 blockMin, blockMax;
	GetBlockExtent(texels, blockMin, blockMax);

	// refine endpoints in log2 RGB space
	uint	i;
	float3	refinedBlockMin = blockMax;
	float3	refinedBlockMax = blockMin;
	for (i = 0; i < 16; ++i) {
		refinedBlockMin = min(refinedBlockMin, texels[i] == blockMin ? refinedBlockMin : texels[i]);
		refinedBlockMax = max(refinedBlockMax, texels[i] == blockMax ? refinedBlockMax : texels[i]);
	}

	float3 logBlockMax			= log2(blockMax + 1);
	float3 logBlockMin			= log2(blockMin + 1);
	float3 logRefinedBlockMax	= log2(refinedBlockMax + 1);
	float3 logRefinedBlockMin	= log2(refinedBlockMin + 1);
	float3 logBlockMaxExt		= (logBlockMax - logBlockMin) / 32;

	logBlockMin += min(logRefinedBlockMin - logBlockMin, logBlockMaxExt);
	logBlockMax -= min(logBlockMax - logRefinedBlockMax, logBlockMaxExt);
	blockMin	= exp2(logBlockMin) - 1;
	blockMax	= exp2(logBlockMax) - 1;

	float3 blockDir = blockMax - blockMin;
	blockDir = blockDir / (blockDir.x + blockDir.y + blockDir.z);

	float3	endpoint0		= QuantizeF16(blockMin, 10);
	float3	endpoint1		= QuantizeF16(blockMax, 10);
	float	endPoint0Pos	= f32tof16(dot(blockMin, blockDir));
	float	endPoint1Pos	= f32tof16(dot(blockMax, blockDir));

	// check if endpoint swap is required
	if (ComputeIndex4(f32tof16(dot(texels[0], blockDir)), endPoint0Pos, endPoint1Pos) > 7) {
		Swap(endPoint0Pos, endPoint1Pos);
		Swap(endpoint0, endpoint1);
	}

	// compute indices
	uint indices[16];
	for (i = 0; i < 16; ++i)
		indices[i] = ComputeIndex4(f32tof16(dot(texels[i], blockDir)), endPoint0Pos, endPoint1Pos);

	// compute compression error (MSLE)
	float3	endpoint0Unq = UnquantizeF16(endpoint0, 10);
	float3	endpoint1Unq = UnquantizeF16(endpoint1, 10);
	float	msle = 0;
	for (i = 0; i < 16; ++i)
		msle += CalcMSLE(texels[i], FinishUnquantizeF16(endpoint0Unq, endpoint1Unq, GetWeight4(indices[i])));

	// encode block for mode 11
	blockMSLE = msle;

	uint3	u0 = MaybeSignExtend(endpoint0, 10, sign);
	uint3	u1 = MaybeSignExtend(endpoint1, 10, sign);

	block = uint4(
		//X
		3						// mode=11
		| (u0.x			<<  5)	// endpoints
		| (u0.y			<< 15)
		| (u0.z			<< 25),
		//Y
		  (u0.z			>>  7)
		| (u1.x			<<  3)
		| (u1.y			<< 13)
		| (u1.z			<< 23),
		//Z
		  (u1.z			>>  9)
		| (indices[ 0]	<<  1)	// indices
		| (indices[ 1]	<<  4)
		| (indices[ 2]	<<  8)
		| (indices[ 3]	<< 12)
		| (indices[ 4]	<< 16)
		| (indices[ 5]	<< 20)
		| (indices[ 6]	<< 24)
		| (indices[ 7]	<< 28),
		//W
		  (indices[ 8]	<<  0)
		| (indices[ 9]	<<  4)
		| (indices[10]	<<  8)
		| (indices[11]	<< 12)
		| (indices[12]	<< 16)
		| (indices[13]	<< 20)
		| (indices[14]	<< 24)
		| (indices[15]	<< 28)
	);
}

void EncodeP2Pattern(inout uint4 block, inout float blockMSLE, int pattern, float3 texels[16], bool sign) {
	float3 p0BlockMin = 65504,		p0BlockMax = -p0BlockMin;
	float3 p1BlockMin = p0BlockMin,	p1BlockMax = p0BlockMax;

	uint mask = patterns[pattern];

	for (uint i = 0; i < 16; ++i) {
		if (((mask >> i) & 1) == 0) {
			p0BlockMin = min(p0BlockMin, texels[i]);
			p0BlockMax = max(p0BlockMax, texels[i]);
		} else {
			p1BlockMin = min(p1BlockMin, texels[i]);
			p1BlockMax = max(p1BlockMax, texels[i]);
		}
	}

	float3 p0BlockDir = p0BlockMax - p0BlockMin;
	float3 p1BlockDir = p1BlockMax - p1BlockMin;
	p0BlockDir = p0BlockDir / (p0BlockDir.x + p0BlockDir.y + p0BlockDir.z);
	p1BlockDir = p1BlockDir / (p1BlockDir.x + p1BlockDir.y + p1BlockDir.z);

	float	p0Endpoint0Pos	= f32tof16(dot(p0BlockMin, p0BlockDir));
	float	p0Endpoint1Pos	= f32tof16(dot(p0BlockMax, p0BlockDir));
	float	p1Endpoint0Pos	= f32tof16(dot(p1BlockMin, p1BlockDir));
	float	p1Endpoint1Pos	= f32tof16(dot(p1BlockMax, p1BlockDir));

	uint	fixupID			= PatternFixupID(pattern);

	// check if endpoint swaps are required
	if (ComputeIndex3(f32tof16(dot(texels[0], p0BlockDir)), p0Endpoint0Pos, p0Endpoint1Pos) > 3) {
		Swap(p0Endpoint0Pos, p0Endpoint1Pos);
		Swap(p0BlockMin, p0BlockMax);
	}
	if (ComputeIndex3(f32tof16(dot(texels[fixupID], p1BlockDir)), p1Endpoint0Pos, p1Endpoint1Pos) > 3) {
		Swap(p1Endpoint0Pos, p1Endpoint1Pos);
		Swap(p1BlockMin, p1BlockMax);
	}

	// compute indices
	uint indices[16];
	for (i = 0; i < 16; ++i) {
		indices[i]			= ((mask >> i) & 1) == 0
			? ComputeIndex3(f32tof16(dot(texels[i], p0BlockDir)), p0Endpoint0Pos, p0Endpoint1Pos)
			: ComputeIndex3(f32tof16(dot(texels[i], p1BlockDir)), p1Endpoint0Pos, p1Endpoint1Pos);
	}

	/* bit sizes available:
	*10.5
	*7.6
	11.5,4,4
	11.4,5,4
	11.4,4,5
	*9.5
	8.6,5,5
	8.5,6,5
	8.5,5,6
	*/

	EndPoints2	q105;
	EndPoints2	q76;
	EndPoints2	q95;

	init(q105, p0BlockMin, p0BlockMax, p1BlockMin, p1BlockMax, 10, 5);
	init(q76,  p0BlockMin, p0BlockMax, p1BlockMin, p1BlockMax,  7, 6);
	init(q95,  p0BlockMin, p0BlockMax, p1BlockMin, p1BlockMax,  9, 5);

	float	msle105 = 0;
	float	msle76	= 0;
	float	msle95	= 0;
	for (i = 0; i < 16; ++i) {
		uint	paletteID	= (mask >> i) & 1;//Pattern(pattern, i);
		int		weight		= GetWeight3(indices[i]);
		msle105	+= CalcMSLE(q105, texels[i], weight, paletteID);
		msle76	+= CalcMSLE(q76,  texels[i], weight, paletteID);
		msle95	+= CalcMSLE(q95,  texels[i], weight, paletteID);
	}

	// better than existing?
	float msle = min(min(msle76, msle95), msle105);

	if (msle < blockMSLE) {
		blockMSLE = msle;

		if (msle == msle76) {
			// mode2: 7.6
			uint3	u0	= MaybeSignExtend(q76.endpoints[0].q0, 7, sign);
			uint3	u1	= SignExtend(q76.endpoints[0].q1, 6);
			uint3	u2	= SignExtend(q76.endpoints[1].q0, 6);
			uint3	u3	= SignExtend(q76.endpoints[1].q1, 6);

			block = uint4(
				//X
					 1							// mode=2
				|	((u2.g & 0x20)	>> 3	)	// endpoints
				|	((u3.g & 0x30)	>> 1	)
				|	(u0.r			<< 5	)
				|	((u3.b & 0x01)	<< 12	)
				|	((u3.b & 0x02)	<< 12	)
				|	((u2.b & 0x10)	<< 10	)
				|	(u0.g			<< 15	)
				|	((u2.b & 0x20)	<< 17	)
				|	((u3.b & 0x04)	<< 21	)
				|	((u2.g & 0x10)	<< 20	)
				|	(u0.b			<< 25	),
				//Y
					((u3.b & 0x08)	>> 3	)
				|	((u3.b & 0x20)	>> 4	)
				|	((u3.b & 0x10)	>> 2	)
				|	(u1.r			<< 3	)
				|	((u2.g & 0x0F)	<< 9	)
				|	(u1.g			<< 13	)
				|	((u3.g & 0x0F)	<< 19	)
				|	(u1.b			<< 23	)
				|	((u2.b & 0x07)	<< 29	),
				//Z
					((u2.b & 0x08)	>> 3	)
				|	(u2.r			<< 1	)
				|	(u3.r			<< 7	),
				//W
				0
			);
		} else if (msle == msle95) {
			// mode6: 9.5
			uint3	u0	= MaybeSignExtend(q95.endpoints[0].q0, 9, sign);
			uint3	u1	= SignExtend(q95.endpoints[0].q1, 5);
			uint3	u2	= SignExtend(q95.endpoints[1].q0, 5);
			uint3	u3	= SignExtend(q95.endpoints[1].q1, 5);

			block = uint4(
				//X
					0xE							// mode=6
				|	(u0.r			<< 5	)	// endpoints
				|	((u2.b & 0x10)	<< 10	)
				|	(u0.g			<< 15	)
				|	((u2.g & 0x10)	<< 20	)
				|	(u0.b			<< 25	),
				//Y
					(u0.b			>> 7	)
				|	((u3.b & 0x10)	>> 2	)
				|	(u1.r			<< 3	)
				|	((u3.g & 0x10)	<< 4	)
				|	((u2.g & 0x0F)	<< 9	)
				|	(u1.g			<< 13	)
				|	((u3.b & 0x01)	<< 18	)
				|	((u3.g & 0x0F)	<< 19	)
				|	(u1.b			<< 23	)
				|	((u3.b & 0x02)	<< 27	)
				|	(u2.b			<< 29	),
				//Z
					((u2.b & 0x08)	>> 3	)
				|	(u2.r			<< 1	)
				|	((u3.b & 0x04)	<< 4	)
				|	(u3.r			<< 7	)
				|	((u3.b & 0x08)	<< 9	),
				//W
				0
			);
		} else {
			// mode1: 10.5
			uint3	u0	= MaybeSignExtend(q105.endpoints[0].q0, 10, sign);
			uint3	u1	= SignExtend(q105.endpoints[0].q1, 5);
			uint3	u2	= SignExtend(q105.endpoints[1].q0, 5);
			uint3	u3	= SignExtend(q105.endpoints[1].q1, 5);

			block = uint4(
				//X
					0							// mode=1
				|	((u2.g & 0x10)	>> 2	)	// endpoints
				|	((u2.b & 0x10)	>> 1	)
				|	((u3.b & 0x10)	>> 0	)
				|	(u0.r			<< 5	)
				|	(u0.g			<< 15	)
				|	(u0.b			<< 25	),
				//Y
					(u0.b			>> 7	)	//same as 9.5
				|	(u1.r			<< 3	)
				|	((u3.g & 0x10)	<< 4	)
				|	((u2.g & 0x0F)	<< 9	)
				|	(u1.g			<< 13	)
				|	((u3.b & 0x01)	<< 18	)
				|	((u3.g & 0x0F)	<< 19	)
				|	(u1.b			<< 23	)
				|	((u3.b & 0x02)	<< 27	)
				|	(u2.b			<< 29	),
				//Z
					((u2.b & 0x08)	>> 3	)	//same as 9.5
				|	(u2.r			<< 1	)
				|	((u3.b & 0x04)	<< 4	)
				|	(u3.r			<< 7	)
				|	((u3.b & 0x08)	<< 9	),
				//W
				0
			);
		}

		block.z |= pattern << 13;

		if (fixupID == 15) {
			block.z |= 
				(indices[ 0] << 18)
			|	(indices[ 1] << 20)
			|	(indices[ 2] << 23)
			|	(indices[ 3] << 26)
			|	(indices[ 4] << 29);
			block.w =
				(indices[ 5] <<  0)
			|	(indices[ 6] <<  3)
			|	(indices[ 7] <<  6)
			|	(indices[ 8] <<  9)
			|	(indices[ 9] << 12)
			|	(indices[10] << 15)
			|	(indices[11] << 18)
			|	(indices[12] << 21)
			|	(indices[13] << 24)
			|	(indices[14] << 27)
			|	(indices[15] << 30);

		} else if (fixupID == 2) {
			block.z |= 
				(indices[ 0] << 18)
			|	(indices[ 1] << 20)
			|	(indices[ 2] << 23)
			|	(indices[ 3] << 25)
			|	(indices[ 4] << 28)
			|	(indices[ 5] << 31);
			block.w =
				(indices[ 5] >>  1)
			|	(indices[ 6] <<  2)
			|	(indices[ 7] <<  5)
			|	(indices[ 8] <<  8)
			|	(indices[ 9] << 11)
			|	(indices[10] << 14)
			|	(indices[11] << 17)
			|	(indices[12] << 20)
			|	(indices[13] << 23)
			|	(indices[14] << 26)
			|	(indices[15] << 29);

		} else {
			block.z |=
				(indices[ 0] << 18)
			|	(indices[ 1] << 20)
			|	(indices[ 2] << 23)
			|	(indices[ 3] << 26)
			|	(indices[ 4] << 29);
			block.w =
				(indices[ 5] <<  0)
			|	(indices[ 6] <<  3)
			|	(indices[ 7] <<  6)
			|	(indices[ 8] <<  9)
			|	(indices[ 9] << 11)
			|	(indices[10] << 14)
			|	(indices[11] << 17)
			|	(indices[12] << 20)
			|	(indices[13] << 23)
			|	(indices[14] << 26)
			|	(indices[15] << 29);
		}
	}
}

[numthreads(1, 1, 1)]
void CS_bc6h(uint3 id : SV_DispatchThreadID) {
	// gather texels for current 4x4 block
	// 0 1 2 3
	// 4 5 6 7
	// 8 9 10 11
	// 12 13 14 15
	float3	texels[16];
	ReadBlock(texels, id.xy);

	float	blockMSLE;
	uint4	block;
	EncodeP1(block, blockMSLE, texels, false);

#ifdef QUALITY
	for (uint i = 0; i < 32; ++i)
		EncodeP2Pattern(block, blockMSLE, i, texels, false);
#endif

	DestTexture[id.xy] = block;
}

//-----------------------------------------------------------------------------
//	DXT1
//-----------------------------------------------------------------------------

void SelectDiagonal(float3 block[16], in out float3 blockMin, in out float3 blockMax) {
	float3 center = (blockMin + blockMax) * 0.5;

	float2 cov = 0;
	for (int i = 0; i < 16; i++) {
		float3 t = block[i] - center;
		cov += t.xy * t.z;
	}

	float2	a = blockMin.xy, b = blockMax.xy;
	blockMin.xy = lerp(a, b, float2(cov.x < 0, cov.y < 0));
	blockMax.xy = a + b - blockMin.xy;
}

float3 RoundAndExpand(float3 v, out uint w) {
	int3 c = round(v * float3(31, 63, 31));
	w = (c.r << 11) | (c.g << 5) | c.b;

	//c.rb = (c.rb << 3) | (c.rb >> 2);//*33/4	//
	//c.g = (c.g << 2) | (c.g >> 4);//*65/16

	c	= c * int3(132, 65, 132) / 16;

	return (float3)c * (1.0 / 255.0);
}

uint EmitEndPointsDXT1(in out float3 blockMin, in out float3 blockMax) {
	uint outx, outy;
	blockMax = RoundAndExpand(blockMax, outx);
	blockMin = RoundAndExpand(blockMin, outy);

	// We have to do this in case we select an alternate diagonal
	if (outx < outy) {
		Swap(blockMin, blockMax);
		return outy | (outx << 16);
	}

	return outx | (outy << 16);
}

uint EmitIndicesDXT1(float3 col[16], float3 blockMin, float3 blockMax) {
	// Compute palette
	float3 c[4];
	c[0] = blockMax;
	c[1] = blockMin;
	c[2] = lerp(c[0], c[1], 1.0/3.0);
	c[3] = lerp(c[0], c[1], 2.0/3.0);

	// Compute indices
	uint indices = 0;
	for (int i = 0; i < 16; i++) {
		// find index of closest color
		float4 dist = float4(
			dist2(col[i], c[0]),
			dist2(col[i], c[1]),
			dist2(col[i], c[2]),
			dist2(col[i], c[3])
		);

		uint4	b		= uint4(dist.xyxy > dist.wzzw);
		uint	b4		= uint(dist.z > dist.w);
		uint	index	= (b.x & b4) | (((b.y & b.z) | (b.x & b.w)) << 1);
		indices |= index << (i * 2);
	}

	// Output indices
	return indices;
}

// compress a 4x4 block to DXT1 format
// integer version, renders to 2 x int32 buffer
[numthreads(1, 1, 1)]
void CS_dxt1(uint3 id : SV_DispatchThreadID) {
	float3 block[16];
	ReadBlock(block, id.xy);

	// find min and max colors
	float3 blockMin, blockMax;
	GetBlockExtent(block, blockMin, blockMax);

	// enable the diagonal selection for better quality at a small performance penalty
	SelectDiagonal(block, blockMin, blockMax);

	InsetBBox(blockMin, blockMax, 16);

#if defined PLAT_METAL
	DestTexture[id.xy] = uint4(
		EmitEndPointsDXT1(blockMin, blockMax),
		EmitIndicesDXT1(block, blockMin, blockMax),
		0, 0
	);
#else
	DestTextureDXT1[id.xy] = uint2(
		EmitEndPointsDXT1(blockMin, blockMax),
		EmitIndicesDXT1(block, blockMin, blockMax)
	);
#endif
}

//-----------------------------------------------------------------------------
//	DXT3
//-----------------------------------------------------------------------------

uint2 EmitAlphaDXT3(float block[16]) {
	uint2	output = 0;
	for (uint i = 0; i < 8; i++) {
		output.x |= uint(round(block[i + 0] * 15)) << (4 * i);
		output.y |= uint(round(block[i + 8] * 15)) << (4 * i);
	}
	return output;
}

// compress a 4x4 block to DXT3 format
// integer version, renders to 4 x int32 buffer
[numthreads(1, 1, 1)]
void CS_dxt3(uint3 id : SV_DispatchThreadID) {
	float4 block[16];
	ReadBlock(block, id.xy);

	// find min and max colors
	float4 blockMin, blockMax;
	GetBlockExtent(block, blockMin, blockMax);

	float3	rgb[16];
	float	alpha[16];
	for (uint i = 0; i < 16; i++) {
		rgb[i]		= block[i].rgb;
		alpha[i]	= block[i].a;
	}

	float3	rgbMin = blockMin.rgb, rgbMax = blockMax.rgb;
	SelectDiagonal(rgb, rgbMin, rgbMax);
	InsetBBox(rgbMin, rgbMax, 16);

	DestTexture[id.xy] = uint4(
		EmitAlphaDXT3(alpha),
		EmitEndPointsDXT1(rgbMin, rgbMax),
		EmitIndicesDXT1(rgb, rgbMin, rgbMax)
	);
}

//-----------------------------------------------------------------------------
//	DXT5
//-----------------------------------------------------------------------------

uint2 EmitAlphaDXT5(float block[16], float blockMin, float blockMax) {
	const int ALPHA_RANGE = 7;

	InsetBBox(blockMin, blockMax, 32);

	uint c0 = round(blockMin * 255);
	uint c1 = round(blockMax * 255);

	uint2	output;
	output.x	= (c0 << 8) | c1;

	float mid = (blockMax - blockMin) / (2.0 * ALPHA_RANGE);

	float ab1 = blockMin + mid;
	float ab2 = (6 * blockMax + 1 * blockMin) / ALPHA_RANGE + mid;
	float ab3 = (5 * blockMax + 2 * blockMin) / ALPHA_RANGE + mid;
	float ab4 = (4 * blockMax + 3 * blockMin) / ALPHA_RANGE + mid;
	float ab5 = (3 * blockMax + 4 * blockMin) / ALPHA_RANGE + mid;
	float ab6 = (2 * blockMax + 5 * blockMin) / ALPHA_RANGE + mid;
	float ab7 = (1 * blockMax + 6 * blockMin) / ALPHA_RANGE + mid;

	uint i, index;
	for (i = 0; i < 6; i++) {
		float a = block[i];
		index	= (1 + int(a <= ab1) + int(a <= ab2) + int(a <= ab3) + int(a <= ab4) + int(a <= ab5) + int(a <= ab6) + int(a <= ab7)) & 7;
		index	^= (2 > index);
		output.x |= index << (3 * i + 16);
	}

	output.y = index >> 1;

	for (i = 6; i < 16; i++) {
		float a = block[i];
		index	= (1 + int(a <= ab1) + int(a <= ab2) + int(a <= ab3) + int(a <= ab4) + int(a <= ab5) + int(a <= ab6) + int(a <= ab7)) & 7;
		index	^= (2 > index);
		output.y |= index << (3 * i - 16);
	}

	return output;
}

// compress a 4x4 block to DXT5 format
// integer version, renders to 4 x int32 buffer
[numthreads(1, 1, 1)]
void CS_dxt5(uint3 id : SV_DispatchThreadID) {
	float4 block[16];
	ReadBlock(block, id.xy);

	// find min and max colors
	float4 blockMin, blockMax;
	GetBlockExtent(block, blockMin, blockMax);

	float3	rgb[16];
	float	alpha[16];
	for (uint i = 0; i < 16; i++) {
		rgb[i]		= block[i].rgb;
		alpha[i]	= block[i].a;
	}

	float3	rgbMin = blockMin.rgb, rgbMax = blockMax.rgb;

	SelectDiagonal(rgb, rgbMin, rgbMax);
	InsetBBox(rgbMin, rgbMax, 16);

	DestTexture[id.xy] = uint4(
		EmitAlphaDXT5(alpha, blockMin.a, blockMax.a),
		EmitEndPointsDXT1(rgbMin, rgbMax),
		EmitIndicesDXT1(rgb, rgbMin, rgbMax)
	);
}

//-----------------------------------------------------------------------------
//	DXT5 YCoCg
//-----------------------------------------------------------------------------

float	from_signed(float f)	{ return f + 128.0 / 255.0; }
float2	from_signed(float2 f)	{ return f + 128.0 / 255.0; }
float3	from_signed(float3 f)	{ return f + 128.0 / 255.0; }
float	to_signed(float f)		{ return f - 128.0 / 255.0; }
float2	to_signed(float2 f)		{ return f - 128.0 / 255.0; }
float3	to_signed(float3 f)		{ return f - 128.0 / 255.0; }

float3 toYCoCg(float3 c) {
	return float3(
		(c.r + 2 * c.g + c.b) * 0.25,
		from_signed(( 2 * c.r - 2 * c.b      ) * 0.25),
		from_signed((    -c.r + 2 * c.g - c.b) * 0.25)
	);
}

int GetYCoCgScale(float2 blockMin, float2 blockMax) {
	float2	m0	= abs(to_signed(blockMin));
	float2	m1	= abs(to_signed(blockMax));
	float	m	= max(max(m0.x, m0.y), max(m1.x, m1.y));

	return	m < 32.0 / 255.0 ? 4
		:	m < 64.0 / 255.0 ? 2
		:	1;
}

void SelectYCoCgDiagonal(const float3 block[16], in out float2 blockMin, in out float2 blockMax) {
	float2 mid = (blockMax + blockMin) * 0.5;

	float cov = 0;
	for (int i = 0; i < 16; i++) {
		float2 t = block[i].yz - mid;
		cov += t.x * t.y;
	}
	if (cov < 0) {
		float t = blockMin.y;
		blockMin.y = blockMax.y;
		blockMax.y = t;
//		Swap(blockMin.y, blockMax.y);
	}
}

uint EmitEndPointsYCoCgDXT5(in out float2 blockMin, in out float2 blockMax, int scale) {
	blockMax = from_signed(to_signed(blockMax) * scale);
	blockMin = from_signed(to_signed(blockMin) * scale);

	InsetBBox(blockMin, blockMax, 16);

	blockMax = round(blockMax * float2(31, 63));
	blockMin = round(blockMin * float2(31, 63));

	int2 imaxcol = blockMax;
	int2 imincol = blockMin;

	uint2 output = uint2(
		(imaxcol.r << 11) | (imaxcol.g << 5) | (scale - 1),
		(imincol.r << 11) | (imincol.g << 5) | (scale - 1)
	);

	imaxcol.r = (imaxcol.r << 3) | (imaxcol.r >> 2);
	imaxcol.g = (imaxcol.g << 2) | (imaxcol.g >> 4);
	imincol.r = (imincol.r << 3) | (imincol.r >> 2);
	imincol.g = (imincol.g << 2) | (imincol.g >> 4);

	blockMax = (float2)imaxcol * (1.0 / 255.0);
	blockMin = (float2)imincol * (1.0 / 255.0);

	// Undo rescale.
	blockMax = from_signed(to_signed(blockMax) / scale);
	blockMin = from_signed(to_signed(blockMin) / scale);

	return output.x | (output.y << 16);
}

uint EmitIndicesYCoCgDXT5(float3 block[16], float2 blockMin, float2 blockMax) {
	// Compute palette
	float2 c[4];
	c[0] = blockMax;
	c[1] = blockMin;
	c[2] = lerp(c[0], c[1], 1.0/3.0);
	c[3] = lerp(c[0], c[1], 2.0/3.0);

	// Compute indices
	uint indices = 0;
	for (int i = 0; i < 16; i++) {
		// find index of closest color
		float4 dist = float4(
			dist2(block[i].yz, c[0]),
			dist2(block[i].yz, c[1]),
			dist2(block[i].yz, c[2]),
			dist2(block[i].yz, c[3])
		);

		uint4	b		= uint4(dist.xyxy > dist.wzzw);
		uint	b4		= uint(dist.z > dist.w);
		uint	index	= (b.x & b4) | (((b.y & b.z) | (b.x & b.w)) << 1);
		indices |= index << (i * 2);
	}

	return indices;
}


// compress a 4x4 block to YCoCg-DXT5 format
// integer version, renders to 4 x int32 buffer
[numthreads(1, 1, 1)]
void CS_dxt5_ycocg(uint3 id : SV_DispatchThreadID) {
	float3 	block[16];
	float	alpha[16];

	ReadBlock(block, id.xy);

	for (uint i = 0; i < 16; i++) {
		block[i] = toYCoCg(block[i]);
		alpha[i] = block[i].x;
	}

	// find min and max colors
	float3 blockMin, blockMax;
	GetBlockExtent(block, blockMin, blockMax);

	float2	CoCgMin	= blockMin.yz, CoCgMax = blockMax.yz;
	SelectYCoCgDiagonal(block, CoCgMin, CoCgMax);

	int scale = GetYCoCgScale(CoCgMin, CoCgMax);

	// Output CoCg in DXT1 block.
	uint4 output;
	output.z = EmitEndPointsYCoCgDXT5(CoCgMin, CoCgMax, scale);
	output.w = EmitIndicesYCoCgDXT5(block, CoCgMin, CoCgMax);

	// Output Y in DXT5 alpha block.
	for (i = 0; i < 16; i++)
	output.xy = EmitAlphaDXT5(alpha, blockMin.x, blockMax.x);

	DestTexture[id.xy] = output;
}

//-----------------------------------------------------------------------------
//	techniques
//-----------------------------------------------------------------------------

technique dxt1 {
	pass p0 {
		SET_CS(CS_dxt1);
	}
}

technique dxt3 {
	pass p0 {
		SET_CS(CS_dxt3);
	}
}

technique dxt5 {
	pass p0 {
		SET_CS(CS_dxt5);
	}
}

technique dxt5_ycocg {
	pass p0 {
		SET_CS(CS_dxt5_ycocg);
	}
}

technique bc6h {
	pass p0 {
		SET_CS(CS_bc6h);
	}
}

