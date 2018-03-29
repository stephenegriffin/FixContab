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

void GetProfile(std::string& profileName)
{
	wprintf(L"Locating profile %hs\n", profileName.c_str());
	LPPROFADMIN lpProfAdmin = nullptr;
	LPSERVICEADMIN lpServiceAdmin = nullptr;
	auto hRes = MAPIAdminProfiles(0, &lpProfAdmin);
	CHECKHRESMSG(hRes, L"MAPIAdminProfiles");

	if (SUCCEEDED(hRes) && lpProfAdmin)
	{
		wprintf(L"Got profadmin\n");
		hRes = lpProfAdmin->AdminServices(
			reinterpret_cast<LPTSTR>(const_cast<LPSTR>(profileName.c_str())),
			reinterpret_cast<LPTSTR>(const_cast<LPSTR>("")),
			NULL,
			MAPI_DIALOG,
			&lpServiceAdmin);
		CHECKHRESMSG(hRes, L"AdminServices");
		if (SUCCEEDED(hRes) && lpServiceAdmin)
		{
			wprintf(L"Got serviceadmin\n");
			LPMAPITABLE lpServiceTable = nullptr;

			hRes = lpServiceAdmin->GetMsgServiceTable(
				0, // fMapiUnicode is not supported
				&lpServiceTable);
			CHECKHRESMSG(hRes, L"GetMsgServiceTable");
			if (SUCCEEDED(hRes) && lpServiceTable)
			{
				wprintf(L"Got service table\n");
				LPSRowSet lpRowSet = nullptr;

				hRes = HrQueryAllRows(lpServiceTable, nullptr, nullptr, nullptr, 0, &lpRowSet);
				CHECKHRESMSG(hRes, L"HrQueryAllRows");
				if (SUCCEEDED(hRes) && lpRowSet && lpRowSet->cRows >= 1)
				{
					for (ULONG i = 0; i < lpRowSet->cRows; i++)
					{
						auto displayname = LpValFindProp(PR_DISPLAY_NAME_A, lpRowSet->aRow[i].cValues, lpRowSet->aRow[i].lpProps);
						if (displayname != nullptr && displayname->ulPropTag == PR_DISPLAY_NAME_A)
						{
							wprintf(L"Display name: %hs\n", displayname->Value.lpszA);
						}

						auto servicename = LpValFindProp(PR_SERVICE_NAME_A, lpRowSet->aRow[i].cValues, lpRowSet->aRow[i].lpProps);
						if (servicename != nullptr && servicename->ulPropTag == PR_SERVICE_NAME_A)
						{
							wprintf(L"Service name: %hs\n", servicename->Value.lpszA);
						}
					}
				}

				FreeProws(lpRowSet);

				lpServiceTable->Release();
			}

			lpServiceAdmin->Release();
		}

		lpProfAdmin->Release();
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