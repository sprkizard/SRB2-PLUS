// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright (C) 1998-2000 by DooM Legacy Team.
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
//-----------------------------------------------------------------------------
/// \file
/// \brief MD2 Handling
///	Inspired from md2.h by Mete Ciragan (mete@swissquake.ch)

#ifndef _HW_MD2_H_
#define _HW_MD2_H_

#include "hw_glob.h"
#include "../r_model.h"

typedef model_header_t md2_header_t;
typedef model_alias_triangleVertex_t md2_alias_triangleVertex_t;
typedef model_triangleVertex_t md2_triangleVertex_t;
typedef model_triangle_t md2_triangle_t;
typedef model_textureCoordinate_t md2_textureCoordinate_t;
typedef model_alias_frame_t md2_alias_frame_t;
typedef model_frame_t md2_frame_t;
typedef model_skin_t md2_skin_t;
typedef model_glCommandVertex_t md2_glCommandVertex_t;
typedef model_t md2_model_t;

typedef struct
{
	char        filename[32];
	float       scale;
	float       offset;
	md2_model_t *model;
	void        *grpatch;
	void        *blendgrpatch;
	boolean     notfound;
	INT32       skin;
	boolean     error;
} md2_t;

extern md2_t md2_models[NUMSPRITES];
extern md2_t md2_playermodels[MAXSKINS];

void HWR_InitMD2(void);
void HWR_DrawMD2(gr_vissprite_t *spr);
void HWR_AddPlayerMD2(INT32 skin);
void HWR_AddSpriteMD2(size_t spritenum);

#endif // _HW_MD2_H_
