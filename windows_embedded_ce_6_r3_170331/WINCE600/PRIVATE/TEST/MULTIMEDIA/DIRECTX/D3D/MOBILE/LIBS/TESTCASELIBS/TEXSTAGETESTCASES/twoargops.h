//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of the Microsoft shared
// source or premium shared source license agreement under which you licensed
// this source code. If you did not accept the terms of the license agreement,
// you are not authorized to use this source code. For the terms of the license,
// please see the license agreement between you and Microsoft or, if applicable,
// see the SOURCE.RTF on your install media or the root of your tools installation.
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES.
//
#pragma once
#include <windows.h>
#include <d3dm.h>

HRESULT SetTwoArgStates(LPDIRECT3DMOBILEDEVICE pDevice,
                          DWORD dwTableIndex);

HRESULT GetTwoArgColors(LPDIRECT3DMOBILEDEVICE pDevice,
                          DWORD dwTableIndex,
                          PDWORD *ppdwDiffuse, 
                          PDWORD *ppdwSpecular,
                          PDWORD *ppdwTFactor, 
                          PDWORD *ppdwTexture); 

BOOL IsTwoArgTestOpSupported(LPDIRECT3DMOBILEDEVICE pDevice,
                               DWORD dwTableIndex);

typedef struct _TWO_ARG_OP_TESTS {
	D3DMTEXTUREOP TextureOp;
	D3DMCOLOR ColorValue1;
	D3DMCOLOR ColorValue2;
	DWORD TextureArg1;
	DWORD TextureArg2;
} TWO_ARG_OP_TESTS;

#define D3DQA_TWO_ARG_OP_TESTS_COUNT 208

__declspec(selectany) TWO_ARG_OP_TESTS TwoArgOpCases[D3DQA_TWO_ARG_OP_TESTS_COUNT] = {
// |           TextureOp             | ColorValue1 | ColorValue2 |   TextureArg1   |   TextureArg2   |
// +---------------------------------+-------------+-------------+-----------------+-----------------+

//
//  #1
//
// D3DMTOP_MODULATE Operation Tests (no D3DMTA_OPTIONMASK options)
//
{                    D3DMTOP_MODULATE,   0xFFF00000,   0xFFF00000,   D3DMTA_TEXTURE,   D3DMTA_TFACTOR }, // RED
{                    D3DMTOP_MODULATE,   0xFF00F000,   0xFF00F000,   D3DMTA_TEXTURE,  D3DMTA_SPECULAR }, // GREEN
{                    D3DMTOP_MODULATE,   0xFF0000F0,   0xFF0000F0,   D3DMTA_TEXTURE,   D3DMTA_DIFFUSE }, // BLUE
{                    D3DMTOP_MODULATE,   0xFFF0F000,   0xFFF0F000,   D3DMTA_TFACTOR,   D3DMTA_TEXTURE }, // YELLOW
{                    D3DMTOP_MODULATE,   0xFF00F0F0,   0xFF00F0F0,   D3DMTA_TFACTOR,  D3DMTA_SPECULAR }, // CYAN
{                    D3DMTOP_MODULATE,   0xFFF000F0,   0xFFF000F0,   D3DMTA_TFACTOR,   D3DMTA_DIFFUSE }, // MAGENTA
{                    D3DMTOP_MODULATE,   0xFFFF0000,   0xFF200000,  D3DMTA_SPECULAR,   D3DMTA_TEXTURE }, // RED (DARKER)
{                    D3DMTOP_MODULATE,   0xFFFF0000,   0xFF400000,  D3DMTA_SPECULAR,   D3DMTA_TFACTOR }, //   ||
{                    D3DMTOP_MODULATE,   0xFFFF0000,   0xFF600000,  D3DMTA_SPECULAR,   D3DMTA_DIFFUSE }, //   ||
{                    D3DMTOP_MODULATE,   0xFFFF0000,   0xFF800000,   D3DMTA_DIFFUSE,   D3DMTA_TEXTURE }, //   ||
{                    D3DMTOP_MODULATE,   0xFFFF0000,   0xFFA00000,   D3DMTA_DIFFUSE,   D3DMTA_SPECULAR}, //   \/
{                    D3DMTOP_MODULATE,   0xFFFF0000,   0xFFC00000,   D3DMTA_DIFFUSE,   D3DMTA_TFACTOR }, // RED (LIGHTER)


//
// D3DMTOP_MODULATE Operation Tests Operation Tests (with D3DMTA_COMPLEMENT)
//
{                    D3DMTOP_MODULATE,   0xFF0F0000,   0xFFF00000,     D3DMTA_DIFFUSE | D3DMTA_COMPLEMENT,   D3DMTA_TFACTOR }, // RED
{                    D3DMTOP_MODULATE,   0xFF000F00,   0xFF00F000,     D3DMTA_TFACTOR | D3DMTA_COMPLEMENT,   D3DMTA_TEXTURE }, // GREEN
{                    D3DMTOP_MODULATE,   0xFF00000F,   0xFF0000F0,     D3DMTA_TEXTURE | D3DMTA_COMPLEMENT,   D3DMTA_SPECULAR}, // BLUE
{                    D3DMTOP_MODULATE,   0xFF0F0F00,   0xFFF0F000,    D3DMTA_SPECULAR | D3DMTA_COMPLEMENT,   D3DMTA_DIFFUSE }, // YELLOW

//
// D3DMTOP_MODULATE Operation Tests (with D3DMTA_ALPHAREPLICATE)
//
{                    D3DMTOP_MODULATE,   0x3F00FF00,   0xFF0000FF,     D3DMTA_DIFFUSE | D3DMTA_ALPHAREPLICATE,   D3DMTA_TFACTOR }, // BLUE (DARK)
{                    D3DMTOP_MODULATE,   0x7F00FF00,   0xFF0000FF,     D3DMTA_TFACTOR | D3DMTA_ALPHAREPLICATE,   D3DMTA_TEXTURE }, //   ||       
{                    D3DMTOP_MODULATE,   0xBF00FF00,   0xFF0000FF,     D3DMTA_TEXTURE | D3DMTA_ALPHAREPLICATE,   D3DMTA_SPECULAR}, //   \/       
{                    D3DMTOP_MODULATE,   0xFF00FF00,   0xFF0000FF,    D3DMTA_SPECULAR | D3DMTA_ALPHAREPLICATE,   D3DMTA_DIFFUSE }, // BLUE (LIGHT)

//
//  #2
//
// D3DMTOP_MODULATE2X Operation Tests (no D3DMTA_OPTIONMASK options)
//
{                  D3DMTOP_MODULATE2X,   0xFF900000,   0xFF900000,   D3DMTA_TEXTURE,   D3DMTA_TFACTOR }, // RED
{                  D3DMTOP_MODULATE2X,   0xFF009000,   0xFF009000,   D3DMTA_TEXTURE,  D3DMTA_SPECULAR }, // GREEN
{                  D3DMTOP_MODULATE2X,   0xFF000090,   0xFF000090,   D3DMTA_TEXTURE,   D3DMTA_DIFFUSE }, // BLUE
{                  D3DMTOP_MODULATE2X,   0xFF909000,   0xFF909000,   D3DMTA_TFACTOR,   D3DMTA_TEXTURE }, // YELLOW
{                  D3DMTOP_MODULATE2X,   0xFF009090,   0xFF009090,   D3DMTA_TFACTOR,  D3DMTA_SPECULAR }, // CYAN
{                  D3DMTOP_MODULATE2X,   0xFFF00090,   0xFF900090,   D3DMTA_TFACTOR,   D3DMTA_DIFFUSE }, // MAGENTA
{                  D3DMTOP_MODULATE2X,   0xFFFF0000,   0xFF200000,  D3DMTA_SPECULAR,   D3DMTA_TEXTURE }, // RED (DARKER)
{                  D3DMTOP_MODULATE2X,   0xFFFF0000,   0xFF400000,  D3DMTA_SPECULAR,   D3DMTA_TFACTOR }, //   ||
{                  D3DMTOP_MODULATE2X,   0xFFFF0000,   0xFF600000,  D3DMTA_SPECULAR,   D3DMTA_DIFFUSE }, //   ||
{                  D3DMTOP_MODULATE2X,   0xFFFF0000,   0xFF800000,   D3DMTA_DIFFUSE,   D3DMTA_TEXTURE }, //   ||
{                  D3DMTOP_MODULATE2X,   0xFFFF0000,   0xFFA00000,   D3DMTA_DIFFUSE,   D3DMTA_SPECULAR}, //   \/
{                  D3DMTOP_MODULATE2X,   0xFFFF0000,   0xFFC00000,   D3DMTA_DIFFUSE,   D3DMTA_TFACTOR }, // RED (LIGHTER)

//
// D3DMTOP_MODULATE2X Operation Tests Operation Tests (with D3DMTA_COMPLEMENT)
//
{                    D3DMTOP_MODULATE2X,   0xFF5F0000,   0xFF900000,     D3DMTA_DIFFUSE | D3DMTA_COMPLEMENT,   D3DMTA_TFACTOR }, // RED
{                    D3DMTOP_MODULATE2X,   0xFF005F00,   0xFF009000,     D3DMTA_TFACTOR | D3DMTA_COMPLEMENT,   D3DMTA_TEXTURE }, // GREEN
{                    D3DMTOP_MODULATE2X,   0xFF00005F,   0xFF000090,     D3DMTA_TEXTURE | D3DMTA_COMPLEMENT,   D3DMTA_SPECULAR}, // BLUE
{                    D3DMTOP_MODULATE2X,   0xFF5F5F00,   0xFF909000,    D3DMTA_SPECULAR | D3DMTA_COMPLEMENT,   D3DMTA_DIFFUSE }, // YELLOW

//
// D3DMTOP_MODULATE2X Operation Tests (with D3DMTA_ALPHAREPLICATE)
//
{                    D3DMTOP_MODULATE2X,   0x1F00FF00,   0xFF0000FF,     D3DMTA_DIFFUSE | D3DMTA_ALPHAREPLICATE,   D3DMTA_TFACTOR }, // BLUE (DARK)
{                    D3DMTOP_MODULATE2X,   0x3F00FF00,   0xFF0000FF,     D3DMTA_TFACTOR | D3DMTA_ALPHAREPLICATE,   D3DMTA_TEXTURE }, //   ||       
{                    D3DMTOP_MODULATE2X,   0x5F00FF00,   0xFF0000FF,     D3DMTA_TEXTURE | D3DMTA_ALPHAREPLICATE,   D3DMTA_SPECULAR}, //   \/       
{                    D3DMTOP_MODULATE2X,   0x7F00FF00,   0xFF0000FF,    D3DMTA_SPECULAR | D3DMTA_ALPHAREPLICATE,   D3DMTA_DIFFUSE }, // BLUE (LIGHT)

//
//  #3
//
// D3DMTOP_MODULATE4X Operation Tests (no D3DMTA_OPTIONMASK options)
//
{                  D3DMTOP_MODULATE4X,   0xFF600000,   0xFF600000,   D3DMTA_TEXTURE,   D3DMTA_TFACTOR }, // RED
{                  D3DMTOP_MODULATE4X,   0xFF006000,   0xFF006000,   D3DMTA_TEXTURE,  D3DMTA_SPECULAR }, // GREEN
{                  D3DMTOP_MODULATE4X,   0xFF000060,   0xFF000060,   D3DMTA_TEXTURE,   D3DMTA_DIFFUSE }, // BLUE
{                  D3DMTOP_MODULATE4X,   0xFF606000,   0xFF606000,   D3DMTA_TFACTOR,   D3DMTA_TEXTURE }, // YELLOW
{                  D3DMTOP_MODULATE4X,   0xFF006060,   0xFF006060,   D3DMTA_TFACTOR,  D3DMTA_SPECULAR }, // CYAN
{                  D3DMTOP_MODULATE4X,   0xFFF00090,   0xFF900090,   D3DMTA_TFACTOR,   D3DMTA_DIFFUSE }, // MAGENTA
{                  D3DMTOP_MODULATE4X,   0xFFFF0000,   0xFF200000,  D3DMTA_SPECULAR,   D3DMTA_TEXTURE }, // RED (DARKER)
{                  D3DMTOP_MODULATE4X,   0xFFFF0000,   0xFF400000,  D3DMTA_SPECULAR,   D3DMTA_TFACTOR }, //   ||
{                  D3DMTOP_MODULATE4X,   0xFFFF0000,   0xFF600000,  D3DMTA_SPECULAR,   D3DMTA_DIFFUSE }, //   ||
{                  D3DMTOP_MODULATE4X,   0xFFFF0000,   0xFF800000,   D3DMTA_DIFFUSE,   D3DMTA_TEXTURE }, //   ||
{                  D3DMTOP_MODULATE4X,   0xFFFF0000,   0xFFA00000,   D3DMTA_DIFFUSE,   D3DMTA_SPECULAR}, //   \/
{                  D3DMTOP_MODULATE4X,   0xFFFF0000,   0xFFC00000,   D3DMTA_DIFFUSE,   D3DMTA_TFACTOR }, // RED (LIGHTER)


//
// D3DMTOP_MODULATE4X Operation Tests Operation Tests (with D3DMTA_COMPLEMENT)
//
{                    D3DMTOP_MODULATE4X,   0xFF9F0000,   0xFF900000,     D3DMTA_DIFFUSE | D3DMTA_COMPLEMENT,   D3DMTA_TFACTOR }, // RED
{                    D3DMTOP_MODULATE4X,   0xFF009F00,   0xFF009000,     D3DMTA_TFACTOR | D3DMTA_COMPLEMENT,   D3DMTA_TEXTURE }, // GREEN
{                    D3DMTOP_MODULATE4X,   0xFF00009F,   0xFF000090,     D3DMTA_TEXTURE | D3DMTA_COMPLEMENT,   D3DMTA_SPECULAR}, // BLUE
{                    D3DMTOP_MODULATE4X,   0xFF9F9F00,   0xFF909000,    D3DMTA_SPECULAR | D3DMTA_COMPLEMENT,   D3DMTA_DIFFUSE }, // YELLOW

//
// D3DMTOP_MODULATE4X Operation Tests (with D3DMTA_ALPHAREPLICATE)
//
{                    D3DMTOP_MODULATE4X,   0x1F00FF00,   0xFF0000FF,     D3DMTA_DIFFUSE | D3DMTA_ALPHAREPLICATE,   D3DMTA_TFACTOR }, // BLUE (DARK)
{                    D3DMTOP_MODULATE4X,   0x2F00FF00,   0xFF0000FF,     D3DMTA_TFACTOR | D3DMTA_ALPHAREPLICATE,   D3DMTA_TEXTURE }, //   ||       
{                    D3DMTOP_MODULATE4X,   0x3F00FF00,   0xFF0000FF,     D3DMTA_TEXTURE | D3DMTA_ALPHAREPLICATE,   D3DMTA_SPECULAR}, //   \/       
{                    D3DMTOP_MODULATE4X,   0x4F00FF00,   0xFF0000FF,    D3DMTA_SPECULAR | D3DMTA_ALPHAREPLICATE,   D3DMTA_DIFFUSE }, // BLUE (LIGHT)

//
//  #4
//
// D3DMTOP_ADD Operation Tests (no D3DMTA_OPTIONMASK options)
//
{                           D3DMTOP_ADD,   0xFF500000,   0xFF700000,   D3DMTA_TEXTURE,   D3DMTA_TFACTOR }, // RED
{                           D3DMTOP_ADD,   0xFF005000,   0xFF007000,   D3DMTA_TEXTURE,  D3DMTA_SPECULAR }, // GREEN
{                           D3DMTOP_ADD,   0xFF000050,   0xFF000070,   D3DMTA_TEXTURE,   D3DMTA_DIFFUSE }, // BLUE
{                           D3DMTOP_ADD,   0xFFF05000,   0xFF707000,   D3DMTA_TFACTOR,   D3DMTA_TEXTURE }, // YELLOW
{                           D3DMTOP_ADD,   0xFF005050,   0xFF007070,   D3DMTA_TFACTOR,  D3DMTA_SPECULAR }, // CYAN
{                           D3DMTOP_ADD,   0xFFF00050,   0xFFF00070,   D3DMTA_TFACTOR,   D3DMTA_DIFFUSE }, // MAGENTA
{                           D3DMTOP_ADD,   0xFF400000,   0xFF200000,  D3DMTA_SPECULAR,   D3DMTA_TEXTURE }, // RED (DARKER)
{                           D3DMTOP_ADD,   0xFF400000,   0xFF400000,  D3DMTA_SPECULAR,   D3DMTA_TFACTOR }, //   ||
{                           D3DMTOP_ADD,   0xFF400000,   0xFF600000,  D3DMTA_SPECULAR,   D3DMTA_DIFFUSE }, //   ||
{                           D3DMTOP_ADD,   0xFF400000,   0xFF800000,   D3DMTA_DIFFUSE,   D3DMTA_TEXTURE }, //   ||
{                           D3DMTOP_ADD,   0xFF400000,   0xFFA00000,   D3DMTA_DIFFUSE,   D3DMTA_SPECULAR}, //   \/
{                           D3DMTOP_ADD,   0xFF400000,   0xFFC00000,   D3DMTA_DIFFUSE,   D3DMTA_TFACTOR }, // RED (LIGHTER)


//
// D3DMTOP_ADD Operation Tests Operation Tests (with D3DMTA_COMPLEMENT)
//
{                           D3DMTOP_ADD,   0xFF50FFFF,   0xFF700000,     D3DMTA_DIFFUSE | D3DMTA_COMPLEMENT,   D3DMTA_TFACTOR }, // RED
{                           D3DMTOP_ADD,   0xFFFF50FF,   0xFF007000,     D3DMTA_TFACTOR | D3DMTA_COMPLEMENT,   D3DMTA_TEXTURE }, // GREEN
{                           D3DMTOP_ADD,   0xFFFFFF50,   0xFF000070,     D3DMTA_TEXTURE | D3DMTA_COMPLEMENT,   D3DMTA_SPECULAR}, // BLUE
{                           D3DMTOP_ADD,   0xFF5050FF,   0xFF707000,    D3DMTA_SPECULAR | D3DMTA_COMPLEMENT,   D3DMTA_DIFFUSE }, // YELLOW

//
// D3DMTOP_ADD Operation Tests (with D3DMTA_ALPHAREPLICATE)
//
{                           D3DMTOP_ADD,   0x5000FF00,   0xFF000070,     D3DMTA_DIFFUSE | D3DMTA_ALPHAREPLICATE,   D3DMTA_TFACTOR }, // BLUE (DARK)
{                           D3DMTOP_ADD,   0x6000FF00,   0xFF000070,     D3DMTA_TFACTOR | D3DMTA_ALPHAREPLICATE,   D3DMTA_TEXTURE }, //   ||       
{                           D3DMTOP_ADD,   0x7000FF00,   0xFF000070,     D3DMTA_TEXTURE | D3DMTA_ALPHAREPLICATE,   D3DMTA_SPECULAR}, //   \/       
{                           D3DMTOP_ADD,   0x8000FF00,   0xFF000070,    D3DMTA_SPECULAR | D3DMTA_ALPHAREPLICATE,   D3DMTA_DIFFUSE }, // BLUE (LIGHT)



//
//  #5
//
// D3DMTOP_ADDSIGNED Operation Tests (no D3DMTA_OPTIONMASK options)
//
{                      D3DMTOP_ADDSIGNED,   0xFFF00000,   0xFFF00000,   D3DMTA_TEXTURE,   D3DMTA_TFACTOR }, // RED
{                      D3DMTOP_ADDSIGNED,   0xFF00F000,   0xFF00F000,   D3DMTA_TEXTURE,  D3DMTA_SPECULAR }, // GREEN
{                      D3DMTOP_ADDSIGNED,   0xFF0000F0,   0xFF0000F0,   D3DMTA_TEXTURE,   D3DMTA_DIFFUSE }, // BLUE
{                      D3DMTOP_ADDSIGNED,   0xFFF0F000,   0xFFF0F000,   D3DMTA_TFACTOR,   D3DMTA_TEXTURE }, // YELLOW
{                      D3DMTOP_ADDSIGNED,   0xFF00F0F0,   0xFF00F0F0,   D3DMTA_TFACTOR,  D3DMTA_SPECULAR }, // CYAN
{                      D3DMTOP_ADDSIGNED,   0xFFF000F0,   0xFFF000F0,   D3DMTA_TFACTOR,   D3DMTA_DIFFUSE }, // MAGENTA
{                      D3DMTOP_ADDSIGNED,   0xFF000000,   0xFF000000,  D3DMTA_SPECULAR,   D3DMTA_TEXTURE }, // BLACK
{                      D3DMTOP_ADDSIGNED,   0xFF800000,   0xFF800000,  D3DMTA_SPECULAR,   D3DMTA_TFACTOR }, //   ||
{                      D3DMTOP_ADDSIGNED,   0xFF900000,   0xFF900000,  D3DMTA_SPECULAR,   D3DMTA_DIFFUSE }, //   ||
{                      D3DMTOP_ADDSIGNED,   0xFFA00000,   0xFFA00000,   D3DMTA_DIFFUSE,   D3DMTA_TEXTURE }, //   ||
{                      D3DMTOP_ADDSIGNED,   0xFFB00000,   0xFFB00000,   D3DMTA_DIFFUSE,   D3DMTA_SPECULAR}, //   \/
{                      D3DMTOP_ADDSIGNED,   0xFFC00000,   0xFFC00000,   D3DMTA_DIFFUSE,   D3DMTA_TFACTOR }, //   RED


//
// D3DMTOP_ADDSIGNED Operation Tests Operation Tests (with D3DMTA_COMPLEMENT)
//
{                      D3DMTOP_ADDSIGNED,   0xFF0FFFFF,   0xFFF00000,     D3DMTA_DIFFUSE | D3DMTA_COMPLEMENT,   D3DMTA_TFACTOR }, // RED
{                      D3DMTOP_ADDSIGNED,   0xFFFF0FFF,   0xFF00F000,     D3DMTA_TFACTOR | D3DMTA_COMPLEMENT,   D3DMTA_TEXTURE }, // GREEN
{                      D3DMTOP_ADDSIGNED,   0xFFFFFF0F,   0xFF0000F0,     D3DMTA_TEXTURE | D3DMTA_COMPLEMENT,   D3DMTA_SPECULAR}, // BLUE
{                      D3DMTOP_ADDSIGNED,   0xFF0F0FFF,   0xFFF0F000,    D3DMTA_SPECULAR | D3DMTA_COMPLEMENT,   D3DMTA_DIFFUSE }, // YELLOW

//
// D3DMTOP_ADDSIGNED Operation Tests (with D3DMTA_ALPHAREPLICATE)
//
{                      D3DMTOP_ADDSIGNED,   0x5000FF00,   0xFF0000A0,     D3DMTA_DIFFUSE | D3DMTA_ALPHAREPLICATE,   D3DMTA_TFACTOR }, // BLUE (DARKER)
{                      D3DMTOP_ADDSIGNED,   0x6000FF00,   0xFF0000A0,     D3DMTA_TFACTOR | D3DMTA_ALPHAREPLICATE,   D3DMTA_TEXTURE }, //   ||       
{                      D3DMTOP_ADDSIGNED,   0x7000FF00,   0xFF0000A0,     D3DMTA_TEXTURE | D3DMTA_ALPHAREPLICATE,   D3DMTA_SPECULAR}, //   \/       
{                      D3DMTOP_ADDSIGNED,   0x8000FF00,   0xFF0000A0,    D3DMTA_SPECULAR | D3DMTA_ALPHAREPLICATE,   D3DMTA_DIFFUSE }, // BLUE (LIGHTER)


//
//  #6
//
// D3DMTOP_ADDSIGNED2X Operation Tests (no D3DMTA_OPTIONMASK options)
//
{                    D3DMTOP_ADDSIGNED2X,   0xFF600000,   0xFF600000,   D3DMTA_TEXTURE,   D3DMTA_TFACTOR }, // RED
{                    D3DMTOP_ADDSIGNED2X,   0xFF006000,   0xFF006000,   D3DMTA_TEXTURE,  D3DMTA_SPECULAR }, // GREEN
{                    D3DMTOP_ADDSIGNED2X,   0xF6000060,   0xFF000060,   D3DMTA_TEXTURE,   D3DMTA_DIFFUSE }, // BLUE
{                    D3DMTOP_ADDSIGNED2X,   0xFF606000,   0xFF606000,   D3DMTA_TFACTOR,   D3DMTA_TEXTURE }, // YELLOW
{                    D3DMTOP_ADDSIGNED2X,   0xFF006060,   0xFF006060,   D3DMTA_TFACTOR,  D3DMTA_SPECULAR }, // CYAN
{                    D3DMTOP_ADDSIGNED2X,   0xFF600060,   0xFF600060,   D3DMTA_TFACTOR,   D3DMTA_DIFFUSE }, // MAGENTA
{                    D3DMTOP_ADDSIGNED2X,   0xFF200000,   0xFF200000,  D3DMTA_SPECULAR,   D3DMTA_TEXTURE }, // RED (DARK)
{                    D3DMTOP_ADDSIGNED2X,   0xFF400000,   0xFF400000,  D3DMTA_SPECULAR,   D3DMTA_TFACTOR }, //   ||
{                    D3DMTOP_ADDSIGNED2X,   0xFF600000,   0xFF600000,  D3DMTA_SPECULAR,   D3DMTA_DIFFUSE }, //   ||
{                    D3DMTOP_ADDSIGNED2X,   0xFF800000,   0xFF800000,   D3DMTA_DIFFUSE,   D3DMTA_TEXTURE }, //   ||
{                    D3DMTOP_ADDSIGNED2X,   0xFF900000,   0xFF900000,   D3DMTA_DIFFUSE,   D3DMTA_SPECULAR}, //   \/
{                    D3DMTOP_ADDSIGNED2X,   0xFFA00000,   0xFFA00000,   D3DMTA_DIFFUSE,   D3DMTA_TFACTOR }, //   RED


//
// D3DMTOP_ADDSIGNED2X Operation Tests Operation Tests (with D3DMTA_COMPLEMENT)
//
{                    D3DMTOP_ADDSIGNED2X,   0xFF0FFFFF,   0xFFF00000,     D3DMTA_DIFFUSE | D3DMTA_COMPLEMENT,   D3DMTA_TFACTOR }, // RED
{                    D3DMTOP_ADDSIGNED2X,   0xFFFF0FFF,   0xFF00F000,     D3DMTA_TFACTOR | D3DMTA_COMPLEMENT,   D3DMTA_TEXTURE }, // GREEN
{                    D3DMTOP_ADDSIGNED2X,   0xFFFFFF0F,   0xFF0000F0,     D3DMTA_TEXTURE | D3DMTA_COMPLEMENT,   D3DMTA_SPECULAR}, // BLUE
{                    D3DMTOP_ADDSIGNED2X,   0xFF0F0FFF,   0xFFF0F000,    D3DMTA_SPECULAR | D3DMTA_COMPLEMENT,   D3DMTA_DIFFUSE }, // YELLOW

//
// D3DMTOP_ADDSIGNED2X Operation Tests (with D3DMTA_ALPHAREPLICATE)
//
{                    D3DMTOP_ADDSIGNED2X,   0x3000FF00,   0xFF0000A0,     D3DMTA_DIFFUSE | D3DMTA_ALPHAREPLICATE,   D3DMTA_TFACTOR }, // BLUE (DARKER)
{                    D3DMTOP_ADDSIGNED2X,   0x4000FF00,   0xFF0000A0,     D3DMTA_TFACTOR | D3DMTA_ALPHAREPLICATE,   D3DMTA_TEXTURE }, //   ||       
{                    D3DMTOP_ADDSIGNED2X,   0x5000FF00,   0xFF0000A0,     D3DMTA_TEXTURE | D3DMTA_ALPHAREPLICATE,   D3DMTA_SPECULAR}, //   \/       
{                    D3DMTOP_ADDSIGNED2X,   0x6000FF00,   0xFF0000A0,    D3DMTA_SPECULAR | D3DMTA_ALPHAREPLICATE,   D3DMTA_DIFFUSE }, // BLUE (LIGHTER)


//
//  #7
//
// D3DMTOP_SUBTRACT Operation Tests (no D3DMTA_OPTIONMASK options)
//
{                    D3DMTOP_SUBTRACT,   0xFFFF0000,   0xFF400000,   D3DMTA_TEXTURE,   D3DMTA_TFACTOR }, // RED
{                    D3DMTOP_SUBTRACT,   0xFF00FF00,   0xFF004000,   D3DMTA_TEXTURE,  D3DMTA_SPECULAR }, // GREEN
{                    D3DMTOP_SUBTRACT,   0xFF0000FF,   0xFF000040,   D3DMTA_TEXTURE,   D3DMTA_DIFFUSE }, // BLUE
{                    D3DMTOP_SUBTRACT,   0xFFFFFF00,   0xFF404000,   D3DMTA_TFACTOR,   D3DMTA_TEXTURE }, // YELLOW
{                    D3DMTOP_SUBTRACT,   0xFF00FFFF,   0xFF004040,   D3DMTA_TFACTOR,  D3DMTA_SPECULAR }, // CYAN
{                    D3DMTOP_SUBTRACT,   0xFFFF00FF,   0xFF400040,   D3DMTA_TFACTOR,   D3DMTA_DIFFUSE }, // MAGENTA
{                    D3DMTOP_SUBTRACT,   0xFFFF0000,   0xFF200000,  D3DMTA_SPECULAR,   D3DMTA_TEXTURE }, // RED (LIGHTER)
{                    D3DMTOP_SUBTRACT,   0xFFFF0000,   0xFF400000,  D3DMTA_SPECULAR,   D3DMTA_TFACTOR }, //   ||
{                    D3DMTOP_SUBTRACT,   0xFFFF0000,   0xFF600000,  D3DMTA_SPECULAR,   D3DMTA_DIFFUSE }, //   ||
{                    D3DMTOP_SUBTRACT,   0xFFFF0000,   0xFF800000,   D3DMTA_DIFFUSE,   D3DMTA_TEXTURE }, //   ||
{                    D3DMTOP_SUBTRACT,   0xFFFF0000,   0xFFA00000,   D3DMTA_DIFFUSE,   D3DMTA_SPECULAR}, //   \/
{                    D3DMTOP_SUBTRACT,   0xFFFF0000,   0xFFC00000,   D3DMTA_DIFFUSE,   D3DMTA_TFACTOR }, // RED (DARKER)


//
// D3DMTOP_SUBTRACT Operation Tests Operation Tests (with D3DMTA_COMPLEMENT)
//
{                    D3DMTOP_SUBTRACT,   0xFF20FFFF,   0xFF200000,     D3DMTA_DIFFUSE | D3DMTA_COMPLEMENT,   D3DMTA_TFACTOR }, // RED
{                    D3DMTOP_SUBTRACT,   0xFFFF20FF,   0xFF002000,     D3DMTA_TFACTOR | D3DMTA_COMPLEMENT,   D3DMTA_TEXTURE }, // GREEN
{                    D3DMTOP_SUBTRACT,   0xFFFFFF20,   0xFF000020,     D3DMTA_TEXTURE | D3DMTA_COMPLEMENT,   D3DMTA_SPECULAR}, // BLUE
{                    D3DMTOP_SUBTRACT,   0xFF2020FF,   0xFF202000,    D3DMTA_SPECULAR | D3DMTA_COMPLEMENT,   D3DMTA_DIFFUSE }, // YELLOW

//
// D3DMTOP_SUBTRACT Operation Tests (with D3DMTA_ALPHAREPLICATE)
//
{                    D3DMTOP_SUBTRACT,   0x3F00FF00,   0xFFFFFF20,     D3DMTA_DIFFUSE | D3DMTA_ALPHAREPLICATE,   D3DMTA_TFACTOR }, // BLUE (DARK)
{                    D3DMTOP_SUBTRACT,   0x7F00FF00,   0xFFFFFF20,     D3DMTA_TFACTOR | D3DMTA_ALPHAREPLICATE,   D3DMTA_TEXTURE }, //   ||       
{                    D3DMTOP_SUBTRACT,   0xBF00FF00,   0xFFFFFF20,     D3DMTA_TEXTURE | D3DMTA_ALPHAREPLICATE,   D3DMTA_SPECULAR}, //   \/       
{                    D3DMTOP_SUBTRACT,   0xFF00FF00,   0xFFFFFF20,    D3DMTA_SPECULAR | D3DMTA_ALPHAREPLICATE,   D3DMTA_DIFFUSE }, // BLUE (LIGHT)


//
//  #8
//
// D3DMTOP_ADDSMOOTH Operation Tests (no D3DMTA_OPTIONMASK options)
//
{                    D3DMTOP_ADDSMOOTH,   0xFF500000,   0xFF700000,   D3DMTA_TEXTURE,   D3DMTA_TFACTOR }, // RED
{                    D3DMTOP_ADDSMOOTH,   0xFF005000,   0xFF007000,   D3DMTA_TEXTURE,  D3DMTA_SPECULAR }, // GREEN
{                    D3DMTOP_ADDSMOOTH,   0xFF000050,   0xFF000070,   D3DMTA_TEXTURE,   D3DMTA_DIFFUSE }, // BLUE
{                    D3DMTOP_ADDSMOOTH,   0xFFF05000,   0xFF707000,   D3DMTA_TFACTOR,   D3DMTA_TEXTURE }, // YELLOW
{                    D3DMTOP_ADDSMOOTH,   0xFF005050,   0xFF007070,   D3DMTA_TFACTOR,  D3DMTA_SPECULAR }, // CYAN
{                    D3DMTOP_ADDSMOOTH,   0xFFF00050,   0xFFF00070,   D3DMTA_TFACTOR,   D3DMTA_DIFFUSE }, // MAGENTA
{                    D3DMTOP_ADDSMOOTH,   0xFF400000,   0xFF200000,  D3DMTA_SPECULAR,   D3DMTA_TEXTURE }, // RED (DARKER)
{                    D3DMTOP_ADDSMOOTH,   0xFF400000,   0xFF400000,  D3DMTA_SPECULAR,   D3DMTA_TFACTOR }, //   ||
{                    D3DMTOP_ADDSMOOTH,   0xFF400000,   0xFF600000,  D3DMTA_SPECULAR,   D3DMTA_DIFFUSE }, //   ||
{                    D3DMTOP_ADDSMOOTH,   0xFF400000,   0xFF800000,   D3DMTA_DIFFUSE,   D3DMTA_TEXTURE }, //   ||
{                    D3DMTOP_ADDSMOOTH,   0xFF400000,   0xFFA00000,   D3DMTA_DIFFUSE,   D3DMTA_SPECULAR}, //   \/
{                    D3DMTOP_ADDSMOOTH,   0xFF400000,   0xFFC00000,   D3DMTA_DIFFUSE,   D3DMTA_TFACTOR }, // RED (LIGHTER)


//
// D3DMTOP_ADDSMOOTH Operation Tests Operation Tests (with D3DMTA_COMPLEMENT)
//
{                    D3DMTOP_ADDSMOOTH,   0xFF50FFFF,   0xFF700000,     D3DMTA_DIFFUSE | D3DMTA_COMPLEMENT,   D3DMTA_TFACTOR }, // RED
{                    D3DMTOP_ADDSMOOTH,   0xFFFF50FF,   0xFF007000,     D3DMTA_TFACTOR | D3DMTA_COMPLEMENT,   D3DMTA_TEXTURE }, // GREEN
{                    D3DMTOP_ADDSMOOTH,   0xFFFFFF50,   0xFF000070,     D3DMTA_TEXTURE | D3DMTA_COMPLEMENT,   D3DMTA_SPECULAR}, // BLUE
{                    D3DMTOP_ADDSMOOTH,   0xFF5050FF,   0xFF707000,    D3DMTA_SPECULAR | D3DMTA_COMPLEMENT,   D3DMTA_DIFFUSE }, // YELLOW

//
// D3DMTOP_ADDSMOOTH Operation Tests (with D3DMTA_ALPHAREPLICATE)
//
{                    D3DMTOP_ADDSMOOTH,   0x5000FF00,   0xFF000070,     D3DMTA_DIFFUSE | D3DMTA_ALPHAREPLICATE,   D3DMTA_TFACTOR }, // BLUE (DARK)
{                    D3DMTOP_ADDSMOOTH,   0x6000FF00,   0xFF000070,     D3DMTA_TFACTOR | D3DMTA_ALPHAREPLICATE,   D3DMTA_TEXTURE }, //   ||       
{                    D3DMTOP_ADDSMOOTH,   0x7000FF00,   0xFF000070,     D3DMTA_TEXTURE | D3DMTA_ALPHAREPLICATE,   D3DMTA_SPECULAR}, //   \/       
{                    D3DMTOP_ADDSMOOTH,   0x8000FF00,   0xFF000070,    D3DMTA_SPECULAR | D3DMTA_ALPHAREPLICATE,   D3DMTA_DIFFUSE }, // BLUE (LIGHT)


//
//  #9
//
// D3DMTOP_BLENDDIFFUSEALPHA Operation Tests (no D3DMTA_OPTIONMASK options)
//
{            D3DMTOP_BLENDDIFFUSEALPHA,   0x40FF0000,   0xFF00FF00,   D3DMTA_DIFFUSE,   D3DMTA_TEXTURE },  // GREEN
{            D3DMTOP_BLENDDIFFUSEALPHA,   0x80FF0000,   0xFF00FF00,   D3DMTA_DIFFUSE,   D3DMTA_SPECULAR},  //  \/
{            D3DMTOP_BLENDDIFFUSEALPHA,   0xC0FF0000,   0xFF00FF00,   D3DMTA_DIFFUSE,   D3DMTA_TFACTOR },  // RED

{            D3DMTOP_BLENDDIFFUSEALPHA,   0xFF00FF00,   0x40FF0000,   D3DMTA_TEXTURE,   D3DMTA_DIFFUSE },  // RED
{            D3DMTOP_BLENDDIFFUSEALPHA,   0xFF00FF00,   0x80FF0000,  D3DMTA_SPECULAR,   D3DMTA_DIFFUSE },  //  \/
{            D3DMTOP_BLENDDIFFUSEALPHA,   0xFF00FF00,   0xC0FF0000,   D3DMTA_TFACTOR,   D3DMTA_DIFFUSE },  // GREEN

//
// D3DMTOP_BLENDDIFFUSEALPHA Operation Tests Operation Tests (with D3DMTA_COMPLEMENT)
//

{            D3DMTOP_BLENDDIFFUSEALPHA,   0x40FF0000,   0xFF00FF00,   D3DMTA_DIFFUSE | D3DMTA_COMPLEMENT,   D3DMTA_TEXTURE },  // GREEN
{            D3DMTOP_BLENDDIFFUSEALPHA,   0x80FF0000,   0xFF00FF00,   D3DMTA_DIFFUSE | D3DMTA_COMPLEMENT,   D3DMTA_SPECULAR},  //  \/
{            D3DMTOP_BLENDDIFFUSEALPHA,   0xC0FF0000,   0xFF00FF00,   D3DMTA_DIFFUSE | D3DMTA_COMPLEMENT,   D3DMTA_TFACTOR },  // GREEN (LIGHT)

//
// D3DMTOP_BLENDDIFFUSEALPHA Operation Tests (with D3DMTA_ALPHAREPLICATE)
//

{            D3DMTOP_BLENDDIFFUSEALPHA,   0x40FF0000,   0xFF00FF00,   D3DMTA_DIFFUSE | D3DMTA_ALPHAREPLICATE,   D3DMTA_TEXTURE },  // GREEN
{            D3DMTOP_BLENDDIFFUSEALPHA,   0x80FF0000,   0xFF00FF00,   D3DMTA_DIFFUSE | D3DMTA_ALPHAREPLICATE,   D3DMTA_SPECULAR},  //  \/
{            D3DMTOP_BLENDDIFFUSEALPHA,   0xC0FF0000,   0xFF00FF00,   D3DMTA_DIFFUSE | D3DMTA_ALPHAREPLICATE,   D3DMTA_TFACTOR },  // GREEN (LIGHT)



//
//  #10
//
// D3DMTOP_BLENDTEXTUREALPHA Operation Tests (no D3DMTA_OPTIONMASK options)
//
{            D3DMTOP_BLENDTEXTUREALPHA,   0x40FF0000,   0xFF00FF00,   D3DMTA_TEXTURE,   D3DMTA_DIFFUSE },  // GREEN
{            D3DMTOP_BLENDTEXTUREALPHA,   0x80FF0000,   0xFF00FF00,   D3DMTA_TEXTURE,   D3DMTA_SPECULAR},  //  \/
{            D3DMTOP_BLENDTEXTUREALPHA,   0xC0FF0000,   0xFF00FF00,   D3DMTA_TEXTURE,   D3DMTA_TFACTOR },  // RED

{            D3DMTOP_BLENDTEXTUREALPHA,   0xFF00FF00,   0x40FF0000,   D3DMTA_DIFFUSE,   D3DMTA_TEXTURE},  // RED
{            D3DMTOP_BLENDTEXTUREALPHA,   0xFF00FF00,   0x80FF0000,  D3DMTA_SPECULAR,   D3DMTA_TEXTURE},  //  \/
{            D3DMTOP_BLENDTEXTUREALPHA,   0xFF00FF00,   0xC0FF0000,   D3DMTA_TFACTOR,   D3DMTA_TEXTURE},  // GREEN

//
// D3DMTOP_BLENDTEXTUREALPHA Operation Tests Operation Tests (with D3DMTA_COMPLEMENT)
//

{            D3DMTOP_BLENDTEXTUREALPHA,   0x40FF0000,   0xFF00FF00,   D3DMTA_TEXTURE | D3DMTA_COMPLEMENT,   D3DMTA_DIFFUSE },  // GREEN
{            D3DMTOP_BLENDTEXTUREALPHA,   0x80FF0000,   0xFF00FF00,   D3DMTA_TEXTURE | D3DMTA_COMPLEMENT,   D3DMTA_SPECULAR},  //  \/
{            D3DMTOP_BLENDTEXTUREALPHA,   0xC0FF0000,   0xFF00FF00,   D3DMTA_TEXTURE | D3DMTA_COMPLEMENT,   D3DMTA_TFACTOR },  // GREEN (LIGHT)

//
// D3DMTOP_BLENDTEXTUREALPHA Operation Tests (with D3DMTA_ALPHAREPLICATE)
//

{            D3DMTOP_BLENDTEXTUREALPHA,   0x40FF0000,   0xFF00FF00,   D3DMTA_TEXTURE | D3DMTA_ALPHAREPLICATE,  D3DMTA_DIFFUSE  },  // GREEN
{            D3DMTOP_BLENDTEXTUREALPHA,   0x80FF0000,   0xFF00FF00,   D3DMTA_TEXTURE | D3DMTA_ALPHAREPLICATE,  D3DMTA_SPECULAR },  //  \/
{            D3DMTOP_BLENDTEXTUREALPHA,   0xC0FF0000,   0xFF00FF00,   D3DMTA_TEXTURE | D3DMTA_ALPHAREPLICATE,  D3DMTA_TFACTOR  },  // GREEN (LIGHT)



//
//  #11
//
// D3DMTOP_BLENDFACTORALPHA Operation Tests (no D3DMTA_OPTIONMASK options)
//
{            D3DMTOP_BLENDFACTORALPHA,   0x40FF0000,   0xFF00FF00,   D3DMTA_TFACTOR,   D3DMTA_DIFFUSE },  // GREEN
{            D3DMTOP_BLENDFACTORALPHA,   0x80FF0000,   0xFF00FF00,   D3DMTA_TFACTOR,   D3DMTA_SPECULAR},  //  \/
{            D3DMTOP_BLENDFACTORALPHA,   0xC0FF0000,   0xFF00FF00,   D3DMTA_TFACTOR,   D3DMTA_TEXTURE },  // RED

{            D3DMTOP_BLENDFACTORALPHA,   0xFF00FF00,   0x40FF0000,   D3DMTA_DIFFUSE,   D3DMTA_TFACTOR},  // RED
{            D3DMTOP_BLENDFACTORALPHA,   0xFF00FF00,   0x80FF0000,  D3DMTA_SPECULAR,   D3DMTA_TFACTOR},  //  \/
{            D3DMTOP_BLENDFACTORALPHA,   0xFF00FF00,   0xC0FF0000,   D3DMTA_TEXTURE,   D3DMTA_TFACTOR},  // GREEN

//
// D3DMTOP_BLENDFACTORALPHA Operation Tests Operation Tests (with D3DMTA_COMPLEMENT)
//

{            D3DMTOP_BLENDFACTORALPHA,   0x40FF0000,   0xFF00FF00,   D3DMTA_TFACTOR | D3DMTA_COMPLEMENT,  D3DMTA_DIFFUSE  },  // GREEN
{            D3DMTOP_BLENDFACTORALPHA,   0x80FF0000,   0xFF00FF00,   D3DMTA_TFACTOR | D3DMTA_COMPLEMENT,  D3DMTA_SPECULAR },  //  \/
{            D3DMTOP_BLENDFACTORALPHA,   0xC0FF0000,   0xFF00FF00,   D3DMTA_TFACTOR | D3DMTA_COMPLEMENT,  D3DMTA_TEXTURE  },  // GREEN (LIGHT)

//
// D3DMTOP_BLENDFACTORALPHA Operation Tests (with D3DMTA_ALPHAREPLICATE)
//

{            D3DMTOP_BLENDFACTORALPHA,   0x40FF0000,   0xFF00FF00,   D3DMTA_TFACTOR | D3DMTA_ALPHAREPLICATE,  D3DMTA_DIFFUSE   },  // GREEN
{            D3DMTOP_BLENDFACTORALPHA,   0x80FF0000,   0xFF00FF00,   D3DMTA_TFACTOR | D3DMTA_ALPHAREPLICATE,  D3DMTA_SPECULAR  },  //  \/
{            D3DMTOP_BLENDFACTORALPHA,   0xC0FF0000,   0xFF00FF00,   D3DMTA_TFACTOR | D3DMTA_ALPHAREPLICATE,  D3DMTA_TEXTURE   },  // GREEN (LIGHT)



//
//  #12
//
// D3DMTOP_BLENDTEXTUREALPHAPM Operation Tests (no D3DMTA_OPTIONMASK options)
//
{          D3DMTOP_BLENDTEXTUREALPHAPM,   0x40FF0000,   0xFF00FF00,   D3DMTA_TEXTURE,   D3DMTA_DIFFUSE },  // ORANGE
{          D3DMTOP_BLENDTEXTUREALPHAPM,   0x80FF0000,   0xFF00FF00,   D3DMTA_TEXTURE,   D3DMTA_SPECULAR},  //  \/
{          D3DMTOP_BLENDTEXTUREALPHAPM,   0xC0FF0000,   0xFF00FF00,   D3DMTA_TEXTURE,   D3DMTA_TFACTOR },  // RED

{          D3DMTOP_BLENDTEXTUREALPHAPM,   0xFF00FF00,   0x40FF0000,   D3DMTA_DIFFUSE,   D3DMTA_TEXTURE},  // YELLOW
{          D3DMTOP_BLENDTEXTUREALPHAPM,   0xFF00FF00,   0x80FF0000,  D3DMTA_SPECULAR,   D3DMTA_TEXTURE},  //  \/
{          D3DMTOP_BLENDTEXTUREALPHAPM,   0xFF00FF00,   0xC0FF0000,   D3DMTA_TFACTOR,   D3DMTA_TEXTURE},  // GREEN

//
// D3DMTOP_BLENDTEXTUREALPHAPM Operation Tests Operation Tests (with D3DMTA_COMPLEMENT)
//

{          D3DMTOP_BLENDTEXTUREALPHAPM,   0x40FF0000,   0xFF00FF00,   D3DMTA_TEXTURE | D3DMTA_COMPLEMENT,   D3DMTA_DIFFUSE },  // CYAN
{          D3DMTOP_BLENDTEXTUREALPHAPM,   0x80FF0000,   0xFF00FF00,   D3DMTA_TEXTURE | D3DMTA_COMPLEMENT,   D3DMTA_SPECULAR},  // CYAN 
{          D3DMTOP_BLENDTEXTUREALPHAPM,   0xC0FF0000,   0xFF00FF00,   D3DMTA_TEXTURE | D3DMTA_COMPLEMENT,   D3DMTA_TFACTOR },  // CYAN 

//
// D3DMTOP_BLENDTEXTUREALPHAPM Operation Tests (with D3DMTA_ALPHAREPLICATE)
//

{          D3DMTOP_BLENDTEXTUREALPHAPM,   0x40FF0000,   0xFF00FF00,   D3DMTA_TEXTURE | D3DMTA_ALPHAREPLICATE,  D3DMTA_DIFFUSE  },  // GREEN (LIGHT)
{          D3DMTOP_BLENDTEXTUREALPHAPM,   0x80FF0000,   0xFF00FF00,   D3DMTA_TEXTURE | D3DMTA_ALPHAREPLICATE,  D3DMTA_SPECULAR },  // GREEN (LIGHT)
{          D3DMTOP_BLENDTEXTUREALPHAPM,   0xC0FF0000,   0xFF00FF00,   D3DMTA_TEXTURE | D3DMTA_ALPHAREPLICATE,  D3DMTA_TFACTOR  },  // GREEN (LIGHT)



};

//
// Future testing:
// 
// D3DMTOP_BLENDCURRENTALPHA    
// D3DMTOP_PREMODULATE          
// D3DMTOP_MODULATEALPHA_ADDCOLOR 
// D3DMTOP_MODULATECOLOR_ADDALPHA  
// D3DMTOP_MODULATEINVALPHA_ADDCOLOR
// D3DMTOP_MODULATEINVCOLOR_ADDALPHA
// D3DMTOP_DOTPRODUCT3          
// 
