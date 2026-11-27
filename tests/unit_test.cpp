// Unit tests for the pure parts of the DLL. Build from a VS x86 prompt in this folder:
//   cl /EHsc /D_CRT_SECURE_NO_WARNINGS unit_test.cpp ..\sha256.cpp ..\fov.cpp ..\names.cpp ..\update.cpp && unit_test.exe
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <assert.h>

bool Sys_FileSha256(const char* path, char hex[65]);
float HorPlusFov(float fov, float aspect);
float HudScale(int width, int height);
char* Com_CleanHostname(char* hostname, int size, bool colors);
char* Com_CleanMapname(char* mapname);
const char* Com_GametypeName(char* gt, bool colors);
int ParseLatestBuild(const char* json);
bool ParseReleaseSha256(const char* json, char hex[65]);
bool Com_IsSafeDownloadName(const char* name);
const char* Com_AddChatTime(const char* cmd, const char* time, char* out, int size);
bool Con_IsSavedHistoryLine(const char* line);
int Com_ParseHostPort(const char* address, char* host, int hostSize, int defaultPort);

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

	assert(HudScale(1024, 768) == 1 && HudScale(1280, 1024) == 1 && HudScale(1440, 1080) == 1);
	assert(fabsf(HudScale(1920, 1080) - 0.75f) < 1e-6f && fabsf(HudScale(2560, 1080) - 0.5625f) < 1e-6f);

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

	assert(ParseLatestBuild("{\"url\":\"x\",\"tag_name\":\"v2045\",\"name\":\"CoD 1.5x 2045\"}") == 2045);
	assert(ParseLatestBuild("{\"tag_name\" : \"2046\"}") == 2046);
	assert(ParseLatestBuild("{\"message\":\"Not Found\"}") == 0);
	assert(ParseLatestBuild("{\"tag_name\":\"beta\"}") == 0);
	assert(ParseLatestBuild(NULL) == 0);

	assert(Com_IsSafeDownloadName("main/mp_dust3.pk3") && Com_IsSafeDownloadName("mymod/ZZZ_Sounds.PK3"));
	assert(!Com_IsSafeDownloadName("main/../../evil.pk3") && !Com_IsSafeDownloadName("../main.pk3"));
	assert(!Com_IsSafeDownloadName("main/evil.dll") && !Com_IsSafeDownloadName("main/evil.pk3.exe"));
	assert(!Com_IsSafeDownloadName("C:/Windows/evil.pk3") && !Com_IsSafeDownloadName("main\\evil.pk3"));
	assert(!Com_IsSafeDownloadName("/main/evil.pk3") && !Com_IsSafeDownloadName("main/sub/evil.pk3"));
	assert(!Com_IsSafeDownloadName("evil.pk3") && !Com_IsSafeDownloadName("main/.pk3") && !Com_IsSafeDownloadName(""));
	assert(!Com_IsSafeDownloadName(NULL));

	char sha[65];
	const char* notes = "{\"body\":\"Built by GitHub Actions. **SHA-256** `cod15x.dll`: `AB12cd34ab12cd34ab12cd34ab12cd34ab12cd34ab12cd34ab12cd34ab12cd34`\"}";
	assert(ParseReleaseSha256(notes, sha) && !strcmp(sha, "ab12cd34ab12cd34ab12cd34ab12cd34ab12cd34ab12cd34ab12cd34ab12cd34"));
	assert(!ParseReleaseSha256("{\"body\":\"SHA-256 `abc`\"}", sha) && !ParseReleaseSha256("{\"body\":\"no hash\"}", sha));
	assert(!ParseReleaseSha256("SHA-256 ab12cd34ab12cd34ab12cd34ab12cd34ab12cd34ab12cd34ab12cd34ab12cd34ab", sha)); // 66 hex
	assert(!ParseReleaseSha256(NULL, sha));

	char hostbuf[16];
	assert(Com_ParseHostPort("master.cod.pm", hostbuf, sizeof(hostbuf), 20510) == 20510 && !strcmp(hostbuf, "master.cod.pm"));
	assert(Com_ParseHostPort("1.2.3.4:28960", hostbuf, sizeof(hostbuf), 20510) == 28960 && !strcmp(hostbuf, "1.2.3.4"));
	assert(!Com_ParseHostPort("", hostbuf, sizeof(hostbuf), 20510) && !Com_ParseHostPort(":20510", hostbuf, sizeof(hostbuf), 20510));
	assert(!Com_ParseHostPort("host:0", hostbuf, sizeof(hostbuf), 20510) && !Com_ParseHostPort("host:99999", hostbuf, sizeof(hostbuf), 20510));
	assert(!Com_ParseHostPort("a.very.long.hostname.example", hostbuf, sizeof(hostbuf), 20510) && !Com_ParseHostPort(NULL, hostbuf, sizeof(hostbuf), 1));

	char chat[64];
	assert(!strcmp(Com_AddChatTime("h \"\x15Jota^7: hola\"", "[12:05]", chat, sizeof(chat)), "h \"\x15[12:05] Jota^7: hola\""));
	assert(!strcmp(Com_AddChatTime("i \"Team: go\"", "[01:00]", chat, sizeof(chat)), "i \"[01:00] Team: go\""));
	const char* other = "cs 1 \"x\"";
	assert(Com_AddChatTime(other, "[12:05]", chat, sizeof(chat)) == other);
	const char* hud = "hud \"x\""; // starts with h but is not chat
	assert(Com_AddChatTime(hud, "[12:05]", chat, sizeof(chat)) == hud);
	const char* tight = "h \"0123456789012345678901234567890123456789012345678901234\"";
	assert(Com_AddChatTime(tight, "[12:05]", chat, sizeof(chat)) == tight);
	assert(Com_AddChatTime(NULL, "[12:05]", chat, sizeof(chat)) == NULL);

	assert(Con_IsSavedHistoryLine("/connect 1.2.3.4") && Con_IsSavedHistoryLine("cg_fov 90"));
	assert(!Con_IsSavedHistoryLine("") && !Con_IsSavedHistoryLine("  /") && !Con_IsSavedHistoryLine(NULL));
	assert(!Con_IsSavedHistoryLine("/rcon login secret") && !Con_IsSavedHistoryLine("RCON status"));
	assert(!Con_IsSavedHistoryLine("/set rconPassword x") && !Con_IsSavedHistoryLine("password abc"));

	printf("ok\n");
	return 0;
}
