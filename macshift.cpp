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

#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
#include <windows.h>
#include <objbase.h>
#include <netcon.h>
#include <stdio.h>


// Function prototypes
static void showHelp(const std::string &exePath);
static bool isValidMac(const std::string &str);
static std::string randomizeMac();
static void setMac(const std::string &AdapterName, const std::string &newMac);
static void resetAdapter(const std::string &AdapterName);


int main(int argc, char **argv) {
	std::cerr << "Macshift v1.1: A utility to change a network adapter's MAC address." << std::endl;
	std::cerr << std::endl;
	
	std::vector<std::string> argVec;
	for (int i = 0; i < argc; i++)
		argVec.push_back(std::string(argv[i]));
	
	std::string adapter = "";
	bool isMacModeSet = false;
	srand(static_cast<unsigned int>(GetTickCount64()));
	std::string newMac = randomizeMac();
	
	// Parse command-line arguments
	for (size_t i = 1; i < argVec.size(); i++) {
		const std::string &arg = argVec.at(i);
		if (arg.find("-") == 0) {  // A flag
			if (arg == "-h") {
				showHelp(argVec[0]);
				return EXIT_FAILURE;
			} else if (arg == "-d" || arg == "-r" || arg == "-a") {
				if (isMacModeSet) {
					std::cerr << "Error: Command-line arguments contain more than one MAC address mode." << std::endl;
					return EXIT_FAILURE;
				}
				isMacModeSet = true;
				if (arg == "-d")
					newMac = "";
				else if (arg == "-r")
					;  // Do nothing else because newMac is already random
				else if (arg == "-a") {
					if (argVec.size() - i <= 1) {
						std::cerr << "Error: Missing MAC address argument." << std::endl;
						return EXIT_FAILURE;
					}
					i++;
					const std::string &val = argVec.at(i);
					if (!isValidMac(val)) {
						std::cerr << "Error: Invalid MAC address, must match pattern /[0-9a-fA-F]{12}/." << std::endl;
						return EXIT_FAILURE;
					}
					newMac = val;
				} else
					throw std::logic_error("Unreachable");
			} else {
				std::cerr << "Error: Unrecognized command-line flag." << std::endl;
				return EXIT_FAILURE;
			}
		} else {  // Not a flag
			if (!adapter.empty()) {
				std::cerr << "Error: Command-line arguments contain more than network adapter name." << std::endl;
				return EXIT_FAILURE;
			}
			adapter = arg;
		}
	}
	if (adapter == "") {
		showHelp(argVec[0]);
		return EXIT_FAILURE;
	}
	
	std::cerr << "Setting MAC on adapter '" << adapter << "' to " << (newMac.size() > 0 ? newMac : std::string("original MAC")) << "..." << std::endl;
	setMac(adapter, newMac.c_str());
	std::cerr << "Resetting adapter..." << std::endl;
	resetAdapter(adapter);
	std::cerr << "Done" << std::endl;
	return EXIT_SUCCESS;
}


static void showHelp(const std::string &exePath) {
	std::cerr << "Usage: " << exePath << " AdapterName [Options]" << std::endl;
	
	const std::vector<const char *> LINES{
		"",
		"Options:",
		"    -r               Use a random MAC address (default action).",
		"    -a MacAddress    Use the given MAC address.",
		"    -d               Restore the original MAC address.",
		"",
		"    -h               Show this help screen.",
		"",
		"Macshift uses special undocumented functions in the Windows COM Interface",
		"that allow you to change an adapter's MAC address without needing to restart.",
		"",
		"When you change a MAC address, all your connections",
		"are closed automatically and your adapter is reset.",
	};
	for (const char *line : LINES)
		std::cerr << line << std::endl;
}


static bool isValidMac(const std::string &str) {
	if (str.size() != 12)
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


extern std::vector<unsigned long> validMacs;

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


static void setMac(const std::string &adapterName, const std::string &newMac) {
	HKEY hListKey = nullptr;
	RegOpenKeyEx(HKEY_LOCAL_MACHINE, "SYSTEM\\CurrentControlSet\\Control\\Network\\{4D36E972-E325-11CE-BFC1-08002BE10318}", 0, KEY_READ, &hListKey);
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
			if (RegQueryValueEx(hKey, "Name", 0, &crap, (LPBYTE)keyNameBuf2.c_str(), &keyNameBufSiz) == ERROR_SUCCESS
					&& keyNameBuf2 == adapterName) {
				std::cerr << "Adapter ID is " << keyNameBuf << std::endl;
				found = true;
				break;
			}
			RegCloseKey(hKey);
		}
		keyNameBufSiz = 512;
	}
	RegCloseKey(hListKey);
	if (!found) {
		std::cerr << "Could not find adapter name '" << adapterName << "'." << std::endl;
		std::cerr << "Please make sure this is the name you gave it in Network Connections." << std::endl;
		return;
	}
	
	RegOpenKeyEx(HKEY_LOCAL_MACHINE, "SYSTEM\\CurrentControlSet\\Control\\Class\\{4D36E972-E325-11CE-BFC1-08002BE10318}", 0, KEY_READ, &hListKey);
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
			if (RegQueryValueEx(hKey, "NetCfgInstanceId", 0, &crap, (LPBYTE)buf, &keyNameBufSiz) == ERROR_SUCCESS
					&& std::string(buf) == std::string(keyNameBuf)) {
				RegSetValueEx(hKey, "NetworkAddress", 0, REG_SZ, (LPBYTE)newMac.c_str(), static_cast<DWORD>(newMac.size() + 1));
				//std::cerr << "Updating adapter index " << keyNameBuf2 << " (" << buf << "=" << keyNameBuf << ")" << std::endl;
				//break;
			}
			RegCloseKey(hKey);
		}
		keyNameBufSiz = 512;
	}
	RegCloseKey(hListKey);
}


static void resetAdapter(const std::string &adapterName) {
	struct _GUID guid = {0xBA126AD1, 0x2166, 0x11D1, {0xB1,0xD0,0x00,0x80,0x5F,0xC1,0x27,0x0E}};
	
	HMODULE netshellLib = LoadLibrary("Netshell.dll");
	if (netshellLib == nullptr) {
		puts("Couldn't load Netshell.dll");
		return;
	}
	void (__stdcall *NcFreeNetConProperties)(NETCON_PROPERTIES *) =
		(void (__stdcall *)(struct tagNETCON_PROPERTIES *))
		GetProcAddress(netshellLib, "NcFreeNetconProperties");
	if (NcFreeNetConProperties == nullptr) {
		puts("Couldn't load required DLL function");
		return;
	}
	
	std::wstring buf;
	for (std::size_t i = 0; i < adapterName.size(); i++)
		buf.push_back(static_cast<wchar_t>(adapterName[i]));
	(void)CoInitialize(nullptr);
	INetConnectionManager *pNCM = nullptr;
	HRESULT hr = ::CoCreateInstance(guid, nullptr, CLSCTX_ALL, __uuidof(INetConnectionManager), (void**)&pNCM);
	if (pNCM == nullptr)
		puts("Failed to instantiate required object");
	else {
		IEnumNetConnection *pENC;
		pNCM->EnumConnections(NCME_DEFAULT, &pENC);
		if (pENC == nullptr)
			puts("Could not enumerate Network Connections");
		else {
			while (true) {
				INetConnection *pNC;
				ULONG fetched;
				pENC->Next(1, &pNC, &fetched);
				if (fetched == 0)
					break;
				if (pNC != nullptr) {
					NETCON_PROPERTIES *pNCP;
					pNC->GetProperties(&pNCP);
					if (pNCP != nullptr) {
						if (wcscmp(pNCP->pszwName, buf.c_str()) == 0) {
							pNC->Disconnect();
							pNC->Connect();
						}
						NcFreeNetConProperties(pNCP);
					}
				}
			}
			pENC->Release();
		}
		pNCM->Release();
	}
	
	FreeLibrary(netshellLib);
	CoUninitialize();
}
