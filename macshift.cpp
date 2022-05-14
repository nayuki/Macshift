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


#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <string>
#include <windows.h>
#include <objbase.h>
#include <netcon.h>
#include <stdio.h>
#include "validmacs.h"


static void setMac(const char *AdapterName, const std::string &newMac) {
	HKEY hListKey = nullptr;
	RegOpenKeyEx(HKEY_LOCAL_MACHINE, "SYSTEM\\CurrentControlSet\\Control\\Network\\{4D36E972-E325-11CE-BFC1-08002BE10318}",
		0, KEY_READ, &hListKey);
	if (hListKey == nullptr) {
		puts("Failed to open adapter list key");
		return;
	}
	FILETIME writtenTime;
	char keyNameBuf[512];
	DWORD keyNameBufSiz = 512;
	DWORD crap;
	bool found = false;
	for (DWORD i = 0; RegEnumKeyEx(hListKey, i, keyNameBuf, &keyNameBufSiz, 0, nullptr, nullptr, &writtenTime) == ERROR_SUCCESS; i++) {
		std::string keyNameBuf2 = keyNameBuf;
		keyNameBuf2 += "\\Connection";
		HKEY hKey = nullptr;
		RegOpenKeyEx(hListKey, keyNameBuf2.c_str(), 0, KEY_READ, &hKey);
		if (hKey != nullptr) {
			keyNameBufSiz = 512;
			if (RegQueryValueEx(hKey, "Name", 0, &crap, (LPBYTE)keyNameBuf2.c_str(), &keyNameBufSiz)
					== ERROR_SUCCESS && std::strcmp(keyNameBuf2.c_str(), AdapterName) == 0) {
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
	if (hListKey == nullptr) {
		puts("Failed to open adapter list key in Phase 2");
		return;
	}
	char keyNameBuf2[512];
	for (DWORD i = 0; RegEnumKeyEx(hListKey, i, keyNameBuf2, &keyNameBufSiz, 0, nullptr, nullptr, &writtenTime) == ERROR_SUCCESS; i++) {
		HKEY hKey = nullptr;
		RegOpenKeyEx(hListKey, keyNameBuf2, 0, KEY_READ | KEY_SET_VALUE, &hKey);
		if (hKey != nullptr) {
			keyNameBufSiz = 512;
			char buf[512];
			if ((RegQueryValueEx(hKey, "NetCfgInstanceId", 0, &crap, (LPBYTE)buf, &keyNameBufSiz)
					== ERROR_SUCCESS) && (std::strcmp(buf, keyNameBuf) == 0)) {
				RegSetValueEx(hKey, "NetworkAddress", 0, REG_SZ, (LPBYTE)newMac.c_str(), static_cast<DWORD>(newMac.size() + 1));
				//printf("Updating adapter index %s (%s=%s)\n", keyNameBuf2, buf, keyNameBuf);
				//break;
			}
			RegCloseKey(hKey);
		}
		keyNameBufSiz = 512;
	}
	RegCloseKey(hListKey);
}


static void resetAdapter(const char *AdapterName) {
	struct _GUID guid = {0xBA126AD1, 0x2166, 0x11D1, 0};
	memcpy(guid.Data4, "\xB1\xD0\x00\x80\x5F\xC1\x27\x0E", 8);
	
	void (__stdcall *NcFreeNetConProperties) (NETCON_PROPERTIES *);
	HMODULE NetShell_Dll = LoadLibrary("Netshell.dll");
	if (NetShell_Dll == nullptr) {
		puts("Couldn't load Netshell.dll");
		return;
	}
	NcFreeNetConProperties = (void (__stdcall *)(struct tagNETCON_PROPERTIES *))GetProcAddress(NetShell_Dll, "NcFreeNetconProperties");
	if (NcFreeNetConProperties == nullptr) {
		puts("Couldn't load required DLL function");
		return;
	}
	
	std::wstring buf;
	for (std::size_t i = 0; i < std::strlen(AdapterName); i++)
		buf.push_back(static_cast<wchar_t>(AdapterName[i]));
	CoInitialize(0);
	INetConnectionManager *pNCM = nullptr;
	HRESULT hr = ::CoCreateInstance(guid,
		nullptr,
		CLSCTX_ALL,
		__uuidof(INetConnectionManager),
		(void**)&pNCM);
	if (pNCM == nullptr)
		puts("Failed to instantiate required object");
	else {
		IEnumNetConnection *pENC;
		pNCM->EnumConnections(NCME_DEFAULT, &pENC);
		if (pENC == nullptr)
			puts("Could not enumerate Network Connections");
		else {
			INetConnection *pNC;
			ULONG fetched;
			NETCON_PROPERTIES *pNCP;
			do {
				pENC->Next(1, &pNC, &fetched);
				if (fetched != 0 && pNC != nullptr) {
					pNC->GetProperties(&pNCP);
					if (pNCP != nullptr) {
						if (wcscmp(pNCP->pszwName, buf.c_str()) == 0) {
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
	CoUninitialize();
}


static bool isValidMac(const char *str) {
	if (std::strlen(str) != 12)
		return false;
	for (int i = 0; i < 12; i++) {
		char c = str[i];
		bool ok = ('0' <= c && c <= '9')
		       || ('a' <= c && c <= 'f')
		       || ('A' <= c && c <= 'F');
		if (!ok)
			return false;
	}
	return true;
}


static void showHelp() {
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
static std::string randomizeMac() {
	long long temp = static_cast<long long>(validMacs[rand() % validMacs.size()]);
	for (int i = 0; i < 3; i++) {
		temp <<= 8;
		temp |= rand() & 0xFF;
	}
	
	static const char *HEX_DIGITS = "0123456789ABCDEF";
	std::string result;
	for (int i = 11; i >= 0; i--) {
		result.insert(result.begin(), HEX_DIGITS[temp & 0xF]);
		temp >>= 4;
	}
	return result;
}


int main(int argc, char **argv) {
	printf("Macshift v%i.%i, MAC Changing Utility by Nathan True, macshift@natetrue.com\n\n", versionMajor, versionMinor);
	
	if (argc == 1) {
		showHelp();
		return EXIT_SUCCESS;
	}
	
	//Start out with a random MAC
	srand(GetTickCount());
	std::string newMac = randomizeMac();
	
	//Parse commandline arguments
	const char *adapter = "Wireless";
	for (int i = 1; i < argc; i++) {
		const char *arg = argv[i];
		if (arg[0] == '-') {
			switch (arg[1]) {
				case '-': //Extended argument
					if (std::strcmp(arg+2, "help") == 0) {
						showHelp();
						return EXIT_SUCCESS;
					}
					break;
				case 'r': //Random setting, this is the default
					break;
				case 'i': //Adapter name follows
					if (argc > i + 1)
						adapter = argv[++i];
					break;
				case 'd': //Reset the MAC address
					newMac = "";
			}
		} else if (isValidMac(arg))
			newMac = arg;
		else
			printf("MAC String %s is not valid. MAC addresses must m/^[0-9a-fA-F]{12}$/.\n", arg);
	}
	
	printf("Setting MAC on adapter '%s' to %s...\n", adapter, newMac.size() > 0 ? newMac.c_str() : "original MAC");
	setMac(adapter, newMac.c_str());
	puts("Resetting adapter...");
	fflush(stdout);
	resetAdapter(adapter);
	puts("Done");
	return EXIT_SUCCESS;
}
