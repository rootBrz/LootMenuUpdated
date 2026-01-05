#include "code/internal/containers.h"
#include "utility.h"
#include "decoding.h"
#include "main.h"
#include "Version.h"
#include "Xinput.h"
#include "LootMenu.h"

extern "C"
{
	BOOL WINAPI DllMain(HMODULE hModule, DWORD dwReason, LPVOID lpreserved)
	{
		if (dwReason == DLL_PROCESS_ATTACH)
		{
			LootMenuHandle = hModule;
			DisableThreadLibraryCalls(hModule);
		}
		return TRUE;
	}

	bool FOSEPlugin_Query(const FOSEInterface* fose, PluginInfo* info)
	{
		info->infoVersion = PluginInfo::kInfoVersion;
		info->name = "Loot Menu";
		info->version = PLUGIN_VERSION;
		return versionCheck(fose);
	}

	bool FOSEPlugin_Load(const FOSEInterface* fose)
	{
		handleIniOptions();
		writePatches();
		return true;
	}
};

void DeferredInitInjector()
{
	static byte callCounter = 0;

	if (callCounter > 0)
	{
		DeferredInit();
		// remove this hook, restoring the original code
		WriteRelCall(0xAA4A26, 0x86C140);
	}
	callCounter++;
	CdeclCall(0x86C140);
}

void __fastcall PCFunctionHook(PlayerCharacter* pc)
{
	ThisCall(0x778920, pc); // call wrapped function
	LootMenu::Update();
}

void writePatches()
{
	// wrap a call in the main loop, restoring it after initialisation is complete
	WriteRelCall(0xAA4A26, UInt32(DeferredInitInjector));
}

void DeferredInit()
{
	g_thePlayer = PlayerCharacter::GetSingleton();
	LootMenu::Init();

	// wrap a function called in PlayerCharacter::Update, and check inputs for menu
	WriteRelCall(0x78917E, UInt32(PCFunctionHook));
}

bool versionCheck(const FOSEInterface* fose)
{
	if (fose->isEditor) return false;

	if (fose->foseVersion < FOSE_VERSION_INTEGER)
	{
		_ERROR("FOSE version too old (got %08X expected at least %08X)", fose->foseVersion, FOSE_VERSION_INTEGER);
		return false;
	}
	if (fose->runtimeVersion != FALLOUT_VERSION)
	{
		_ERROR("incorrect runtime version (got %08X need %08X)", fose->runtimeVersion, FALLOUT_VERSION);
		return false;
	}
	return true;
}