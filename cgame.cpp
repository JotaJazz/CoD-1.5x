#include "shared.h"
#include "client.h"
#include "render.h"
#include <mmsystem.h>
#pragma comment(lib, "winmm.lib")

extern pmove_t* pm;

#define PMF_JUMPING 0x2000
#define JUMP_LAND_SLOWDOWN_TIME 1800
#define VectorScale( v, s, o )      ( ( o )[0] = ( v )[0] * ( s ),( o )[1] = ( v )[1] * ( s ),( o )[2] = ( v )[2] * ( s ) )


DWORD cgame_mp;
#define M_DrawShadowString(x,y,font,fontscale,color,text,a,b,c) \
	RE_SetColor(vColorBlack); \
	SCR_DrawString(x + 1,y + 1,font,fontscale,vColorBlack,text,a,b,c); \
	RE_SetColor(color); \
	SCR_DrawString(x,y,font,fontscale,color,text,a,b,c); \
	RE_SetColor(NULL);

void Jump_ApplySlowdown()
{
	int* pm = (int*)(cgame_mp + 0x1a0ed0);
	playerState_t* ps = ((pmove_t*)*((int*)pm))->ps;

	if (ps->pm_flags & PMF_JUMPING)
	{
		float scale = 1.0;

		if (ps->pm_time <= JUMP_LAND_SLOWDOWN_TIME)
		{
			if (!ps->pm_time)
			{
				if ((float)(ps->jumpOriginZ + 18.0) <= ps->origin[2])
				{
					ps->pm_time = 1200;
					scale = 0.5;
				}
				else
				{
					ps->pm_time = JUMP_LAND_SLOWDOWN_TIME;
					scale = 0.64999998;
				}
			}
		}
		else
		{
			// Clear jump state
			ps->pm_flags &= ~PMF_JUMPING;
			ps->jumpOriginZ = 0.0;
			scale = 0.64999998;
		}

		char* jump_slowdownEnable = Info_ValueForKey(cs1, "jump_slowdownEnable");
		if (*jump_slowdownEnable && atoi(jump_slowdownEnable) == 0)
			return;
		VectorScale(ps->velocity, scale, ps->velocity);
	}
}
uintptr_t resume_addr_PM_WalkMove;
__declspec(naked) void hook_PM_WalkMove_Naked()
{
	__asm
	{
		pushad;
		call Jump_ApplySlowdown;
		popad;
		jmp resume_addr_PM_WalkMove;
	}
}

void hook_PM_SlideMove(float primal_velocity_0, float primal_velocity_1, float primal_velocity_2)
{
	char* jump_slowdownEnable = Info_ValueForKey(cs1, "jump_slowdownEnable");
	if (*jump_slowdownEnable && atoi(jump_slowdownEnable) == 0)
		return;

	int* pm = (int*)(cgame_mp + 0x1a0ed0);
	playerState_t* ps = ((pmove_t*)*((int*)pm))->ps;
	if (ps->pm_time)
	{
		ps->velocity[0] = primal_velocity_0;
		ps->velocity[1] = primal_velocity_1;
		ps->velocity[2] = primal_velocity_2;
	}
}
uintptr_t resume_addr_PM_SlideMove;
__declspec(naked) void hook_PM_SlideMove_Naked()
{
	__asm
	{
		mov eax, dword ptr[esp + 0x110 - 0xA8]
		mov ecx, dword ptr[esp + 0x110 - 0xAC]
		mov edx, dword ptr[esp + 0x110 - 0xB0]

		push eax
		push ecx
		push edx

		call hook_PM_SlideMove
		add esp, 12

		jmp resume_addr_PM_SlideMove
	}
}

void Jump_GetLandFactor()
{
	int* pm = (int*)(cgame_mp + 0x1a0ed0);
	playerState_t* ps = ((pmove_t*)*((int*)pm))->ps;

	double factor;

	char* jump_slowdownEnable = Info_ValueForKey(cs1, "jump_slowdownEnable");
	if (*jump_slowdownEnable && atoi(jump_slowdownEnable) == 0)
		factor = 1.0;
	else if (ps->pm_time < 1700)
		factor = (double)ps->pm_time * 0.00088235294 + 1.0;
	else
		factor = 2.5;

	__asm fld factor
}
uintptr_t resume_addr_Jump_Start;
__declspec(naked) void hook_Jump_Start_Naked()
{
	__asm
	{
		pushad;
		call Jump_GetLandFactor;
		popad;
		jmp resume_addr_Jump_Start;
	}
}

void custom_PM_GetReducedFriction()
{
	double friction;

	char* jump_slowdownEnable = Info_ValueForKey(cs1, "jump_slowdownEnable");
	if (*jump_slowdownEnable && atoi(jump_slowdownEnable) == 0)
	{
		friction = 1.0;
	}
	else
	{
		int* pm = (int*)(cgame_mp + 0x1a0ed0);
		playerState_t* ps = ((pmove_t*)*((int*)pm))->ps;

		if (ps->pm_time < 1700)
			friction = (double)ps->pm_time * 0.00088235294 + 1.0;
		else
			friction = 2.5;
	}

	__asm fld friction
}
__declspec(naked) void custom_PM_GetReducedFriction_Naked()
{
	__asm
	{
		pushad
		call custom_PM_GetReducedFriction
		popad
		ret
	}
}

// Hor+ widescreen FOV: cg_fov is treated as the 4:3 horizontal FOV, so wider screens gain
// view on the sides instead of losing it at the top and bottom (stock behaviour).
static float fovScale = 1.0f; // widened fov_x / stock fov_x for the current frame

float CG_GetFov_HorPlus() {
	float(*CG_GetFov)() = (float(*)())CGAME_OFF(0x300344C0); // cg_fov clamped + zoom, returned in st0
	float fov = CG_GetFov();
	fovScale = 1.0f;

	int width = *(int*)CGAME_OFF(0x3020D338), height = *(int*)CGAME_OFF(0x3020D33C); // cg.refdef
	float aspect = height > 0 ? (float)width / height : 0;
	if (aspect > 4.0f / 3.0f && Cvar_VariableIntegerValue("cg_widescreenfov")) {
		float HorPlusFov(float fov, float aspect);
		float widened = HorPlusFov(fov, aspect);
		fovScale = widened / fov;
		fov = widened;
	}
	return fov;
}

int CG_CalcFov_HorPlus() {
	int(*CG_CalcFov)() = (int(*)())CGAME_OFF(0x300345E0);
	int inWater = CG_CalcFov();
	*(float*)CGAME_OFF(0x3020F3A8) /= fovScale; // zoomSensitivity = fov_x / cg_fov: keep mouse speed unchanged
	return inWater;
}

void CG_DrawFPS(float y) {
	cvar_t* xui_fps = Cvar_Get("cg_xui_fps", "1", CVAR_ARCHIVE);

	if (xui_fps->integer) {
		cvar_t* x = Cvar_Get("cg_xui_fps_x", "597", CVAR_ARCHIVE); // uh this x y values just look good with my hp bar
		cvar_t* y = Cvar_Get("cg_xui_fps_y", "8", CVAR_ARCHIVE);

		#define	FPS_FRAMES 4
		static int previousTimes[FPS_FRAMES];
		static int index;
		int	i, total;
		int	fps;
		static int previous;
		int	t, frameTime;

		t = timeGetTime();
		frameTime = t - previous;
		previous = t;
		previousTimes[index % FPS_FRAMES] = frameTime;
		index++;

		if (index > FPS_FRAMES) {
			total = 0;
			for (i = 0; i < FPS_FRAMES; i++) {
				total += previousTimes[i];
			}
			if (!total) {
				total = 1;
			}
			fps = 1000 * FPS_FRAMES / total;

			M_DrawShadowString(x->integer, y->integer, 1, .20, vColorWhite, va("FPS: %d", fps), NULL, NULL, NULL);
		}
	} else {
		void(*call)(float);
		*(int*)&call = CGAME_OFF(0x30015AE0);
		call(y);
	}
}
void CG_Init(DWORD base) {
	cgame_mp = base;
	
	__call(CGAME_OFF(0x3001617E), (int)CG_DrawFPS);

	void HUD_InitCgame(DWORD base);
	HUD_InitCgame(base); // widescreen HUD: projected points

	Cvar_Get("cg_widescreenfov", "1", CVAR_ARCHIVE);
	__call(CGAME_OFF(0x300345E1), (int)CG_GetFov_HorPlus);
	__jmp(CGAME_OFF(0x30034E31), (int)CG_CalcFov_HorPlus);
	
	*(UINT32*)CGAME_OFF(0x300769EC) = CVAR_ARCHIVE; // Enable cg_fov
	*(UINT32*)CGAME_OFF(0x30076E6C) = CVAR_ARCHIVE; // Unlock cg_thirdpersonangle
	*(UINT32*)CGAME_OFF(0x30076E7C) = CVAR_ARCHIVE; // Unlock cg_thirdperson

	__jmp(CGAME_OFF(0x30008822), (int)hook_PM_WalkMove_Naked);
	resume_addr_PM_WalkMove = CGAME_OFF(0x300088be);

	__jmp(CGAME_OFF(0x3000e171), (int)hook_PM_SlideMove_Naked);
	resume_addr_PM_SlideMove = CGAME_OFF(0x3000e18e);

	__jmp(CGAME_OFF(0x30008320), (int)hook_Jump_Start_Naked);
	resume_addr_Jump_Start = CGAME_OFF(0x3000833a);

	__jmp(CGAME_OFF(0x30007ae0), (int)custom_PM_GetReducedFriction_Naked);
}