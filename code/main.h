#pragma once

#include <time.h>
#include "fose\PluginAPI.h"
#include "fose_common\SafeWrite.h"
#include "fose\GameAPI.h"
#include "fose\GameInterface.h"
#include "fose\GameObjects.h"
#include "fose/ParamInfos.h"
#include "fose/GameExtraData.h"
#include "fose/GameSettings.h"
#include <string>
#include <fstream>
#include <algorithm>
#include <regex>
#include <list>
#include <unordered_set>
#include "settings.h"

bool versionCheck(const FOSEInterface* fose);
void writePatches();
void DeferredInit();
TimeGlobal* timeGlobals = (TimeGlobal*)0x1090BA0;

HMODULE LootMenuHandle;

PlayerCharacter* g_thePlayer = NULL;
DataHandler* g_dataHandler;