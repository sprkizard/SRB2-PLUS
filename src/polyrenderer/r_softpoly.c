// SONIC ROBO BLAST 2
//-----------------------------------------------------------------------------
// Copyright (C) 2019 by Jaime "Jimita" Passos.
// Copyright (C) 2019 by Vinícius "Arkus-Kotan" Telésforo.
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
fpmatrix16_t *rsp_projectionmatrix = NULL;

// init the polygon renderer
void RSP_Init(void)
{
	// set pixel functions
	rsp_basepixelfunc = RSP_DrawPixel;
	rsp_transpixelfunc = RSP_DrawTranslucentPixel;

	// set triangle functions
	rsp_fixedtrifunc = RSP_TexturedMappedTriangle;
	rsp_floattrifunc = RSP_TexturedMappedTriangleFP;

	// run other initialisation code
	RSP_SetDrawerFunctions();
	RSP_InitModels();
}

// make the viewport, after resolution change
void RSP_Viewport(INT32 width, INT32 height)
{
	float fov = FIXED_TO_FLOAT(viewfov);

	// viewport width and height
	rsp_target.width = width;
	rsp_target.height = height;

	// viewport aspect ratio and fov
	rsp_target.aspectratio = (float)rsp_target.width / (float)rsp_target.height;
	rsp_target.fov = fov;
	fov *= (M_PI / 180.f);

	// make fixed-point depth buffer
	if (rsp_target.depthbuffer)
		Z_Free(rsp_target.depthbuffer);
	rsp_target.depthbuffer = (fixed_t *)Z_Malloc(sizeof(fixed_t) * (rsp_target.width * rsp_target.height), PU_SOFTPOLY, NULL);

	// renderer modes
	rsp_target.mode = (RENDERMODE_COLOR|RENDERMODE_DEPTH);
	rsp_target.cullmode = TRICULL_FRONT;

	// far and near plane (frustum clipping)
	rsp_target.far_plane = 32768.0f;
	rsp_target.near_plane = 16.0f;

	// make projection matrix
	RSP_MakePerspectiveMatrix(&rsp_viewpoint.projection_matrix, fov, rsp_target.aspectratio, 0.1f, rsp_target.far_plane);
}

// up vector
// right vector not needed
static fpvector4_t upvector = {0.0f, 1.0f, 0.0f, 1.0f};

// make all the vectors and matrixes
static void RSP_SetupFrame(fixed_t vx, fixed_t vy, fixed_t vz, angle_t vangle)
{
	fpmatrix16_t modelview;
	fixed_t angle = AngleFixed(vangle - ANGLE_90);
	float viewang = FIXED_TO_FLOAT(angle);

	// make position and target vectors
	RSP_MakeVector4(rsp_viewpoint.position_vector, FIXED_TO_FLOAT(vx), -FIXED_TO_FLOAT(vz), -FIXED_TO_FLOAT(vy));
	RSP_MakeVector4(rsp_viewpoint.target_vector, 0, 0, -1);

	// make view matrix
	RSP_VectorRotate(&rsp_viewpoint.target_vector, (viewang * M_PI / 180.0f), upvector.x, upvector.y, upvector.z);
	RSP_MakeViewMatrix(&rsp_viewpoint.view_matrix, &rsp_viewpoint.position_vector, &rsp_viewpoint.target_vector, &upvector);

	// make "model view projection" matrix
	// in reality, there is no model matrix
	modelview = RSP_MatrixMultiply(&rsp_viewpoint.view_matrix, &rsp_viewpoint.projection_matrix);
	if (rsp_projectionmatrix == NULL)
		rsp_projectionmatrix = Z_Malloc(sizeof(fpmatrix16_t), PU_SOFTPOLY, NULL);
	M_Memcpy(rsp_projectionmatrix, &modelview, sizeof(fpmatrix16_t));
}

void RSP_ModelView(void)
{
	// set drawer functions
	RSP_SetDrawerFunctions();

	// Clear the depth buffer, and setup the matrixes.
	RSP_ClearDepthBuffer();
	RSP_SetupFrame(viewx, viewy, viewz, viewangle);
}

void RSP_SetDrawerFunctions(void)
{
	// Arkus: Set pixel drawer.
	rsp_curpixelfunc = rsp_basepixelfunc;

	// also set triangle drawer
	if (cv_texturemapping.value == TEXMAP_FIXED)
		rsp_curtrifunc = rsp_fixedtrifunc;
	else if (cv_texturemapping.value == TEXMAP_FLOAT)
		rsp_curtrifunc = rsp_floattrifunc;
	else
		rsp_curtrifunc = NULL;
}

// on frame start
void RSP_OnFrame(void)
{
	RSP_ModelView();
	rsp_viewwindowx = viewwindowx;
	rsp_viewwindowy = viewwindowy;
	rsp_target.aiming = true;
	rsp_portalrender = 0;
}

// PORTAL STUFF
UINT8 rsp_portalrender;

// Store the current viewpoint
void RSP_StoreViewpoint(void)
{
	rsp_viewpoint.viewx = viewx;
	rsp_viewpoint.viewy = viewy;
	rsp_viewpoint.viewz = viewz;
	rsp_viewpoint.viewangle = viewangle;
	rsp_viewpoint.aimingangle = aimingangle;
	rsp_viewpoint.viewcos = viewcos;
	rsp_viewpoint.viewsin = viewsin;
}

// Restore the stored viewpoint
void RSP_RestoreViewpoint(void)
{
	viewx = rsp_viewpoint.viewx;
	viewy = rsp_viewpoint.viewy;
	viewz = rsp_viewpoint.viewz;
	viewangle = rsp_viewpoint.viewangle;
	aimingangle = rsp_viewpoint.aimingangle;
	viewcos = rsp_viewpoint.viewcos;
	viewsin = rsp_viewpoint.viewsin;
	RSP_ModelView();
}

// Store a sprite's viewpoint
void RSP_StoreSpriteViewpoint(vissprite_t *spr)
{
	spr->viewx = viewx;
	spr->viewy = viewy;
	spr->viewz = viewz;
	spr->viewangle = viewangle;
	spr->aimingangle = aimingangle;
	spr->viewcos = viewcos;
	spr->viewsin = viewsin;
}

// Set viewpoint to a sprite's viewpoint
void RSP_RestoreSpriteViewpoint(vissprite_t *spr)
{
	viewx = spr->viewx;
	viewy = spr->viewy;
	viewz = spr->viewz;
	viewangle = spr->viewangle;
	aimingangle = spr->aimingangle;
	viewcos = spr->viewcos;
	viewsin = spr->viewsin;
	RSP_ModelView();
}

// clear the depth buffer
void RSP_ClearDepthBuffer(void)
{
	memset(rsp_target.depthbuffer, 0, sizeof(fixed_t) * rsp_target.width * rsp_target.height);
}

// Arkus: Debug rendering.
void RSP_DebugRender(INT32 model)
{
	fixed_t vx = 0, vy = 0, vz = 0;
	angle_t va = ANGLE_90;
	INT32 skinnum = R_SkinAvailable(cv_skin.string);
	UINT8 skincolour = cv_playercolor.value;
	static float angle;
	static fixed_t frame;

	if (!cv_models.value)
		return;

	if (!netgame && gamestate == GS_LEVEL && players[consoleplayer].mo)
	{
		skinnum = ((skin_t*)players[0].mo->skin)-skins;
		skincolour = (players[consoleplayer].mo)->color;
	}

	if (skinnum < 0)
		skinnum = 0;
	if (skinnum > MAXSKINS)
		skinnum = 0;
	if (skincolour >= MAXTRANSLATIONS)
		skincolour = MAXTRANSLATIONS-1;

	if (model < 2)
	{
		vz = 32*FRACUNIT;
		vy = -128*FRACUNIT;
	}
	else if (model == 2)
	{
		vz = 48*FRACUNIT;
		vy = -256*FRACUNIT;
	}
	else if (model == 3)
	{
		vz = 40*FRACUNIT;
		vy = -128*FRACUNIT;
	}

	// store viewpoint
	rsp_viewpoint.viewx = vx;
	rsp_viewpoint.viewy = vy;
	rsp_viewpoint.viewz = vz;
	rsp_viewpoint.viewangle = va;
	rsp_viewpoint.aimingangle = 0;
	rsp_viewpoint.viewcos = FINECOSINE(va>>ANGLETOFINESHIFT);
	rsp_viewpoint.viewsin = FINESINE(va>>ANGLETOFINESHIFT);

	// set viewpoint
	RSP_Viewport(vid.width, vid.height);
	RSP_SetupFrame(vx, vy, vz, va);
	RSP_ClearDepthBuffer();
	rsp_viewwindowx = 0;
	rsp_viewwindowy = (40 * vid.dupy);
	rsp_target.aiming = false;

	// standing player
	if (model == 0)
	{
		RSP_RenderModelSimple(SPR_PLAY, 0, 0, 0, 0, angle, skincolour, &skins[skinnum], false, false);
		angle -= 2.0f;
	}
	// walking player
	else if (model == 1)
	{
		const INT32 numframes = 8;
		INT32 curframe = FixedInt(frame) % numframes;
		INT32 curframenum = states[S_PLAY_RUN1+curframe].frame;
		INT32 nextframe, nextframenum;
		float pol = 0.0f;

		if (cv_modelinterpolation.value)
		{
			nextframe = FixedInt(frame+FRACUNIT) % numframes;
			nextframenum = states[S_PLAY_RUN1+nextframe].frame;
			pol = FIXED_TO_FLOAT(frame % FRACUNIT);
			RSP_RenderInterpolatedModelSimple(SPR_PLAY, curframenum, nextframenum, pol, (cv_modelinterpolation.value == 2), 0, 0, 0, angle, skincolour, &skins[skinnum], false, false);
		}
		else
			RSP_RenderModelSimple(SPR_PLAY, curframenum, 0, 0, 0, angle, skincolour, &skins[skinnum], false, false);

		angle -= 1.0f;
		frame += ((FRACUNIT*2) / 10);
	}
	// monitors
	else if (model == 2)
	{
		INT32 i;
		const INT32 nummonitors = 10;
		const float distance = 40.0f;
		for (i = 0; i < nummonitors; i++)
			RSP_RenderModelSimple(SPR_YLTV+i, 0, ((-nummonitors/2 + i) * distance) + (distance / 2.0f), 0, 0, 0, 0, NULL, false, true);
	}
	// sign post
	else if (model == 3)
	{
		RSP_RenderModelSimple(SPR_SIGN, states[S_SIGN53].frame + Color_Opposite[skincolour*2+1], 0, 0, 0, -90.0f, Color_Opposite[skincolour*2], NULL, false, false);
		RSP_RenderModelSimple(SPR_PLAY, states[S_PLAY_SIGN].frame, 0, 0, 24.0f, -90.0f, skincolour, &skins[skinnum], false, false);
	}

	// restore the viewport!!!!!!!!!
	RSP_Viewport(viewwidth, viewheight);
}
