#include <3ds.h>
#include <citro3d.h>
#include <string.h>
#include "vshader_shbin.h"

#define CLEAR_COLOR 0x68B0D8FF

#define DISPLAY_TRANSFER_FLAGS \
	(GX_TRANSFER_FLIP_VERT(0) | GX_TRANSFER_OUT_TILED(0) | GX_TRANSFER_RAW_COPY(0) | \
	GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGBA8) | GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGB8) | \
	GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_NO))

static DVLB_s* vshader_dvlb;
static shaderProgram_s program;
static int uLoc_projection;
static C3D_Mtx projection;

static void sceneInit(void)
{
	// Load the vertex shader, create a shader program and bind it
	vshader_dvlb = DVLB_ParseFile((u32*)vshader_shbin, vshader_shbin_size);
	shaderProgramInit(&program);
	shaderProgramSetVsh(&program, &vshader_dvlb->DVLE[0]);
	C3D_BindProgram(&program);

	// Get the location of the uniforms
	uLoc_projection = shaderInstanceGetUniformLocation(program.vertexShader, "projection");

	// Configure attributes for use with the vertex shader
	// Attribute format and element count are ignored in immediate mode
	C3D_AttrInfo* attrInfo = C3D_GetAttrInfo();
	AttrInfo_Init(attrInfo);
	AttrInfo_AddLoader(attrInfo, 0, GPU_FLOAT, 3); // v0=position
	AttrInfo_AddLoader(attrInfo, 1, GPU_FLOAT, 3); // v1=color

	// Compute the projection matrix
	Mtx_OrthoTilt(&projection, 0.0, 400.0, 0.0, 240.0, 0.0, 1.0, true);

	// Configure the first fragment shading substage to just pass through the vertex color
	// See https://www.opengl.org/sdk/docs/man2/xhtml/glTexEnv.xml for more insight
	C3D_TexEnv* env = C3D_GetTexEnv(0);
	C3D_TexEnvInit(env);
	C3D_TexEnvSrc(env, C3D_Both, GPU_PRIMARY_COLOR, 0, 0);
	C3D_TexEnvFunc(env, C3D_Both, GPU_REPLACE);
}

static void sceneRender(void)
{
	// Update the uniforms
	C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uLoc_projection, &projection);

	// Draw the triangle directly
	C3D_ImmDrawBegin(GPU_TRIANGLES);
        // Triangle 1
		C3D_ImmSendAttrib(200.0f, 200.0f, 0.5f, 0.0f); // v0=position
		C3D_ImmSendAttrib(1.0f, 0.0f, 0.0f, 1.0f);     // v1=color

		C3D_ImmSendAttrib(100.0f, 40.0f, 0.5f, 0.0f);
		C3D_ImmSendAttrib(0.0f, 1.0f, 0.0f, 1.0f);

		C3D_ImmSendAttrib(300.0f, 40.0f, 0.5f, 0.0f);
		C3D_ImmSendAttrib(0.0f, 0.0f, 1.0f, 1.0f);

        // Triangle 2
        C3D_ImmSendAttrib(10.0f, 20.0f, 0.5f, 0.0f);
		C3D_ImmSendAttrib(1.0f, 0.0f, 0.0f, 1.0f);

        C3D_ImmSendAttrib(90.0f, 20.0f, 0.5f, 0.0f);
		C3D_ImmSendAttrib(1.0f, 0.0f, 0.0f, 1.0f);
        
        C3D_ImmSendAttrib(40.0f, 40.0f, 0.5f, 0.0f);
		C3D_ImmSendAttrib(1.0f, 0.0f, 0.0f, 1.0f);
	C3D_ImmDrawEnd();
}

static void sceneExit(void)
{
	// Free the shader program
	shaderProgramFree(&program);
	DVLB_Free(vshader_dvlb);
}

int main()
{
	// Initialize graphics
	gfxInitDefault();
	C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);

	// Initialize the render target
	C3D_RenderTarget* target = C3D_RenderTargetCreate(240, 400, GPU_RB_RGBA8, GPU_RB_DEPTH24_STENCIL8);
	C3D_RenderTargetSetOutput(target, GFX_TOP, GFX_LEFT, DISPLAY_TRANSFER_FLAGS);

	// Initialize the scene
	sceneInit();

	// Main loop
	while (aptMainLoop())
	{
		hidScanInput();

		// Respond to user input
		u32 kDown = hidKeysDown();
		if (kDown & KEY_START)
			break; // break in order to return to hbmenu

		// Render the scene
		C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
			C3D_RenderTargetClear(target, C3D_CLEAR_ALL, CLEAR_COLOR, 0);
			C3D_FrameDrawOn(target);
			sceneRender();
		C3D_FrameEnd(0);
	}

	// Deinitialize the scene
	sceneExit();

	// Deinitialize graphics
	C3D_Fini();
	gfxExit();
	return 0;
}
