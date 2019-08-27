// SONIC ROBO BLAST 2
//-----------------------------------------------------------------------------
// Copyright (C) 2019 by Jaime "Jimita" Passos.
// Copyright (C) 2019 by Vinícius Telésforo.
// Copyright (C) 2017 by Krzysztof Kondrak.
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------
/// \file  rsp_spans.c
/// \brief Span drawers

#include "r_softpoly.h"

static UINT32 tex_width, tex_height;
static UINT8 *tex_data;
static UINT8 *tex_translation;
static UINT8 *tex_colormap;
static UINT8 *tex_transmap;

static inline void spanloop(rsp_triangle_t *triangle, fixed_t y, fixed_t startXPrestep, fixed_t endXPrestep, fixed_t startX, fixed_t startInvZ, fixed_t endInvZ, fixed_t startU, fixed_t endU, fixed_t startV, fixed_t endV, fixed_t invLineLength)
{
	fixed_t x, r, z;
	fixed_t lerpInvZ;
	UINT16 u, v;
	UINT8 pixel = 0;
	boolean depth_only = ((rsp_target.mode & (RENDERMODE_DEPTH|RENDERMODE_COLOR)) == RENDERMODE_DEPTH);
	INT32 ix, iy;

	// avoid a crash here
	if (!rsp_curpixelfunc)
		I_Error("RSP_TexturedMappedTriangle: no pixel drawer set!");

	iy = ((y>>FRACBITS) + SOFTWARE_AIMING);
	rsp_ypix = iy;
	rsp_tpix = tex_transmap;

	for (x = startXPrestep; x <= endXPrestep;)
	{
		x += FRACUNIT;
		ix = x>>FRACBITS;
		if (ix >= rsp_target.width) break;
		if (rsp_mfloorclip && rsp_mceilingclip)
		{
			if (ix < 0) continue;
			if (ix >= vid.width) break;
			if (iy >= rsp_mfloorclip[ix]) continue;
			if (iy <= rsp_mceilingclip[ix]) continue;
		}
		if (ix >= 0)
		{
			// interpolate 1/z for each pixel in the scanline
			r = FixedMul((x - startX), invLineLength);
			lerpInvZ = FixedLerp(startInvZ, endInvZ, r);
			z = FixedDiv(FRACUNIT, lerpInvZ);
			rsp_xpix = ix;
			rsp_zpix = lerpInvZ;
			if (!depth_only)
			{
				u = FixedMul(z, FixedLerp(startU, endU, r))>>FRACBITS;
				v = FixedMul(z, FixedLerp(startV, endV, r))>>FRACBITS;
				u %= tex_width;
				v %= tex_height;
				pixel = tex_data[((UINT16)u + (UINT16)v * tex_width)];
				if (pixel != TRANSPARENTPIXEL)
				{
					if (tex_translation)
						pixel = tex_translation[pixel];
					if (tex_colormap)
						pixel = tex_colormap[pixel];
					rsp_cpix = pixel;
					rsp_curpixelfunc();
				}
			}
			else
				rsp_curpixelfunc();
		}
	}
}

void RSP_TexturedMappedTriangle(rsp_triangle_t *triangle, rsp_trimode_t type)
{
	rsp_vertex_t *v0 = &triangle->vertices[0];
	rsp_vertex_t *v1 = &triangle->vertices[1];
	rsp_vertex_t *v2 = &triangle->vertices[2];
	fixed_t y, invDy, dxLeft, dxRight, prestep, yDir = FRACUNIT;
	fixed_t startX, endX, startXPrestep, endXPrestep, lineLength;
	UINT32 texW = triangle->texture->width;
	UINT32 texH = triangle->texture->height;
	INT32 currLine, numScanlines;
	fixed_t invZ0, invZ1, invZ2, invY02 = FRACUNIT;

	fixed_t v0x = FLOAT_TO_FIXED(v0->position.x);
	fixed_t v0y = FLOAT_TO_FIXED(v0->position.y);
	fixed_t v0z = FLOAT_TO_FIXED(v0->position.z);
	fixed_t v0u = FLOAT_TO_FIXED(v0->uv.u);
	fixed_t v0v = FLOAT_TO_FIXED(v0->uv.v);

	fixed_t v1x = FLOAT_TO_FIXED(v1->position.x);
	fixed_t v1y = FLOAT_TO_FIXED(v1->position.y);
	fixed_t v1z = FLOAT_TO_FIXED(v1->position.z);
	fixed_t v1u = FLOAT_TO_FIXED(v1->uv.u);
	fixed_t v1v = FLOAT_TO_FIXED(v1->uv.v);

	fixed_t v2x = FLOAT_TO_FIXED(v2->position.x);
	fixed_t v2y = FLOAT_TO_FIXED(v2->position.y);
	fixed_t v2z = FLOAT_TO_FIXED(v2->position.z);
	fixed_t v2u = FLOAT_TO_FIXED(v2->uv.u);
	fixed_t v2v = FLOAT_TO_FIXED(v2->uv.v);

	(void)v1y;

	tex_data = triangle->texture->data;
	tex_width = texW;
	tex_height = texH;
	tex_translation = triangle->translation;
	tex_colormap = triangle->colormap;
	tex_transmap = triangle->transmap;

	// set pixel drawer
	rsp_curpixelfunc = rsp_basepixelfunc;
	if (tex_transmap)
		rsp_curpixelfunc = rsp_transpixelfunc;

	if (type == TRI_FLATBOTTOM)
	{
		invDy = FixedDiv(FRACUNIT, FixedCeil(v2y - v0y));
		numScanlines = FixedCeil(v2y - v0y)>>FRACBITS;
		prestep = (FixedCeil(v0y) - v0y);
		if (v2->position.y - v0->position.y < 1)
			return;
	}
	else if (type == TRI_FLATTOP)
	{
		yDir = -FRACUNIT;
		invDy = FixedDiv(FRACUNIT, FixedCeil(v0y - v2y));
		numScanlines = FixedCeil(v0y - v2y)>>FRACBITS;
		prestep = (FixedCeil(v2y) - v2y);
		if (v0->position.y - v2->position.y < 1)
			return;
	}
	else
		I_Error("RSP_TexturedMappedTriangle: unknown triangle type");

	if (numScanlines >= viewheight)
		return;

	dxLeft = FixedMul((v2x - v0x), invDy);
	dxRight = FixedMul((v1x - v0x), invDy);
	startX = v0x;
	endX = startX;
	startXPrestep = v0x + FixedMul(dxLeft, prestep);
	endXPrestep = v0x + FixedMul(dxRight, prestep);

	invZ0 = FixedDiv(FRACUNIT, v0z);
	invZ1 = FixedDiv(FRACUNIT, v1z);
	invZ2 = FixedDiv(FRACUNIT, v2z);
	invY02 = FixedDiv(FRACUNIT, (v0y - v2y));

	for (currLine = 0, y = v0y; currLine <= numScanlines; y += yDir)
	{
		fixed_t startInvZ, endInvZ, r1, invLineLength;
		fixed_t startU = texW<<FRACBITS, startV = texH<<FRACBITS, endU = texW<<FRACBITS, endV = texH<<FRACBITS;
		lineLength = endX - startX;

		r1 = FixedMul((v0y - y), invY02);
		startInvZ = FixedLerp(invZ0, invZ2, r1);
		endInvZ = FixedLerp(invZ0, invZ1, r1);

		startU = FixedMul(startU, FixedLerp(FixedMul(v0u, invZ0), FixedMul(v2u, invZ2), r1));
		startV = FixedMul(startV, FixedLerp(FixedMul(v0v, invZ0), FixedMul(v2v, invZ2), r1));
		endU = FixedMul(endU, FixedLerp(FixedMul(v0u, invZ0), FixedMul(v1u, invZ1), r1));
		endV = FixedMul(endV, FixedLerp(FixedMul(v0v, invZ0), FixedMul(v1v, invZ1), r1));

		// skip zero-length lines
		if (lineLength > 0)
		{
			invLineLength = FixedDiv(FRACUNIT, lineLength);
			spanloop(triangle, y, startXPrestep, endXPrestep, startX, startInvZ, endInvZ, startU, endU, startV, endV, invLineLength);
		}

		startX += dxLeft;
		endX += dxRight;

		currLine++;
		if (currLine < numScanlines)
		{
			startXPrestep += dxLeft;
			endXPrestep += dxRight;
		}
	}
}
