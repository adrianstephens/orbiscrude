Texture2D		SrcTexture		: register(t0);
SamplerState	PointSampler	: register(s0);
RWBuffer<uint4>	DestTexture		: register(u0);

#define QUALITY

cbuffer MainCB : register(b0) {
	float2	ScreenSizeRcp;
	float2	TextureSizeRcp;
	float2	TexelBias;
	float	TexelScale;
	float	Exposure;
};

static const float	HALF_MAX	= 65504;

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

float3 Quantize(float3 x, int bits) {
	return f32tof16(x) * (1 << bits) / 0x7c00;
}

float3 Unquantize(float3 x, int bits) {
	return (x * 65536 + 0x8000) /  (1 << bits);
}

float3 QuantizeDelta(float3 x, float3 x0, int bits, int delta_bits) {
	int m = 1 << (delta_bits - 1);
	return clamp(floor(Quantize(x, bits) - x0), -m, m);
}

float3 FinishUnquantize(float3 endpoint0Unq, float3 endpoint1Unq, float weight) {
	float3 comp = (endpoint0Unq * (64 - weight) + endpoint1Unq * weight + 32) * 31 / 4096;
	return f16tof32(uint3(comp));
}

float GetWeight3(int i) {
	return floor(i * 64 /  7.f + 0.5);
}

float GetWeight4(int i) {
	return floor(i * 64 / 15.f + 0.5);
}

uint ComputeIndex3(float texelPos, float endPoint0Pos, float endPoint1Pos) {
	float r = (texelPos - endPoint0Pos) / (endPoint1Pos - endPoint0Pos);
	return (uint)clamp(r * 6.98182 + 0.00909 + 0.5, 0, 7);
}

uint ComputeIndex4(float texelPos, float endPoint0Pos, float endPoint1Pos) {
	float r = (texelPos - endPoint0Pos) / (endPoint1Pos - endPoint0Pos);
	return (uint)clamp(r * 14.93333 + 0.03333 + 0.5, 0, 15);
}

uint3 SignExtend(float3 v, int bits) {
	uint	sign	= 1 << (bits - 1);
	uint	mask	= sign - 1;
	return (uint3(v) & mask) | (v < 0 ? sign : 0);
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
	endpoints.q0	= floor(Quantize(blockMin, bits));
	endpoints.q1	= QuantizeDelta(blockMax, blockMax, bits, delta_bits);
	endpoints.uq0	= Unquantize(endpoints.q0, 10);
	endpoints.uq1	= Unquantize(endpoints.q0 + endpoints.q1, bits);
}

void init(out EndPoints2 x, float3 block0Min, float3 block0Max, float3 block1Min, float3 block1Max, int bits, int delta_bits) {
	init(x.endpoints[0], block0Min, block0Max, bits, delta_bits);
	init(x.endpoints[1], block1Min, block1Max, bits, delta_bits);
}

float CalcMSLE(in EndPoints e, float3 a, float weight) {
	return CalcMSLE(a, FinishUnquantize(e.uq0, e.uq1, weight));
}

float CalcMSLE(in EndPoints2 e, float3 a, float weight, int paletteID) {
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
	float3 blockMin = texels[0];
	float3 blockMax = texels[0];
	for (uint i = 1; i < 16; ++i) {
		blockMin = min(blockMin, texels[i]);
		blockMax = max(blockMax, texels[i]);
	}

	// refine endpoints in log2 RGB space
	float3 refinedBlockMin = blockMax;
	float3 refinedBlockMax = blockMin;
	for (uint i = 0; i < 16; ++i) {
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

	float3	endpoint0		= Quantize(blockMin, 10);
	float3	endpoint1		= Quantize(blockMax, 10);
	float	endPoint0Pos	= f32tof16(dot(blockMin, blockDir));
	float	endPoint1Pos	= f32tof16(dot(blockMax, blockDir));

	// check if endpoint swap is required
	if (ComputeIndex4(f32tof16(dot(texels[0], blockDir)), endPoint0Pos, endPoint1Pos) > 7) {
		Swap(endPoint0Pos, endPoint1Pos);
		Swap(endpoint0, endpoint1);
	}

	// compute indices
	uint indices[16];
	for (uint i = 0; i < 16; ++i)
		indices[i] = ComputeIndex4(f32tof16(dot(texels[i], blockDir)), endPoint0Pos, endPoint1Pos);

	// compute compression error (MSLE)
	float3	endpoint0Unq = Unquantize(endpoint0, 10);
	float3	endpoint1Unq = Unquantize(endpoint1, 10);
	float	msle = 0;
	for (uint i = 0; i < 16; ++i)
		msle += CalcMSLE(texels[i], FinishUnquantize(endpoint0Unq, endpoint1Unq, GetWeight4(indices[i])));

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
	float3 p0BlockMin = float3(HALF_MAX, HALF_MAX, HALF_MAX);
	float3 p0BlockMax = -p0BlockMin;
	float3 p1BlockMin =  p0BlockMin;
	float3 p1BlockMax = -p0BlockMin;

	uint mask = patterns[pattern];


	for (uint i = 0; i < 16; ++i) {
		uint paletteID = (mask >> i) & 1;//Pattern(pattern, i);
		if (paletteID == 0) {
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
	for (uint i = 0; i < 16; ++i) {
		uint	paletteID	= (mask >> i) & 1;//Pattern(pattern, i);
		indices[i]			= paletteID == 0
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
	for (uint i = 0; i < 16; ++i) {
		uint	paletteID	= (mask >> i) & 1;//Pattern(pattern, i);
		float	weight		= GetWeight3(indices[i]);
		msle105	+= CalcMSLE(q105, texels[i], weight, paletteID);
		msle76	+= CalcMSLE(q76,  texels[i], weight, paletteID);
		msle95	+= CalcMSLE(q95,  texels[i], weight, paletteID);
	}

	// better than exisiting?
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

[numthreads(4, 4, 1)]
void main(uint3 id : SV_DispatchThreadID) {
	// gather texels for current 4x4 block
	// 0 1 2 3
	// 4 5 6 7
	// 8 9 10 11
	// 12 13 14 15
	float3	texels[16];
#if 0
	float2 block0UV = (id.xy * 4 + int2(-1, -1)) * TextureSizeRcp;
	float2 block1UV = (id.xy * 4 + int2(+1, -1)) * TextureSizeRcp;
	float2 block2UV = (id.xy * 4 + int2(-1, +1)) * TextureSizeRcp;
	float2 block3UV = (id.xy * 4 + int2(+1, +1)) * TextureSizeRcp;

	float4 block0R	= SrcTexture.GatherRed(PointSampler, block0UV);
	float4 block1R	= SrcTexture.GatherRed(PointSampler, block1UV);
	float4 block2R	= SrcTexture.GatherRed(PointSampler, block2UV);
	float4 block3R	= SrcTexture.GatherRed(PointSampler, block3UV);

	float4 block0G	= SrcTexture.GatherGreen(PointSampler, block0UV);
	float4 block1G	= SrcTexture.GatherGreen(PointSampler, block1UV);
	float4 block2G	= SrcTexture.GatherGreen(PointSampler, block2UV);
	float4 block3G	= SrcTexture.GatherGreen(PointSampler, block3UV);

	float4 block0B	= SrcTexture.GatherBlue(PointSampler, block0UV);
	float4 block1B	= SrcTexture.GatherBlue(PointSampler, block1UV);
	float4 block2B	= SrcTexture.GatherBlue(PointSampler, block2UV);
	float4 block3B	= SrcTexture.GatherBlue(PointSampler, block3UV);

	texels[0]		= float3(block0R.w, block0G.w, block0B.w);
	texels[1]		= float3(block0R.z, block0G.z, block0B.z);
	texels[2]		= float3(block1R.w, block1G.w, block1B.w);
	texels[3]		= float3(block1R.z, block1G.z, block1B.z);

	texels[4]		= float3(block0R.x, block0G.x, block0B.x);
	texels[5]		= float3(block0R.y, block0G.y, block0B.y);
	texels[6]		= float3(block1R.x, block1G.x, block1B.x);
	texels[7]		= float3(block1R.y, block1G.y, block1B.y);

	texels[8]		= float3(block2R.w, block2G.w, block2B.w);
	texels[9]		= float3(block2R.z, block2G.z, block2B.z);
	texels[10]		= float3(block3R.w, block3G.w, block3B.w);
	texels[11]		= float3(block3R.z, block3G.z, block3B.z);

	texels[12]		= float3(block2R.x, block2G.x, block2B.x);
	texels[13]		= float3(block2R.y, block2G.y, block2B.y);
	texels[14]		= float3(block3R.x, block3G.x, block3B.x);
	texels[15]		= float3(block3R.y, block3G.y, block3B.y);

#else
	texels[0]		= SrcTexture[id.xy * 4 + uint2(0, 0)].rgb;
	texels[1]		= SrcTexture[id.xy * 4 + uint2(1, 0)].rgb;
	texels[2]		= SrcTexture[id.xy * 4 + uint2(2, 0)].rgb;
	texels[3]		= SrcTexture[id.xy * 4 + uint2(3, 0)].rgb;
	texels[4]		= SrcTexture[id.xy * 4 + uint2(0, 1)].rgb;
	texels[5]		= SrcTexture[id.xy * 4 + uint2(1, 1)].rgb;
	texels[6]		= SrcTexture[id.xy * 4 + uint2(2, 1)].rgb;
	texels[7]		= SrcTexture[id.xy * 4 + uint2(3, 1)].rgb;
	texels[8]		= SrcTexture[id.xy * 4 + uint2(0, 2)].rgb;
	texels[9]		= SrcTexture[id.xy * 4 + uint2(1, 2)].rgb;
	texels[10]		= SrcTexture[id.xy * 4 + uint2(2, 2)].rgb;
	texels[11]		= SrcTexture[id.xy * 4 + uint2(3, 2)].rgb;
	texels[12]		= SrcTexture[id.xy * 4 + uint2(0, 3)].rgb;
	texels[13]		= SrcTexture[id.xy * 4 + uint2(1, 3)].rgb;
	texels[14]		= SrcTexture[id.xy * 4 + uint2(2, 3)].rgb;
	texels[15]		= SrcTexture[id.xy * 4 + uint2(3, 3)].rgb;
#endif

	float	blockMSLE;
	uint4	block;
	EncodeP1(block, blockMSLE, texels, false);

#ifdef QUALITY
	for (uint i = 0; i < 32; ++i)
		EncodeP2Pattern(block, blockMSLE, i, texels, false);
#endif

	DestTexture[id.x + id.y * 256] = block;
}