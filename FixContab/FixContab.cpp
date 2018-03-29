#include "stdafx.h"
#include <initguid.h>
#include <MAPIX.h>
#include <MAPIUtil.h>
#include <string>
#include <vector>

#define MAPI_FORCE_ACCESS 0x00080000

#define CHECKHRES(hRes) (LogError(hRes, nullptr, __FILE__, __LINE__))
#define CHECKHRESMSG(hRes, comment) (LogError(hRes, comment, __FILE__, __LINE__))
void LogError(HRESULT hRes, LPCWSTR comment, LPCSTR file, int line)
{
	if (FAILED(hRes))
	{
		wprintf(L"File %hs:%d\n", file, line);
		if (comment)
		{
			wprintf(L"%ws: ", comment);
		}

		wprintf(L"Error 0x%08X\n", hRes);
	}
}

std::wstring BinToHexString(_In_opt_count_(cb) const BYTE* lpb, size_t cb)
{
	std::wstring lpsz;

	if (!cb || !lpb)
	{
		lpsz += L"NULL";
	}
	else
	{
		for (ULONG i = 0; i < cb; i++)
		{
			auto bLow = static_cast<BYTE>(lpb[i] & 0xf);
			auto bHigh = static_cast<BYTE>(lpb[i] >> 4 & 0xf);
			auto szLow = static_cast<wchar_t>(bLow <= 0x9 ? L'0' + bLow : L'A' + bLow - 0xa);
			auto szHigh = static_cast<wchar_t>(bHigh <= 0x9 ? L'0' + bHigh : L'A' + bHigh - 0xa);

			lpsz += szHigh;
			lpsz += szLow;
		}
	}

	return lpsz;
}

std::wstring BinToHexString(_In_opt_ const SBinary* lpBin)
{
	if (!lpBin) return L"";

	return BinToHexString(
		lpBin->lpb,
		lpBin->cb);
}

std::wstring BinToHexString(const std::vector<BYTE>& lpByte)
{
	SBinary sBin = { 0 };
	sBin.cb = static_cast<ULONG>(lpByte.size());
	sBin.lpb = const_cast<LPBYTE>(lpByte.data());
	return BinToHexString(&sBin);
}

LPSERVICEADMIN GetServiceAdmin(std::string& profileName)
{
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

	}

	if (profAdmin) profAdmin->Release();

	return serviceAdmin;
}

LPPROFSECT GetContabProfileSection(LPSERVICEADMIN lpServiceAdmin, std::string& profileName)
{
	if (!lpServiceAdmin || profileName.empty()) return nullptr;

	wprintf(L"Locating profile %hs\n", profileName.c_str());

	LPPROFSECT profileSection = nullptr;
	LPMAPITABLE lpServiceTable = nullptr;

	auto hRes = lpServiceAdmin->GetMsgServiceTable(
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
				auto servicenameProp = LpValFindProp(PR_SERVICE_NAME_A, lpRowSet->aRow[i].cValues, lpRowSet->aRow[i].lpProps);
				std::string serviceName;
				if (servicenameProp && servicenameProp->ulPropTag == PR_SERVICE_NAME_A)
				{
					serviceName = servicenameProp->Value.lpszA;
				}

				if (serviceName == std::string("CONTAB"))
				{
					wprintf(L"Found contab\n");

					auto displayNameProp = LpValFindProp(PR_DISPLAY_NAME_A, lpRowSet->aRow[i].cValues, lpRowSet->aRow[i].lpProps);
					std::string displayName;
					if (displayNameProp && displayNameProp->ulPropTag == PR_DISPLAY_NAME_A)
					{
						displayName = displayNameProp->Value.lpszA;
					}

					wprintf(L"Display name: %hs\n", displayName.c_str());
					wprintf(L"Service name: %hs\n", serviceName.c_str());

					auto uidProp = LpValFindProp(PR_SERVICE_UID, lpRowSet->aRow[i].cValues, lpRowSet->aRow[i].lpProps);
					if (uidProp && uidProp->ulPropTag == PR_SERVICE_UID)
					{
						auto uid = BinToHexString(&uidProp->Value.bin);
						wprintf(L"PR_SERVICE_UID : %ws\n", uid.c_str());

						hRes = lpServiceAdmin->OpenProfileSection(
							reinterpret_cast<LPMAPIUID>(uidProp->Value.bin.lpb),
							nullptr,
							MAPI_FORCE_ACCESS,
							&profileSection);
						CHECKHRESMSG(hRes, L"lpServiceAdmin->OpenProfileSection");
					}
				}
			}
		}

		FreeProws(lpRowSet);
	}

	if (lpServiceTable) lpServiceTable->Release();

	return profileSection;
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
	auto hRes = S_OK;
	std::string profileName;
	if (argc == 2)
	{
		profileName = argv[1];
	}
	else
	{
		DisplayUsage();
		return 0;
	}

	wprintf(L"Modifying profile %hs\n", profileName.c_str());
	MAPIINIT_0 mapiInit = { MAPI_INIT_VERSION, NULL };
	MAPIInitialize(&mapiInit);

	LPSERVICEADMIN lpServiceAdmin = GetServiceAdmin(profileName);

	if (lpServiceAdmin)
	{
		auto contab = GetContabProfileSection(lpServiceAdmin, profileName);

		if (contab)
		{
			ULONG cProps = 0;
			LPSPropValue props = nullptr;
			SPropTagArray tags = { 1,{ PR_AB_PROVIDERS } };

			hRes = contab->GetProps(&tags, 0, &cProps, &props);
			CHECKHRESMSG(hRes, L"contab->GetProps");
			if (SUCCEEDED(hRes) && props)
			{
				auto abProviders = BinToHexString(&props[0].Value.bin);
				wprintf(L"PR_AB_PROVIDERS : %ws\n", abProviders.c_str());
			}

			MAPIFreeBuffer(props);
			contab->Release();
		}

		lpServiceAdmin->Release();
	}

	MAPIUninitialize();
	return 0;
}