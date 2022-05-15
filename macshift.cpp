/* 
 * Macshift - the simple Windows MAC address changing utility
 * 
 * Copyright (c) 2004 Nathan True <macshift@natetrue.com>
 * http://www.natetrue.com/
 * Copyright (c) Project Nayuki
 * https://www.nayuki.io/
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
#include <windows.h>
#include <objbase.h>
#include <netcon.h>


// Function prototypes
static void showHelp(const std::string &exePath);
static bool isValidMac(const std::string &str);
static std::string randomizeMac();
static void setMac(const std::string &AdapterName, const std::string &newMac);
static void resetAdapter(const std::string &AdapterName);


int main(int argc, char **argv) {
	std::cerr << "Macshift v1.1 - the simple Windows MAC address changing utility" << std::endl;
	std::cerr << std::endl;
	
	std::vector<std::string> argVec;
	for (int i = 0; i < argc; i++)
		argVec.push_back(std::string(argv[i]));
	
	std::string adapter = "";
	bool isMacModeSet = false;
	srand(static_cast<unsigned int>(GetTickCount64()));
	std::string newMac = randomizeMac();
	
	// Parse command-line arguments
	try {
		for (size_t i = 1; i < argVec.size(); i++) {
			const std::string &arg = argVec.at(i);
			if (arg.find("-") == 0) {  // A flag
				if (arg == "-h") {
					showHelp(argVec[0]);
					return EXIT_FAILURE;
				} else if (arg == "-d" || arg == "-r" || arg == "-a") {
					if (isMacModeSet)
						throw std::invalid_argument("Command-line arguments contain more than one MAC address mode.");
					isMacModeSet = true;
					if (arg == "-d")
						newMac = "";
					else if (arg == "-r")
						;  // Do nothing else because newMac is already random
					else if (arg == "-a") {
						if (argVec.size() - i <= 1)
							throw std::invalid_argument("Missing MAC address argument.");
						i++;
						const std::string &val = argVec.at(i);
						if (!isValidMac(val))
							throw std::invalid_argument("Invalid MAC address, must match pattern /[0-9a-fA-F]{12}/.");
						newMac = val;
					} else
						throw std::logic_error("Unreachable");
				} else
					throw std::invalid_argument("Unrecognized command-line flag.");
			} else {  // Not a flag
				if (!adapter.empty())
					throw std::invalid_argument("Command-line arguments contain more than network adapter name.");
				adapter = arg;
			}
		}
	} catch (const std::invalid_argument &e) {
		std::cerr << "Error: " << e.what() << std::endl;
		return EXIT_FAILURE;
	}
	
	if (adapter == "") {
		showHelp(argVec[0]);
		return EXIT_FAILURE;
	}
	
	try {
		std::cerr << "Setting MAC on adapter '" << adapter << "' to " << (newMac.size() > 0 ? newMac : std::string("original MAC")) << "..." << std::endl;
		setMac(adapter, newMac);
		std::cerr << "Resetting adapter..." << std::endl;
		resetAdapter(adapter);
		std::cerr << "Done" << std::endl;
		return EXIT_SUCCESS;
	}
	catch (const std::runtime_error &e) {
		std::cerr << "Error: " << e.what() << std::endl;
		return EXIT_FAILURE;
	}
}


static void showHelp(const std::string &exePath) {
	std::cerr << "Usage: " << exePath << " AdapterName [Options]" << std::endl;
	std::cerr << std::endl;
	std::cerr << "Example: " << exePath << " \"Wi-Fi\" -r" << std::endl;
	std::cerr << "Example: " << exePath << " \"Ethernet\" -a 02ABCDEF9876" << std::endl;
	
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

// Generates a random MAC that is actually plausible.
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


// https://stackoverflow.com/questions/161177/does-c-support-finally-blocks-and-whats-this-raii-i-keep-hearing-about/25510879#25510879
template <typename F>
class Finally {
	private: F func;
	public: Finally(F f) :
		func(f) {}
	public: ~Finally() {
		func();
	}
};
template <typename F>
Finally<F> finally(F f) {
	return Finally<F>(f);
}


static void setMac(const std::string &adapterName, const std::string &newMac) {
	std::vector<char> id(512);
	{
		HKEY hListKey;
		if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, "SYSTEM\\CurrentControlSet\\Control\\Network\\{4D36E972-E325-11CE-BFC1-08002BE10318}", 0, KEY_READ, &hListKey) != ERROR_SUCCESS)
			throw std::runtime_error("Failed to open adapter list key");
		auto hListKeyFinally = finally([hListKey]{ RegCloseKey(hListKey); });
		
		bool found = false;
		for (DWORD i = 0; ; i++) {
			DWORD idLen = static_cast<DWORD>(id.size());
			FILETIME discard0;
			if (RegEnumKeyEx(hListKey, i, id.data(), &idLen, 0, nullptr, nullptr, &discard0) != ERROR_SUCCESS)
				break;
			
			std::string subkey = id.data();
			subkey += "\\Connection";
			HKEY hKey;
			if (RegOpenKeyEx(hListKey, subkey.c_str(), 0, KEY_READ, &hKey) != ERROR_SUCCESS)
				continue;
			auto hKeyFinally = finally([hKey]{ RegCloseKey(hKey); });
			
			std::vector<char> value(512);
			DWORD valueLen = static_cast<DWORD>(value.size());
			DWORD discard1;
			if (RegQueryValueEx(hKey, "Name", nullptr, &discard1, reinterpret_cast<LPBYTE>(value.data()), &valueLen) == ERROR_SUCCESS
					&& std::string(value.data()) == adapterName) {
				std::cerr << "Adapter ID is " << id.data() << std::endl;
				found = true;
				break;
			}
		}
		if (!found)
			throw std::runtime_error("Failed to find an adapter with the given name; please recheck your Network Connections");
	}
	
	{
		HKEY hListKey;
		if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, "SYSTEM\\CurrentControlSet\\Control\\Class\\{4D36E972-E325-11CE-BFC1-08002BE10318}", 0, KEY_READ, &hListKey) != ERROR_SUCCESS)
			throw std::runtime_error("Failed to open adapter list key in Phase 2");
		auto hListKeyFinally = finally([hListKey]{ RegCloseKey(hListKey); });
		
		for (DWORD i = 0; ; i++) {
			std::vector<char> name(512);
			DWORD nameLen = static_cast<DWORD>(name.size());
			FILETIME discard0;
			if (RegEnumKeyEx(hListKey, i, name.data(), &nameLen, 0, nullptr, nullptr, &discard0) != ERROR_SUCCESS)
				break;
			
			HKEY hKey;
			if (RegOpenKeyEx(hListKey, name.data(), 0, KEY_READ | KEY_SET_VALUE, &hKey) != ERROR_SUCCESS)
				continue;
			auto hKeyFinally = finally([hKey]{ RegCloseKey(hKey); });
			
			std::vector<char> value(512);
			DWORD valueLen = static_cast<DWORD>(value.size());
			DWORD discard1;
			if (RegQueryValueEx(hKey, "NetCfgInstanceId", nullptr, &discard1, reinterpret_cast<LPBYTE>(value.data()), &valueLen) == ERROR_SUCCESS
					&& std::string(value.data()) == std::string(id.data())) {
				RegSetValueEx(hKey, "NetworkAddress", 0, REG_SZ, reinterpret_cast<const BYTE *>(newMac.c_str()), static_cast<DWORD>(newMac.size() + 1));
				//std::cerr << "Updating adapter index " << keyNameBuf2 << " (" << buf << "=" << keyNameBuf << ")" << std::endl;
				//break;
			}
		}
	}
}


static void resetAdapter(const std::string &adapterName) {
	HMODULE netshellLib = LoadLibrary("Netshell.dll");
	if (netshellLib == nullptr)
		throw std::runtime_error("Failed to load Netshell.dll");
	auto netshellLibFinally = finally([netshellLib]{ FreeLibrary(netshellLib); });
	
	void (__stdcall *NcFreeNetConProperties)(NETCON_PROPERTIES *) =
		(void (__stdcall *)(struct tagNETCON_PROPERTIES *))
		GetProcAddress(netshellLib, "NcFreeNetconProperties");
	if (NcFreeNetConProperties == nullptr)
		throw std::runtime_error("Failed to load function from DLL");
	
	std::wstring buf;
	for (std::size_t i = 0; i < adapterName.size(); i++)
		buf.push_back(static_cast<wchar_t>(adapterName[i]));
	
	(void)CoInitialize(nullptr);
	auto comFinally = finally([]{ CoUninitialize(); });
	
	INetConnectionManager *conMgr = nullptr;
	struct _GUID guid = {0xBA126AD1, 0x2166, 0x11D1, {0xB1,0xD0,0x00,0x80,0x5F,0xC1,0x27,0x0E}};
	HRESULT hr = ::CoCreateInstance(guid, nullptr, CLSCTX_ALL, __uuidof(INetConnectionManager), (void**)&conMgr);
	if (conMgr == nullptr)
		throw std::runtime_error("Failed to create connection manager");
	auto conMgrFinally = finally([conMgr]{ conMgr->Release(); });
	
	IEnumNetConnection *enumCon;
	conMgr->EnumConnections(NCME_DEFAULT, &enumCon);
	if (enumCon == nullptr)
		throw std::runtime_error("Could not enumerate Network Connections");
	auto enumConFinally = finally([enumCon]{ enumCon->Release(); });
	
	while (true) {
		INetConnection *netCon;
		ULONG fetched;
		enumCon->Next(1, &netCon, &fetched);
		if (fetched == 0)
			break;
		if (netCon == nullptr)
			continue;
		
		NETCON_PROPERTIES *conProp;
		netCon->GetProperties(&conProp);
		if (conProp == nullptr)
			continue;
		auto conPropFinally = finally([conProp, NcFreeNetConProperties]{ NcFreeNetConProperties(conProp); });
		
		if (wcscmp(conProp->pszwName, buf.c_str()) == 0) {
			netCon->Disconnect();
			netCon->Connect();
		}
	}
}
