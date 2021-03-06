#include "stdafx.h"
#include <initguid.h>
#include <MAPIX.h>
#include <MAPIUtil.h>
#include <string>
#include <vector>
#include <functional>
#include <sstream>

#define MAPI_FORCE_ACCESS 0x00080000
#define PS_MAPI_PROVIDERS_INIT		{ 0x92,0x07,0xF3,0xE0, \
									  0xA3,0xB1,0x10,0x19, \
									  0x90,0x8B,0x08,0x00, \
									  0x2B,0x2A,0x56,0xC2 }
const MAPIUID muidProviderSection = PS_MAPI_PROVIDERS_INIT;

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

std::wstring BinToHexString(_In_opt_ const SBinary* lpBin)
{
	if (!lpBin) return L"";
	auto lpb = lpBin->lpb;
	auto cb = lpBin->cb;

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

// Converts hex string in input to a binary buffer.
// If cbTarget != 0, caps the number of bytes converted at cbTarget
std::vector<BYTE> HexStringToBin(const std::wstring& input)
{
	auto cchStrLen = input.length();

	std::vector<BYTE> lpb;
	WCHAR szTmp[3] = { 0 };
	size_t iCur = 0;
	size_t cbConverted = 0;

	// Convert two characters at a time
	while (iCur < cchStrLen)
	{
		// Check for valid hex characters
		if (input[iCur] > 255 || input[iCur + 1] > 255 || !isxdigit(input[iCur]) || !isxdigit(input[iCur + 1]))
		{
			return std::vector<BYTE>();
		}

		szTmp[0] = input[iCur];
		szTmp[1] = input[iCur + 1];
		lpb.push_back(static_cast<BYTE>(wcstol(szTmp, nullptr, 16)));
		iCur += 2;
		cbConverted++;
	}

	return lpb;
}

LPSERVICEADMIN GetServiceAdmin(const std::string& profileName)
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

LPPROFSECT GetContabProfileSection(LPSERVICEADMIN lpServiceAdmin, const std::string& profileName)
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
						CHECKHRESMSG(hRes, L"lpServiceAdmin->OpenProfileSection(CONTAB)");
					}
				}
			}
		}

		FreeProws(lpRowSet);
	}

	if (lpServiceTable) lpServiceTable->Release();

	return profileSection;
}

LPPROFSECT GetProvidersSection(LPSERVICEADMIN lpServiceAdmin)
{
	LPPROFSECT profileSection = nullptr;

	auto hRes = lpServiceAdmin->OpenProfileSection(const_cast<LPMAPIUID>(&muidProviderSection), nullptr, MAPI_MODIFY | MAPI_FORCE_ACCESS, &profileSection);
	CHECKHRESMSG(hRes, L"lpServiceAdmin->OpenProfileSection(muidProviderSection)");

	return profileSection;
}

std::wstring GetProvidersString(LPPROFSECT section)
{
	ULONG cProps = 0;
	SPropTagArray tags = { 1,{ PR_AB_PROVIDERS } };
	std::wstring abProviders;
	LPSPropValue sectionProps = nullptr;
	auto hRes = section->GetProps(&tags, 0, &cProps, &sectionProps);
	CHECKHRESMSG(hRes, L"section->GetProps");
	if (SUCCEEDED(hRes) && sectionProps)
	{
		abProviders = BinToHexString(&sectionProps[0].Value.bin);
	}

	MAPIFreeBuffer(sectionProps);

	return abProviders;
}

void SetProviders(LPPROFSECT section, const SBinary& bin)
{
	wprintf(L"Writing providers to profile section\n");
	SPropValue provider = { };
	provider.ulPropTag = PR_AB_PROVIDERS;
	provider.dwAlignPad = 0;
	provider.Value.bin = bin;

	auto hRes = section->SetProps(1, &provider, nullptr);
	CHECKHRESMSG(hRes, L"section->SetProps");

	if (SUCCEEDED(hRes))
	{
		wprintf(L"Success!\n");
	}
}

std::vector<std::wstring> split(const std::wstring& s, size_t length)
{
	std::vector<std::wstring> tokens;
	size_t offset = 0;
	while (offset < s.size())
	{
		if (offset + length < s.size())
			tokens.push_back(s.substr(offset, length));
		else
			tokens.push_back(s.substr(offset, s.size() - offset));

		offset += length;
	}

	return tokens;
}

std::wstring join(const std::vector<std::wstring>& v)
{
	std::wstringstream s;
	for (const auto& i : v) {
		s << i;
	}

	return s.str();
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
		auto providers = GetProvidersSection(lpServiceAdmin);

		if (contab && providers)
		{
			auto contabProviders = GetProvidersString(contab);
			wprintf(L"Contab PR_AB_PROVIDERS    : %ws\n", contabProviders.c_str());

			auto providersProviders = GetProvidersString(providers);
			wprintf(L"Providers PR_AB_PROVIDERS : %ws\n", providersProviders.c_str());

			auto subProviders = split(providersProviders, 32);

			wprintf(L"Swapping\n");
			for (auto i = subProviders.begin(); i != subProviders.end(); i++)
			{
				if (*i == contabProviders)
				{
					// Found contab - rotate it to the beginning of the vector
					std::rotate(subProviders.begin(), i, i + 1);
				}
			}

			auto swappedProviders = join(subProviders);
			wprintf(L"After swap: %ws\n", swappedProviders.c_str());

			auto bin = HexStringToBin(swappedProviders);
			SetProviders(providers, SBinary{ static_cast<ULONG>(bin.size()), bin.data() });
		}

		if (providers) providers->Release();
		if (contab) contab->Release();
		lpServiceAdmin->Release();
	}

	MAPIUninitialize();
	return 0;
}