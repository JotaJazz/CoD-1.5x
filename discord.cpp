#include "shared.h"
#include "client.h"
#pragma comment(lib, "libs/discord/discord-rpc.lib")
#include "libs/discord/discord_rpc.h"
#include "libs/discord/discord_register.h"

// Reference: https://github.com/callofduty4x/CoD4x_Client_pub

struct discordData_s {
	unsigned int nextupdatetime;
};
static struct discordData_s discordData;

// The Discord application has no uploaded assets; Discord accepts an image URL instead of an asset key.
#define DISCORD_LOGO "https://raw.githubusercontent.com/JotaJazz/CoD-1.5x/main/assets/discord-logo.png"

void Discord_StatusDemo() {
	DiscordRichPresence discordPresence;
	memset(&discordPresence, 0, sizeof(discordPresence));
	discordPresence.state = "Watching a demo";
	discordPresence.largeImageKey = DISCORD_LOGO;

	Discord_UpdatePresence(&discordPresence);
}

void Discord_StatusIdle() {
	DiscordRichPresence discordPresence;
	memset(&discordPresence, 0, sizeof(discordPresence));
	discordPresence.state = "Looking to play";
	discordPresence.largeImageKey = DISCORD_LOGO;

	Discord_UpdatePresence(&discordPresence);
}

void Discord_StatusPlaying() {
	char* info = clc_stringData + clc_stringOffsets[0];

	char* hostname_p = Info_ValueForKey(info, "sv_hostname");
	char hostname[64] = { 0 };
	Q_strncpyz(hostname, hostname_p, sizeof(hostname));
	char* mapnameClean_p = Info_ValueForKey(info, "mapname");
	char mapnameClean[64] = { 0 };
	Q_strncpyz(mapnameClean, mapnameClean_p, sizeof(mapnameClean));
	char* gametype_p = Info_ValueForKey(info, "g_gametype");
	char gametype[64] = { 0 };
	Q_strncpyz(gametype, gametype_p, sizeof(gametype));

	DiscordRichPresence discordPresence;
	memset(&discordPresence, 0, sizeof(discordPresence));

	discordPresence.state = Com_GametypeName(gametype, false);
	discordPresence.details = Com_CleanHostname(hostname, sizeof(hostname), false);
	discordPresence.largeImageKey = DISCORD_LOGO;
	discordPresence.largeImageText = Com_CleanMapname(mapnameClean);  // cleaned in place: own copy of mapname

	
	int player_count = cgame_mp ? *(int*)CGAME_OFF(0x3020F7B0) : 0;
	if(player_count > 0 && player_count < 65) {
		discordPresence.partySize = player_count;
		discordPresence.partyMax = atoi(Info_ValueForKey(info, "sv_maxclients"));
	}
	
	

	Discord_UpdatePresence(&discordPresence);
}

void Discord_Update() {
	if (discordData.nextupdatetime < *cls_realtime) {
		discordData.nextupdatetime = *cls_realtime + 10 * 1000; // 10 seconds

		if (*clc_demoplaying) {
			Discord_StatusDemo();
		} else if (*cls_state == 6) { // CA_ACTIVE
			Discord_StatusPlaying();
		} else {
			Discord_StatusIdle();
		}
	}
}

void CL_DiscordInitialize() {
	DiscordEventHandlers handlers;
	memset(&handlers, 0, sizeof(handlers));
	Discord_Initialize("1393986752221282394", &handlers, 1, NULL);

	Discord_StatusIdle();
}

void CL_DiscordFrame() {
	Discord_Update();
}

void CL_DiscordShutdown() {
	Discord_Shutdown();
}
