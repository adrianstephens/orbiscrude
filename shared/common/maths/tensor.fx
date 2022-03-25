//#include "common.fxh"

Texture2D			Srce	: register(t0);
Texture2D			SrceI	: register(t1);
RWTexture2D<float4>	Dest	: register(u0);
RWTexture2D<float4>	DestI	: register(u1);
Buffer				filter	: register(t1);

//-----------------------------------------------------------------------------
// common
//-----------------------------------------------------------------------------

cbuffer common_consts : register(b0) {
	int4	size;	//x, y, w, h
	float2	minmax;	//min, max
	float   beta;
};

static const float PI = 3.14159265f;

float4 clamped_read(Texture2D tex, uint2 uv) {
	if (all(uv < size.zw))
		return tex[uv];
	return 0;
}

uint reversebits(uint i, uint n) {
	return reversebits(i) >> (32 - n);
}

uint2 reversebits(uint2 i, uint n) {
	return reversebits(i) >> (32 - n);
}

uint2 block_split(uint x, uint log2block) {
	int		block	= 1 << log2block;
	return uint2(0, block) + ((x & -block) * 2 + (x & (block - 1)));
}

//-----------------------------------------------------------------------------
// Multiply
//-----------------------------------------------------------------------------

[numthreads(1, 1, 1)]
void CS_multiply(uint3 id : SV_DispatchThreadID) {
	Dest[id.xy] = clamp(Srce[id.xy] * SrceI[id.xy], minmax.x, minmax.y);
}

[numthreads(1, 1, 1)]
void CS_multiply_scalar(uint3 id : SV_DispatchThreadID) {
	Dest[id.xy] = clamp(Srce[id.xy] * SrceI[id.xy].x, minmax.x, minmax.y);
}

technique11 multiply {
	pass p0 { SetComputeShader(CompileShader(cs_5_0, CS_multiply())); }
	pass p0 { SetComputeShader(CompileShader(cs_5_0, CS_multiply_scalar())); }
}

//-----------------------------------------------------------------------------
// Max
//-----------------------------------------------------------------------------

groupshared float4 values[1 << 9];

float4 for_group(float4 v, uint x) {
	if (x == 0)
		values[0] = v;
	GroupMemoryBarrierWithGroupSync();
	return values[0];
}

float4 get_max(float4 v, uint x, uint n) {
	for (uint i = 0; i < n; i++) {
		uint	m	= 1 << i, m2 = m * 2 - 1;
		uint	j	= x >> (i + 1);

		if ((x & m2) == m)
			values[j] = v;

		GroupMemoryBarrierWithGroupSync();

		if ((x & m2) == 0)
			v = max(v, values[j]);
	}
	return v;  // note - only valid for x == 0
}

void get_max(uint2 in_uv, uint2 out_uv, uint i, uint n) {
	float4 m = get_max(clamped_read(Srce, in_uv), i, n);
	if (i == 0)
		Dest[out_uv] = m;
}

[numthreads(64, 1, 1)] void CS_MaxRow(uint3 id : SV_DispatchThreadID, uint3 group : SV_GroupID, uint i : SV_GroupIndex) {
	get_max(id.xy, group.xy, i, 6);
}

[numthreads(1, 64, 1)] void CS_MaxCol(uint3 id : SV_DispatchThreadID, uint3 group : SV_GroupID, uint i : SV_GroupIndex) {
	get_max(id.xy, group.xy, i, 6);
}

[numthreads(8, 8, 1)] void CS_Max2D(uint3 id : SV_DispatchThreadID, uint3 group : SV_GroupID, uint i : SV_GroupIndex) {
	get_max(id.xy, group.xy, i, 6);
} 

technique11 getmax {
	pass p0 { SetComputeShader(CompileShader(cs_5_0, CS_MaxRow())); }
	pass p1 { SetComputeShader(CompileShader(cs_5_0, CS_MaxCol())); }
	pass p2 { SetComputeShader(CompileShader(cs_5_0, CS_Max2D())); }
}

//-----------------------------------------------------------------------------
// Sum
//-----------------------------------------------------------------------------

float4 get_sum(float4 v, uint x, uint n) {
	for (uint i = 0; i < n; i++) {
		uint m = 1 << i, m2 = m * 2 - 1;
		uint j = x >> (i + 1);

		if ((x & m2) == m)
			values[j] = v;

		GroupMemoryBarrierWithGroupSync();

		if ((x & m2) == 0)
			v += values[j];
	}
	return v;	// note - only valid for x == 0
}

void get_sum(uint2 in_uv, uint2 out_uv, uint i, uint n) {
	float4 m = get_sum(clamped_read(Srce, in_uv), i, n);
	if (i == 0)
		Dest[out_uv] = m;
}

[numthreads(64, 1, 1)] void CS_SumRow(uint3 id : SV_DispatchThreadID, uint3 group : SV_GroupID, uint i : SV_GroupIndex) {
	get_sum(id.xy, group.xy, i, 6);
}
[numthreads(1, 64, 1)] void CS_SumCol(uint3 id : SV_DispatchThreadID, uint3 group : SV_GroupID, uint i : SV_GroupIndex) {
	get_sum(id.xy, group.xy, i, 6);
}
[numthreads(8, 8, 1)] void CS_Sum2D(uint3 id : SV_DispatchThreadID, uint3 group : SV_GroupID, uint i : SV_GroupIndex) {
	get_sum(id.xy, group.xy, i, 6);
}

technique11 getsum {
	pass p0 { SetComputeShader(CompileShader(cs_5_0, CS_SumRow())); }
	pass p1 { SetComputeShader(CompileShader(cs_5_0, CS_SumCol())); }
	pass p2 { SetComputeShader(CompileShader(cs_5_0, CS_Sum2D())); }
}

//-----------------------------------------------------------------------------
// SoftMax
//-----------------------------------------------------------------------------

// one pass (if small enough)
void softmax_all(uint2 uv, uint i, uint n) {
	float4	v	= Srce[uv];
	float4	M	= for_group(get_max(v, i, n), i);
	float4	e	= exp((v - M) * beta);
	float4	S	= get_sum(e, i, n);
	Dest[uv] = e / S;
}

[numthreads(1<< 6, 1, 1)] void CS_softmax6 (uint3 id : SV_DispatchThreadID, uint i : SV_GroupIndex)	{ softmax_all(id.xy, i,  6); }
[numthreads(1<< 7, 1, 1)] void CS_softmax7 (uint3 id : SV_DispatchThreadID, uint i : SV_GroupIndex)	{ softmax_all(id.xy, i,  7); }
[numthreads(1<< 8, 1, 1)] void CS_softmax8 (uint3 id : SV_DispatchThreadID, uint i : SV_GroupIndex)	{ softmax_all(id.xy, i,  8); }
[numthreads(1<< 9, 1, 1)] void CS_softmax9 (uint3 id : SV_DispatchThreadID, uint i : SV_GroupIndex)	{ softmax_all(id.xy, i,  9); }
[numthreads(1<<10, 1, 1)] void CS_softmax10(uint3 id : SV_DispatchThreadID, uint i : SV_GroupIndex)	{ softmax_all(id.xy, i, 10); }

Texture2D MaxesSums : register(t1);

//initial max and sum
void softmax_pre0(uint2 in_uv, uint2 out_uv, uint i, uint n) {
	float4 v = Srce[in_uv];
	float4 M = for_group(get_max(v, i, n), i);
	float4 S = get_sum(exp((v - M) * beta), i, n);
	if (i == 0) {
		Dest[uint2(out_uv.x * 2 + 0, out_uv.y)] = M;
		Dest[uint2(out_uv.x * 2 + 1, out_uv.y)] = S;
	}
}

// subsequent max and sum
void softmax_pre1(uint2 in_uv, uint2 out_uv, uint x, uint n) {
	float4 M = 0;
	float4 S = 0;

	if (all(in_uv < size.zw)) {
		M = Srce[uint2(in_uv.x * 2 + 0, in_uv.y)];
		S = Srce[uint2(in_uv.x * 2 + 1, in_uv.y)];
	}

	for (uint i = 0; i < n; i++) {
		uint m = 1 << i, m2 = m * 2 - 1;
		uint j = x >> (i + 1);

		if ((x & m2) == m) {
			values[j * 2 + 0] = M;
			values[j * 2 + 1] = S;
		}

		GroupMemoryBarrierWithGroupSync();

		if ((x & m2) == 0) {
			float4	M2	= values[j * 2 + 0];
			float4	S2	= values[j * 2 + 1];

			S = exp(beta * min(M - M2, 0)) * S + exp(beta * min(M2 - M, 0)) * S2;
			M = max(M, M2);
		}
	}
	if (x == 0) {
		Dest[uint2(out_uv.x * 2 + 0, out_uv.y)] = M;
		Dest[uint2(out_uv.x * 2 + 1, out_uv.y)] = S;
	}
}

[numthreads(64, 1, 1)] void CS_softmaxRowPre0(uint3 id : SV_DispatchThreadID, uint3 group : SV_GroupID, uint i : SV_GroupIndex) {
	softmax_pre0(id.xy, group.xy, i, 6);
}
[numthreads(64, 1, 1)] void CS_softmaxColPre0(uint3 id : SV_DispatchThreadID, uint3 group : SV_GroupID, uint i : SV_GroupIndex) {
	softmax_pre0(id.xy, group.xy, i, 6);
}
[numthreads(8, 8, 1)] void CS_softmax2dPre0(uint3 id : SV_DispatchThreadID, uint3 group : SV_GroupID, uint i : SV_GroupIndex) {
	softmax_pre0(id.xy, group.xy, i, 6);
}

[numthreads(64, 1, 1)] void CS_softmaxRowPre1(uint3 id : SV_DispatchThreadID, uint3 group : SV_GroupID, uint i : SV_GroupIndex) {
	softmax_pre1(id.xy, group.xy, i, 6);
}
[numthreads(64, 1, 1)] void CS_softmaxColPre1(uint3 id : SV_DispatchThreadID, uint3 group : SV_GroupID, uint i : SV_GroupIndex) {
	softmax_pre1(id.xy, group.xy, i, 6);
}
[numthreads(8, 8, 1)] void CS_softmax2dPre1(uint3 id : SV_DispatchThreadID, uint3 group : SV_GroupID, uint i : SV_GroupIndex) {
	softmax_pre1(id.xy, group.xy, i, 6);
}

[numthreads(1, 1, 1)] void CS_softmaxFinal(uint3 id : SV_DispatchThreadID) {
	Dest[id.xy] = exp((Srce[id.xy] - MaxesSums[uint2(0, 0)]) * beta) / MaxesSums[uint2(1, 0)];
}

technique11 softmax {
	pass pre_x0		{ SetComputeShader(CompileShader(cs_5_0, CS_softmaxRowPre0()));	}
	pass pre_y0		{ SetComputeShader(CompileShader(cs_5_0, CS_softmaxColPre0()));	}
	pass pre_2d0	{ SetComputeShader(CompileShader(cs_5_0, CS_softmax2dPre0()));	}
	pass pre_x1		{ SetComputeShader(CompileShader(cs_5_0, CS_softmaxRowPre1())); }
	pass pre_y1		{ SetComputeShader(CompileShader(cs_5_0, CS_softmaxColPre1())); }
	pass pre_2d1	{ SetComputeShader(CompileShader(cs_5_0, CS_softmax2dPre1())); }
	pass final		{ SetComputeShader(CompileShader(cs_5_0, CS_softmaxFinal())); }

	pass p6			{ SetComputeShader(CompileShader(cs_5_0, CS_softmax6()));	}
	pass p7			{ SetComputeShader(CompileShader(cs_5_0, CS_softmax7()));	}
	pass p8			{ SetComputeShader(CompileShader(cs_5_0, CS_softmax8()));	}
	pass p9			{ SetComputeShader(CompileShader(cs_5_0, CS_softmax9()));	}
	pass p10		{ SetComputeShader(CompileShader(cs_5_0, CS_softmax10()));	}
}

//-----------------------------------------------------------------------------
// LogSoftMax
//-----------------------------------------------------------------------------

// one pass (if small enough)
void logsoftmax_all(uint2 uv, uint i, uint n) {
	float4 v = Srce[uv];
	float4 M = for_group(get_max(v, i, n), i);
	float4 e = exp((v - M) * beta);
	float4 S = get_sum(e, i, n);
	Dest[uv] = v - M - log(S);
}

[numthreads(1 <<  6, 1, 1)] void CS_logsoftmax6 (uint3 id : SV_DispatchThreadID, uint i : SV_GroupIndex) { logsoftmax_all(id.xy, i,  6); }
[numthreads(1 <<  7, 1, 1)] void CS_logsoftmax7 (uint3 id : SV_DispatchThreadID, uint i : SV_GroupIndex) { logsoftmax_all(id.xy, i,  7); }
[numthreads(1 <<  8, 1, 1)] void CS_logsoftmax8 (uint3 id : SV_DispatchThreadID, uint i : SV_GroupIndex) { logsoftmax_all(id.xy, i,  8); }
[numthreads(1 <<  9, 1, 1)] void CS_logsoftmax9 (uint3 id : SV_DispatchThreadID, uint i : SV_GroupIndex) { logsoftmax_all(id.xy, i,  9); }
[numthreads(1 << 10, 1, 1)] void CS_logsoftmax10(uint3 id : SV_DispatchThreadID, uint i : SV_GroupIndex) { logsoftmax_all(id.xy, i, 10); }

[numthreads(1, 1, 1)] void CS_logsoftmaxFinal(uint3 id : SV_DispatchThreadID) {
	Dest[id.xy] = Srce[id.xy] - MaxesSums[uint2(0, 0)] - log(MaxesSums[uint2(1, 0)]);
}

technique11 logsoftmax{
	pass final	{ SetComputeShader(CompileShader(cs_5_0, CS_logsoftmaxFinal())); }
	pass p6		{ SetComputeShader(CompileShader(cs_5_0, CS_logsoftmax6())); }
	pass p7		{ SetComputeShader(CompileShader(cs_5_0, CS_logsoftmax7())); }
	pass p8		{ SetComputeShader(CompileShader(cs_5_0, CS_logsoftmax8())); }
	pass p9		{ SetComputeShader(CompileShader(cs_5_0, CS_logsoftmax9())); }
	pass p10	{ SetComputeShader(CompileShader(cs_5_0, CS_logsoftmax10())); }
}

//-----------------------------------------------------------------------------
// Convolve
//-----------------------------------------------------------------------------

[numthreads(1, 1, 1)]
void CS_convolve(uint3 id : SV_DispatchThreadID) {
	float4	t = 0;
	int2   uv = id.xy + size.xy;
	for (int y = 0; y < size.w; y++) {
		for (int x = 0; x < size.z; x++)
			t += Srce[uv + int2(x, y)]  * filter[y * size.z + x].x;
	}
	Dest[id.xy] = clamp(t, minmax.x, minmax.y);
}

technique11 convolve {
	pass p0 {
		SetComputeShader(CompileShader(cs_5_0, CS_convolve()));
	}
}


//-----------------------------------------------------------------------------
// Average
//-----------------------------------------------------------------------------

[numthreads(1, 1, 1)] void CS_average(uint3 id : SV_DispatchThreadID) {
	uint2	dim;
	Srce.GetDimensions(dim.x, dim.y);

	int2  uv	= id.xy * size.zw + size.xy;
	int2  uv0	= max(uv, 0), uv1 = min(uv + size.zw, dim);
	float4 t	= 0;

	for (uint y = uv0.y; y < uv1.y; y++) {
		for (uint x = uv0.x; x < uv1.x; x++)
			t += Srce[uint2(x, y)];
	}
	dim			= uv1 - uv0;
	Dest[id.xy] = t / (dim.x * dim.y);
}

technique11 average {
	pass p0 { SetComputeShader(CompileShader(cs_5_0, CS_average())); }
}

//-----------------------------------------------------------------------------
// FFT
//-----------------------------------------------------------------------------

cbuffer fft_consts : register(b0) {
	uint	log2block;
	uint	log2size;
	int2	offset;
	bool	want_imag;
};

uint2 ButterflyIndices(uint log2block, uint x, out float2 weights) {
	int		block	= 1 << log2block;
	uint	off		= x & (block - 1);
	sincos(-PI * off / (float)block, weights.y, weights.x);
	return uint2(0, block) + ((x - off) * 2 + off);
}

uint4 ButterflyIndices(uint log2size, uint log2block, uint x, out float2 weights) {
	uint2	indices = ButterflyIndices(log2block, x, weights);
	return uint4(log2block == 0 ? reversebits(indices, log2size) : indices, indices);
}

float3 fix_weights(float2 weights, bool inv) {
	return inv ? float3(weights.x, -weights.y, 1) * 0.5 : float3(weights, 1);
}

void Butterfly(float4 r1, float4 i1, float4 r2, float4 i2, float3 weights, out float4 out_r1, out float4 out_i1, out float4 out_r2, out float4 out_i2) {
	out_r1 = weights.z * r1 + weights.x * r2 - weights.y * i2;
	out_i1 = weights.z * i1 + weights.y * r2 + weights.x * i2;
	out_r2 = weights.z * r1 - weights.x * r2 + weights.y * i2;
	out_i2 = weights.z * i1 - weights.y * r2 - weights.x * i2;
}

void Butterfly(float4 r1, float4 r2, float4 i2, float3 weights, out float4 out_r1, out float4 out_r2) {
	out_r1 = weights.z * r1 + weights.x * r2 - weights.y * i2;
	out_r2 = weights.z * r1 - weights.x * r2 + weights.y * i2;
}

//-----------------------------------------------------------------------------
// UAV version
//-----------------------------------------------------------------------------

void ButterflyUAV(uint4 in_uv, uint4 out_uv, float3 weights) {
	Butterfly(
		Srce[in_uv.xz],		SrceI[in_uv.xz],
		Srce[in_uv.yw],		SrceI[in_uv.yw],
		weights,
		Dest[out_uv.xz],	DestI[out_uv.xz],
		Dest[out_uv.yw],	DestI[out_uv.yw]
	);
}

[numthreads(16, 16, 1)]
void ButterflyRow(uint3 pos : SV_DispatchThreadID) {
	float2	weights;
	uint4	indices = ButterflyIndices(log2size, log2block, pos.x, weights);
	ButterflyUAV(uint4(indices.xy, pos.yy) + offset.xxyy, uint4(indices.zw, pos.yy), fix_weights(weights, false));
}

[numthreads(16, 16, 1)]
void ButterflyInvRow(uint3 pos : SV_DispatchThreadID) {
	float2	weights;
	uint4	indices = ButterflyIndices(log2size, log2block, pos.x, weights);
	ButterflyUAV(uint4(indices.xy, pos.yy) + offset.xxyy, uint4(indices.zw, pos.yy), fix_weights(weights, true));
}

[numthreads(16, 16, 1)]
void ButterflyCol(uint3 pos : SV_DispatchThreadID) {
	float2	weights;
	uint4	indices = ButterflyIndices(log2size, log2block, pos.x, weights);
	ButterflyUAV(uint4(pos.yy, indices.xy) + offset.xxyy, uint4(pos.yy, indices.zw), fix_weights(weights, false));
}

[numthreads(16, 16, 1)]
void ButterflyInvCol(uint3 pos : SV_DispatchThreadID) {
	float2	weights;
	uint4	indices = ButterflyIndices(log2size, log2block, pos.x, weights);
	ButterflyUAV(uint4(pos.yy, indices.xy) + offset.xxyy, uint4(pos.yy, indices.zw), fix_weights(weights, true));
}


technique11 butterfly_uav2 {
	pass p0 { SetComputeShader(CompileShader(cs_5_0, ButterflyRow())); }
	pass p1 { SetComputeShader(CompileShader(cs_5_0, ButterflyCol())); }
	pass p2 { SetComputeShader(CompileShader(cs_5_0, ButterflyInvRow())); }
	pass p3 { SetComputeShader(CompileShader(cs_5_0, ButterflyInvCol())); }
}

//-----------------------------------------------------------------------------
// SLM version
//-----------------------------------------------------------------------------

#define MAX_LOG2_WIDTH	9
#define MAX_WIDTH		(1<<MAX_LOG2_WIDTH)

groupshared float4 slm[2][MAX_WIDTH];

// Load entire row or column into scratch array
void SLM_read(uint x, uint2 uv) {
	slm[0][x] = Srce [uv];
	slm[1][x] = SrceI[uv];
}

void SLM_FFT(bool inv, uint x, uint n) {
	for (uint i = 0; i < n; i++) {
		GroupMemoryBarrierWithGroupSync();

		float2	weights;
		uint2	indices	= ButterflyIndices(i, x, weights);
		Butterfly(
			slm[0][indices.x],		slm[1][indices.x],
			slm[0][indices.y],		slm[1][indices.y],
			fix_weights(weights, inv),
			slm[0][indices.x],		slm[1][indices.x],
			slm[0][indices.y],		slm[1][indices.y]
		);
	}
}

void ButterflySLMRow0(uint3 pos, uint log2size, bool inv, bool last_imag) {
	uint	x		= pos.x & (MAX_WIDTH / 2 - 1);
	uint	last	= min(log2size, MAX_LOG2_WIDTH) - 1;

	SLM_read(x * 2 + 0, uint2(reversebits(pos.x * 2 + 0, log2size), pos.y) + offset);
	SLM_read(x * 2 + 1, uint2(reversebits(pos.x * 2 + 1, log2size), pos.y) + offset);
	SLM_FFT(inv, x, last);

	GroupMemoryBarrierWithGroupSync();
	float2	weights;
	uint2	indices	= ButterflyIndices(last, x, weights);
	uint3	uvout	= uint3(block_split(pos.x, last), pos.y);
	if (last_imag)
		Butterfly(
			slm[0][indices.x],	slm[1][indices.x],
			slm[0][indices.y],	slm[1][indices.y],
			fix_weights(weights, inv),
			Dest[uvout.xz],		DestI[uvout.xz],
			Dest[uvout.yz],		DestI[uvout.yz]
		);
	else
		Butterfly(
			slm[0][indices.x],
			slm[0][indices.y],	slm[1][indices.y],
			fix_weights(weights, inv),
			Dest[uvout.xz],		Dest[uvout.yz]
		);
}

void ButterflySLMCol0(uint3 pos, uint log2size, bool inv, bool last_imag) {
	uint	x		= pos.x & (MAX_WIDTH / 2 - 1);
	uint	last	= min(log2size, MAX_LOG2_WIDTH) - 1;

	SLM_read(x * 2 + 0, uint2(pos.y, reversebits(pos.x * 2 + 0, log2size)) + offset);
	SLM_read(x * 2 + 1, uint2(pos.y, reversebits(pos.x * 2 + 1, log2size)) + offset);
	SLM_FFT(inv, x, last);

	GroupMemoryBarrierWithGroupSync();
	float2	weights;
	uint2	indices	= ButterflyIndices(last, x, weights);
	uint3	uvout	= uint3(block_split(pos.x, last), pos.y);
	if (last_imag)
		Butterfly(
			slm[0][indices.x],	slm[1][indices.x],
			slm[0][indices.y],	slm[1][indices.y],
			fix_weights(weights, inv),
			Dest[uvout.zx],		DestI[uvout.zx],
			Dest[uvout.zy],		DestI[uvout.zy]
		);
	else
		Butterfly(
			slm[0][indices.x],
			slm[0][indices.y],	slm[1][indices.y],
			fix_weights(weights, inv),
			Dest[uvout.zx],		Dest[uvout.zy]
		);
}

[numthreads(MAX_WIDTH/2, 1, 1 )]
void ButterflySLMRow(uint3 pos : SV_DispatchThreadID) {
	ButterflySLMRow0(pos, log2size, false, want_imag);
}

[numthreads(MAX_WIDTH/2, 1, 1 )]
void ButterflySLMInvRow(uint3 pos : SV_DispatchThreadID) {
	ButterflySLMRow0(pos, log2size, true, want_imag);
}

[numthreads(MAX_WIDTH/2, 1, 1)]
void ButterflySLMCol(uint3 pos : SV_DispatchThreadID) {
	ButterflySLMCol0(pos, log2size, false, want_imag);
}

[numthreads(MAX_WIDTH/2, 1, 1)]
void ButterflySLMInvCol(uint3 pos : SV_DispatchThreadID) {
	ButterflySLMCol0(pos, log2size, true, want_imag);
}

technique11 butterfly_slm2 {
	pass p0 { SetComputeShader(CompileShader(cs_5_0, ButterflySLMRow())); }
	pass p1 { SetComputeShader(CompileShader(cs_5_0, ButterflySLMCol())); }
	pass p2 { SetComputeShader(CompileShader(cs_5_0, ButterflySLMInvRow())); }
	pass p3 { SetComputeShader(CompileShader(cs_5_0, ButterflySLMInvCol())); }
}
