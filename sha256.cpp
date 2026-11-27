#include <windows.h>
#include <wincrypt.h>
#include <stdio.h>
#pragma comment(lib, "advapi32.lib")

// SHA-256 of a file as 64 lowercase hex chars; false if it can't be read.
bool Sys_FileSha256(const char* path, char hex[65]) {
	bool ok = false;
	HCRYPTPROV prov = 0;
	HCRYPTHASH hash = 0;
	FILE* f = fopen(path, "rb");
	if (!f)
		return false;

	if (CryptAcquireContextA(&prov, NULL, NULL, PROV_RSA_AES, CRYPT_VERIFYCONTEXT) && CryptCreateHash(prov, CALG_SHA_256, 0, 0, &hash)) {
		BYTE buf[16384];
		size_t n;
		ok = true;
		while (ok && (n = fread(buf, 1, sizeof(buf), f)) > 0)
			ok = CryptHashData(hash, buf, (DWORD)n, 0) != FALSE;
		ok = ok && !ferror(f);

		BYTE digest[32];
		DWORD len = sizeof(digest);
		if (ok && CryptGetHashParam(hash, HP_HASHVAL, digest, &len, 0) && len == 32) {
			for (int i = 0; i < 32; i++)
				sprintf(hex + i * 2, "%02x", digest[i]);
		}
		else ok = false;
	}

	if (hash) CryptDestroyHash(hash);
	if (prov) CryptReleaseContext(prov, 0);
	fclose(f);
	return ok;
}
