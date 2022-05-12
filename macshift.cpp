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

const int versionMajor = 1;
const int versionMinor = 1;

#include <windows.h>
#include <objbase.h>
#include <netcon.h>
#include <stdio.h>
#include "validmacs.h"

void SetMAC(char * AdapterName, char * NewMAC) {
  HKEY hListKey = NULL;
  HKEY hKey = NULL;
  RegOpenKeyEx(HKEY_LOCAL_MACHINE, "SYSTEM\\CurrentControlSet\\Control\\Network\\{4D36E972-E325-11CE-BFC1-08002BE10318}",
    0, KEY_READ, &hListKey);
  if (!hListKey) {
    printf("Failed to open adapter list key\n");
    return;
    }
  FILETIME writtenTime;
  char keyNameBuf[512], keyNameBuf2[512];
  DWORD keyNameBufSiz = 512;
  DWORD crap;
  int i = 0;
  bool found = false;
  while (RegEnumKeyEx(hListKey, i++, keyNameBuf, &keyNameBufSiz, 0, NULL, NULL, &writtenTime)
        == ERROR_SUCCESS) {
    _snprintf(keyNameBuf2, 512, "%s\\Connection", keyNameBuf);
    hKey = NULL;
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
    printf("Failed to open adapter list key in Phase 2\n");
    return;
    }
  i = 0;
  char buf[512];
  while (RegEnumKeyEx(hListKey, i++, keyNameBuf2, &keyNameBufSiz, 0, NULL, NULL, &writtenTime)
        == ERROR_SUCCESS) {
    hKey = NULL;
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

void ResetAdapter(char * AdapterName) {
  struct _GUID guid = {0xBA126AD1,0x2166,0x11D1,0};
  memcpy(guid.Data4, "\xB1\xD0\x00\x80\x5F\xC1\x27\x0E", 8);
  unsigned short * buf = new unsigned short[strlen(AdapterName)+1];
  
  void (__stdcall *NcFreeNetConProperties) (NETCON_PROPERTIES *);
  HMODULE NetShell_Dll = LoadLibrary("Netshell.dll");
  if (!NetShell_Dll) {
    printf("Couldn't load Netshell.dll\n");
    return;
    }
  NcFreeNetConProperties = (void (__stdcall *)(struct tagNETCON_PROPERTIES *))GetProcAddress(NetShell_Dll, "NcFreeNetconProperties");
  if (!NcFreeNetConProperties) {
    printf("Couldn't load required DLL function\n");
    return;
    }

  for (unsigned int i = 0; i <= strlen(AdapterName); i++) {
    buf[i] = AdapterName[i];
    }
  CoInitialize(0);
    INetConnectionManager * pNCM = NULL;    
    HRESULT hr = ::CoCreateInstance(guid,
                                     NULL,
                                     CLSCTX_ALL,
                                     __uuidof(INetConnectionManager),
                                     (void**)&pNCM);
    if (!pNCM)
        printf("Failed to instantiate required object\n");
    else {
        IEnumNetConnection * pENC;
        pNCM->EnumConnections(NCME_DEFAULT, &pENC);
        if (!pENC) {
          printf("Could not enumerate Network Connections\n");
          }
        else {
          INetConnection * pNC;
          ULONG fetched;
          NETCON_PROPERTIES * pNCP;
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

bool IsValidMAC(char * str) {
  if (strlen(str) != 12) return false;
  for (int i = 0; i < 12; i++) {
    if ((str[i] < '0' || str[i] > '9') 
          && (str[i] < 'a' || str[i] > 'f')
          && (str[i] < 'A' || str[i] > 'F')) {
      return false;
      }
    }
  return true;
  }

void ShowHelp() {
  printf("Usage: macshift [options] [mac-address]\n\n");
  printf("Options:\n");
  printf("\t-i [adapter-name]     The adapter name from Network Connections.\n");
  printf("\t-r                    Uses a random MAC address. This is the default.\n");
  printf("\t-d                    Restores the original MAC address.\n");
  printf("\t--help                Shows this screen.\n\n");
  printf("Macshift uses special undocumented functions in the Windows COM Interface that\n");
  printf(" allow you to change an adapter's MAC address without needing to restart.\n");
  printf("When you change a MAC address, all your connections are closed automatically\n");
  printf(" and your adapter is reset.\n");
  }

//Generates a random MAC that is actually plausible
void RandomizeMAC(char * newmac) {
  _snprintf(newmac, 6, "%06X", rand() % numMacs);
  for (int i = 3; i < 6; i++) {
    _snprintf(&newmac[i*2], 2, "%02X", rand() & 0xFF);
    }
  newmac[12] = 0;
  }

int main(int argc, char * * argv) {
  printf("Macshift v%i.%i, MAC Changing Utility by Nathan True, macshift@natetrue.com\n\n", versionMajor, versionMinor);

  //Parse commandline arguments
  char * adapter = "Wireless";
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
          if (argc > i + 1) adapter = argv[++i];
          break;     
        case 'd': //Reset the MAC address
          newmac[0] = 0;
        }
      }
    else {
      if (IsValidMAC(argv[i])) strncpy(newmac, argv[i], 13);
      else printf("MAC String %s is not valid. MAC addresses must m/^[0-9a-fA-F]{12}$/.\n", argv[i]);
      }
    }

  printf("Setting MAC on adapter '%s' to %s...\n", adapter, newmac[0] ? newmac : "original MAC");
  SetMAC(adapter, newmac);
  printf("Resetting adapter...\n");
  fflush(stdout);
  ResetAdapter(adapter);
  printf("Done\n");
  return 0;
  }
