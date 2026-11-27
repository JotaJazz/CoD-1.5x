#include "shared.h"

void Q_strncpyz(char *dest, const char *src, int destsize) {
	if (!src) {
		Com_Error(ERR_FATAL, "Q_strncpyz: NULL src");
	}
	if (destsize < 1) {
		Com_Error(ERR_FATAL, "Q_strncpyz: destsize < 1");
	}

	strncpy(dest, src, destsize - 1);
	dest[destsize - 1] = 0;
}

int Q_stricmpn(const char *s1, const char *s2, int n) {
	int c1, c2;

	do {
		c1 = *s1++;
		c2 = *s2++;

		if (!n--) {
			return 0;       // strings are equal until end point
		}

		if (c1 != c2) {
			if (c1 >= 'a' && c1 <= 'z') {
				c1 -= ('a' - 'A');
			}
			if (c2 >= 'a' && c2 <= 'z') {
				c2 -= ('a' - 'A');
			}
			if (c1 != c2) {
				return c1 < c2 ? -1 : 1;
			}
		}
	} while (c1);

	return 0;       // strings are equal
}

int Q_stricmp(const char *s1, const char *s2) {
	return (s1 && s2) ? Q_stricmpn(s1, s2, 99999) : -1;
}

int Cmd_Argc() {
	return *(int*)0x8D5020;
}

char** cmd_argv = (char**)0x8D2620;

char* Cmd_Argv(int index) {
	if (index >= Cmd_Argc())
		return "";
	return cmd_argv[index];
}

char *Info_ValueForKey(const char *s, const char *key) {
	char pkey[BIG_INFO_KEY];
	static char value[2][BIG_INFO_VALUE];   // use two buffers so compares
	// work without stomping on each other
	static int valueindex = 0;
	char    *o;

	if (!s || !key) {
		return "";
	}

	if (strlen(s) >= BIG_INFO_STRING) {
		Com_Error(ERR_DROP, "Info_ValueForKey: oversize infostring [%s] [%s]", s, key);
	}

	valueindex ^= 1;
	if (*s == '\\') {
		s++;
	}
	while (1)
	{
		o = pkey;
		while (*s != '\\')
		{
			if (!*s) {
				return "";
			}
			*o++ = *s++;
		}
		*o = 0;
		s++;

		o = value[valueindex];

		while (*s != '\\' && *s)
		{
			*o++ = *s++;
		}
		*o = 0;

		if (!Q_stricmp(key, pkey)) {
			return value[valueindex];
		}

		if (!*s) {
			break;
		}
		s++;
	}

	return "";
}

bool GetDesktopResolution(int* pHorizontal, int* pVertical) {
	// real pixels even when Windows scaling is above 100% (GetDesktopWindow's rect is DPI-virtualized)
	DEVMODEA dm = { 0 };
	dm.dmSize = sizeof(dm);

	if (EnumDisplaySettingsA(NULL, ENUM_CURRENT_SETTINGS, &dm)) {
		*pHorizontal = dm.dmPelsWidth;
		*pVertical = dm.dmPelsHeight;

		return true;
	}

	return false;
}
