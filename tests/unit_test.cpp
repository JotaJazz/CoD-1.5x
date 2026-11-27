// Unit tests for the pure parts of the DLL. Build from a VS x86 prompt in this folder:
//   cl /EHsc /D_CRT_SECURE_NO_WARNINGS unit_test.cpp ..\sha256.cpp ..\fov.cpp ..\names.cpp && unit_test.exe
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <assert.h>

bool Sys_FileSha256(const char* path, char hex[65]);
float HorPlusFov(float fov, float aspect);
char* Com_CleanHostname(char* hostname, int size, bool colors);
char* Com_CleanMapname(char* mapname);
const char* Com_GametypeName(char* gt, bool colors);

static float VerticalFov(float fovX, float aspect) { // stock cgame formula: fov_y from fov_x
	return atanf(tanf(fovX * 3.14159265f / 360.0f) / aspect) * 360.0f / 3.14159265f;
}

int main() {
	char h[65];
	FILE* f = fopen("abc.txt", "wb"); fputs("abc", f); fclose(f);
	f = fopen("empty.txt", "wb"); fclose(f);
	assert(Sys_FileSha256("abc.txt", h) && !strcmp(h, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"));
	assert(Sys_FileSha256("empty.txt", h) && !strcmp(h, "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"));
	assert(!Sys_FileSha256("missing.txt", h));

	assert(HorPlusFov(80, 4.0f / 3.0f) == 80 && HorPlusFov(80, 5.0f / 4.0f) == 80);
	assert(fabsf(HorPlusFov(80, 16.0f / 9.0f) - 96.42f) < 0.01f);
	for (float fov = 55; fov <= 160; fov += 5) // 16:9 keeps the 4:3 vertical view
		assert(fabsf(VerticalFov(HorPlusFov(fov, 16.0f / 9.0f), 16.0f / 9.0f) - VerticalFov(fov, 4.0f / 3.0f)) < 0.01f);

	char host[64];
	strcpy(host, "  !^1My ^7Server"); assert(!strcmp(Com_CleanHostname(host, sizeof(host), false), "My Server"));
	strcpy(host, "^1^2   ");          assert(!strcmp(Com_CleanHostname(host, sizeof(host), false), "Unnamed Server"));
	strcpy(host, "^1^2");             assert(!strcmp(Com_CleanHostname(host, sizeof(host), true), "Unnamed Server"));
	strcpy(host, "^1Red");            assert(!strcmp(Com_CleanHostname(host, sizeof(host), true), "^1Red"));

	char map[64];
	strcpy(map, "mp_carentan");  assert(!strcmp(Com_CleanMapname(map), "Carentan"));
	strcpy(map, "XP_my_map");    assert(!strcmp(Com_CleanMapname(map), "My Map"));
	strcpy(map, "dm_mp_test");   assert(!strcmp(Com_CleanMapname(map), "Dm Mp Test"));

	char gt[16];
	strcpy(gt, "sd");    assert(!strcmp(Com_GametypeName(gt, false), "Search & Destroy"));
	strcpy(gt, "^1ctf"); assert(!strcmp(Com_GametypeName(gt, false), "ctf"));
	assert(!strcmp(Com_GametypeName((char*)"", false), "Unknown Gametype"));

	printf("ok\n");
	return 0;
}
