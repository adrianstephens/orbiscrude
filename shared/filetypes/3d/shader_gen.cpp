#include "shader_gen.h"
#include "bitmap/bitmap.h"

using namespace iso;

#undef auto_new
#define auto_new(T) ref_ptr<T>::make

ISO::Browser2 GetProperties(const ISO::Browser2 &b);

class UnskinnedVerts : public ShaderSource {
public:
	UnskinnedVerts()	: ShaderSource(this)	{}
	void	WriteShader(ShaderWriter& shader, ShaderWriter::CONTEXT context) {
		if (context == ShaderWriter::CTX_WORLDMAT)
			shader << "(float4x3)world";
	}
};

class SkinnedVerts : public ShaderSource {
public:
	SkinnedVerts()	: ShaderSource(this)	{}
	void	WriteShader(ShaderWriter& shader, ShaderWriter::CONTEXT context) {
		switch (context) {
			case ShaderWriter::CTX_GLOBALS:
				shader << "typedef row_major float4x4 Bone;\n";
				shader << "StructuredBuffer<Bone> bones;\n";
				break;
			case ShaderWriter::CTX_WORLDMAT:
				shader << "(float4x3)mul(to4x4((float4x3)(bones[indices.x] * weights.x + bones[indices.y] * weights.y + bones[indices.z] * weights.z + bones[indices.w] * weights.w)), world)";
				break;
			default:
				break;
		}
	}
};

ISO_DEFCOMP0(UnskinnedVerts);
ISO_DEFCOMP0(SkinnedVerts);

//-----------------------------------------------------------------------------
//	MaterialMaker
//-----------------------------------------------------------------------------

ShaderSource *MaterialMaker::GetVertSource()	{
	return flags & SKIN ? (ShaderSource*)new SkinnedVerts : (ShaderSource*)new UnskinnedVerts;
}

void MaterialMaker::Generate(tag2 id, ShadingModel *sm) {
	if (!technique) {
		technique = ISO_ptr<void>(ISO_ptr<inline_technique>(0));
		ISO_ptr<inline_pass>	pass(id);
		technique->Append(pass);

		ShaderWriter	psb(ShaderWriter::PS);
		sm->WriteShader(psb, *this);
		pass->Append(ISO_ptr<string>("PS", move(psb)));

		ShaderWriter	vsb(ShaderWriter::VS);
		sm->WriteShader(vsb, *this);
		pass->Append(ISO_ptr<string>("VS", move(vsb)));
	}
}

//-----------------------------------------------------------------------------
//	ShadingModel
//-----------------------------------------------------------------------------

void ShadingModel::OpenMain(ShaderWriter& shader, MaterialMaker &maker) {
	shader.Open("float4 main");
	bool	shadow = !!(maker.flags & maker.SHADOW);
	if (shader.stage == ShaderWriter::PS) {
		shader.Arg() << "float4 screenpos : POSITION_OUT, float3 worldpos : POSITION, float3 normal : NORMAL";

		int	texcoord = 0;
		if (maker.flags & maker.UVDEFAULT)
			shader.Arg() << "float2 uv : TEXCOORD" << texcoord++;
		for (auto &i : maker.inputs)
			shader.Arg() << "float2 " << i << " : TEXCOORD" << texcoord++;
		if (maker.tangent_uvs)
			shader.Arg() << "float3 tangent : TANGENT";
		if (shadow)
			shader.Arg() << "float3 shadow_uv : SHADOWCOORD";

		shader.Arg() << "float4 ambient : AMBIENT";
		shader.Close();

		shader.Open(" : OUTPUT0 ", '{');

		if (shadow)
			shader.Stmt() << "float shadow = GetShadow(shadow_uv)";

	} else {
		shader.Arg() << "float3 position : POSITION, out float3 out_position : POSITION";
		shader.Arg() << "float3 normal : NORMAL, out float3 out_normal : NORMAL";

		if (maker.flags & maker.SKIN)
			shader.Arg() << "uint4 indices : BLENDINDICES, float4 weights : BLENDWEIGHT";

		int	texcoord = 0;
		if (maker.flags & maker.UVDEFAULT) {
			shader.Arg() << "float2 uv : TEXCOORD" << texcoord << ", out float2 out_uv : TEXCOORD" << texcoord;
			++texcoord;
		}
		for (auto &i : maker.inputs) {
			shader.Arg() << "float2 " << i << " : TEXCOORD" << texcoord << ", out float2 out_" << i << " : TEXCOORD" << texcoord;
			++texcoord;
		}
		if (maker.tangent_uvs)
			shader.Arg() << "float4 tangent : TANGENT, out float3 out_tangent : TANGENT";
		if (shadow)
			shader.Arg() << "out float3 out_shadow_uv : SHADOWCOORD";

		shader.Arg() << "out float4 out_ambient : AMBIENT";

		shader.Close();

		shader.Open(" : POSITION_OUT ", '{');
	}
}

void ShadingModel::WriteVS(ShaderWriter& shader, MaterialMaker &maker, ShaderSource *vert_source) {
	shader << "#include \"shader_gen.fxh\"\n\n";

	vert_source->WriteShader(shader, ShaderWriter::CTX_GLOBALS);

	OpenMain(shader, maker);

	shader.Stmt() << "float4x3 world2 = ";
	vert_source->WriteShader(shader, ShaderWriter::CTX_WORLDMAT);

	shader.Stmt() << "out_position	= mul(float4(position, 1), world2)";
	shader.Stmt() << "out_normal	= normalize(mul(normal, (float3x3)world2))";

	if (maker.flags & maker.UVDEFAULT)
		shader.Stmt() << "out_uv	= uv";

	for (auto &i : maker.inputs)
		shader.Stmt() << "out_" << i << "\t= " << i;

	if (maker.tangent_uvs)
		shader.Stmt() << "out_tangent	= mul(tangent.xyz, (float3x3)world2)";

	if (maker.flags & maker.SHADOW)
		shader.Stmt() << "out_shadow_uv	= mul(float4(out_position, 1), shadow_global_proj).xyz";

	shader.Stmt() << "out_ambient	= DiffuseLight(out_position, out_normal)";
	shader.Stmt() << "return mul(float4(out_position, 1.0), ViewProj())";
	shader.Close();
}

//-----------------------------------------------------------------------------
//	COLOUR SOURCES
//-----------------------------------------------------------------------------

class CannedSource : public ColourSource {
	string	s;
public:
	CannedSource(const char *_s)	: ColourSource(this), s(_s)	{}
	void	WriteShader(ShaderWriter& shader, ShaderWriter::CONTEXT context) {
		if (context != ShaderWriter::CTX_GLOBALS)
			shader << s;
	}
};

ISO_DEFCOMP0(CannedSource);

class SeparateTransparency : public ColourSource {
	ref_ptr<ColourSource> rgb, a;
public:
	SeparateTransparency(ref_ptr<ColourSource> _rgb, ref_ptr<ColourSource> _a) : ColourSource(this), rgb(_rgb), a(_a)	{}
	void	WriteShader(ShaderWriter& shader, ShaderWriter::CONTEXT context) {
		switch (context) {
			case ShaderWriter::CTX_GLOBALS:
				rgb->WriteShader(shader, context);
				a->WriteShader(shader, context);
				break;
			case ShaderWriter::CTX_COLOUR:
				shader << "float4(";
				rgb->WriteShader(shader, context);
				shader << ".rgb, ";
				a->WriteShader(shader, ShaderWriter::CTX_MONO);
				shader << ")";
				break;
			case ShaderWriter::CTX_MONO:
				rgb->WriteShader(shader, ShaderWriter::CTX_MONO);
				break;
		}
	}
};

ISO_DEFCOMP0(SeparateTransparency);

class ColourSourceMulS : public ColourSource {
	ref_ptr<ColourSource> v, s;
public:
	ColourSourceMulS(ref_ptr<ColourSource> _v, ref_ptr<ColourSource> _s) : ColourSource(this), v(_v), s(_s)	{}
	void	WriteShader(ShaderWriter& shader, ShaderWriter::CONTEXT context) {
		switch (context) {
			case ShaderWriter::CTX_GLOBALS:
				v->WriteShader(shader, context);
				s->WriteShader(shader, context);
				break;
			case ShaderWriter::CTX_COLOUR:
			case ShaderWriter::CTX_MONO:
				v->WriteShader(shader, context);
				shader << " * ";
				s->WriteShader(shader, ShaderWriter::CTX_MONO);
				break;
		}
	}
};

ISO_DEFCOMP0(ColourSourceMulS);

void TextureMap::WriteShader(ShaderWriter& shader, ShaderWriter::CONTEXT context) {
#if 1
	switch (context) {
		case ShaderWriter::CTX_GLOBALS:
			shader << "sampler_def2D(" << name << ",\n";
			shader << "\tFILTER_MIN_MAG_MIP_LINEAR;\n";
			shader << "\tADDRESSU = WRAP;\n";
			shader << "\tADDRESSV = WRAP;\n";
			shader << ");\n";
			break;
		case ShaderWriter::CTX_COLOUR:
			shader << "tex2D(" << name << ", " << uv_set << ")";
			break;
		case ShaderWriter::CTX_MONO:
			shader << "tex2D(" << name << ", " << uv_set << ").x";
			break;
	}
#else
	switch (context) {
		case ShaderWriter::CTX_GLOBALS:
			shader << "Texture2D " << name << "_t;\n";
			shader << "SamplerState " << name << "_s;\n";
			break;
		case ShaderWriter::CTX_COLOUR:
			shader << name << "_t.Sample(" << name << "_s, " << uv_set << ")";
			break;
		case ShaderWriter::CTX_MONO:
			shader << name << "_t.Sample(" << name << "_s, " << uv_set << ").x";
			break;
	}
#endif
}

//-----------------------------------------------------------------------------
//	composite texmap
//-----------------------------------------------------------------------------

struct CompositeParams {
	enum {IDA = 0x0000280, IDB = 0};

	dynamic_array<bool>				mapEnabled;
	dynamic_array<bool>				maskEnabled;
	dynamic_array<int>				blendMode;
	dynamic_array<string>			layerName;
	dynamic_array<float>			opacity;
	dynamic_array<ref_ptr<ColourSource>>	mapList;
	dynamic_array<ref_ptr<ColourSource>>	mask;

	CompositeParams(MaterialMaker &maker, ISO::Browser b) {
		read_props(maker, b, F(mapEnabled), F(maskEnabled), F(blendMode), F(layerName), F(opacity), F(mapList), F(mask));
	}
};

MAXColourSourceCreatorT<CompositeParams> composite_creator;

template<> void MAXColourSourceCreatorT<CompositeParams>::Source::WriteShader(ShaderWriter& shader, ShaderWriter::CONTEXT context) {
	int	n = mapEnabled.size32();

	if (context == ShaderWriter::CTX_GLOBALS) {
		for (int i = 0; i < n; i++) {
			if (mapEnabled[i] && mapList[i])
				mapList[i]->WriteShader(shader, context);
			if (maskEnabled[i] && mask[i])
				mask[i]->WriteShader(shader, context);
		}
	} else {
		bool	first = true;
		for (int i = 0; i < n; i++) {
			if (mapEnabled[i] && mapList[i]) {
				if (first) {
					first = false;
				} else {
					shader << " + ";
				}
				mapList[i]->WriteShader(shader, context);
				if (maskEnabled[i] && mask[i]) {
					shader << " * ";
					mask[i]->WriteShader(shader, ShaderWriter::CTX_MONO);
				}
			}
		}
		if (first)
			SolidColour(1).WriteShader(shader, context);
	}
}
ISO_DEFCOMP0(MAXColourSourceCreatorT<CompositeParams>::Source);

//-----------------------------------------------------------------------------
//	mix material
//-----------------------------------------------------------------------------

struct MixParams {
	enum {IDA = 0x0000230, IDB = 0};
	float			mixAmount;
	float			lower;
	float			upper;
	bool			useCurve;
	colour			color1;
	colour			color2;
	ref_ptr<ColourSource>	map1;
	ref_ptr<ColourSource>	map2;
	ref_ptr<ColourSource>	mask;
	bool			map1Enabled;
	bool			map2Enabled;
	bool			maskEnabled;
	TextureOutput	*output;

	MixParams(MaterialMaker &maker, ISO::Browser b) {
		read_props(maker, b, F(mixAmount), F(lower), F(upper), F(useCurve), F(color1), F(color2), F(map1), F(map2), F(mask), F(map1Enabled), F(map2Enabled), F(maskEnabled), F(output));
	}
};

MAXColourSourceCreatorT<MixParams> mix_creator;

template<> void MAXColourSourceCreatorT<MixParams>::Source::WriteShader(ShaderWriter& shader, ShaderWriter::CONTEXT context) {
	if (context == ShaderWriter::CTX_GLOBALS) {
		if (map1Enabled && map1)
			map1->WriteShader(shader, context);
		if (map2Enabled && map2)
			map2->WriteShader(shader, context);
		if (maskEnabled && mask)
			map2->WriteShader(shader, context);
	} else {
		shader.Open("lerp");
		if (map1Enabled && map1)
			map1->WriteShader(shader.Arg(), context);
		else
			shader.Arg().Write(context, color1);

		if (map2Enabled && map2)
			map2->WriteShader(shader.Arg(), context);
		else
			shader.Arg().Write(context, color2);

		if (useCurve)
			shader.Arg().Open("smoothstep").Arg() << lower << ", " << upper;

		if (maskEnabled && mask)
			map2->WriteShader(shader.Arg(), ShaderWriter::CTX_MONO);
		else
			shader.Arg() << mixAmount;

		if (useCurve)
			shader.Close();
		shader.Close();
	}
}
ISO_DEFCOMP0(MAXColourSourceCreatorT<MixParams>::Source);

//-----------------------------------------------------------------------------
//	noise material
//-----------------------------------------------------------------------------
struct NoiseParams {
	enum { IDA = 0x0000234, IDB = 0 };
	enum TYPE {
		REGULAR,
		FRACTAL,
		TURB,
	};
	colour			color1;
	colour			color2;
	ref_ptr<ColourSource>	map1;
	ref_ptr<ColourSource>	map2;
	bool			map1Enabled;
	bool			map2Enabled;
	distance_unit	size;
	float			phase;
	float			levels;
	float			thresholdLow;
	float			thresholdHigh;
	int				type;
	ref_ptr<PositionSource>	coords;
	TextureOutput	*output;

	NoiseParams(MaterialMaker &maker, ISO::Browser b) {
		read_props(maker, b
			, F(color1), F(color2), F(map1), F(map2), F(map1Enabled), F(map2Enabled)
			, F(size), F(phase), F(levels), F(thresholdLow), F(thresholdHigh), F(type), F(coords), F(output)
		);
	}
};

MAXColourSourceCreatorT<NoiseParams> noise_creator;

template<> void MAXColourSourceCreatorT<NoiseParams>::Source::WriteShader(ShaderWriter& shader, ShaderWriter::CONTEXT context) {
	if (context == ShaderWriter::CTX_GLOBALS) {
		if (map1Enabled && map1)
			map1->WriteShader(shader, context);
		if (map2Enabled && map2)
			map2->WriteShader(shader, context);
	} else {
		shader.Open("lerp");
		if (map1Enabled && map1)
			map1->WriteShader(shader.Arg(), context);
		else
			shader.Arg().Write(context, color1);

		if (map2Enabled && map2)
			map2->WriteShader(shader.Arg(), context);
		else
			shader.Arg().Write(context, color2);

		if (thresholdLow < thresholdHigh)
			shader.Arg().Open("smooth_threshold");

		static const char *fns[] = {
			"noise_regular",
			"noise_fractal",
			"noise_turb",
		};
		shader.Arg() << fns[type] << "(float4(worldpos, " << phase << ") / " << size << ", " << levels << ")";

		if (thresholdLow < thresholdHigh) {
			shader.Arg() << thresholdLow << ", " << thresholdHigh << ", smooth_width(worldpos)";
			shader.Close();
		}
		shader.Close();
	}
}
ISO_DEFCOMP0(MAXColourSourceCreatorT<NoiseParams>::Source);

//-----------------------------------------------------------------------------
//	Falloff Texmap
//-----------------------------------------------------------------------------

struct FalloffParams {
	enum { IDA = 0x6ec3730c, IDB = 0 };
	enum TYPE {
		TOWARDS_AWAY,
		PERPENDICULAR_PARALLEL,
		FRESNEL,
		SHADOW_LIGHT,
		DISTANCE_BLEND,
	};
	enum DIRECTION {
		CAMERA_Z,
		CAMERA_X,
		CAMERA_Y,
		OBJECT,
		LOCAL_X,
		LOCAL_Y,
		LOCAL_Z,
		WORLD_X,
		WORLD_Y,
		WORLD_Z,
	};

	colour			color1;
	float			map1Amount;
	ref_ptr<ColourSource>	map1;
	bool			map1On;
	colour			color2;
	float			map2Amount;
	ref_ptr<ColourSource>	map2;
	bool			map2On;
	int				type;
	int				direction;
	//Reference		node
	bool			mtlIOROverride;
	float			ior;
	bool			extrapolateOn;
	distance_unit	nearDistance;
	distance_unit	farDistance;

	FalloffParams(MaterialMaker &maker, ISO::Browser b) {
		read_props(maker, b
			, F(color1), F(map1Amount), F(map1), F(map1On), F(color2), F(map2Amount), F(map2)
			, F(map2On), F(type), F(direction), /*F(node),*/ F(mtlIOROverride), F(ior), F(extrapolateOn), F(nearDistance), F(farDistance)
		);

		if (type == SHADOW_LIGHT)
			maker.flags |= maker.SHADOW;
	}
};

MAXColourSourceCreatorT<FalloffParams> falloff_creator;

template<> void MAXColourSourceCreatorT<FalloffParams>::Source::WriteShader(ShaderWriter& shader, ShaderWriter::CONTEXT context) {
	if (context == ShaderWriter::CTX_GLOBALS) {
		if (map1On && map1)
			map1->WriteShader(shader, context);
		if (map2On && map2)
			map2->WriteShader(shader, context);
	} else {
		shader.Open("lerp");
		if (map1On && map1)
			map1->WriteShader(shader.Arg(), context);
		else
			shader.Arg().Write(context, color1);
		shader << " * " << map1Amount / 100;

		if (map2On && map2)
			map2->WriteShader(shader.Arg(), context);
		else
			shader.Arg().Write(context, color2);
		shader << " * " << map2Amount / 100;

		shader.Arg().Open(extrapolateOn ? "interp" : "threshold");

		const char *mat = 0, *axis = 0;
		switch (direction) {
			case CAMERA_Z:	case CAMERA_X:	case CAMERA_Y:	mat = "view"; break;
			case LOCAL_X:	case LOCAL_Y:	case LOCAL_Z:	mat = "iworld"; break;
		}
		switch (direction) {
			case CAMERA_X:	case LOCAL_X:	case WORLD_X:	axis = "x"; break;
			case CAMERA_Y:	case LOCAL_Y:	case WORLD_Y:	axis = "y"; break;
			case CAMERA_Z:	case LOCAL_Z:	case WORLD_Z:	axis = "z"; break;
		}

		shader.Arg();
		switch (type) {
			case SHADOW_LIGHT:
				shader << "shadow";
				break;
			case DISTANCE_BLEND:
				if (mat)
					shader << "mul(worldPos, " << mat << ", view)";
				else
					shader << "worldPos";
				shader << '.' << axis;
				break;
			case TOWARDS_AWAY:
				if (mat)
					shader << "mul(normal, " << mat << ", view)";
				else
					shader << "normal";
				shader << '.' << axis;
				break;
			case PERPENDICULAR_PARALLEL:
				shader << "abs(";
				if (mat)
					shader << "mul(normal, " << mat << ", view)";
				else
					shader << "normal";
				shader << '.' << axis << ')';
				break;
			case FRESNEL:
				//TBD
				break;
		}

		shader.Arg() << nearDistance << ", " << farDistance;
		shader.Close();

		shader.Close();
	}
}
ISO_DEFCOMP0(MAXColourSourceCreatorT<FalloffParams>::Source);

//-----------------------------------------------------------------------------
//	Gradient
//-----------------------------------------------------------------------------

struct GradientParams {
	enum {IDA = 0x270, IDB = 0};
	colour			color1;
	colour			color2;
	colour			color3;
	ref_ptr<ColourSource>	map1;
	ref_ptr<ColourSource>	map2;
	ref_ptr<ColourSource>	map3;
	int				map1Enabled;
	int				map2Enabled;
	int				map3Enabled;
	float			color2Pos;
	int				gradientType;
	float			noiseAmount;
	int				noiseType;
	float			noiseSize;
	float			noisePhase;
	float			noiseLevels;
	float			noiseThresholdLow;
	float			noiseThresholdHigh;
	float			noiseThresholdSMooth;
	ref_ptr<PositionSource>	coords;
	TextureOutput	*output;

	GradientParams(MaterialMaker &maker, ISO::Browser b) {
		read_props(maker, b
			, F(color1), F(color2), F(color3), F(map1), F(map2), F(map3), F(map1Enabled), F(map2Enabled), F(map3Enabled)
			, F(color2Pos), F(gradientType)
			, F(noiseAmount), F(noiseType), F(noiseSize), F(noisePhase), F(noiseLevels), F(noiseThresholdLow), F(noiseThresholdHigh), F(noiseThresholdSMooth)
			, F(coords), F(output)
		);
	}
};
MAXColourSourceCreatorT<GradientParams> gradient_creator;
template<> void MAXColourSourceCreatorT<GradientParams>::Source::WriteShader(ShaderWriter& shader, ShaderWriter::CONTEXT context) {
}

ISO_DEFCOMP0(MAXColourSourceCreatorT<GradientParams>::Source);

//-----------------------------------------------------------------------------
//	Bump
//-----------------------------------------------------------------------------

struct BumpParams0 {
	enum {IDA = 2004031007, IDB = 1963415618};
	float			Multiplier;
	ref_ptr<ColourSource>	Map;

	BumpParams0(MaterialMaker &maker, ISO::Browser b) {
		read_props(maker, b, F(Multiplier), F(Map));
	}
};
struct BumpParams : BumpParams0 {
	BumpParams(MaterialMaker &maker, ISO::Browser b) : BumpParams0(maker, b["max_Bump Parameters"]) {}
};

MAXColourSourceCreatorT<BumpParams> bump_creator;
template<> void MAXColourSourceCreatorT<BumpParams>::Source::WriteShader(ShaderWriter& shader, ShaderWriter::CONTEXT context) {
	if (Map)
		Map->WriteShader(shader, context);
	if (context != ShaderWriter::CTX_GLOBALS) {
		if (Map)
			shader << " * " << Multiplier;
		else
			shader << Multiplier;
	}
}

ISO_DEFCOMP0(MAXColourSourceCreatorT<BumpParams>::Source);

//-----------------------------------------------------------------------------
//	MATERIALS
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
//	Phong Material
//-----------------------------------------------------------------------------

void Phong::WritePS(ShaderWriter& shader, MaterialMaker &maker, ColourSource *ambient, ColourSource *diffuse, ColourSource *specular, ColourSource *shininess, ColourSource *normalmap, bool bump_map) {
	shader << "#include \"shader_gen.fxh\"\n\n";

	ambient		->WriteShader(shader, ShaderWriter::CTX_GLOBALS);
	diffuse		->WriteShader(shader, ShaderWriter::CTX_GLOBALS);
	specular	->WriteShader(shader, ShaderWriter::CTX_GLOBALS);
	shininess	->WriteShader(shader, ShaderWriter::CTX_GLOBALS);
	if (normalmap)
		normalmap->WriteShader(shader, ShaderWriter::CTX_GLOBALS);

	OpenMain(shader, maker);
	shader.Stmt().Open("return phong");

	shader.Arg() << "worldpos";
	if (normalmap) {
		if (bump_map) {
			shader.Arg().Open("GetBump");
			shader.Arg() << "worldpos, normal";
			normalmap->WriteShader(shader.Arg(), ShaderWriter::CTX_MONO);
		} else {
			shader.Arg().Open("GetNormal");
			normalmap->WriteShader(shader.Arg(), ShaderWriter::CTX_COLOUR);
			shader.Arg() << "normal, tangent";
		}
		shader.Close();
	} else {
		shader.Arg() << "normal";
	}

	ambient		->WriteShader(shader.Arg(), ShaderWriter::CTX_COLOUR); shader << " * ambient";
	diffuse		->WriteShader(shader.Arg(), ShaderWriter::CTX_COLOUR);
	specular	->WriteShader(shader.Arg(), ShaderWriter::CTX_COLOUR);
	shininess	->WriteShader(shader.Arg(), ShaderWriter::CTX_MONO);
	shader.Arg() << (maker.flags & maker.SHADOW ? "shadow" : "1");

	shader.Close(ShaderWriter::MODE_STMT);
	shader.Close();
}

Phong::Phong(MaterialMaker &maker, ISO::Browser b, ShaderSource *_vert_source) : ShadingModel(this), vert_source(_vert_source) {
	read_props(maker, b
		, F(AmbientColor), F(DiffuseColor), F(DiffuseFactor), F(TransparencyFactor)
		, F(SpecularColor), F(SpecularFactor), F(ShininessExponent)
		, F(ReflectionFactor), F(NormalMap), F(Bump), F(BumpFactor)
	);
	if (NormalMap)
		maker.tangent_uvs	= maker.GetUVSet(b["NormalMap"]);
	maker.flags	|= maker.NORMALS;
}

void	Phong::WriteShader(ShaderWriter& shader, MaterialMaker &maker) {
	if (shader.stage == ShaderWriter::VS) {
		WriteVS(shader, maker, vert_source);
		return;
	}

	ColourSource*	DiffuseColor2 = !TransparencyFactor ? (ColourSource*)DiffuseColor.get() : auto_new(SeparateTransparency)(DiffuseColor.get(), TransparencyFactor);

	WritePS(shader, maker
		, AmbientColor.get()
		, !DiffuseFactor		? (ColourSource*)DiffuseColor.get()			: auto_new(ColourSourceMulS)(DiffuseColor.get(), DiffuseFactor)
		, !SpecularFactor		? (ColourSource*)SpecularColor.get()		: auto_new(ColourSourceMulS)(SpecularColor.get(), SpecularFactor)
		, ShininessExponent		? (ColourSource*)ShininessExponent.get()	: auto_new(SolidColour)(60)
		, Bump ? (!BumpFactor ? Bump.get() : auto_new(ColourSourceMulS)(Bump.get(), BumpFactor)) : NormalMap.get()
		, !!Bump
	);
}

//-----------------------------------------------------------------------------
//	Lambert Material
//-----------------------------------------------------------------------------

Lambert::Lambert(MaterialMaker &maker, ISO::Browser b, ShaderSource *_vert_source) : ShadingModel(this), vert_source(_vert_source) {
	read_props(maker, b
		, F(AmbientColor)
		, F(DiffuseColor)
		, F(TransparencyFactor)
		, F(DiffuseFactor)
	);
	maker.flags	|= maker.NORMALS;
}
void	Lambert::WriteShader(ShaderWriter& shader, MaterialMaker &maker) {
	if (shader.stage == ShaderWriter::VS) {
		WriteVS(shader, maker, vert_source);
		return;
	}

	ColourSource*	DiffuseColor2 = DiffuseColor;
	if (TransparencyFactor)
		DiffuseColor2 = auto_new(SeparateTransparency)(DiffuseColor2, TransparencyFactor);
	if (DiffuseFactor)
		DiffuseColor2 = auto_new(ColourSourceMulS)(DiffuseColor2, DiffuseFactor);

	shader << "#include \"shader_gen.fxh\"\n\n";

	AmbientColor	->WriteShader(shader, ShaderWriter::CTX_GLOBALS);
	DiffuseColor2	->WriteShader(shader, ShaderWriter::CTX_GLOBALS);

	OpenMain(shader, maker);

	shader.Stmt().Open("return lambert");
	shader.Arg() << "worldpos, normal";

	AmbientColor	->WriteShader(shader.Arg(), ShaderWriter::CTX_COLOUR); shader << " * ambient";
	DiffuseColor2	->WriteShader(shader.Arg(), ShaderWriter::CTX_COLOUR);
	shader.Arg() << (maker.flags & maker.SHADOW ? "shadow" : "1");

	shader.Close(ShaderWriter::MODE_STMT);
	shader.Close();
}


//-----------------------------------------------------------------------------
//	Matte/Shadow/Reflection
//-----------------------------------------------------------------------------

struct MatteShadowReflectionParams {
	enum {IDA = 0x7773160f, IDB = 0xaf2d513f};

	struct Surface {
		colour			background;
		float			opacity;
		ref_ptr<ColourSource>	bump;
		float			bump_amount;
		Surface(MaterialMaker &maker, ISO::Browser b) {
			read_props(maker, b, F(background), F(opacity), F(bump), F(bump_amount));
		}
	} surface;
	struct Shadows {
		bool		catch_shadows;
		colour		shadows;
		float		ambient_intensity;
		colour		ambient;
		bool		no_self_shadow;
		bool		use_dot_nl;
		float		colored_shadows;
		bool		UseLights;
		Shadows(MaterialMaker &maker, ISO::Browser b) {
			read_props(maker, b, F(catch_shadows), F(shadows), F(ambient_intensity), F(ambient), F(no_self_shadow), F(use_dot_nl), F(colored_shadows), F(UseLights));
		}
	} shadows;
	struct AO {
		bool		ao_on;
		int			ao_samples;
		float		ao_distance;
		colour		ao_dark;
		AO(MaterialMaker &maker, ISO::Browser b) {
			read_props(maker, b, F(ao_on), F(ao_samples), F(ao_distance), F(ao_dark));
		}
	} ao;
	struct Reflections {
		bool		catch_reflections;
		colour		refl_color;
		colour		refl_subtractive;
		float		refl_glossiness;
		int			refl_samples;
		float		refl_max_dist;
		float		refl_falloff;
		Reflections(MaterialMaker &maker, ISO::Browser b) {
			read_props(maker, b, F(catch_reflections), F(refl_color), F(refl_subtractive), F(refl_glossiness), F(refl_samples), F(refl_max_dist), F(refl_falloff));
		}
	} reflections;
	struct IndirectLighting {
		bool		catch_indirect;
		float		indirect;
		colour		additional_color;
		IndirectLighting(MaterialMaker &maker, ISO::Browser b) {
			read_props(maker, b, F(catch_indirect), F(indirect), F(additional_color));
		}
	} indirect_lighting;
	struct Lighting {
		bool		catch_illuminators;
		bool		UseLights;
		Lighting(MaterialMaker &maker, ISO::Browser b) {
			read_props(maker, b, F(catch_illuminators), F(UseLights));
		}
	} lighting;
	int			mode;

	MatteShadowReflectionParams(MaterialMaker &maker, ISO::Browser material, ISO::Browser b)
		: surface(maker, b["surface"])
		, shadows(maker, b["shadows"])
		, ao(maker, b["aO"])
		, reflections(maker, b["reflections"])
		, indirect_lighting(maker, b["indirect_lighting"])
		, lighting(maker, b["lighting"])
	{
		read_prop(maker, b, F(mode));
	}
};

MAXShadingModelCreatorT<MatteShadowReflectionParams> matteshadowreflection_creator;

template<> void MAXShadingModelCreatorT<MatteShadowReflectionParams>::Model::WriteShader(ShaderWriter& shader, MaterialMaker &maker) {
	maker.flags	|= maker.NORMALS;

	if (shader.stage == ShaderWriter::VS) {
		WriteVS(shader, maker, vert_source);
		return;
	}
#if 1
	return Phong::WritePS(shader, maker
		, auto_new(SolidColour)(shadows.ambient * shadows.ambient_intensity)
		, auto_new(SolidColour)(colour(surface.background.rgb, surface.opacity))
		, auto_new(SolidColour)(reflections.refl_color)
		, auto_new(SolidColour)(reflections.refl_glossiness)
		, surface.bump ? auto_new(ColourSourceMulS)(surface.bump, auto_new(SolidColour)(surface.bump_amount)) : nullptr
		, true
	);
#else
	shader << "#include \"shader_gen.fxh\"\n\n";
	OpenMain(shader, maker);
	shader.Stmt() << "return ";
	shader.Write(ShaderWriter::CTX_COLOUR, surface.background);
	shader << ";";
	shader.Close(ShaderWriter::MODE_STMT);
#endif
}

ISO_DEFCOMP0(MAXShadingModelCreatorT<MatteShadowReflectionParams>::Model);

//-----------------------------------------------------------------------------
//	Architecture & Design
//-----------------------------------------------------------------------------

struct ArchDesignParams0 {
	enum {IDA = 0x70b05735, IDB = 0x4a163654};

	colour			diff_color;
	float			diff_rough;
	float			diff_weight;

	colour			refl_color;
	float			refl_gloss;
	int				refl_samples;
	int				refl_interp;
	int				refl_hlonly;
	int				refl_metal;
	float			refl_weight;

	colour			refr_color;
	float			refr_gloss;
	int				refr_samples;
	int				refr_interp;
	float			refr_ior;
	float			refr_weight;
	int				refr_trans_on;
	colour			refr_transc;
	float			refr_transw;

	float			anisotropy;
	float			anisoangle;
	int				aniso_mode;
	int				aniso_channel;

	int				refl_func_fresnel;
	float			refl_func_low;
	float			refl_func_high;
	float			refl_func_curve;

	int				refl_falloff_on;
	float			refl_falloff_dist;
	int				refl_falloff_color_on;
	colour			refl_falloff_color;
	int				opts_refl_depth;
	float			refl_cutoff;

	int				refr_falloff_on;
	float			refr_falloff_dist;
	int				refr_falloff_color_on;
	colour			refr_falloff_color;
	int				opts_refr_depth;
	float			refr_cutoff;

	float			opts_indirect_multiplier;
	float			opts_fg_quality;
	int				inter_density;
	int				intr_refl_samples;
	int				intr_refl_ddist_on;
	float			intr_refl_ddist;
	int				intr_refr_samples;
	int				single_env_sample;

	int				opts_round_corners_on;
	float			opts_round_corners_radius;
	int				opts_round_corners_any_mtl;

	int				opts_ao_on;
	int				opts_ao_exact;
	int				opts_ao_use_global_ambient;
	int				opts_ao_samples;
	float			opts_ao_distance;
	colour			opts_ao_dark;
	colour			opts_ao_ambient;
	int				opts_ao_do_details;

	int				opts_no_area_hl;
	int				opts_1sided;
	int				opts_do_refractive_caustics;
	int				opts_skip_inside;
	float			opts_hl_to_refl_balance;
	int				opts_backface_cull;
	int				opts_propagate_alpha;

	int				self_illum_on;
	int				self_illum_color_mode;
	int				self_illum_int_mode;
	colour			self_illum_color_filter;
	colour			self_illum_color_light;
	float			self_illum_color_kelvin;
	float			self_illum_int_physical;
	float			self_illum_int_arbitrary;
	int				self_illum_in_reflections;
	int				self_illum_in_fg;

	int				no_diffuse_bump;

	ref_ptr<ColourSource>	diff_color_map;
	ref_ptr<ColourSource>	diff_rough_map;
	ref_ptr<ColourSource>	refl_color_map;
	ref_ptr<ColourSource>	refl_gloss_map;
	ref_ptr<ColourSource>	refr_color_map;
	ref_ptr<ColourSource>	refr_gloss_map;
	ref_ptr<ColourSource>	refr_ior_map;
	ref_ptr<ColourSource>	refr_transc_map;
	ref_ptr<ColourSource>	refr_transw_map;
	ref_ptr<ColourSource>	anisotropy_map;
	ref_ptr<ColourSource>	anisoangle_map;
	ref_ptr<ColourSource>	refl_falloff_color_map;
	ref_ptr<ColourSource>	refr_falloff_color_map;
	ref_ptr<ColourSource>	indirect_multiplier_map;
	ref_ptr<ColourSource>	fg_quality_map;
	ref_ptr<ColourSource>	ao_dark_map;
	ref_ptr<ColourSource>	ao_ambient_map;
	ref_ptr<ColourSource>	bump_map;
	ref_ptr<ColourSource>	displacement_map;
	ref_ptr<ColourSource>	cutout_map;
	ref_ptr<ColourSource>	environment_map;
	ref_ptr<ColourSource>	add_color_map;
	ref_ptr<ColourSource>	radius_map;
	ref_ptr<ColourSource>	self_illum_map;

	bool			diff_color_map_on;
	bool			diff_rough_map_on;
	bool			refl_color_map_on;
	bool			refl_gloss_map_on;
	bool			refr_color_map_on;
	bool			refr_gloss_map_on;
	bool			refr_ior_map_on;
	bool			refr_transc_map_on;
	bool			refr_transw_map_on;
	bool			anisotropy_map_on;
	bool			anisoangle_map_on;
	bool			refl_falloff_color_map_on;
	bool			refr_falloff_color_map_on;
	bool			indirect_multiplier_map_on;
	bool			fg_quality_map_on;
	bool			ao_dark_map_on;
	bool			ao_ambient_map_on;
	bool			bump_map_on;
	bool			displacement_map_on;
	bool			cutout_map_on;
	bool			environment_map_on;
	bool			add_color_map_on;
	bool			radius_map_on;
	bool			self_illum_map_on;

	float			fg_quality_map_amt;
	float			bump_map_amt;
	float			displacement_map_amt;

	ref_ptr<ColourSource>	mapM0;
	ref_ptr<ColourSource>	mapM1;
	ref_ptr<ColourSource>	mapM2;
	ref_ptr<ColourSource>	mapM3;
	ref_ptr<ColourSource>	mapM4;
	ref_ptr<ColourSource>	mapM5;
	ref_ptr<ColourSource>	mapM6;
	ref_ptr<ColourSource>	mapM7;
	ref_ptr<ColourSource>	mapM8;
	ref_ptr<ColourSource>	mapM9;
	ref_ptr<ColourSource>	mapM10;
	ref_ptr<ColourSource>	mapM11;
	ref_ptr<ColourSource>	mapM12;
	ref_ptr<ColourSource>	mapM13;
	ref_ptr<ColourSource>	mapM14;
	ref_ptr<ColourSource>	mapM15;
	ref_ptr<ColourSource>	mapM16;
	ref_ptr<ColourSource>	mapM22;
	ref_ptr<ColourSource>	mapM23;

	ArchDesignParams0(MaterialMaker &maker, ISO::Browser b) {
		read_props(maker, b
		, F(diff_color),				F(diff_rough),				F(diff_weight)
		, F(refl_color),				F(refl_gloss),				F(refl_samples),				F(refl_interp),	F(refl_hlonly),	F(refl_metal),	F(refl_weight)
		, F(refr_color),				F(refr_gloss),				F(refr_samples),				F(refr_interp),	F(refr_ior),	F(refr_weight),	F(refr_trans_on),	F(refr_transc),	F(refr_transw)
		, F(anisotropy),				F(anisoangle),				F(aniso_mode),					F(aniso_channel)
		, F(refl_func_fresnel),			F(refl_func_low),			F(refl_func_high),				F(refl_func_curve)
		, F(refl_falloff_on),			F(refl_falloff_dist),		F(refl_falloff_color_on),		F(refl_falloff_color)
		, F(opts_refl_depth),			F(refl_cutoff),				F(refr_falloff_on),				F(refr_falloff_dist),	F(refr_falloff_color_on),	F(refr_falloff_color)
		, F(opts_refr_depth),			F(refr_cutoff),				F(opts_indirect_multiplier),	F(opts_fg_quality),		F(inter_density)
		, F(intr_refl_samples),			F(intr_refl_ddist_on),		F(intr_refl_ddist),				F(intr_refr_samples),	F(single_env_sample)
		, F(opts_round_corners_on),		F(opts_round_corners_radius),F(opts_round_corners_any_mtl)
		, F(opts_ao_on),				F(opts_ao_exact),			F(opts_ao_use_global_ambient),	F(opts_ao_samples),	F(opts_ao_distance),	F(opts_ao_dark),	F(opts_ao_ambient),	F(opts_ao_do_details)
		, F(opts_no_area_hl)
		, F(opts_1sided)
		, F(opts_do_refractive_caustics)
		, F(opts_skip_inside)
		, F(opts_hl_to_refl_balance)
		, F(opts_backface_cull)
		, F(opts_propagate_alpha)
		, F(self_illum_on),				F(self_illum_color_mode),	F(self_illum_int_mode),		F(self_illum_color_filter)
		, F(self_illum_color_light),	F(self_illum_color_kelvin),	F(self_illum_int_physical),	F(self_illum_int_arbitrary)
		, F(self_illum_in_reflections),	F(self_illum_in_fg)
		, F(no_diffuse_bump)

		, F(diff_color_map),			F(diff_rough_map),			F(refl_color_map),		F(refl_gloss_map),		F(refr_color_map),		F(refr_gloss_map)
		, F(refr_ior_map),				F(refr_transc_map),			F(refr_transw_map),		F(anisotropy_map),		F(anisoangle_map),		F(refl_falloff_color_map)
		, F(refr_falloff_color_map),	F(indirect_multiplier_map),	F(fg_quality_map),		F(ao_dark_map),			F(ao_ambient_map),		F(bump_map)
		, F(displacement_map),			F(cutout_map),				F(environment_map),		F(add_color_map),		F(radius_map),			F(self_illum_map)

		, F(diff_color_map_on),			F(diff_rough_map_on),		F(refl_color_map_on),	F(refl_gloss_map_on),	F(refr_color_map_on),	F(refr_gloss_map_on)
		, F(refr_ior_map_on),			F(refr_transc_map_on),		F(refr_transw_map_on),	F(anisotropy_map_on),	F(anisoangle_map_on),	F(refl_falloff_color_map_on)
		, F(refr_falloff_color_map_on),	F(indirect_multiplier_map_on),F(fg_quality_map_on),	F(ao_dark_map_on),		F(ao_ambient_map_on),	F(bump_map_on)
		, F(displacement_map_on),		F(cutout_map_on),			F(environment_map_on),	F(add_color_map_on),	F(radius_map_on),		F(self_illum_map_on)

		, F(fg_quality_map_amt),			F(bump_map_amt),				F(displacement_map_amt)

		, F(mapM0),			F(mapM1),		F(mapM2),		F(mapM3),		F(mapM4)
		, F(mapM5),			F(mapM6),		F(mapM7),		F(mapM8),		F(mapM9)
		, F(mapM10),		F(mapM11),		F(mapM12),		F(mapM13),		F(mapM14)
		, F(mapM15),		F(mapM16),		F(mapM22),		F(mapM23)
		);
	}
};

struct ArchDesignParams : ArchDesignParams0 {
	ArchDesignParams(MaterialMaker &maker, ISO::Browser material, ISO::Browser props) : ArchDesignParams0(maker, props["adsk_Mtl_MatteShadowReflections Parameters"]) {}
};

MAXShadingModelCreatorT<ArchDesignParams> archdesign_creator;

template<> void MAXShadingModelCreatorT<ArchDesignParams>::Model::WriteShader(ShaderWriter& shader, MaterialMaker &maker) {
	maker.flags	|= maker.NORMALS;

	if (shader.stage == ShaderWriter::VS) {
		WriteVS(shader, maker, vert_source);
		return;
	}
	return Phong::WritePS(shader, maker
		, auto_new(SolidColour)(1)
		, diff_color_map && diff_color_map_on ? diff_color_map.get() : auto_new(SolidColour)(diff_color)
		, refl_color_map && refl_color_map_on ? refl_color_map.get() : auto_new(SolidColour)(refl_color)
		, refl_gloss_map && refl_gloss_map_on ? refl_gloss_map.get() : auto_new(SolidColour)(refl_gloss)
		, bump_map && bump_map_on ? bump_map.get() : 0
		, true
	);
}

ISO_DEFCOMP0(MAXShadingModelCreatorT<ArchDesignParams>::Model);

//-----------------------------------------------------------------------------
//	Subsurface Scattering Fast Skin
//-----------------------------------------------------------------------------

struct SSFastSkinParams0 {
	enum {IDA = 0x7773160f, IDB = 0x96338789};

	string			lightmap;
	string			depthmap;
	string			lightmap_group;
	float			lightmap_size;
	int				samples;
	ref_ptr<ColourSource>	bump;

	struct Diffuse {
		colour			ambient;
		ref_ptr<ColourSource>	overall_color;
		colour			diffuse_color;
		float			diffuse_weight;
		ref_ptr<ColourSource>	front_sss_color;
		float			front_sss_weight;
		float			front_sss_radius;
		colour			mid_sss_color;
		float			mid_sss_weight;
		float			mid_sss_radius;
		colour			back_sss_color;
		float			back_sss_weight;
		float			back_sss_radius;
		float			back_sss_depth;
		Diffuse(MaterialMaker &maker, ISO::Browser b) {
			read_props(maker, b, F(ambient), F(overall_color), F(diffuse_color), F(diffuse_weight)
				, F(front_sss_color), F(front_sss_weight), F(front_sss_radius)
				, F(mid_sss_color), F(mid_sss_weight), F(mid_sss_radius)
				, F(back_sss_color), F(back_sss_weight), F(back_sss_radius), F(back_sss_depth)
			);
		}
	} d;
	struct Specular {
		ref_ptr<ColourSource>	overall_weight;
		float			edge_factor;
		ref_ptr<ColourSource>	primary_spec_color;
		float			primary_weight;
		float			primary_edge_weight;
		float			primary_shinyness;
		ref_ptr<ColourSource>	secondary_spec_color;
		float			secondary_weight;
		float			secondary_edge_weight;
		float			secondary_shinyness;
		float			reflect_weight;
		float			reflect_edge_weight;
		float			reflect_shinyness;
		bool			reflect_environment_only;
		ref_ptr<ColourSource>	environment;
		Specular(MaterialMaker &maker, ISO::Browser b) {
			read_props(maker, b
				, F(overall_weight), F(edge_factor)
				, F(primary_spec_color), F(primary_weight), F(primary_edge_weight), F(primary_shinyness)
				, F(secondary_spec_color), F(secondary_weight), F(secondary_edge_weight), F(secondary_shinyness)
				, F(reflect_weight), F(reflect_edge_weight), F(reflect_shinyness), F(reflect_environment_only)
				, F(environment)
			);
		}
	} s;
	struct Ambient {
		float			lightmap_gamma;
		int				indirect;
		float			scale_conversion;
		float			scatter_bias;
		float			falloff;
		int				screen_composit;
		Ambient(MaterialMaker &maker, ISO::Browser b) {
			read_props(maker, b, F(lightmap_gamma), F(indirect), F(scale_conversion), F(scatter_bias), F(falloff), F(screen_composit));
		}
	} a;
	int				mode;
	bool			Use_lights;
	SSFastSkinParams0(MaterialMaker &maker, ISO::Browser b)
		: d(maker, b["d"])
		, s(maker, b["s"])
		, a(maker, b["a"]) {
		read_props(maker, b
			, F(lightmap), F(depthmap), F(lightmap_group), F(lightmap_size)
			, F(samples), F(bump)
			, F(mode), F(Use_lights)
		);
	}
};

struct SSFastSkinParams : SSFastSkinParams0 {
	SSFastSkinParams(MaterialMaker &maker, ISO::Browser material, ISO::Browser props) : SSFastSkinParams0(maker, props["misss_fast_skin_phen Parameters"]) {}
};

MAXShadingModelCreatorT<SSFastSkinParams> ssfastskin_creator;

template<> void MAXShadingModelCreatorT<SSFastSkinParams>::Model::WriteShader(ShaderWriter& shader, MaterialMaker &maker) {
	maker.flags	|= maker.NORMALS;
	if (shader.stage == ShaderWriter::VS) {
		WriteVS(shader, maker, vert_source);
		return;
	}
	return Phong::WritePS(shader, maker
		, auto_new(SolidColour)(d.ambient)
		, d.overall_color
		, s.primary_spec_color
		, auto_new(SolidColour)(s.primary_shinyness)
		, bump
		, true
	);
}

ISO_DEFCOMP0(MAXShadingModelCreatorT<SSFastSkinParams>::Model);

//-----------------------------------------------------------------------------
//	Make Material
//-----------------------------------------------------------------------------
#define DXMATERIAL_CLASS_ID		Class_ID(0xed995e4, 0x6133daf2)


struct DXShader : ShadingModel {
	DXShader(MaterialMaker &maker, const ISO::Browser mat, ISO::Browser params, ShaderSource *vert_source) : ShadingModel(this) {
		static const struct FLAG_PARAM { const char *name; uint32 flag; } rflags[] = {
			{"sort",			RMASK_SORT			},
			{"double_sided",	RMASK_DOUBLESIDED	},
			{"no_shadow",		RMASK_NOSHADOW		},
			{"use_texture",		RMASK_USETEXTURE	},
			{"upper_shadow",	RMASK_UPPERSHADOW	},
			{"middle_shadow",	RMASK_MIDDLESHADOW	},
			{"draw_first",		RMASK_DRAWFIRST		},
			{"draw_last",		RMASK_DRAWLAST		},
		};

		ISO::Browser	imp		= mat["from"]["Implementation"];
		ISO::Browser	bt		= GetProperties(imp["BindingTable"]);
		const char *rel		= bt["DescAbsoluteURL"]["RelativeFilename"].GetString();
		filename	fn		= FileHandler::FindAbsolute(rel);
		if (!fn)
			fn = rel;

		maker.flags	|= maker.NORMALS;

		const char *tech = bt["DescTAG"].GetString();
		maker.technique = ISO::MakePtrExternal<iso::technique>(fn + ";" + tech);

		for (auto i : params) {
			tag2	name		= i.GetName();
			if (i.Is<anything>()) {
				const char *type = i["Type"].GetString();
				if (str(type) == "TextureVideoClip") {

					const char *rel = i["RelativeFilename"].GetString();
					filename	fn	= FileHandler::FindAbsolute(rel);
					if (!fn)
						fn = rel;

					const char *uv_set		= maker.GetUVSet(i);

					if (str(tech).ends("vc"))
						maker.flags |= maker.COLOURS;

					if (name.get_tag().begins("normal"))
						maker.tangent_uvs = uv_set;

					ISO_ptr<void>	p		= MakePtr(ISO::getdef<Texture>(),name);
					*(ISO_ptr<void>*)p		= MakePtrExternal(ISO::getdef<bitmap>(), fn);
					maker.parameters->Append(p);
					if (find(maker.inputs, uv_set) == maker.inputs.end())
						maker.inputs.push_back(uv_set);
				}

			} else if (i.Is("ColorAndAlpha")) {
				float4p	col = to<float>(*(double4p*)i);

				maker.parameters->Append(ISO_ptr<float4p>(name, col));
			} else {
				const FLAG_PARAM *f = find_if(rflags, [name](const FLAG_PARAM &f) { return f.name == name; });
				if (f != end(rflags)) {
					maker.flags |= f->flag;
				} else {
					maker.parameters->Append(i);
				}
			}
		}
	}

	void	WriteShader(ShaderWriter& shader, MaterialMaker &maker) {
	}
};

ISO_DEFCOMP0(DXShader);

struct MAXSDXhadingModelCreator : MAXShadingModelCreator {
	MAXSDXhadingModelCreator() : MAXShadingModelCreator(this, DXMATERIAL_CLASS_ID) {}
	ShadingModel *operator()(MaterialMaker &maker, ISO::Browser material, const ISO::Browser &props, ShaderSource *vert_source) {
		return new DXShader(maker, material, props, vert_source);
	}
} dxshader_creator;
