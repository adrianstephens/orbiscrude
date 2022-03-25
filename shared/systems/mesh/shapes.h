#ifndef SHAPES_H
#define SHAPES_H

#include "base/vector.h"
#include "common/shader.h"
#include "maths/geometry_iso.h"
#include "maths/polygon.h"

namespace iso {
struct RenderEvent;
struct VertexIndexBuffer;

template<typename S> const VertexIndexBuffer &GetVIB();


void			DrawProj(RenderEvent *re, pass *t, const VertexIndexBuffer &vib, param(float4x4) matrix);
void			Draw(RenderEvent *re, pass *t, const VertexIndexBuffer &vib, param(float3x4) matrix);

template<typename S> void	DrawProj(RenderEvent *re, pass *t, param(float4x4) matrix)			{ DrawProj(re, t, GetVIB<S>(), matrix); }
template<typename S> void	Draw(RenderEvent *re, pass *t, param(float3x4) matrix)				{ Draw(re, t, GetVIB<S>(), matrix); }
template<typename S> void	Draw(RenderEvent *re, const S &s, pass *t)							{ Draw<S>(re, t, float3x4(s.matrix())); }
template<typename S> void	Draw(RenderEvent *re, const S &s, pass *t, param(float3x4) matrix)	{ Draw<S>(re, t, matrix * float3x4(s.matrix())); }
void						Draw(RenderEvent *re, param(capsule) s, pass *t, param(float3x4) matrix = identity);

void			DrawGrid(RenderEvent *re, int nx, int ny, pass *t, param(float3x4) matrix = identity);
void			DrawGrid(GraphicsContext &ctx, int nx, int ny);
void			DrawWireFrameGrid(GraphicsContext &ctx, int nx, int ny);
void			DrawWireFrameBox(GraphicsContext &ctx);
void			DrawWireFrameRect(GraphicsContext &ctx);

void 			DrawArrow(GraphicsContext &ctx, float axis_radius, float cone_radius, float cone_length, float axis_offset, pass *t, param(float3) dir, const ISO::Browser &params = ISO::Browser());
void			DrawAxes(GraphicsContext &ctx, float axis_radius, float cone_radius, float cone_length, float axis_offset, pass *t, param(float3x4) mat, const ISO::Browser &params = ISO::Browser());
int				GetAxesClick(const float4x4 &viewproj, param(float2) p);
quaternion		GetAxesRot(int c);

quadrilateral	XYPlaneQuad(param(float4x4) viewproj, float du = 0);
quadrilateral	PlaneQuad(param(float4x4) viewproj, param(plane) p, float du = 0);
void			DrawPlane(GraphicsContext &ctx, const plane &p, const float4x4 &wvp);
void			DrawPlane(RenderEvent *re, param(plane) &p);

}

#endif // SHAPES_H
