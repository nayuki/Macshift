/***************************************************
Macshift - the simple windows MAC address changing utility
Copyright (C) 2004  Nathan True macshift@natetrue.com www.natetrue.com

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
***************************************************/

//To compile, all you need is an updated Windows Platform SDK and some form of compiler.

static const int versionMajor = 1;
static const int versionMinor = 1;

#include <windows.h>
#include <objbase.h>
#include <netcon.h>
#include <stdio.h>
#include "validmacs.h"

static void SetMAC(const char *AdapterName, const char *NewMAC) {
	HKEY hListKey = nullptr;
	HKEY hKey = nullptr;
	RegOpenKeyEx(HKEY_LOCAL_MACHINE, "SYSTEM\\CurrentControlSet\\Control\\Network\\{4D36E972-E325-11CE-BFC1-08002BE10318}",
		0, KEY_READ, &hListKey);
	if (!hListKey) {
		puts("Failed to open adapter list key");
		return;
	}
	FILETIME writtenTime;
	char keyNameBuf[512], keyNameBuf2[512];
	DWORD keyNameBufSiz = 512;
	DWORD crap;
	int i = 0;
	bool found = false;
	while (RegEnumKeyEx(hListKey, i++, keyNameBuf, &keyNameBufSiz, 0, nullptr, nullptr, &writtenTime)
			== ERROR_SUCCESS) {
		_snprintf(keyNameBuf2, 512, "%s\\Connection", keyNameBuf);
		hKey = nullptr;
		RegOpenKeyEx(hListKey, keyNameBuf2, 0, KEY_READ, &hKey);
		if (hKey) {
			keyNameBufSiz = 512;
			if (RegQueryValueEx(hKey, "Name", 0, &crap, (LPBYTE)keyNameBuf2, &keyNameBufSiz)
					== ERROR_SUCCESS && strcmp(keyNameBuf2, AdapterName) == 0) {
				printf("Adapter ID is %s\n", keyNameBuf);
				found = true;
				break;
			}
			RegCloseKey(hKey);
		}
		keyNameBufSiz = 512;
	}
	RegCloseKey(hListKey);
	if (!found) {
		printf("Could not find adapter name '%s'.\nPlease make sure this is the name you gave it in Network Connections.\n", AdapterName);
		return;
	}
	RegOpenKeyEx(HKEY_LOCAL_MACHINE, "SYSTEM\\CurrentControlSet\\Control\\Class\\{4D36E972-E325-11CE-BFC1-08002bE10318}",
		0, KEY_READ, &hListKey);
	if (!hListKey) {
		puts("Failed to open adapter list key in Phase 2");
		return;
	}
	i = 0;
	char buf[512];
	while (RegEnumKeyEx(hListKey, i++, keyNameBuf2, &keyNameBufSiz, 0, nullptr, nullptr, &writtenTime)
			== ERROR_SUCCESS) {
		hKey = nullptr;
		RegOpenKeyEx(hListKey, keyNameBuf2, 0, KEY_READ | KEY_SET_VALUE, &hKey);
		if (hKey) {
			keyNameBufSiz = 512;
			if ((RegQueryValueEx(hKey, "NetCfgInstanceId", 0, &crap, (LPBYTE)buf, &keyNameBufSiz)
					== ERROR_SUCCESS) && (strcmp(buf, keyNameBuf) == 0)) {
				RegSetValueEx(hKey, "NetworkAddress", 0, REG_SZ, (LPBYTE)NewMAC, strlen(NewMAC) + 1);
				//printf("Updating adapter index %s (%s=%s)\n", keyNameBuf2, buf, keyNameBuf);
				//break;
			}
			RegCloseKey(hKey);
		}
		keyNameBufSiz = 512;
	}
	RegCloseKey(hListKey);
	
}

static void ResetAdapter(const char *AdapterName) {
	struct _GUID guid = {0xBA126AD1,0x2166,0x11D1,0};
	memcpy(guid.Data4, "\xB1\xD0\x00\x80\x5F\xC1\x27\x0E", 8);
	unsigned short *buf = new unsigned short[strlen(AdapterName)+1];
	
	void (__stdcall *NcFreeNetConProperties) (NETCON_PROPERTIES *);
	HMODULE NetShell_Dll = LoadLibrary("Netshell.dll");
	if (!NetShell_Dll) {
		puts("Couldn't load Netshell.dll");
		return;
	}
	NcFreeNetConProperties = (void (__stdcall *)(struct tagNETCON_PROPERTIES *))GetProcAddress(NetShell_Dll, "NcFreeNetconProperties");
	if (!NcFreeNetConProperties) {
		puts("Couldn't load required DLL function");
		return;
	}
	
	for (unsigned int i = 0; i <= strlen(AdapterName); i++) {
		buf[i] = AdapterName[i];
	}
	CoInitialize(0);
	INetConnectionManager *pNCM = nullptr;
	HRESULT hr = ::CoCreateInstance(guid,
		nullptr,
		CLSCTX_ALL,
		__uuidof(INetConnectionManager),
		(void**)&pNCM);
	if (!pNCM)
		puts("Failed to instantiate required object");
	else {
		IEnumNetConnection *pENC;
		pNCM->EnumConnections(NCME_DEFAULT, &pENC);
		if (!pENC) {
			puts("Could not enumerate Network Connections");
		}
		else {
			INetConnection *pNC;
			ULONG fetched;
			NETCON_PROPERTIES *pNCP;
			do {
				pENC->Next(1, &pNC, &fetched);
				if (fetched && pNC) {
					pNC->GetProperties(&pNCP);
					if (pNCP) {
						if (wcscmp(pNCP->pszwName, buf) == 0) {
							pNC->Disconnect();
							pNC->Connect();
						}
						NcFreeNetConProperties(pNCP);
					}
				}
			} while (fetched);
			pENC->Release();
		}
		pNCM->Release();
	}
	
	FreeLibrary(NetShell_Dll);
	CoUninitialize ();
}

static bool IsValidMAC(const char *str) {
	if (strlen(str) != 12)
		return false;
	for (int i = 0; i < 12; i++) {
		if ((str[i] < '0' || str[i] > '9')
				&& (str[i] < 'a' || str[i] > 'f')
				&& (str[i] < 'A' || str[i] > 'F')) {
			return false;
		}
	}
	return true;
}

static void ShowHelp() {
	puts("Usage: macshift [options] [mac-address]\n");
	puts("Options:");
	puts("\t-i [adapter-name]     The adapter name from Network Connections.");
	puts("\t-r                    Uses a random MAC address. This is the default.");
	puts("\t-d                    Restores the original MAC address.");
	puts("\t--help                Shows this screen.\n");
	puts("Macshift uses special undocumented functions in the Windows COM Interface that");
	puts(" allow you to change an adapter's MAC address without needing to restart.");
	puts("When you change a MAC address, all your connections are closed automatically");
	puts(" and your adapter is reset.");
}

//Generates a random MAC that is actually plausible
static void RandomizeMAC(char *newmac) {
	_snprintf(newmac, 6, "%06X", rand() % numMacs);
	for (int i = 3; i < 6; i++) {
		_snprintf(&newmac[i*2], 2, "%02X", rand() & 0xFF);
	}
	newmac[12] = 0;
}

int main(int argc, char **argv) {
	printf("Macshift v%i.%i, MAC Changing Utility by Nathan True, macshift@natetrue.com\n\n", versionMajor, versionMinor);
	
	//Parse commandline arguments
	const char *adapter = "Wireless";
	char newmac[13];
	int i;
	if (argc == 1) {
		ShowHelp();
		return 0;
	}
	//Start out with a random MAC
	srand(GetTickCount());
	RandomizeMAC(newmac);
	for (i = 1; i < argc; i++) {
		if (argv[i][0] == '-') {
			switch (argv[i][1]) {
				case '-': //Extended argument
					if (strcmp(argv[i]+2, "help") == 0) {
						ShowHelp();
						return 0;
					}
					break;
				case 'r': //Random setting, this is the default
					break;
				case 'i': //Adapter name follows
					if (argc > i + 1)
						adapter = argv[++i];
					break;
				case 'd': //Reset the MAC address
					newmac[0] = 0;
			}
		} else if (IsValidMAC(argv[i]))
			strncpy(newmac, argv[i], 13);
		else
			printf("MAC String %s is not valid. MAC addresses must m/^[0-9a-fA-F]{12}$/.\n", argv[i]);
	}
	
	printf("Setting MAC on adapter '%s' to %s...\n", adapter, newmac[0] ? newmac : "original MAC");
	SetMAC(adapter, newmac);
	puts("Resetting adapter...");
	fflush(stdout);
	ResetAdapter(adapter);
	puts("Done");
	return 0;
}
