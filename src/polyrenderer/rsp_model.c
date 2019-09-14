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
#include "../r_things.h"
#include "../v_video.h"
#include "../r_draw.h"
#include "../p_tick.h"
#include "../r_model.h"

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
	return LoadModel(va("%s"PATHSEP"%s", srb2home, filename), PU_STATIC);
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

// Define for getting accurate color brightness readings according to how the human eye sees them.
// https://en.wikipedia.org/wiki/Relative_luminance
// 0.2126 to red
// 0.7152 to green
// 0.0722 to blue
// (See this same define in hw_md2.c!)
#define SETBRIGHTNESS(brightness,r,g,b) \
	brightness = (UINT8)(((1063*(UINT16)(r))/5000) + ((3576*(UINT16)(g))/5000) + ((361*(UINT16)(b))/5000))

//#define HAVETCRAINDBOW
#define SKIN_RAMP_LENGTH 16

void RSP_CreateModelTexture(rsp_md2_t *model, INT32 skinnum, INT32 skincolor)
{
	rsp_modeltexture_t *texture = model->texture;
	size_t i, size = 0;

	// vanilla port
	UINT8 translation[SKIN_RAMP_LENGTH];
	const UINT8 skinbasecolors[] = {
		0x00, // SKINCOLOR_WHITE
		0x03, // SKINCOLOR_SILVER
		0x08, // SKINCOLOR_GREY
		0x18, // SKINCOLOR_BLACK
		0xd0, // SKINCOLOR_CYAN
		0xdc, // SKINCOLOR_TEAL
		0xc8, // SKINCOLOR_STEELBLUE
		0xe2, // SKINCOLOR_BLUE
		0x40, // SKINCOLOR_PEACH
		0x48, // SKINCOLOR_TAN
		0x90, // SKINCOLOR_PINK
		0xf8, // SKINCOLOR_LAVENDER
		0xc0, // SKINCOLOR_PURPLE
		0x52, // SKINCOLOR_ORANGE
		0x5c, // SKINCOLOR_ROSEWOOD
		0x20, // SKINCOLOR_BEIGE
		0x30, // SKINCOLOR_BROWN
		0x7d, // SKINCOLOR_RED
		0x85, // SKINCOLOR_DARKRED
		0xb8, // SKINCOLOR_NEONGREEN
		0xa0, // SKINCOLOR_GREEN
		0xb0, // SKINCOLOR_ZIM
		0x69, // SKINCOLOR_OLIVE
		0x67, // SKINCOLOR_YELLOW
		0x70, // SKINCOLOR_GOLD
	};

	memset(translation, 0, sizeof(translation));

	// get texture size
	if (texture)
		size = (texture->width * texture->height);

	// base texture
	if (!skincolor)
	{
		RGBA_t *image = texture->data;
		// doesn't exist?
		if (!image)
			return;

		model->rsp_tex.width = texture->width;
		model->rsp_tex.height = texture->height;

		if (model->rsp_tex.data)
			Z_Free(model->rsp_tex.data);
		model->rsp_tex.data = Z_Calloc(size, PU_SOFTPOLY, NULL);

		if (skinnum < TC_DEFAULT)
		{
			model->rsp_transtex[skincolor].width = texture->width;
			model->rsp_transtex[skincolor].height = texture->height;
			model->rsp_transtex[skincolor].data = Z_Calloc(size, PU_SOFTPOLY, NULL);
		}

		for (i = 0; i < size; i++)
		{
			if (image[i].s.alpha < 1)
				model->rsp_tex.data[i] = TRANSPARENTPIXEL;
			else if (skinnum == TC_BOSS)
			{
				// Pure black turns into white
				if (image[i].s.red == 0 && image[i].s.green == 0 && image[i].s.blue == 0)
				{
					model->rsp_transtex[skincolor].data[i] = 0;
				}
				else
				{
					model->rsp_transtex[skincolor].data[i] = NearestColor(image[i].s.red, image[i].s.green, image[i].s.blue);
				}
			}
			else if (skinnum == TC_METALSONIC)
			{
				// Turn everything below a certain blue threshold white
				if (image[i].s.red == 0 && image[i].s.green == 0 && image[i].s.blue <= 82)
				{
					model->rsp_transtex[skincolor].data[i] = 0;
				}
				else
				{
					model->rsp_transtex[skincolor].data[i] = NearestColor(image[i].s.red, image[i].s.green, image[i].s.blue);
				}
			}
			else if (skinnum == TC_ALLWHITE)
			{
				// Turn everything white
				model->rsp_transtex[skincolor].data[i] = 0;
			}
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
		model->rsp_transtex[skincolor].data = Z_Calloc(size, PU_SOFTPOLY, NULL);

		blendcolor = V_GetColor(0); // initialize
		if (skincolor != SKINCOLOR_NONE)
		{
			// -----------------------------------------------------------------
			// Sal: BECAUSE SRB2 SUCKS AND TRIES TO GENERATE THESE ON DEMAND
			// INSTEAD OF JUST COMPILING THEM ALL INTO A TABLE LIKE KART DOES IT
			// I HAVE TO USE ALL OF THIS DUPLICATED CODE FROM R_DRAW
			// AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA
			// -----------------------------------------------------------------

			switch (skincolor)
			{
				case SKINCOLOR_SILVER:
				case SKINCOLOR_GREY:
				case SKINCOLOR_PEACH:
				case SKINCOLOR_BEIGE:
				case SKINCOLOR_BROWN:
				case SKINCOLOR_RED:
				case SKINCOLOR_GREEN:
				case SKINCOLOR_BLUE:
					// 16 color ramp
					for (i = 0; i < SKIN_RAMP_LENGTH; i++)
						translation[i] = (UINT8)(skinbasecolors[skincolor - 1] + i);
					break;

				case SKINCOLOR_ORANGE:
					// 14 colors of orange + brown
					for (i = 0; i < SKIN_RAMP_LENGTH-2; i++)
						translation[i] = (UINT8)(skinbasecolors[skincolor - 1] + i);
					for (i = 0; i < 2; i++)
						translation[i+SKIN_RAMP_LENGTH-2] = (UINT8)(152 + i);
					break;

				case SKINCOLOR_CYAN:
					// 12 color ramp
					for (i = 0; i < SKIN_RAMP_LENGTH; i++)
						translation[i] = (UINT8)(skinbasecolors[skincolor - 1] + (12*i/SKIN_RAMP_LENGTH));
					break;

				case SKINCOLOR_WHITE:
				case SKINCOLOR_BLACK:
				case SKINCOLOR_STEELBLUE:
				case SKINCOLOR_PINK:
				case SKINCOLOR_LAVENDER:
				case SKINCOLOR_PURPLE:
				case SKINCOLOR_DARKRED:
				case SKINCOLOR_ZIM:
				case SKINCOLOR_YELLOW:
				case SKINCOLOR_GOLD:
					// 8 color ramp
					for (i = 0; i < SKIN_RAMP_LENGTH; i++)
						translation[i] = (UINT8)(skinbasecolors[skincolor - 1] + (i >> 1));
					break;

				case SKINCOLOR_TEAL:
					// 5 color ramp
					for (i = 0; i < SKIN_RAMP_LENGTH; i++)
					{
						if (5*i/16 == 0)
							translation[i] = 0xf7;
						else
							translation[i] = (UINT8)(skinbasecolors[skincolor - 1] + (5*i/SKIN_RAMP_LENGTH) - 1);
					}
					break;

				case SKINCOLOR_OLIVE:
					// 7 color ramp
					for (i = 0; i < SKIN_RAMP_LENGTH; i++)
						translation[i] = (UINT8)(skinbasecolors[skincolor - 1] + (7*i/SKIN_RAMP_LENGTH));
					break;

				case SKINCOLOR_TAN:
					// 16 color ramp, from two color ranges
					for (i = 0; i < SKIN_RAMP_LENGTH/2; i++) // Peach half
						translation[i] = (UINT8)(skinbasecolors[skincolor - 1] + i);
					for (i = 0; i < SKIN_RAMP_LENGTH/2; i++) // Brown half
						translation[i+8] = (UINT8)(48 + i);
					break;

				case SKINCOLOR_ROSEWOOD:
					// 12 color ramp, from two color ranges!
					for (i = 0; i < 6; i++) // Orange ...third?
						translation[i] = (UINT8)(skinbasecolors[skincolor - 1] + (12*i/SKIN_RAMP_LENGTH));
					for (i = 0; i < 10; i++) // Rosewood two-thirds-ish
						translation[i+6] = (UINT8)(152 + (12*i/SKIN_RAMP_LENGTH));
					break;

				case SKINCOLOR_NEONGREEN:
					// Multi-color ramp
					translation[0] = 0xA0; // Brighter green
					for (i = 0; i < SKIN_RAMP_LENGTH-1; i++) // Neon Green
						translation[i+1] = (UINT8)(skinbasecolors[skincolor - 1] + (6*i/(SKIN_RAMP_LENGTH-1)));
					break;

				// Super colors, from lightest to darkest!
				case SKINCOLOR_SUPER1:
					// Super White
					for (i = 0; i < 10; i++)
						translation[i] = 120; // True white
					for (; i < SKIN_RAMP_LENGTH; i++) // White-yellow fade
						translation[i] = (UINT8)(96 + (i-10));
					break;

				case SKINCOLOR_SUPER2:
					// Super Bright
					for (i = 0; i < 5; i++) // White-yellow fade
						translation[i] = (UINT8)(96 + i);
					translation[5] = 112; // Golden shine
					for (i = 0; i < 8; i++) // Yellow
						translation[i+6] = (UINT8)(101 + (i>>1));
					for (i = 0; i < 2; i++) // With a fine golden finish! :3
						translation[i+14] = (UINT8)(113 + i);
					break;

				case SKINCOLOR_SUPER3:
					// Super Yellow
					for (i = 0; i < 3; i++) // White-yellow fade
						translation[i] = (UINT8)(98 + i);
					translation[3] = 112; // Golden shine
					for (i = 0; i < 8; i++) // Yellow
						translation[i+4] = (UINT8)(101 + (i>>1));
					for (i = 0; i < 4; i++) // With a fine golden finish! :3
						translation[i+12] = (UINT8)(113 + i);
					break;

				case SKINCOLOR_SUPER4:
					// "The SSNTails"
					translation[0] = 112; // Golden shine
					for (i = 0; i < 8; i++) // Yellow
						translation[i+1] = (UINT8)(101 + (i>>1));
					for (i = 0; i < 7; i++) // With a fine golden finish! :3
						translation[i+9] = (UINT8)(113 + i);
					break;

				case SKINCOLOR_SUPER5:
					// Golden Delicious
					for (i = 0; i < 8; i++) // Yellow
						translation[i] = (UINT8)(101 + (i>>1));
					for (i = 0; i < 7; i++) // With a fine golden finish! :3
						translation[i+8] = (UINT8)(113 + i);
					translation[15] = 155;
					break;

				// Super Tails
				case SKINCOLOR_TSUPER1:
					for (i = 0; i < 10; i++) // white
						translation[i] = 120;
					for (; i < SKIN_RAMP_LENGTH; i++) // orange
						translation[i] = (UINT8)(80 + (i-10));
					break;

				case SKINCOLOR_TSUPER2:
					for (i = 0; i < 4; i++) // white
						translation[i] = 120;
					for (; i < SKIN_RAMP_LENGTH; i++) // orange
						translation[i] = (UINT8)(80 + ((i-4)>>1));
					break;

				case SKINCOLOR_TSUPER3:
					translation[0] = 120; // pure white
					translation[1] = 120;
					for (i = 2; i < SKIN_RAMP_LENGTH; i++) // orange
						translation[i] = (UINT8)(80 + ((i-2)>>1));
					break;

				case SKINCOLOR_TSUPER4:
					translation[0] = 120; // pure white
					for (i = 1; i < 9; i++) // orange
						translation[i] = (UINT8)(80 + (i-1));
					for (; i < SKIN_RAMP_LENGTH; i++) // gold
						translation[i] = (UINT8)(115 + (5*(i-9)/7));
					break;

				case SKINCOLOR_TSUPER5:
					for (i = 0; i < 8; i++) // orange
						translation[i] = (UINT8)(80 + i);
					for (; i < SKIN_RAMP_LENGTH; i++) // gold
						translation[i] = (UINT8)(115 + (5*(i-8)/8));
					break;

				// Super Knuckles
				case SKINCOLOR_KSUPER1:
					for (i = 0; i < SKIN_RAMP_LENGTH; i++)
						translation[i] = (UINT8)(120 + (i >> 2));
					break;

				case SKINCOLOR_KSUPER2:
					for (i = 0; i < SKIN_RAMP_LENGTH; i++)
						translation[i] = (UINT8)(120 + (6*i/SKIN_RAMP_LENGTH));
					break;

				case SKINCOLOR_KSUPER3:
					for (i = 0; i < SKIN_RAMP_LENGTH; i++)
						translation[i] = (UINT8)(120 + (i >> 1));
					break;

				case SKINCOLOR_KSUPER4:
					for (i = 0; i < SKIN_RAMP_LENGTH; i++)
						translation[i] = (UINT8)(121 + (i >> 1));
					break;

				case SKINCOLOR_KSUPER5:
					for (i = 0; i < SKIN_RAMP_LENGTH; i++)
						translation[i] = (UINT8)(122 + (i >> 1));
					break;

				default:
					I_Error("Invalid skin color #%hu.", (UINT16)skincolor);
					break;
			}
		}

		for (i = 0; i < size; i++)
		{
			RGBA_t *image = texture->data;
			RGBA_t *blendimage = blendtexture->data;

			if (image[i].s.alpha < 1)
				model->rsp_transtex[skincolor].data[i] = TRANSPARENTPIXEL;
			else if (skinnum == TC_BOSS)
			{
				// Pure black turns into white
				if (image[i].s.red == 0 && image[i].s.green == 0 && image[i].s.blue == 0)
				{
					model->rsp_transtex[skincolor].data[i] = 0;
				}
				else
				{
					model->rsp_transtex[skincolor].data[i] = NearestColor(image[i].s.red, image[i].s.green, image[i].s.blue);
				}
			}
			else if (skinnum == TC_METALSONIC)
			{
				// Turn everything below a certain blue threshold white
				if (image[i].s.red == 0 && image[i].s.green == 0 && image[i].s.blue <= 82)
				{
					model->rsp_transtex[skincolor].data[i] = 0;
				}
				else
				{
					model->rsp_transtex[skincolor].data[i] = NearestColor(image[i].s.red, image[i].s.green, image[i].s.blue);
				}
			}
			else if (skinnum == TC_ALLWHITE)
			{
				// Turn everything white
				model->rsp_transtex[skincolor].data[i] = 0;
			}
			else
			{
				UINT16 brightness;

				// Don't bother with blending the pixel if the alpha of the blend pixel is 0
#ifdef HAVETCRAINDBOW
				if (skinnum == TC_RAINBOW)
				{
					if (image[i].s.alpha == 0 && blendimage[i].s.alpha == 0)
					{
						model->rsp_transtex[skincolor].data[i] = TRANSPARENTPIXEL;
						continue;
					}
					else
					{
						UINT16 imagebright, blendbright;
						SETBRIGHTNESS(imagebright,image[i].s.red,image[i].s.green,image[i].s.blue);
						SETBRIGHTNESS(blendbright,blendimage[i].s.red,blendimage[i].s.green,blendimage[i].s.blue);
						// slightly dumb average between the blend image color and base image colour, usually one or the other will be fully opaque anyway
						brightness = (imagebright*(255-blendimage[i].s.alpha))/255 + (blendbright*blendimage[i].s.alpha)/255;
					}
				}
				else
#endif
				{
					if (blendimage[i].s.alpha == 0)
					{
						model->rsp_transtex[skincolor].data[i] = model->rsp_tex.data[i];
						continue;
					}
					else
					{
						SETBRIGHTNESS(brightness,blendimage[i].s.red,blendimage[i].s.green,blendimage[i].s.blue);
					}
				}

				// Calculate a sort of "gradient" for the skincolor
				// (Me splitting this into a function didn't work, so I had to ruin this entire function's groove...)
				{
					RGBA_t nextcolor;
					UINT8 firsti, secondi, mul;
					UINT32 r, g, b;

#ifdef HAVETCRAINDBOW
					// Rainbow needs to find the closest match to the textures themselves, instead of matching brightnesses to other colors.
					// Ensue horrible mess.
					if (skinnum == TC_RAINBOW)
					{
						UINT16 brightdif = 256;
						UINT8 colorbrightnesses[16];
						INT32 compare, m, d;
						UINT8 i;

						// Ignore pure white & pitch black
						if (brightness > 253 || brightness < 2)
						{
							model->rsp_transtex[skincolor].data[i] = NearestColor(image[i].s.red,image[i].s.green,image[i].s.blue);
							continue;
						}

						firsti = 0;
						mul = 0;

						for (i = 0; i < 16; i++)
						{
							RGBA_t tempc = V_GetColor(translation[i]);
							SETBRIGHTNESS(colorbrightnesses[i], tempc.s.red, tempc.s.green, tempc.s.blue); // store brightnesses for comparison
						}

						for (i = 0; i < 16; i++)
						{
							if (brightness > colorbrightnesses[i]) // don't allow greater matches (because calculating a makeshift gradient for this is already a huge mess as is)
								continue;
							compare = abs((INT16)(colorbrightnesses[i]) - (INT16)(brightness));
							if (compare < brightdif)
							{
								brightdif = (UINT16)compare;
								firsti = i; // best matching color that's equal brightness or darker
							}
						}

						secondi = firsti+1; // next color in line
						if (secondi == 16)
						{
							m = (INT16)brightness; // - 0;
							d = (INT16)colorbrightnesses[firsti]; // - 0;
						}
						else
						{
							m = (INT16)brightness - (INT16)colorbrightnesses[secondi];
							d = (INT16)colorbrightnesses[firsti] - (INT16)colorbrightnesses[secondi];
						}

						if (m >= d)
							m = d-1;

						// calculate the "gradient" multiplier based on how close this color is to the one next in line
						if (m <= 0 || d <= 0)
							mul = 0;
						else
							mul = 15 - ((m * 16) / d);
					}
					else
#endif
					{
						// Thankfully, it's normally way more simple.
						// Just convert brightness to a skincolor value, use remainder to find the gradient multipler
						firsti = ((UINT8)(255-brightness) / 16);
						secondi = firsti+1;
						mul = ((UINT8)(255-brightness) % 16);
					}

					blendcolor = V_GetColor(translation[firsti]);

					if (mul > 0 // If it's 0, then we only need the first color.
						&& translation[firsti] != translation[secondi]) // Some colors have duplicate colors in a row, so let's just save the process
					{
						if (secondi == 16) // blend to black
							nextcolor = V_GetColor(31);
						else
							nextcolor = V_GetColor(translation[secondi]);

						// Find difference between points
						r = (UINT32)(nextcolor.s.red - blendcolor.s.red);
						g = (UINT32)(nextcolor.s.green - blendcolor.s.green);
						b = (UINT32)(nextcolor.s.blue - blendcolor.s.blue);

						// Find the gradient of the two points
						r = ((mul * r) / 16);
						g = ((mul * g) / 16);
						b = ((mul * b) / 16);

						// Add gradient value to color
						blendcolor.s.red += r;
						blendcolor.s.green += g;
						blendcolor.s.blue += b;
					}
				}

#ifdef HAVETCRAINDBOW
				if (skinnum == TC_RAINBOW)
				{
					UINT32 tempcolor;
					UINT16 colorbright;
					UINT8 red, green, blue;

					SETBRIGHTNESS(colorbright,blendcolor.s.red,blendcolor.s.green,blendcolor.s.blue);
					if (colorbright == 0)
						colorbright = 1; // no dividing by 0 please

					tempcolor = (brightness * blendcolor.s.red) / colorbright;
					tempcolor = min(255, tempcolor);
					red = (UINT8)tempcolor;

					tempcolor = (brightness * blendcolor.s.green) / colorbright;
					tempcolor = min(255, tempcolor);
					green = (UINT8)tempcolor;

					tempcolor = (brightness * blendcolor.s.blue) / colorbright;
					tempcolor = min(255, tempcolor);
					blue = (UINT8)tempcolor;
					model->rsp_transtex[skincolor].data[i] = NearestColor(red, green, blue);
				}
				else
#endif
				{
					// Color strength depends on image alpha
					INT32 tempcolor;
					UINT8 red, green, blue;

					tempcolor = ((image[i].s.red * (255-blendimage[i].s.alpha)) / 255) + ((blendcolor.s.red * blendimage[i].s.alpha) / 255);
					tempcolor = min(255, tempcolor);
					red = (UINT8)tempcolor;

					tempcolor = ((image[i].s.green * (255-blendimage[i].s.alpha)) / 255) + ((blendcolor.s.green * blendimage[i].s.alpha) / 255);
					tempcolor = min(255, tempcolor);
					green = (UINT8)tempcolor;

					tempcolor = ((image[i].s.blue * (255-blendimage[i].s.alpha)) / 255) + ((blendcolor.s.blue * blendimage[i].s.alpha) / 255);
					tempcolor = min(255, tempcolor);
					blue = (UINT8)tempcolor;
					model->rsp_transtex[skincolor].data[i] = NearestColor(red, green, blue);
				}
			}
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

void RSP_LoadModelTexture(rsp_md2_t *model, INT32 skinnum)
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
	RSP_CreateModelTexture(model, skinnum, 0);
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
	}
	for (i = 0; i < NUMSPRITES; i++)
	{
		rsp_md2_models[i].scale = -1.0f;
		rsp_md2_models[i].model = NULL;
		rsp_md2_models[i].texture = NULL;
		rsp_md2_models[i].skin = -1;
		rsp_md2_models[i].notfound = true;
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
			//CONS_Printf("MD2 for sprite PLAY detected in md2.dat, use a player skin instead!\n");
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

	if (!md2->model)
	{
		//CONS_Debug(DBG_RENDER, "Loading MD2... (%s)", sprnames[spritenum]);
		sprintf(filename, "md2/%s", md2->filename);
		md2->model = RSP_LoadModel(filename);

		if (!md2->model)
		{
			md2->notfound = true;
			return NULL;
		}
	}

	return md2;
}

// Macros to improve code readability
#define MD3_XYZ_SCALE   (1.0f / 64.0f)
#define VERTEX_OFFSET   ((i * 9) + (j * 3))		/* (i * 9) = (XYZ coords * vertex count) */
#define UV_OFFSET       ((i * 6) + (j * 2))		/* (i * 6) = (UV coords * vertex count) */

static boolean RSP_CanInterpolateModel(mobj_t *mobj, model_t *model)
{
	if (cv_modelinterpolation.value == 2)
		return true;
	return model->interpolate[(mobj->frame & FF_FRAMEMASK)];
}

boolean RSP_RenderModel(vissprite_t *spr)
{
	INT32 frameIndex;
	INT32 nextFrameIndex = -1;
	rsp_md2_t *md2;
	INT32 meshnum;

	// Not funny iD Software, didn't laugh.
	unsigned short idx = 0;
	boolean useTinyFrames;

	mobj_t *mobj = spr->mobj;
	if (!mobj)
		return false;

	// load sprite viewpoint
	if (rsp_portalrender)
		RSP_RestoreSpriteViewpoint(spr);

	// Look at R_ProjectSprite for more
	{
		rsp_texture_t *texture, sprtex;
		rsp_spritetexture_t *sprtexp;
		INT32 durs = mobj->state->tics;
		INT32 tics = mobj->tics;
		const boolean flip = (UINT8)((mobj->eflags & MFE_VERTICALFLIP) == MFE_VERTICALFLIP);
		spritedef_t *sprdef;
		spriteframe_t *sprframe;
		float finalscale;
		float pol = 0.0f;
		INT32 skinnum = 0;

		skincolors_t skincolor = SKINCOLOR_NONE;
		UINT8 *translation = NULL;

		md2 = RSP_ModelAvailable(spr->spritenum, (skin_t *)spr->skin);
		if (!md2)
		{
			// restore previous viewpoint
			if (rsp_portalrender)
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
				skinnum = TC_ALLWHITE;
			else if (mobj->type == MT_METALSONIC_BATTLE)
				skinnum = TC_METALSONIC;
			else
				skinnum = TC_BOSS;
			translation = R_GetTranslationColormap(skinnum, 0, GTC_CACHE);
			skincolor = -skinnum;
		}

		// load normal texture
		if (!md2->texture)
		{
			if (mobj->skin && mobj->sprite == SPR_PLAY)
				skinnum = (skin_t*)mobj->skin-skins;
			RSP_LoadModelTexture(md2, skinnum);
		}

		// load blend texture
		if (!md2->blendtexture)
			RSP_LoadModelBlendTexture(md2);

		// load translated texture
		if ((skincolor > 0) && (md2->rsp_transtex[skincolor].data == NULL))
			RSP_CreateModelTexture(md2, skinnum, skincolor);

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
				;	// Already set above
			else if (mobj->color)
			{
				// New colormap stuff for skins Tails 06-07-2002
				if (mobj->skin && mobj->sprite == SPR_PLAY) // This thing is a player!
					translation = R_GetTranslationColormap((INT32)skinnum, mobj->color, GTC_CACHE);
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
				if (rsp_portalrender)
					RSP_RestoreViewpoint();
				return false;
			}

			sprtex.width = sprtexp->width;
			sprtex.height = sprtexp->height;
			sprtex.data = sprtexp->data;
			texture = &sprtex;
		}

		//FIXME: this is not yet correct
		frameIndex = (mobj->frame & FF_FRAMEMASK) % md2->model->meshes[0].numFrames;

		if (RSP_CanInterpolateModel(mobj, md2->model) && tics <= durs)
		{
			// frames are handled differently for states with FF_ANIMATE, so get the next frame differently for the interpolation
			if (mobj->frame & FF_ANIMATE)
			{
				nextFrameIndex = (mobj->frame & FF_FRAMEMASK) + 1;
				if (nextFrameIndex >= mobj->state->var1)
					nextFrameIndex = (mobj->state->frame & FF_FRAMEMASK);
				nextFrameIndex %= md2->model->meshes[0].numFrames;
				//next = &md2->model->meshes[0].frames[nextFrame];
			}
			else
			{
				if (mobj->state->nextstate != S_NULL && states[mobj->state->nextstate].sprite != SPR_NULL
					&& !(mobj->player && (mobj->state->nextstate == S_PLAY_TAP1 || mobj->state->nextstate == S_PLAY_TAP2) && mobj->state == &states[S_PLAY_STND]))
				{
					nextFrameIndex = (states[mobj->state->nextstate].frame & FF_FRAMEMASK) % md2->model->meshes[0].numFrames;
					//next = &md2->model->meshes[0].frames[nextFrame];
				}
			}

			// don't interpolate if instantaneous or infinite in length
			if (durs != 0 && durs != -1 && tics != -1)
			{
				UINT32 newtime = (durs - tics);
				pol = (newtime)/(float)durs;
				if (pol > 1.0f)
					pol = 1.0f;
				if (pol < 0.0f)
					pol = 0.0f;
			}
		}

		// SRB2CBTODO: MD2 scaling support
		finalscale = md2->scale * FIXED_TO_FLOAT(mobj->scale);
		finalscale *= 0.5f;

		// Render every mesh
		for (meshnum = 0; meshnum < md2->model->numMeshes; meshnum++)
		{
			mesh_t *mesh = &md2->model->meshes[meshnum];
			rsp_triangle_t triangle;
			float theta, cs, sn;
			fixed_t model_angle;
			UINT16 i, j;
			float scale = finalscale;

			mdlframe_t *frame = NULL, *nextframe = NULL;
			tinyframe_t *tinyframe = NULL, *tinynextframe = NULL;

			useTinyFrames = md2->model->meshes[meshnum].tinyframes != NULL;
			if (useTinyFrames)
			{
				tinyframe = &mesh->tinyframes[frameIndex % mesh->numFrames];
				if (nextFrameIndex != -1)
					tinynextframe = &mesh->tinyframes[nextFrameIndex % mesh->numFrames];
				scale *= MD3_XYZ_SCALE;
			}
			else
			{
				frame = &mesh->frames[frameIndex % mesh->numFrames];
				if (nextFrameIndex != -1)
					nextframe = &mesh->frames[nextFrameIndex % mesh->numFrames];
			}

			// clear triangle struct
			// avoid undefined behaviour.............
			memset(&triangle, 0x00, sizeof(rsp_triangle_t));

			// set triangle texture
			if (texture->data)
				triangle.texture = texture;

			// set colormap, translation and transmap
			triangle.colormap = spr->colormap;
			if (spr->extra_colormap)
			{
				if (!triangle.colormap)
					triangle.colormap = spr->extra_colormap->colormap;
				else
					triangle.colormap = &spr->extra_colormap->colormap[triangle.colormap - colormaps];
			}
			triangle.translation = translation;
			triangle.transmap = spr->transmap;

			// vertical flip
			triangle.flipped = flip;

			// set model angle
			// \todo adapt for 2.2 directionchar? The below code is from Kart
#if 0
			if (mobj->player)
				model_angle = AngleFixed(mobj->player->frameangle);
			else
#endif
			model_angle = AngleFixed(mobj->angle);
			if (!sprframe->rotate)
			{
				model_angle = AngleFixed(viewangle - ANGLE_180);
				if (cv_modelbillboarding.value == BILLBOARD_CAMERA)
				{
					INT32 mid = spr->x1;
					mid += (spr->x2 - spr->x1) / 2;
					model_angle += AngleFixed(xtoviewangle[mid]);
				}
			}

			// model angle in radians
			theta = -(FIXED_TO_FLOAT(model_angle) * M_PI / 180.0f);
			cs = cos(theta);
			sn = sin(theta);

			// render every triangle
			for (i = 0; i < mesh->numTriangles; i++)
			{
				float x, y, z;
				float s, t;
				float *uv = mesh->uvs;

				x = FIXED_TO_FLOAT(mobj->x);
				y = FIXED_TO_FLOAT(mobj->y) + md2->offset;

				if (mobj->eflags & MFE_VERTICALFLIP)
					z = FIXED_TO_FLOAT(mobj->z + mobj->height);
				else
					z = FIXED_TO_FLOAT(mobj->z);

				for (j = 0; j < 3; j++)
				{
					if (useTinyFrames)
					{
						idx = mesh->indices[(i * 3) + j];
						s = *(uv + (idx * 2));
						t = *(uv + (idx * 2) + 1);
					}
					else
					{
						s = uv[UV_OFFSET];
						t = uv[UV_OFFSET+1];
					}

					if (!(nextframe || tinynextframe) || fpclassify(pol) == FP_ZERO)
					{
						float vx, vy, vz;
						float mx, my, mz;

						if (useTinyFrames)
						{
							short *vert = tinyframe->vertices;
							vx = *(vert + (idx * 3)) * scale;
							vy = *(vert + (idx * 3) + 1) * scale;
							vz = *(vert + (idx * 3) + 2) * scale;
						}
						else
						{
							vx = frame->vertices[VERTEX_OFFSET] * scale;
							vy = frame->vertices[VERTEX_OFFSET+1] * scale;
							vz = frame->vertices[VERTEX_OFFSET+2] * scale;
						}

						// QUICK MATHS
						mx = (vx * cs) - (vy * sn);
						my = (vx * sn) + (vy * cs);
						mz = vz * (flip ? -1 : 1);

						RSP_MakeVector4(triangle.vertices[j].position,
							 x + mx,
							-z + mz,
							-y + my
						);
					}
					else
					{
						float px1, py1, pz1;
						float px2, py2, pz2;
						float mx1, my1, mz1;
						float mx2, my2, mz2;

						// Interpolate
						if (useTinyFrames)
						{
							short *vert = tinyframe->vertices;
							short *nvert = tinynextframe->vertices;
							px1 = *(vert + (idx * 3)) * scale;
							py1 = *(vert + (idx * 3) + 1) * scale;
							pz1 = *(vert + (idx * 3) + 2) * scale;
							px2 = *(nvert + (idx * 3)) * scale;
							py2 = *(nvert + (idx * 3) + 1) * scale;
							pz2 = *(nvert + (idx * 3) + 2) * scale;
						}
						else
						{
							px1 = frame->vertices[VERTEX_OFFSET] * scale;
							py1 = frame->vertices[VERTEX_OFFSET+1] * scale;
							pz1 = frame->vertices[VERTEX_OFFSET+2] * scale;
							px2 = nextframe->vertices[VERTEX_OFFSET] * scale;
							py2 = nextframe->vertices[VERTEX_OFFSET+1] * scale;
							pz2 = nextframe->vertices[VERTEX_OFFSET+2] * scale;
						}

						// QUICK MATHS
						mx1 = (px1 * cs) - (py1 * sn);
						my1 = (px1 * sn) + (py1 * cs);
						mz1 = pz1 * (flip ? -1 : 1);

						mx2 = (px2 * cs) - (py2 * sn);
						my2 = (px2 * sn) + (py2 * cs);
						mz2 = pz2 * (flip ? -1 : 1);

						RSP_MakeVector4(triangle.vertices[j].position,
							 x + FloatLerp(mx1, mx2, pol),
							-z + FloatLerp(mz1, mz2, pol),
							-y + FloatLerp(my1, my2, pol)
						);
					}

					triangle.vertices[j].uv.u = s;
					triangle.vertices[j].uv.v = t;
				}

				RSP_TransformTriangle(&triangle);
			}
		}
	}

	RSP_ClearDepthBuffer();
	return true;
}

// fml
static INT32 project_sprite(fixed_t x, fixed_t y, spriteframe_t *sprframe, boolean flip)
{
	fixed_t tr_x, tr_y;
	fixed_t gxt, gyt;
	fixed_t tx, tz;
	fixed_t xscale; //added : 02-02-98 : aaargll..if I were a math-guy!!!

	INT32 x1, x2;
	INT32 mid;

	// transform the origin point
	tr_x = x - rsp_viewpoint.viewx;
	tr_y = y - rsp_viewpoint.viewy;

	gxt = FixedMul(tr_x, rsp_viewpoint.viewcos);
	gyt = -FixedMul(tr_y, rsp_viewpoint.viewsin);

	tz = gxt - gyt;

	// thing is behind view plane?
	if (tz < (FRACUNIT*4))
		return -1;

	gxt = -FixedMul(tr_x, rsp_viewpoint.viewsin);
	gyt = FixedMul(tr_y, rsp_viewpoint.viewcos);
	tx = -(gyt + gxt);

	// too far off the side?
	if (abs(tx) > tz<<2)
		return -1;

	// aspect ratio stuff
	xscale = FixedDiv(projection, tz);

	// calculate edges of the shape
	if (flip)
		tx -= spritecachedinfo[sprframe->lumpid[0]].width-spritecachedinfo[sprframe->lumpid[0]].offset;
	else
		tx -= spritecachedinfo[sprframe->lumpid[0]].offset;
	x1 = (centerxfrac + FixedMul (tx,xscale)) >>FRACBITS;

	// off the right side?
	if (x1 > viewwidth)
		return -1;

	tx += spritecachedinfo[sprframe->lumpid[0]].width;
	x2 = ((centerxfrac + FixedMul (tx,xscale)) >>FRACBITS) - 1;

	// off the left side
	if (x2 < 0)
		return -1;

	x1 = x1 < 0 ? 0 : x1;
	x2 = x2 >= viewwidth ? viewwidth-1 : x2;

	mid = x1;
	mid += (x2 - x1) / 2;

	return mid;
}

boolean RSP_RenderModelSimple(spritenum_t spritenum, INT32 frameIndex, float x, float y, float z, float model_angle, skincolors_t skincolor, skin_t *skin, boolean flip, boolean billboard)
{
	rsp_md2_t *md2;
	INT32 meshnum;
	rsp_texture_t *texture, sprtex;
	rsp_spritetexture_t *sprtexp;
	spritedef_t *sprdef;
	spriteframe_t *sprframe;
	float finalscale;
	size_t skinnum = 0;

	// Not funny iD Software, didn't laugh.
	unsigned short idx = 0;
	boolean useTinyFrames;

	UINT8 *translation = NULL;
	md2 = RSP_ModelAvailable(spritenum, skin);
	if (!md2)
		return false;

	// load normal texture
	if (!md2->texture)
	{
		if (skin && spritenum == SPR_PLAY)
			skinnum = (skin_t*)skin-skins;
		RSP_LoadModelTexture(md2, skinnum);
	}

	// load blend texture
	if (!md2->blendtexture)
		RSP_LoadModelBlendTexture(md2);

	// load translated texture
	if ((skincolor > 0) && (md2->rsp_transtex[skincolor].data == NULL))
		RSP_CreateModelTexture(md2, skinnum, skincolor);

	// use corresponding texture for this model
	if (md2->rsp_transtex[skincolor].data != NULL)
		texture = &md2->rsp_transtex[skincolor];
	else
		texture = &md2->rsp_tex;

	if (skin && spritenum == SPR_PLAY)
		sprdef = &skin->spritedef;
	else
		sprdef = &sprites[spritenum];

	sprframe = &sprdef->spriteframes[frameIndex];

	if (!texture->data)
	{
		// sprite translation
		if (skincolor)
		{
			// New colormap stuff for skins Tails 06-07-2002
			if (skin && spritenum == SPR_PLAY) // This thing is a player!
				translation = R_GetTranslationColormap((INT32)skinnum, skincolor, GTC_CACHE);
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

	// SRB2CBTODO: MD2 scaling support
	finalscale = md2->scale * 0.5f;

	// Render every mesh
	for (meshnum = 0; meshnum < md2->model->numMeshes; meshnum++)
	{
		mesh_t *mesh = &md2->model->meshes[meshnum];
		mdlframe_t *frame;
		tinyframe_t *tinyframe;
		rsp_triangle_t triangle;
		float theta, cs, sn;
		UINT16 i, j;
		float scale = finalscale;

		useTinyFrames = md2->model->meshes[meshnum].tinyframes != NULL;
		if (useTinyFrames)
		{
			tinyframe = &mesh->tinyframes[frameIndex % mesh->numFrames];
			scale *= MD3_XYZ_SCALE;
		}
		else
			frame = &mesh->frames[frameIndex % mesh->numFrames];

		// clear triangle struct
		// avoid undefined behaviour.............
		memset(&triangle, 0x00, sizeof(rsp_triangle_t));
		if (texture->data)
			triangle.texture = texture;
		triangle.translation = translation;
		triangle.flipped = flip;

		if (billboard && !sprframe->rotate)
		{
			fixed_t mdlang = AngleFixed(rsp_viewpoint.viewangle - ANGLE_180);
			if (cv_modelbillboarding.value == BILLBOARD_CAMERA)
			{
				// Now I could have been lazy and done
				// --> AngleFixed((R_PointToAngleEx(viewx, viewy, x, y))-ANGLE_180) <--
				// but it actually looks worse, so, uhhh.....
				// Here's my own function that projects the
				// "model" to screen coordinates that will then
				// get mapped to xtoviewangle[]
				// (135 degrees -> 45 degrees)
				INT32 mid = project_sprite(FLOAT_TO_FIXED(x), FLOAT_TO_FIXED(y), sprframe, flip);
				if (mid < 0)
					return false;
				mdlang += AngleFixed(xtoviewangle[mid]);
			}
			model_angle = FIXED_TO_FLOAT(mdlang);
		}

		// model angle in radians
		theta = -(model_angle * M_PI / 180.0f);
		cs = cos(theta);
		sn = sin(theta);

		y += md2->offset;

		// render every triangle
		for (i = 0; i < mesh->numTriangles; i++)
		{
			for (j = 0; j < 3; j++)
			{
				float vx, vy, vz;
				float mx, my, mz;
				float s, t;
				float *uv = mesh->uvs;

				if (useTinyFrames)
				{
					idx = mesh->indices[(i * 3) + j];
					s = *(uv + (idx * 2));
					t = *(uv + (idx * 2) + 1);
				}
				else
				{
					s = uv[UV_OFFSET];
					t = uv[UV_OFFSET+1];
				}

				if (useTinyFrames)
				{
					short *vert = tinyframe->vertices;
					vx = *(vert + (idx * 3)) * scale;
					vy = *(vert + (idx * 3) + 1) * scale;
					vz = *(vert + (idx * 3) + 2) * scale;
				}
				else
				{
					vx = frame->vertices[VERTEX_OFFSET] * scale;
					vy = frame->vertices[VERTEX_OFFSET+1] * scale;
					vz = frame->vertices[VERTEX_OFFSET+2] * scale;
				}

				// QUICK MATHS
				mx = (vx * cs) - (vy * sn);
				my = (vx * sn) + (vy * cs);
				mz = vz * (flip ? -1 : 1);

				RSP_MakeVector4(triangle.vertices[j].position,
					 x + mx,
					-z + mz,
					-y + my
				);

				triangle.vertices[j].uv.u = s;
				triangle.vertices[j].uv.v = t;
			}

			RSP_TransformTriangle(&triangle);
		}
	}

	return true;
}

boolean RSP_RenderInterpolatedModelSimple(spritenum_t spritenum, INT32 frameIndex, INT32 nextFrameIndex, float pol, boolean alwaysinterpolate, float x, float y, float z, float model_angle, skincolors_t skincolor, skin_t *skin, boolean flip, boolean billboard)
{
	rsp_md2_t *md2;
	INT32 meshnum;
	rsp_texture_t *texture, sprtex;
	rsp_spritetexture_t *sprtexp;
	spritedef_t *sprdef;
	spriteframe_t *sprframe;
	float finalscale;
	size_t skinnum = 0;

	// Not funny iD Software, didn't laugh.
	unsigned short idx = 0;
	boolean useTinyFrames;

	UINT8 *translation = NULL;
	md2 = RSP_ModelAvailable(spritenum, skin);
	if (!md2)
		return false;

	if (pol > 1.0f)
		pol = 1.0f;
	if (pol < 0.0f)
		pol = 0.0f;

	// load normal texture
	if (!md2->texture)
	{
		if (skin && spritenum == SPR_PLAY)
			skinnum = (skin_t*)skin-skins;
		RSP_LoadModelTexture(md2, skinnum);
	}

	// load blend texture
	if (!md2->blendtexture)
		RSP_LoadModelBlendTexture(md2);

	// load translated texture
	if ((skincolor > 0) && (md2->rsp_transtex[skincolor].data == NULL))
		RSP_CreateModelTexture(md2, skinnum, skincolor);

	// use corresponding texture for this model
	if (md2->rsp_transtex[skincolor].data != NULL)
		texture = &md2->rsp_transtex[skincolor];
	else
		texture = &md2->rsp_tex;

	if (skin && spritenum == SPR_PLAY)
		sprdef = &skin->spritedef;
	else
		sprdef = &sprites[spritenum];

	sprframe = &sprdef->spriteframes[frameIndex];

	if (!texture->data)
	{
		// sprite translation
		if (skincolor)
		{
			// New colormap stuff for skins Tails 06-07-2002
			if (skin && spritenum == SPR_PLAY) // This thing is a player!
				translation = R_GetTranslationColormap((INT32)skinnum, skincolor, GTC_CACHE);
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

	// SRB2CBTODO: MD2 scaling support
	finalscale = md2->scale * 0.5f;

	if ((!alwaysinterpolate) && (!md2->model->interpolate[frameIndex]))
		nextFrameIndex = frameIndex;

	// Render every mesh
	for (meshnum = 0; meshnum < md2->model->numMeshes; meshnum++)
	{
		mesh_t *mesh = &md2->model->meshes[meshnum];
		mdlframe_t *frame, *nextframe;
		tinyframe_t *tinyframe, *tinynextframe;
		rsp_triangle_t triangle;
		float theta, cs, sn;
		UINT16 i, j;
		float scale = finalscale;

		useTinyFrames = md2->model->meshes[meshnum].tinyframes != NULL;
		if (useTinyFrames)
		{
			tinyframe = &mesh->tinyframes[frameIndex % mesh->numFrames];
			tinynextframe = &mesh->tinyframes[nextFrameIndex % mesh->numFrames];
			scale *= MD3_XYZ_SCALE;
			if (!tinynextframe)
				continue;
		}
		else
		{
			frame = &mesh->frames[frameIndex % mesh->numFrames];
			nextframe = &mesh->frames[nextFrameIndex % mesh->numFrames];
			if (!nextframe)
				continue;
		}

		// clear triangle struct
		// avoid undefined behaviour.............
		memset(&triangle, 0x00, sizeof(rsp_triangle_t));
		if (texture->data)
			triangle.texture = texture;
		triangle.translation = translation;
		triangle.flipped = flip;

		if (billboard && !sprframe->rotate)
		{
			fixed_t mdlang = AngleFixed(rsp_viewpoint.viewangle - ANGLE_180);
			if (cv_modelbillboarding.value == BILLBOARD_CAMERA)
			{
				// Now I could have been lazy and done
				// --> AngleFixed((R_PointToAngleEx(viewx, viewy, x, y))-ANGLE_180) <--
				// but it actually looks worse, so, uhhh.....
				// Here's my own function that projects the
				// "model" to screen coordinates that will then
				// get mapped to xtoviewangle[]
				// (135 degrees -> 45 degrees)
				INT32 mid = project_sprite(FLOAT_TO_FIXED(x), FLOAT_TO_FIXED(y), sprframe, flip);
				if (mid < 0)
					return false;
				mdlang += AngleFixed(xtoviewangle[mid]);
			}
			model_angle = FIXED_TO_FLOAT(mdlang);
		}

		// model angle in radians
		theta = -(model_angle * M_PI / 180.0f);
		cs = cos(theta);
		sn = sin(theta);

		y += md2->offset;

		// render every triangle
		for (i = 0; i < mesh->numTriangles; i++)
		{
			for (j = 0; j < 3; j++)
			{
				float px1, py1, pz1;
				float px2, py2, pz2;
				float mx1, my1, mz1;
				float mx2, my2, mz2;
				float s, t;
				float *uv = mesh->uvs;

				if (useTinyFrames)
				{
					idx = mesh->indices[(i * 3) + j];
					s = *(uv + (idx * 2));
					t = *(uv + (idx * 2) + 1);
				}
				else
				{
					s = uv[UV_OFFSET];
					t = uv[UV_OFFSET+1];
				}

				// Interpolate
				if (useTinyFrames)
				{
					short *vert = tinyframe->vertices;
					short *nvert = tinynextframe->vertices;
					px1 = *(vert + (idx * 3)) * scale;
					py1 = *(vert + (idx * 3) + 1) * scale;
					pz1 = *(vert + (idx * 3) + 2) * scale;
					px2 = *(nvert + (idx * 3)) * scale;
					py2 = *(nvert + (idx * 3) + 1) * scale;
					pz2 = *(nvert + (idx * 3) + 2) * scale;
				}
				else
				{
					px1 = frame->vertices[VERTEX_OFFSET] * scale;
					py1 = frame->vertices[VERTEX_OFFSET+1] * scale;
					pz1 = frame->vertices[VERTEX_OFFSET+2] * scale;
					px2 = nextframe->vertices[VERTEX_OFFSET] * scale;
					py2 = nextframe->vertices[VERTEX_OFFSET+1] * scale;
					pz2 = nextframe->vertices[VERTEX_OFFSET+2] * scale;
				}

				// QUICK MATHS
				mx1 = (px1 * cs) - (py1 * sn);
				my1 = (px1 * sn) + (py1 * cs);
				mz1 = pz1 * (flip ? -1 : 1);

				mx2 = (px2 * cs) - (py2 * sn);
				my2 = (px2 * sn) + (py2 * cs);
				mz2 = pz2 * (flip ? -1 : 1);

				RSP_MakeVector4(triangle.vertices[j].position,
					 x + FloatLerp(mx1, mx2, pol),
					-z + FloatLerp(mz1, mz2, pol),
					-y + FloatLerp(my1, my2, pol)
				);

				triangle.vertices[j].uv.u = s;
				triangle.vertices[j].uv.v = t;
			}

			RSP_TransformTriangle(&triangle);
		}
	}

	return true;
}
