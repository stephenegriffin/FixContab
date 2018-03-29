#include "stdafx.h"
#include <initguid.h>
#include <MAPIX.h>
#include <MAPIUtil.h>
#include <string>

#define CHECKHRES(hRes) (LogError(hRes, nullptr, __FILE__, __LINE__))
#define CHECKHRESMSG(hRes, comment) (LogError(hRes, comment, __FILE__, __LINE__))
void LogError(HRESULT hRes, LPCWSTR comment, LPCSTR file, int line)
{
	if (FAILED(hRes))
	{
		wprintf(L"File %hs::%d\n", file, line);
		if (comment)
		{
			wprintf(L"%ws: ", comment);
		}

		wprintf(L"Error 0x%08X\n", hRes);
	}
}

LPSERVICEADMIN GetServiceAdmin(std::string& profileName)
{
	wprintf(L"Getting service admin for profile %hs\n", profileName.c_str());
	LPPROFADMIN profAdmin = nullptr;
	LPSERVICEADMIN serviceAdmin = nullptr;
	auto hRes = MAPIAdminProfiles(0, &profAdmin);
	CHECKHRESMSG(hRes, L"MAPIAdminProfiles");

	if (SUCCEEDED(hRes) && profAdmin)
	{
		hRes = profAdmin->AdminServices(
			reinterpret_cast<LPTSTR>(const_cast<LPSTR>(profileName.c_str())),
			reinterpret_cast<LPTSTR>(const_cast<LPSTR>("")),
			NULL,
			MAPI_DIALOG,
			&serviceAdmin);
		CHECKHRESMSG(hRes, L"AdminServices");

		profAdmin->Release();
	}

	return serviceAdmin;
}

void GetProfile(std::string& profileName)
{
	auto hRes = S_OK;
	wprintf(L"Locating profile %hs\n", profileName.c_str());
	LPSERVICEADMIN lpServiceAdmin = GetServiceAdmin(profileName);

	if (lpServiceAdmin)
	{
		LPMAPITABLE lpServiceTable = nullptr;

		hRes = lpServiceAdmin->GetMsgServiceTable(
			0, // fMapiUnicode is not supported
			&lpServiceTable);
		CHECKHRESMSG(hRes, L"GetMsgServiceTable");
		if (SUCCEEDED(hRes) && lpServiceTable)
		{
			LPSRowSet lpRowSet = nullptr;

			hRes = HrQueryAllRows(lpServiceTable, nullptr, nullptr, nullptr, 0, &lpRowSet);
			CHECKHRESMSG(hRes, L"HrQueryAllRows");
			if (SUCCEEDED(hRes) && lpRowSet && lpRowSet->cRows >= 1)
			{
				for (ULONG i = 0; i < lpRowSet->cRows; i++)
				{
					auto displayNameProp = LpValFindProp(PR_DISPLAY_NAME_A, lpRowSet->aRow[i].cValues, lpRowSet->aRow[i].lpProps);
					std::string displayName;
					if (displayNameProp != nullptr && displayNameProp->ulPropTag == PR_DISPLAY_NAME_A)
					{
						displayName = displayNameProp->Value.lpszA;
					}

					auto servicenameProp = LpValFindProp(PR_SERVICE_NAME_A, lpRowSet->aRow[i].cValues, lpRowSet->aRow[i].lpProps);
					std::string serviceName;
					if (servicenameProp != nullptr && servicenameProp->ulPropTag == PR_SERVICE_NAME_A)
					{
						serviceName = servicenameProp->Value.lpszA;
					}

					if (serviceName == std::string("CONTAB"))
					{
						wprintf(L"Found contab\n");
						wprintf(L"Display name: %hs\n", displayNameProp->Value.lpszA);
						wprintf(L"Service name: %hs\n", servicenameProp->Value.lpszA);
					}
				}
			}

			FreeProws(lpRowSet);
			lpServiceTable->Release();
		}

		lpServiceAdmin->Release();
	}
}

void DisplayUsage()
{
	wprintf(L"FixContab - MAPI Profile Contab Fix\n");
	wprintf(L"   Puts Contab first in Address Book load order to fix MAPI crashes.\n");
	wprintf(L"\n");
	wprintf(L"Usage:  FixContab profile\n");
	wprintf(L"\n");
	wprintf(L"Options:\n");
	wprintf(L"        profile - Name of profile to modify.\n");
}

int main(int argc, char* argv[])
{
	std::string profileName;
	if (argc == 2)
	{
		profileName = argv[1];
	}
	else
	{
		DisplayUsage();
	}

	wprintf(L"Modifying profile %hs\n", profileName.c_str());
	MAPIINIT_0 mapiInit = { MAPI_INIT_VERSION, NULL };
	MAPIInitialize(&mapiInit);
	GetProfile(profileName);

	MAPIUninitialize();
	return 0;
}