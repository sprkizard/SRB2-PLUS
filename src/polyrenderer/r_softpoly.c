// SONIC ROBO BLAST 2
//-----------------------------------------------------------------------------
// Copyright (C) 2019 by Jaime "Jimita" Passos.
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------
/// \file  r_softpoly.c
/// \brief .

#include "r_softpoly.h"

rendertarget_t rsp_target;
viewpoint_t rsp_viewpoint;
fpmatrix4_t rsp_projectionmatrix;
fpmatrix4_t rsp_frustummatrix;

void RSP_Viewport(INT32 width, INT32 height)
{
	const float den = 1.7f;
	float fov = 90.0f - (48.0f / den);
	float aspecty = (BASEVIDHEIGHT * vid.dupy);
	if (splitscreen)
	{
		fov /= den;
		aspecty /= 2.0f;
	}

	rsp_target.width = width;
	rsp_target.height = height;

	rsp_target.aspectratio = ((float)(BASEVIDWIDTH * vid.dupx) / aspecty);
	rsp_target.fov = fov;

	if (rsp_target.depthbuffer)
		Z_Free(rsp_target.depthbuffer);

	rsp_target.depthbuffer = (fixed_t *)Z_Malloc(sizeof(fixed_t) * (rsp_target.width * rsp_target.height), PU_STATIC, NULL);
	rsp_target.mode = (RENDERMODE_COLOR|RENDERMODE_DEPTH);
	rsp_target.cullmode = TRICULL_FRONT;

	rsp_target.far_plane = 32768.0f;
	rsp_target.near_plane = 16.0f;

	RSP_MakePerspectiveMatrix(&rsp_viewpoint.projection, fov * M_PI / 180.f, rsp_target.aspectratio, 0.1f, rsp_target.far_plane);

	rsp_basepixelfunc = RSP_DrawPixel;
	rsp_transpixelfunc = RSP_DrawTranslucentPixel;
}

static void RSP_SetupFrame(void)
{
	fixed_t angle = AngleFixed(viewangle - ANGLE_90);
	float viewangle = FIXED_TO_FLOAT(angle);

	RSP_MakeVector4(rsp_viewpoint.position, FIXED_TO_FLOAT(viewx), -FIXED_TO_FLOAT(viewz), -FIXED_TO_FLOAT(viewy));
	RSP_MakeVector4(rsp_viewpoint.up, 0, 1, 0);
	RSP_MakeVector4(rsp_viewpoint.right, 1, 0, 0);
	RSP_MakeVector4(rsp_viewpoint.target, 0, 0, -1);

	RSP_VectorRotate(&rsp_viewpoint.target, (viewangle * M_PI / 180.0), rsp_viewpoint.up.x, rsp_viewpoint.up.y, rsp_viewpoint.up.z);

    RSP_MakeViewMatrix(&rsp_viewpoint.view, &rsp_viewpoint.position, &rsp_viewpoint.target, &rsp_viewpoint.up);
    rsp_frustummatrix = rsp_viewpoint.projection;
	rsp_projectionmatrix = RSP_MatrixMultiply(&rsp_viewpoint.view, &rsp_frustummatrix);
}

void RSP_ModelView(void)
{
	// Arkus: Set pixel drawer.
	rsp_curpixelfunc = rsp_basepixelfunc;

	// Clear the depth buffer, and setup the matrixes.
	RSP_ClearDepthBuffer();
	RSP_SetupFrame();
}

void RSP_ClearDepthBuffer(void)
{
	memset(rsp_target.depthbuffer, 0, sizeof(fixed_t) * rsp_target.width * rsp_target.height);
}
