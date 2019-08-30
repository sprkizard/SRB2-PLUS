// SONIC ROBO BLAST 2
//-----------------------------------------------------------------------------
// Copyright (C) 1993-1996 by id Software, Inc.
// Copyright (C) 1998-2000 by DooM Legacy Team.
// Copyright (C) 1999-2018 by Sonic Team Junior.
// Copyright (C) 2019 by Jaime "Jimita" Passos.
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------
/// \file  rsp_model.c
/// \brief Model loading

#include "r_softpoly.h"

#ifdef __GNUC__
#include <unistd.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "../doomdef.h"
#include "../doomstat.h"
#include "../d_main.h"
#include "../r_bsp.h"
#include "../r_main.h"
#include "../m_misc.h"
#include "../w_wad.h"
#include "../z_zone.h"
#include "../r_draw.h"
#include "../r_things.h"
#include "../v_video.h"
#include "../r_data.h"
#ifdef ESLOPE
#include "../p_slopes.h"
#endif // ESLOPE

#ifdef HAVE_PNG

#ifndef _MSC_VER
#ifndef _LARGEFILE64_SOURCE
#define _LARGEFILE64_SOURCE
#endif
#endif

#ifndef _LFS64_LARGEFILE
#define _LFS64_LARGEFILE
#endif

#ifndef _FILE_OFFSET_BITS
#define _FILE_OFFSET_BITS 0
#endif

 #include "png.h"
 #ifndef PNG_READ_SUPPORTED
 #undef HAVE_PNG
 #endif
 #if PNG_LIBPNG_VER < 100207
 //#undef HAVE_PNG
 #endif
#endif

#ifndef errno
#include "errno.h"
#endif

rsp_md2_t rsp_md2_models[NUMSPRITES];
rsp_md2_t rsp_md2_playermodels[MAXSKINS];

//
// load model
//
model_t *RSP_LoadModel(const char *filename)
{
	FILE *file;
	model_t *model;
	UINT8 buffer[MD2_MAX_FRAMESIZE];
	size_t i;
	const float model_rotate = 0.707f;

	model = Z_Calloc(sizeof (*model), PU_SOFTPOLY, NULL);
	if (model == NULL)
		return 0;

	//Filename checking fixed ~Monster Iestyn and Golden
	file = fopen(va("%s"PATHSEP"%s", srb2home, filename), "rb");
	if (!file)
	{
		Z_Free(model);
		return 0;
	}

	// initialize model and read header

	if (fread(&model->header, sizeof (model->header), 1, file) != 1
		|| model->header.magic != MD2_IDENT
		|| model->header.version != MD2_VERSION)
	{
		fclose(file);
		Z_Free(model);
		return 0;
	}

	model->header.numSkins = 1;

#define MD2LIMITCHECK(field, max, msgname) \
	if (field > max) \
	{ \
		CONS_Alert(CONS_ERROR, "RSP_LoadModel: %s has too many " msgname " (# found: %d, maximum: %d)\n", filename, field, max); \
		model_freeModel (model); \
		fclose(file); \
		return 0; \
	}

	// Uncomment if these are actually needed
//	MD2LIMITCHECK(model->header.numSkins,     MD2_MAX_SKINS,     "skins")
//	MD2LIMITCHECK(model->header.numTexCoords, MD2_MAX_TEXCOORDS, "texture coordinates")
	MD2LIMITCHECK(model->header.numTriangles, MD2_MAX_TRIANGLES, "triangles")
	MD2LIMITCHECK(model->header.numFrames,    MD2_MAX_FRAMES,    "frames")
	MD2LIMITCHECK(model->header.numVertices,  MD2_MAX_VERTICES,  "vertices")

#undef MD2LIMITCHECK

	// read skins
	fseek(file, model->header.offsetSkins, SEEK_SET);
	if (model->header.numSkins > 0)
	{
		model->skins = Z_Calloc(sizeof (model_skin_t) * model->header.numSkins, PU_SOFTPOLY, NULL);
		if (!model->skins || model->header.numSkins !=
			fread(model->skins, sizeof (model_skin_t), model->header.numSkins, file))
		{
			model_freeModel (model);
			fclose(file);
			return 0;
		}
	}

	// read texture coordinates
	fseek(file, model->header.offsetTexCoords, SEEK_SET);
	if (model->header.numTexCoords > 0)
	{
		model->texCoords = Z_Calloc(sizeof (model_textureCoordinate_t) * model->header.numTexCoords, PU_SOFTPOLY, NULL);
		if (!model->texCoords || model->header.numTexCoords !=
			fread(model->texCoords, sizeof (model_textureCoordinate_t), model->header.numTexCoords, file))
		{
			model_freeModel (model);
			fclose(file);
			return 0;
		}
	}

	// read triangles
	fseek(file, model->header.offsetTriangles, SEEK_SET);
	if (model->header.numTriangles > 0)
	{
		model->triangles = Z_Calloc(sizeof (model_triangle_t) * model->header.numTriangles, PU_SOFTPOLY, NULL);
		if (!model->triangles || model->header.numTriangles !=
			fread(model->triangles, sizeof (model_triangle_t), model->header.numTriangles, file))
		{
			model_freeModel (model);
			fclose(file);
			return 0;
		}
	}

	// read alias frames
	fseek(file, model->header.offsetFrames, SEEK_SET);
	if (model->header.numFrames > 0)
	{
		model->frames = Z_Calloc(sizeof (model_frame_t) * model->header.numFrames, PU_SOFTPOLY, NULL);
		if (!model->frames)
		{
			model_freeModel (model);
			fclose(file);
			return 0;
		}

		for (i = 0; i < model->header.numFrames; i++)
		{
			model_alias_frame_t *frame = (model_alias_frame_t *)(void *)buffer;
			size_t j;

			model->frames[i].vertices = Z_Calloc(sizeof (model_triangleVertex_t) * model->header.numVertices, PU_SOFTPOLY, NULL);
			if (!model->frames[i].vertices || model->header.frameSize !=
				fread(frame, 1, model->header.frameSize, file))
			{
				model_freeModel (model);
				fclose(file);
				return 0;
			}

			strcpy(model->frames[i].name, frame->name);
			for (j = 0; j < model->header.numVertices; j++)
			{
				fpvector4_t vec;
				fpquaternion_t quaternion;

				model->frames[i].vertices[j].vertex[0] = (float) ((INT32) frame->alias_vertices[j].vertex[0]) * frame->scale[0] + frame->translate[0];
				model->frames[i].vertices[j].vertex[2] = -1* ((float) ((INT32) frame->alias_vertices[j].vertex[1]) * frame->scale[1] + frame->translate[1]);
				model->frames[i].vertices[j].vertex[1] = (float) ((INT32) frame->alias_vertices[j].vertex[2]) * frame->scale[2] + frame->translate[2];

				RSP_MakeVector4(vec, model->frames[i].vertices[j].vertex[0], model->frames[i].vertices[j].vertex[1], model->frames[i].vertices[j].vertex[2]);

				// Rotate model
				quaternion.x = model_rotate;
				quaternion.y = 0;
				quaternion.z = 0;
				quaternion.w = -model_rotate;
				RSP_QuaternionRotateVector(&vec, &quaternion);

				model->frames[i].vertices[j].vertex[0] = vec.x;
				model->frames[i].vertices[j].vertex[1] = vec.y;
				model->frames[i].vertices[j].vertex[2] = vec.z;
			}
		}
	}

	fclose(file);

	return model;
}

#ifdef HAVE_PNG
static void PNG_error(png_structp PNG, png_const_charp pngtext)
{
	CONS_Debug(DBG_RENDER, "libpng error at %p: %s", PNG, pngtext);
	//I_Error("libpng error at %p: %s", PNG, pngtext);
}

static void PNG_warn(png_structp PNG, png_const_charp pngtext)
{
	CONS_Debug(DBG_RENDER, "libpng warning at %p: %s", PNG, pngtext);
}

static int PNG_Load(const char *filename, int *w, int *h, rsp_modeltexture_t *texture)
{
	png_structp png_ptr;
	png_infop png_info_ptr;
	png_uint_32 width, height;
	int bit_depth, color_type;
#ifdef PNG_SETJMP_SUPPORTED
#ifdef USE_FAR_KEYWORD
	jmp_buf jmpbuf;
#endif
#endif
	png_FILE_p png_FILE;
	//Filename checking fixed ~Monster Iestyn and Golden
	char *pngfilename = va("%s"PATHSEP"md2"PATHSEP"%s", srb2home, filename);

	FIL_ForceExtension(pngfilename, ".png");
	png_FILE = fopen(pngfilename, "rb");
	if (!png_FILE)
	{
		//CONS_Debug(DBG_RENDER, "M_SavePNG: Error on opening %s for loading\n", filename);
		return 0;
	}

	png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL,
		PNG_error, PNG_warn);
	if (!png_ptr)
	{
		CONS_Debug(DBG_RENDER, "PNG_Load: Error on initialize libpng\n");
		fclose(png_FILE);
		return 0;
	}

	png_info_ptr = png_create_info_struct(png_ptr);
	if (!png_info_ptr)
	{
		CONS_Debug(DBG_RENDER, "PNG_Load: Error on allocate for libpng\n");
		png_destroy_read_struct(&png_ptr, NULL, NULL);
		fclose(png_FILE);
		return 0;
	}

#ifdef USE_FAR_KEYWORD
	if (setjmp(jmpbuf))
#else
	if (setjmp(png_jmpbuf(png_ptr)))
#endif
	{
		//CONS_Debug(DBG_RENDER, "libpng load error on %s\n", filename);
		png_destroy_read_struct(&png_ptr, &png_info_ptr, NULL);
		fclose(png_FILE);
		Z_Free(texture->data);
		return 0;
	}
#ifdef USE_FAR_KEYWORD
	png_memcpy(png_jmpbuf(png_ptr), jmpbuf, sizeof jmp_buf);
#endif

	png_init_io(png_ptr, png_FILE);

#ifdef PNG_SET_USER_LIMITS_SUPPORTED
	png_set_user_limits(png_ptr, 2048, 2048);
#endif

	png_read_info(png_ptr, png_info_ptr);

	png_get_IHDR(png_ptr, png_info_ptr, &width, &height, &bit_depth, &color_type,
	 NULL, NULL, NULL);

	if (bit_depth == 16)
		png_set_strip_16(png_ptr);

	if (color_type == PNG_COLOR_TYPE_GRAY || color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
		png_set_gray_to_rgb(png_ptr);
	else if (color_type == PNG_COLOR_TYPE_PALETTE)
		png_set_palette_to_rgb(png_ptr);

	if (png_get_valid(png_ptr, png_info_ptr, PNG_INFO_tRNS))
		png_set_tRNS_to_alpha(png_ptr);
	else if (color_type != PNG_COLOR_TYPE_RGB_ALPHA && color_type != PNG_COLOR_TYPE_GRAY_ALPHA)
	{
#if PNG_LIBPNG_VER < 10207
		png_set_filler(png_ptr, 0xFF, PNG_FILLER_AFTER);
#else
		png_set_add_alpha(png_ptr, 0xFF, PNG_FILLER_AFTER);
#endif
	}

	png_read_update_info(png_ptr, png_info_ptr);

	{
		png_uint_32 i, pitch = png_get_rowbytes(png_ptr, png_info_ptr);
		png_bytep PNG_image = Z_Malloc(pitch*height, PU_CACHE, &texture->data);
		png_bytepp row_pointers = png_malloc(png_ptr, height * sizeof (png_bytep));
		for (i = 0; i < height; i++)
			row_pointers[i] = PNG_image + i*pitch;
		png_read_image(png_ptr, row_pointers);
		png_free(png_ptr, (png_voidp)row_pointers);
	}

	png_destroy_read_struct(&png_ptr, &png_info_ptr, NULL);

	fclose(png_FILE);
	*w = (int)width;
	*h = (int)height;
	return 1;
}
#endif

void RSP_CreateModelTexture(rsp_md2_t *model, INT32 skincolor)
{
	rsp_modeltexture_t *texture = model->texture;
	size_t i, size = 0;

	// get texture size
	if (texture)
		size = (texture->width * texture->height);

	// base texture
	if (!skincolor)
	{
		// doesn't exist?
		if (!texture->data)
			return;

		model->rsp_tex.width = texture->width;
		model->rsp_tex.height = texture->height;

		if (model->rsp_tex.data)
			Z_Free(model->rsp_tex.data);
		model->rsp_tex.data = Z_Malloc(size, PU_SOFTPOLY, NULL);

		for (i = 0; i < size; i++)
		{
			RGBA_t *image = texture->data;
			if (image[i].s.alpha < 1)
				model->rsp_tex.data[i] = TRANSPARENTPIXEL;
			else
				model->rsp_tex.data[i] = NearestColor(image[i].s.red, image[i].s.green, image[i].s.blue);
		}
	}
	else
	{
		// create translations
		rsp_modeltexture_t *blendtexture = model->blendtexture;
		RGBA_t blendcolor;

		model->rsp_transtex[skincolor].width = 1;
		model->rsp_transtex[skincolor].height = 1;
		model->rsp_transtex[skincolor].data = NULL;

		// doesn't exist?
		if (!texture->data)
			return;

		model->rsp_transtex[skincolor].width = texture->width;
		model->rsp_transtex[skincolor].height = texture->height;
		model->rsp_transtex[skincolor].data = Z_Malloc(size, PU_SOFTPOLY, NULL);

		switch (skincolor)		// color
		{
			case SKINCOLOR_WHITE:
				blendcolor = V_GetColor(3);
				break;
			case SKINCOLOR_SILVER:
				blendcolor = V_GetColor(10);
				break;
			case SKINCOLOR_GREY:
				blendcolor = V_GetColor(15);
				break;
			case SKINCOLOR_BLACK:
				blendcolor = V_GetColor(27);
				break;
			case SKINCOLOR_CYAN:
				blendcolor = V_GetColor(215);
				break;
			case SKINCOLOR_TEAL:
				blendcolor = V_GetColor(221);
				break;
			case SKINCOLOR_STEELBLUE:
				blendcolor = V_GetColor(203);
				break;
			case SKINCOLOR_BLUE:
				blendcolor = V_GetColor(232);
				break;
			case SKINCOLOR_PEACH:
				blendcolor = V_GetColor(71);
				break;
			case SKINCOLOR_TAN:
				blendcolor = V_GetColor(79);
				break;
			case SKINCOLOR_PINK:
				blendcolor = V_GetColor(147);
				break;
			case SKINCOLOR_LAVENDER:
				blendcolor = V_GetColor(251);
				break;
			case SKINCOLOR_PURPLE:
				blendcolor = V_GetColor(195);
				break;
			case SKINCOLOR_ORANGE:
				blendcolor = V_GetColor(87);
				break;
			case SKINCOLOR_ROSEWOOD:
				blendcolor = V_GetColor(94);
				break;
			case SKINCOLOR_BEIGE:
				blendcolor = V_GetColor(40);
				break;
			case SKINCOLOR_BROWN:
				blendcolor = V_GetColor(57);
				break;
			case SKINCOLOR_RED:
				blendcolor = V_GetColor(130);
				break;
			case SKINCOLOR_DARKRED:
				blendcolor = V_GetColor(139);
				break;
			case SKINCOLOR_NEONGREEN:
				blendcolor = V_GetColor(184);
				break;
			case SKINCOLOR_GREEN:
				blendcolor = V_GetColor(166);
				break;
			case SKINCOLOR_ZIM:
				blendcolor = V_GetColor(180);
				break;
			case SKINCOLOR_OLIVE:
				blendcolor = V_GetColor(108);
				break;
			case SKINCOLOR_YELLOW:
				blendcolor = V_GetColor(104);
				break;
			case SKINCOLOR_GOLD:
				blendcolor = V_GetColor(115);
				break;

			case SKINCOLOR_SUPER1:
				blendcolor = V_GetColor(97);
				break;
			case SKINCOLOR_SUPER2:
				blendcolor = V_GetColor(100);
				break;
			case SKINCOLOR_SUPER3:
				blendcolor = V_GetColor(103);
				break;
			case SKINCOLOR_SUPER4:
				blendcolor = V_GetColor(113);
				break;
			case SKINCOLOR_SUPER5:
				blendcolor = V_GetColor(116);
				break;

			case SKINCOLOR_TSUPER1:
				blendcolor = V_GetColor(81);
				break;
			case SKINCOLOR_TSUPER2:
				blendcolor = V_GetColor(82);
				break;
			case SKINCOLOR_TSUPER3:
				blendcolor = V_GetColor(84);
				break;
			case SKINCOLOR_TSUPER4:
				blendcolor = V_GetColor(85);
				break;
			case SKINCOLOR_TSUPER5:
				blendcolor = V_GetColor(87);
				break;

			case SKINCOLOR_KSUPER1:
				blendcolor = V_GetColor(122);
				break;
			case SKINCOLOR_KSUPER2:
				blendcolor = V_GetColor(123);
				break;
			case SKINCOLOR_KSUPER3:
				blendcolor = V_GetColor(124);
				break;
			case SKINCOLOR_KSUPER4:
				blendcolor = V_GetColor(125);
				break;
			case SKINCOLOR_KSUPER5:
				blendcolor = V_GetColor(126);
				break;
			default:
				blendcolor = V_GetColor(247);
				break;
		}

		for (i = 0; i < size; i++)
		{
			RGBA_t *image = texture->data;
			RGBA_t *blendimage = blendtexture->data;

			INT32 red, green, blue;
			INT32 tempcolor;
			INT16 tempmult, tempalpha;

			if (!blendimage)
				return;

			if (blendimage[i].s.alpha == 0)
			{
				model->rsp_transtex[skincolor].data[i] = model->rsp_tex.data[i];
				continue;
			}

			tempalpha = -(abs(blendimage[i].s.red-127)-127)*2;
			if (tempalpha > 255)
				tempalpha = 255;
			else if (tempalpha < 0)
				tempalpha = 0;

			tempmult = (blendimage[i].s.red-127)*2;
			if (tempmult > 255)
				tempmult = 255;
			else if (tempmult < 0)
				tempmult = 0;

			tempcolor = (image[i].s.red*(255-blendimage[i].s.alpha))/255 + ((tempmult + ((tempalpha*blendcolor.s.red)/255)) * blendimage[i].s.alpha)/255;
			red = (UINT8)tempcolor;
			tempcolor = (image[i].s.green*(255-blendimage[i].s.alpha))/255 + ((tempmult + ((tempalpha*blendcolor.s.green)/255)) * blendimage[i].s.alpha)/255;
			green = (UINT8)tempcolor;
			tempcolor = (image[i].s.blue*(255-blendimage[i].s.alpha))/255 + ((tempmult + ((tempalpha*blendcolor.s.blue)/255)) * blendimage[i].s.alpha)/255;
			blue = (UINT8)tempcolor;

			model->rsp_transtex[skincolor].data[i] = NearestColor(red, green, blue);
		}
	}
}

void RSP_FreeModelTexture(rsp_md2_t *model)
{
	rsp_modeltexture_t *texture = model->texture;
	model->rsp_tex.width = 1;
	model->rsp_tex.height = 1;

	if (texture)
	{
		if (texture->data)
			Z_Free(texture->data);
		Z_Free(model->texture);
	}
	if (model->rsp_tex.data)
		Z_Free(model->rsp_tex.data);

	model->texture = NULL;
	model->rsp_tex.data = NULL;
}

void RSP_FreeModelBlendTexture(rsp_md2_t *model)
{
	rsp_modeltexture_t *blendtexture = model->blendtexture;
	if (model->blendtexture)
	{
		if (blendtexture->data)
			Z_Free(blendtexture->data);
		Z_Free(model->blendtexture);
	}
	model->blendtexture = NULL;
}

void RSP_LoadModelTexture(rsp_md2_t *model)
{
	rsp_modeltexture_t *texture;
	const char *filename = model->filename;
	int w = 1, h = 1;

	// texture was already loaded, don't do JACK
	if (model->texture)
		return;

	// make new texture
	RSP_FreeModelTexture(model);
	texture = Z_Calloc(sizeof *texture, PU_CACHE, &(model->texture));

#ifdef HAVE_PNG
	if (!(PNG_Load(filename, &w, &h, texture)))
#else
	if (true)
#endif
		return;

	// load!
	texture->width = (INT16)w;
	texture->height = (INT16)h;
	RSP_CreateModelTexture(model, 0);
}

void RSP_LoadModelBlendTexture(rsp_md2_t *model)
{
	rsp_modeltexture_t *blendtexture;
	char *filename = Z_Malloc(strlen(model->filename)+7, PU_SOFTPOLY, NULL);
	int w = 1, h = 1;

	strcpy(filename, model->filename);
	FIL_ForceExtension(filename, "_blend.png");

	// blend texture was already loaded, don't do JACK
	if (model->blendtexture)
		return;

	RSP_FreeModelBlendTexture(model);
	blendtexture = Z_Calloc(sizeof *blendtexture, PU_CACHE, &(model->blendtexture));

#ifdef HAVE_PNG
	if (!(PNG_Load(filename, &w, &h, blendtexture)))
#else
	if (true)
#endif
		return;

	blendtexture->width = (INT16)w;
	blendtexture->height = (INT16)h;

	Z_Free(filename);
}

// Don't spam the console, or the OS with fopen requests!
static boolean nomd2s = false;

void RSP_InitModels(void)
{
	size_t i;
	INT32 s;
	FILE *f;
	char name[18], filename[32];
	float scale, offset;

	for (s = 0; s < MAXSKINS; s++)
	{
		rsp_md2_playermodels[s].scale = -1.0f;
		rsp_md2_playermodels[s].model = NULL;
		rsp_md2_playermodels[s].texture = NULL;
		rsp_md2_playermodels[s].skin = -1;
		rsp_md2_playermodels[s].notfound = true;
		rsp_md2_playermodels[s].error = false;
	}
	for (i = 0; i < NUMSPRITES; i++)
	{
		rsp_md2_models[i].scale = -1.0f;
		rsp_md2_models[i].model = NULL;
		rsp_md2_models[i].texture = NULL;
		rsp_md2_models[i].skin = -1;
		rsp_md2_models[i].notfound = true;
		rsp_md2_models[i].error = false;
	}

	// read the md2.dat file
	//Filename checking fixed ~Monster Iestyn and Golden
	f = fopen(va("%s"PATHSEP"%s", srb2home, "md2.dat"), "rt");

	if (!f)
	{
		CONS_Printf("%s %s\n", M_GetText("Error while loading md2.dat:"), strerror(errno));
		nomd2s = true;
		return;
	}
	while (fscanf(f, "%19s %31s %f %f", name, filename, &scale, &offset) == 4)
	{
		if (stricmp(name, "PLAY") == 0)
		{
			CONS_Printf("MD2 for sprite PLAY detected in md2.dat, use a player skin instead!\n");
			continue;
		}

		for (i = 0; i < NUMSPRITES; i++)
		{
			if (stricmp(name, sprnames[i]) == 0)
			{
				rsp_md2_models[i].scale = scale;
				rsp_md2_models[i].offset = offset;
				rsp_md2_models[i].notfound = false;
				strcpy(rsp_md2_models[i].filename, filename);
				goto md2found;
			}
		}

		for (s = 0; s < MAXSKINS; s++)
		{
			if (stricmp(name, skins[s].name) == 0)
			{
				rsp_md2_playermodels[s].skin = s;
				rsp_md2_playermodels[s].scale = scale;
				rsp_md2_playermodels[s].offset = offset;
				rsp_md2_playermodels[s].notfound = false;
				strcpy(rsp_md2_playermodels[s].filename, filename);
				goto md2found;
			}
		}
		// no sprite/player skin name found?!?
		//CONS_Printf("Unknown sprite/player skin %s detected in md2.dat\n", name);
md2found:
		// move on to next line...
		continue;
	}
	fclose(f);
}

void RSP_AddPlayerModel(int skin) // For MD2's that were added after startup
{
	FILE *f;
	char name[18], filename[32];
	float scale, offset;

	if (nomd2s)
		return;

	//CONS_Printf("RSP_AddPlayerModel()...\n");

	// read the md2.dat file
	//Filename checking fixed ~Monster Iestyn and Golden
	f = fopen(va("%s"PATHSEP"%s", srb2home, "md2.dat"), "rt");

	if (!f)
	{
		CONS_Printf("Error while loading md2.dat\n");
		nomd2s = true;
		return;
	}

	// Check for any MD2s that match the names of player skins!
	while (fscanf(f, "%19s %31s %f %f", name, filename, &scale, &offset) == 4)
	{
		if (stricmp(name, skins[skin].name) == 0)
		{
			rsp_md2_playermodels[skin].skin = skin;
			rsp_md2_playermodels[skin].scale = scale;
			rsp_md2_playermodels[skin].offset = offset;
			rsp_md2_playermodels[skin].notfound = false;
			strcpy(rsp_md2_playermodels[skin].filename, filename);
			goto playermd2found;
		}
	}

	rsp_md2_playermodels[skin].notfound = true;
playermd2found:
	fclose(f);
}

void RSP_AddSpriteModel(size_t spritenum) // For MD2s that were added after startup
{
	FILE *f;
	// name[18] is used to check for names in the md2.dat file that match with sprites or player skins
	// sprite names are always 4 characters long, and names is for player skins can be up to 19 characters long
	char name[18], filename[32];
	float scale, offset;

	if (nomd2s)
		return;

	if (spritenum == SPR_PLAY) // Handled already NEWMD2: Per sprite, per-skin check
		return;

	// Read the md2.dat file
	//Filename checking fixed ~Monster Iestyn and Golden
	f = fopen(va("%s"PATHSEP"%s", srb2home, "md2.dat"), "rt");

	if (!f)
	{
		CONS_Printf("Error while loading md2.dat\n");
		nomd2s = true;
		return;
	}

	// Check for any MD2s that match the names of player skins!
	while (fscanf(f, "%19s %31s %f %f", name, filename, &scale, &offset) == 4)
	{
		if (stricmp(name, sprnames[spritenum]) == 0)
		{
			rsp_md2_models[spritenum].scale = scale;
			rsp_md2_models[spritenum].offset = offset;
			rsp_md2_models[spritenum].notfound = false;
			strcpy(rsp_md2_models[spritenum].filename, filename);
			goto spritemd2found;
		}
	}

	//CONS_Printf("MD2 for sprite %s not found\n", sprnames[spritenum]);
	rsp_md2_models[spritenum].notfound = true;
spritemd2found:
	fclose(f);
}

rsp_md2_t *RSP_ModelAvailable(spritenum_t spritenum, skin_t *skin)
{
	char filename[64];
	rsp_md2_t *md2;

	// invalid sprite number
	if ((unsigned)spritenum >= NUMSPRITES || (unsigned)spritenum == SPR_NULL)
		return NULL;

	if (skin && spritenum == SPR_PLAY) // Use the player MD2 list if the mobj has a skin and is using the player sprites
	{
		md2 = &rsp_md2_playermodels[skin-skins];
		md2->skin = skin-skins;
	}
	else if (spritenum == SPR_PLAY)	// use default model
	{
		md2 = &rsp_md2_playermodels[0];
		md2->skin = 0;
	}
	else
		md2 = &rsp_md2_models[spritenum];

	if (md2->error)
		return NULL; // we already failed loading this before :(
	if (!md2->model)
	{
		//CONS_Debug(DBG_RENDER, "Loading MD2... (%s)", sprnames[spritenum]);
		sprintf(filename, "md2/%s", md2->filename);
		md2->model = RSP_LoadModel(filename);

		if (md2->model)
			model_printModelInfo(md2->model);
		else
		{
			md2->error = true;
			return NULL;
		}
	}

	return md2;
}

boolean RSP_RenderModel(vissprite_t *spr)
{
	INT32 frame;
	rsp_md2_t *md2;

	float tr_x, tr_y;
	float tz;

	mobj_t *mobj = spr->mobj;
	if (!mobj)
		return false;

	// load sprite viewpoint
	if (portalrender)
	{
		RSP_StoreViewpoint();
		RSP_RestoreSpriteViewpoint(spr);
	}

	// transform the origin point
	tr_x = mobj->x - viewx;
	tr_y = mobj->y - viewy;

	// rotation around vertical axis
	tz = (tr_x * viewcos) + (tr_y * viewsin);

	// thing is behind view plane?
	if (tz < FRACUNIT*4)
		return true;

	// Look at HWR_ProjectSprite for more
	{
		rsp_texture_t *texture, sprtex;
		rsp_spritetexture_t *sprtexp;
		INT32 durs = mobj->state->tics;
		INT32 tics = mobj->tics;
		model_frame_t *curr, *next = NULL;
		const UINT8 flip = (UINT8)((mobj->eflags & MFE_VERTICALFLIP) == MFE_VERTICALFLIP);
		spritedef_t *sprdef;
		spriteframe_t *sprframe;
		float finalscale;

		skincolors_t skincolor = SKINCOLOR_NONE;
		UINT8 *translation = NULL;

		md2 = RSP_ModelAvailable(spr->spritenum, (skin_t *)spr->skin);
		if (!md2)
		{
			// restore previous viewpoint
			if (portalrender)
				RSP_RestoreViewpoint();
			return false;
		}

		// texture blending
		if (mobj->color)
			skincolor = (skincolors_t)mobj->color;
		else if (mobj->sprite == SPR_PLAY) // Looks like a player, but doesn't have a color? Get rid of green sonic syndrome.
			skincolor = (skincolors_t)skins[0].prefcolor;

		// set translation
		if ((mobj->flags & MF_BOSS) && (mobj->flags2 & MF2_FRET) && (leveltime & 1)) // Bosses "flash"
		{
			if (mobj->type == MT_CYBRAKDEMON)
				translation = R_GetTranslationColormap(TC_ALLWHITE, 0, GTC_CACHE);
			else if (mobj->type == MT_METALSONIC_BATTLE)
				translation = R_GetTranslationColormap(TC_METALSONIC, 0, GTC_CACHE);
			else
				translation = R_GetTranslationColormap(TC_BOSS, 0, GTC_CACHE);
		}

		// load normal texture
		if (!md2->texture)
			RSP_LoadModelTexture(md2);

		// load blend texture
		if (!md2->blendtexture)
			RSP_LoadModelBlendTexture(md2);

		// load translated texture
		if ((skincolor > 0) && (md2->rsp_transtex[skincolor].data == NULL))
			RSP_CreateModelTexture(md2, skincolor);

		// use corresponding texture for this model
		if (md2->rsp_transtex[skincolor].data != NULL)
			texture = &md2->rsp_transtex[skincolor];
		else
			texture = &md2->rsp_tex;

		if (mobj->skin && mobj->sprite == SPR_PLAY)
			sprdef = &((skin_t *)mobj->skin)->spritedef;
		else
			sprdef = &sprites[mobj->sprite];

		sprframe = &sprdef->spriteframes[mobj->frame & FF_FRAMEMASK];

		if (!texture->data)
		{
			unsigned rot;
			angle_t ang;

			if (sprframe->rotate)
			{
				// choose a different rotation based on player view
				ang = R_PointToAngle(mobj->x, mobj->y); // uses viewx,viewy
				rot = (ang-mobj->angle+ANGLE_202h)>>29;
			}
			else
				rot = 0;	// use single rotation for all views

			// sprite translation
			if ((mobj->flags & MF_BOSS) && (mobj->flags2 & MF2_FRET) && (leveltime & 1)) // Bosses "flash"
				;	// already set
			else if (mobj->color)
			{
				// New colormap stuff for skins Tails 06-07-2002
				if (mobj->skin && mobj->sprite == SPR_PLAY) // This thing is a player!
				{
					size_t skinnum = (skin_t*)mobj->skin-skins;
					translation = R_GetTranslationColormap((INT32)skinnum, mobj->color, GTC_CACHE);
				}
				else
					translation = R_GetTranslationColormap(TC_DEFAULT, mobj->color ? mobj->color : SKINCOLOR_GREEN, GTC_CACHE);
			}
			else if (mobj->sprite == SPR_PLAY) // Looks like a player, but doesn't have a color? Get rid of green sonic syndrome.
				translation = R_GetTranslationColormap(TC_DEFAULT, skins[0].prefcolor, GTC_CACHE);
			else
				translation = NULL;

			// get rsp_texture
			sprtexp = &sprframe->rsp_texture[rot];
			if (!sprtexp)
			{
				// restore previous viewpoint
				if (portalrender)
					RSP_RestoreViewpoint();
				return false;
			}

			sprtex.width = sprtexp->width;
			sprtex.height = sprtexp->height;
			sprtex.data = sprtexp->data;
			texture = &sprtex;
		}

		if (mobj->frame & FF_ANIMATE)
		{
			// set duration and tics to be the correct values for FF_ANIMATE states
			durs = mobj->state->var2;
			tics = mobj->anim_duration;
		}

		//FIXME: this is not yet correct
		if (frame < 0)
			frame = 0;
		frame = (mobj->frame & FF_FRAMEMASK) % md2->model->header.numFrames;
		curr = &md2->model->frames[frame];
		if (cv_modelinterpolation.value && tics <= durs)
		{
			// frames are handled differently for states with FF_ANIMATE, so get the next frame differently for the interpolation
			if (mobj->frame & FF_ANIMATE)
			{
				UINT32 nextframe = (mobj->frame & FF_FRAMEMASK) + 1;
				if (nextframe >= (UINT32)mobj->state->var1)
					nextframe = (mobj->state->frame & FF_FRAMEMASK);
				nextframe %= md2->model->header.numFrames;
				next = &md2->model->frames[nextframe];
			}
			else
			{
				if (mobj->state->nextstate != S_NULL && states[mobj->state->nextstate].sprite != SPR_NULL
					&& !(mobj->player && (mobj->state->nextstate == S_PLAY_TAP1 || mobj->state->nextstate == S_PLAY_TAP2) && mobj->state == &states[S_PLAY_STND]))
				{
					const UINT32 nextframe = (states[mobj->state->nextstate].frame & FF_FRAMEMASK) % md2->model->header.numFrames;
					next = &md2->model->frames[nextframe];
				}
			}
		}

		// SRB2CBTODO: MD2 scaling support
		finalscale = md2->scale * FIXED_TO_FLOAT(mobj->scale);

		// Render individual triangles
		{
			rsp_triangle_t triangle;
			model_triangleVertex_t *pvert;
			model_triangleVertex_t *nvert;
			float theta, cs, sn;
			fixed_t model_angle;
			UINT16 i, j;

			// clear triangle struct
			// avoid undefined behaviour.............
			memset(&triangle, 0x00, sizeof(rsp_triangle_t));

			// set model angle
			model_angle = AngleFixed(mobj->angle);
			if (!sprframe->rotate)
				model_angle = AngleFixed((R_PointToAngle(mobj->x, mobj->y))-ANGLE_180);

			// model angle in radians
			theta = -(FIXED_TO_FLOAT(model_angle) * M_PI / 180.0f);
			cs = cos(theta);
			sn = sin(theta);

			// render every triangle
			for (i = 0; i < md2->model->header.numTriangles; ++i)
			{
				for (j = 0; j < 3; ++j)
				{
					float x, y, z;
					float s, t;

					x = FIXED_TO_FLOAT(mobj->x);
					y = FIXED_TO_FLOAT(mobj->y) + md2->offset;

					if (mobj->eflags & MFE_VERTICALFLIP)
						z = FIXED_TO_FLOAT(mobj->z + mobj->height);
					else
						z = FIXED_TO_FLOAT(mobj->z);

					s = (float)md2->model->texCoords[md2->model->triangles[i].textureIndices[j]].s;
					t = (float)md2->model->texCoords[md2->model->triangles[i].textureIndices[j]].t;

					pvert = &curr->vertices[md2->model->triangles[i].vertexIndices[j]];
					if (next)
						nvert = &next->vertices[md2->model->triangles[i].vertexIndices[j]];

					if (!next)
					{
						float vx = (pvert->vertex[0] * finalscale/2.0f);
						float vy = (pvert->vertex[1] * finalscale/2.0f);
						float vz = (pvert->vertex[2] * finalscale/2.0f);

						// QUICK MATHS
						float mx = (vx * cs) - (vy * sn);
						float my = (vx * sn) + (vy * cs);
						float mz = vz * (flip ? -1 : 1);

						RSP_MakeVector4(triangle.vertices[j].position,
							 x + mx,
							-z + mz,
							-y + my
						);
					}
					else
					{
						// Interpolate
						float px1 = (pvert->vertex[0] * finalscale/2.0f);
						float px2 = (nvert->vertex[0] * finalscale/2.0f);
						float py1 = (pvert->vertex[1] * finalscale/2.0f);
						float py2 = (nvert->vertex[1] * finalscale/2.0f);
						float pz1 = (pvert->vertex[2] * finalscale/2.0f);
						float pz2 = (nvert->vertex[2] * finalscale/2.0f);
						float pol = 0.0f;

						// QUICK MATHS
						float mx1 = (px1 * cs) - (py1 * sn);
						float my1 = (px1 * sn) + (py1 * cs);
						float mz1 = pz1 * (flip ? -1 : 1);

						// QUICK MATHS
						float mx2 = (px2 * cs) - (py2 * sn);
						float my2 = (px2 * sn) + (py2 * cs);
						float mz2 = pz2 * (flip ? -1 : 1);

						if (durs != 0 && durs != -1 && tics != -1) // don't interpolate if instantaneous or infinite in length
						{
							UINT32 newtime = (durs - tics); // + 1;

							pol = (newtime)/(float)durs;

							if (pol > 1.0f)
								pol = 1.0f;

							if (pol < 0.0f)
								pol = 0.0f;
						}

						RSP_MakeVector4(triangle.vertices[j].position,
							 x + (mx1 + pol * (mx2 - mx1)),
							-z + (mz1 + pol * (mz2 - mz1)),
							-y + (my1 + pol * (my2 - my1))
						);
					}

					triangle.vertices[j].uv.u = (s + 0.5f) / md2->model->header.skinWidth;
					triangle.vertices[j].uv.v = (t + 0.5f) / md2->model->header.skinHeight;
				}

				triangle.texture = NULL;
				if (texture->data)
					triangle.texture = texture;

				triangle.colormap = spr->colormap;
				if (spr->extra_colormap)
				{
					if (!dc_colormap)
						triangle.colormap = spr->extra_colormap->colormap;
					else
						triangle.colormap = &spr->extra_colormap->colormap[triangle.colormap - colormaps];
				}

				triangle.translation = translation;
				triangle.transmap = spr->transmap;
				triangle.flipped = flip;

				RSP_TransformTriangle(&triangle);
			}
		}
	}

	// restore previous viewpoint
	if (portalrender)
		RSP_RestoreViewpoint();
	RSP_ClearDepthBuffer();
	return true;
}

boolean RSP_RenderModelSimple(spritenum_t spritenum, UINT32 framenum, float x, float y, float z, float model_angle, skincolors_t skincolor, skin_t *skin, boolean flip)
{
	rsp_md2_t *md2;
	rsp_texture_t *texture, sprtex;
	rsp_spritetexture_t *sprtexp;
	model_frame_t *curr;
	spritedef_t *sprdef;
	spriteframe_t *sprframe;
	float finalscale;

	UINT8 *translation = NULL;
	md2 = RSP_ModelAvailable(spritenum, skin);
	if (!md2)
		return false;

	// load normal texture
	if (!md2->texture)
		RSP_LoadModelTexture(md2);

	// load blend texture
	if (!md2->blendtexture)
		RSP_LoadModelBlendTexture(md2);

	// load translated texture
	if ((skincolor > 0) && (md2->rsp_transtex[skincolor].data == NULL))
		RSP_CreateModelTexture(md2, skincolor);

	// use corresponding texture for this model
	if (md2->rsp_transtex[skincolor].data != NULL)
		texture = &md2->rsp_transtex[skincolor];
	else
		texture = &md2->rsp_tex;

	if (skin && spritenum == SPR_PLAY)
		sprdef = &skin->spritedef;
	else
		sprdef = &sprites[spritenum];

	sprframe = &sprdef->spriteframes[framenum];

	if (!texture->data)
	{
		// sprite translation
		if (skincolor)
		{
			// New colormap stuff for skins Tails 06-07-2002
			if (skin && spritenum == SPR_PLAY) // This thing is a player!
			{
				size_t skinnum = skin-skins;
				translation = R_GetTranslationColormap((INT32)skinnum, skincolor, GTC_CACHE);
			}
			else
				translation = R_GetTranslationColormap(TC_DEFAULT, skincolor ? skincolor : SKINCOLOR_GREEN, GTC_CACHE);
		}
		else
			translation = NULL;

		// get rsp_texture
		sprtexp = &sprframe->rsp_texture[0];
		if (!sprtexp)
			return false;

		sprtex.width = sprtexp->width;
		sprtex.height = sprtexp->height;
		sprtex.data = sprtexp->data;
		texture = &sprtex;
	}

	//FIXME: this is not yet correct
	curr = &md2->model->frames[framenum];

	// SRB2CBTODO: MD2 scaling support
	finalscale = md2->scale;

	// Render individual triangles
	{
		rsp_triangle_t triangle;
		model_triangleVertex_t *vert;
		float theta, cs, sn;
		UINT16 i, j;

		// clear triangle struct
		// avoid undefined behaviour.............
		memset(&triangle, 0x00, sizeof(rsp_triangle_t));

		// calculate model orientation
		theta = -(model_angle * M_PI / 180.0f);
		cs = cos(theta);
		sn = sin(theta);

		y += md2->offset;

		// render every triangle
		for (i = 0; i < md2->model->header.numTriangles; ++i)
		{
			for (j = 0; j < 3; ++j)
			{
				float vx, vy, vz;
				float mx, my, mz;
				float s = (float)md2->model->texCoords[md2->model->triangles[i].textureIndices[j]].s;
				float t = (float)md2->model->texCoords[md2->model->triangles[i].textureIndices[j]].t;

				vert = &curr->vertices[md2->model->triangles[i].vertexIndices[j]];
				vx = (vert->vertex[0] * finalscale/2.0f);
				vy = (vert->vertex[1] * finalscale/2.0f);
				vz = (vert->vertex[2] * finalscale/2.0f);

				// QUICK MATHS
				mx = (vx * cs) - (vy * sn);
				my = (vx * sn) + (vy * cs);
				mz = vz * (flip ? -1 : 1);

				RSP_MakeVector4(triangle.vertices[j].position,
					 x + mx,
					-z + mz,
					-y + my
				);

				triangle.vertices[j].uv.u = (s + 0.5f) / md2->model->header.skinWidth;
				triangle.vertices[j].uv.v = (t + 0.5f) / md2->model->header.skinHeight;
			}

			triangle.texture = NULL;
			if (texture->data)
				triangle.texture = texture;

			triangle.colormap = NULL;
			triangle.translation = translation;
			triangle.transmap = NULL;
			triangle.flipped = flip;

			RSP_TransformTriangle(&triangle);
		}
	}

	RSP_ClearDepthBuffer();
	return true;
}
