#pragma once

tList<ContChangesEntry> s_tempContChangesEntries;
ContChangesEntry* __fastcall CreateTempContChange(TESForm* itemForm, SInt32 countDelta, ExtraDataList* xData)
{
	ContChangesEntry* entry = (ContChangesEntry*)GameHeapAlloc(sizeof(ContChangesEntry));
	if (xData)
	{
		entry->extendData = (ExtraContainerChanges::ExtendDataList*)GameHeapAlloc(8);
		entry->extendData->Init(xData);
	}
	else entry->extendData = NULL;
	entry->countDelta = countDelta;
	entry->type = itemForm;
	s_tempContChangesEntries.Insert(entry);
	return entry;
}

namespace LootMenu
{
	InventoryItemsMap inventoryItems(0x49);
	InventoryItemsVec items;

	void Update();
	namespace Hooks { void Apply(); };

	Tile* mainTile = NULL;
	Tile** ItemTiles;
	TESObjectREFR* ref = NULL;

	int JLMIndex = 0;
	int ScrollOffset = 0;
	int NumContainerItems = 0;
	std::atomic<bool> JLMVisible{ false };
	int isControllerEnabled = -1;
	std::atomic<bool> JLMRefresh{ false };
	bool JLMStealing = false;
	bool shouldRunCloseContainerScript = false;
	ControlCode ActivateContainerCode = ControlCode::Rest;
	ControlCode TakeItemControlCode = ControlCode::Activate;

	UInt32 TextWidthTrait;

	enum PromptStrings
	{
		kPrompt_Take = 1,
		kPrompt_Steal = 2,
		kPrompt_Equip = 3,
		kPrompt_TakeAll = 4,
		kPrompt_Open = 5
	};

	bool InjectMenuXML(Menu* menu)
	{
		if (!menu) return false;
		Tile* hudMenuTile = menu->tile;

		mainTile = hudMenuTile->ReadXML("data\\menus\\prefabs\\LootMenu\\JLM.xml");
		return mainTile != NULL;
	}

	void SetScrollOffset(UInt32 height)
	{
		ScrollOffset = height;
		SetTileComponentValue(mainTile, "_JLMOffset", ScrollOffset);
	}

	void SetNumItems(UInt32 num)
	{
		NumContainerItems = num;
		SetTileComponentValue(mainTile, "_JLMTotal", num);
	}

	void SetContainer(TESObjectREFR* container)
	{
		ref = container;
	}

	InventoryItemData* GetSelectedItem()
	{
		auto itemIndex = JLMIndex + ScrollOffset;
		itemIndex = min(itemIndex, (NumContainerItems - 1));
		return items.Get(itemIndex);
	}

	void UpdateItems()
	{
		items.Clear();

		// store the inventory items of the current ref into Loot Menu's inv items
		ref->GetInventoryItems(&inventoryItems);

		TESForm* item;
		for (InventoryItemsMap::Iterator itemIter(inventoryItems); !itemIter.End(); ++itemIter)
		{
			item = itemIter.Key();
			if (!item->IsItemPlayable() || !*item->GetTheName())
				continue;

			auto data = itemIter.Get();
			if (data.entry && data.entry->type)
			{
				items.Emplace(data);
			}
			else
			{
				// handles 'dropped' weapons and notes
				InventoryItemData tempData;
				ContChangesEntry* tempChange = CreateTempContChange(item, 1, nullptr);
				tempData.entry = tempChange;
				tempData.count = data.count;
				items.Emplace(tempData);
			}
		}

		inventoryItems.Clear();

		SetNumItems(items.Size());
	}

	bool IsDroppedWeapon(InventoryItemData* data)
	{
		return data->count == -1;
	}

	bool SortingFunctionAlphabetical(InventoryItemData& a, InventoryItemData& b)
	{
		auto nameA = a.entry->type->GetTheName();
		auto nameB = b.entry->type->GetTheName();
		return stricmp(nameA, nameB) < 0;
	}

	// Used for sorting forms by type
	UInt8 ItemTypeToPriority[kFormType_MAX] = { 0 };

	bool SortingFunctionLootMenu(InventoryItemData& a, InventoryItemData& b)
	{
		// sort quest items to the top
		auto formA = a.entry->type;
		auto formB = b.entry->type;

		auto isQuestA = formA->IsQuestItem();
		auto isQuestB = formB->IsQuestItem();
		if (isQuestA && !isQuestB) return 1;
		if (isQuestB && !isQuestA) return 0;

		auto isWeightlessA = formA->GetWeight() == 0;
		auto isWeightlessB = formB->GetWeight() == 0;
		if (isWeightlessA && !isWeightlessB) return 1;
		if (isWeightlessB && !isWeightlessA) return 0;

		auto typePriorityA = ItemTypeToPriority[formA->typeID];
		auto typePriorityB = ItemTypeToPriority[formB->typeID];
		if (typePriorityA > typePriorityB) return 1;
		if (typePriorityB > typePriorityA) return 0;

		return SortingFunctionAlphabetical(a, b);
	}

	void ScrollToIndex(int index)
	{
		if (index < 0)
		{
			--ScrollOffset;
			index = 0;
		}
		else if (index >= JLMItems)
		{
			ScrollOffset = min(NumContainerItems - JLMItems, ScrollOffset + 1);
			index = JLMItems - 1;
		}
		index = min(NumContainerItems - 1, index);
		ScrollOffset = max(0, ScrollOffset);

		JLMIndex = index;
		SetTileComponentValue(mainTile, "_JLMIndex", index);
		SetTileComponentValue(mainTile, "_JLMOffset", ScrollOffset);
	}

	void RefreshItemDisplay()
	{
		SetTileComponentValue(mainTile, "_JLMTitle", ref ? ref->GetTheName() : "");

		TESForm* item;
		int index = 0;
		float width = 0;
		for (int n = items.Size() - ScrollOffset; index < JLMItems && index < n; ++index)
		{
			static char itemNameBuf[0x100];
			auto data = items.Get(index + ScrollOffset);
			item = data->entry->type;
			if (data->count > 1)
			{
				sprintf(itemNameBuf, "%s (%d)", item->GetTheName(), data->count);
			}
			else
			{
				sprintf(itemNameBuf, "%s", item->GetTheName());
			}

			SetTileComponentValue(ItemTiles[index], "_JLMEquipped", data->entry->GetEquippedExtra() != nullptr);
			ItemTiles[index]->SetString(kTileValue_string, itemNameBuf, false);
			ItemTiles[index]->SetFloat(kTileValue_visible, true);
			width = max(width, ItemTiles[index]->GetValueFloat(TextWidthTrait));
		}

		for (; index < JLMItems; ++index)
		{
			ItemTiles[index]->SetString(kTileValue_string, "", false);
			ItemTiles[index]->SetFloat(kTileValue_visible, false);
		}

		float height = 0;
		if (NumContainerItems)
		{
			height = ItemTiles[0]->GetValueFloat(kTileValue_height) * min(NumContainerItems, JLMItems);
		}

		SetTileComponentValue(mainTile, "_JLMItemHeightCur", height);
		SetTileComponentValue(mainTile, "_JLMItemWidthCur", width);
	}

	void UpdateStealingColor()
	{
		JLMStealing = ThisCall<bool>(0x4F3B80, ref);
		SetTileComponentValue(mainTile, "_JLMStealing", JLMStealing);
	}

	void HandleChangedRefScripts()
	{
		if (ref && shouldRunCloseContainerScript)
		{
			// trigger the OnClose event for the previous ref
			ExtraScript* xScript = GetExtraType(ref->extraDataList, Script);
			if (xScript && xScript->eventList)
			{
				auto pRef = ref;

				pRef->DisableScriptedActivate(true);

				// prevent container menu showing
				NopFunctionCall(0x54FC37);
				NopFunctionCall(0x4B758F);
				NopFunctionCall(0x5572DA);

				// prevent the container playing the open animation, since the container menu would show when it ends
				SafeWrite8(0x44E120, 0xC3);

				pRef->Activate(g_thePlayer, 0, 0, 1);

				SafeWrite8(0x44E120, 0x56);

				WriteRelCall(0x54FC37, 0x61D5A0);
				WriteRelCall(0x4B758F, 0x61D5A0);
				WriteRelCall(0x5572DA, 0x61D5A0);
				pRef->DisableScriptedActivate(false);

				ThisCall(0x5183C0, xScript->eventList, g_thePlayer, kEvent_OnClose);
			}
			shouldRunCloseContainerScript = false;
		}
	}

	void ResetAndHideMenu()
	{
		HandleChangedRefScripts();
		SetContainer(nullptr);
		items.Clear();
		SetScrollOffset(0);
		SetNumItems(0);
		RefreshItemDisplay();
	}

	bool HasScript(TESObjectREFR* ref)
	{
		return ref->extraDataList.HasType(kExtraData_Script);
	}

	std::string ToLower(std::string s)
	{
		std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::tolower(c); });
		return s;
	}

	std::string GetWhitelistPath()
	{
		char path[MAX_PATH];
		GetModuleFileNameA(LootMenuHandle, path, MAX_PATH);

		std::string fullPath(path);
		size_t pos = fullPath.find_last_of("\\/");
		fullPath = fullPath.substr(0, pos + 1);

		return fullPath + "lootMenuWhitelistedScripts.txt";
	}

	const std::unordered_set<std::string>& GetScriptWhitelist()
	{
		static std::unordered_set<std::string> whitelist;
		static bool loaded = false;
		if (loaded) return whitelist;

		std::ifstream file(GetWhitelistPath());
		if (!file.is_open()) return whitelist;

		std::string line;
		while (std::getline(file, line))
		{
			if (!line.empty() && (line.back() == '\r' || line.back() == '\n' || line.back() == ' ')) line.pop_back();
			if (!line.empty()) whitelist.insert(ToLower(line));
		}

		loaded = true;
		return whitelist;
	}

	bool IsScriptWhitelisted(TESObjectREFR* ref) {
		TESForm* form = ref->baseForm;
		Script* script;
		if IS_TYPE(form, Script)
			script = (Script*)form;
		else
		{
			TESScriptableForm* scriptable = DYNAMIC_CAST(form, TESForm, TESScriptableForm);
			script = scriptable ? scriptable->script : NULL;
			if (!script) return false;
		}

		if (!script || !script->GetEditorID())
			return false;

		const auto& whitelist = GetScriptWhitelist();
		return whitelist.find(ToLower(script->GetEditorID())) != whitelist.end();
	}

	void AddScriptToWhitelist(TESObjectREFR* ref)
	{
		TESForm* form = ref->baseForm;
		Script* script;
		if IS_TYPE(form, Script)
			script = (Script*)form;
		else
		{
			TESScriptableForm* scriptable = DYNAMIC_CAST(form, TESForm, TESScriptableForm);
			script = scriptable ? scriptable->script : NULL;
			if (!script) return;
		}

		if (!script || !script->GetEditorID())
			return;

		std::string lowerID = ToLower(script->GetEditorID());

		auto& whitelist = const_cast<std::unordered_set<std::string>&>(GetScriptWhitelist());
		if (whitelist.find(lowerID) != whitelist.end()) return;

		whitelist.insert(lowerID);
		std::ofstream file(GetWhitelistPath(), std::ios_base::app);
		if (file.is_open())
		{
			file << std::endl << lowerID;
			file.close();
		}
	}

	bool IsControllerPresent(DWORD index = 0)
	{
		HMODULE xinput = GetModuleHandleA("XINPUT1_3.dll");
		if (!xinput)
			return false;

		auto XInputGetState = (DWORD(WINAPI*)(DWORD, XINPUT_STATE*))GetProcAddress(xinput, "XInputGetState");

		if (!XInputGetState)
			return false;

		XINPUT_STATE state{};
		return XInputGetState(index, &state) == ERROR_SUCCESS;
	}


	void SetButtonPromptTexts()
	{
		auto input = OSInputGlobals::GetSingleton();
		bool isControllerPresent = IsControllerPresent() && input->isControllerEnabled;

		if (isControllerEnabled == isControllerPresent)
			return;

		char buf[0x20];
		isControllerEnabled = isControllerPresent;

		auto takeItemKey = isControllerEnabled ? 0x1E : input->keyBinds[TakeItemControlCode];
		sprintf(buf, "%s)", GetDXDescription(takeItemKey));
		SetTileComponentValue(mainTile, "_JLMButtonString1", buf);

		auto activateContainerKey = isControllerEnabled ? 0x2D : input->keyBinds[ActivateContainerCode];
		sprintf(buf, "%s)", GetDXDescription(activateContainerKey));
		SetTileComponentValue(mainTile, "_JLMButtonString2", buf);
	}

	void HandlePrompt(bool lootMenuIsVisible) {

		if (!JLMHidePrompt || IsVATSKillCamActive())
			return;

		auto hud = HUDMainMenu::GetSingleton();
		if (lootMenuIsVisible)
		{
			hud->info->SetFloat(kTileValue_visible, false, 0);
			hud->visibilityFlags &= ~HUDMainMenu::kInfo;
		}
		else if (!(g_thePlayer->disabledControlFlags & PlayerCharacter::kControlFlag_RolloverText))
		{
			hud->info->SetFloat(kTileValue_visible, true, 0);
			hud->visibilityFlags |= HUDMainMenu::kInfo;
		}
	}

	void __fastcall SetVisible(bool isVisible, bool firstTime = false)
	{
		if (JLMVisible.exchange(isVisible) == isVisible && !firstTime)
			return;

		if (!isVisible) {
			JLMRefresh.store(isVisible);
			JLMVisible.store(isVisible);
			mainTile->SetFloat(kTileValue_visible, isVisible, 1);
			return;
		}

		TESObjectREFR* newRef = HUDMainMenu::GetSingleton()->crosshairRef;
		if (!IsVATSKillCamActive() && newRef && !IsLockedRef(newRef) && g_thePlayer->eGrabType != PlayerCharacter::GrabMode::kGrabMode_ZKey && (!HasScript(newRef) || (!newRef->ResolveAshpile()->HasActivateScriptBlock() && newRef->ResolveAshpile()->IsActor()) || IsScriptWhitelisted(newRef->ResolveAshpile())))
		{
			SetButtonPromptTexts();
			newRef = newRef->ResolveAshpile();
			if (newRef != ref)
			{
				HandleChangedRefScripts();
				s_tempContChangesEntries.DeleteAll();

				SetContainer(newRef);
				UpdateItems();
				items.QuickSort(0, items.Size(), SortingFunctionLootMenu);

				ScrollToIndex(0);
				SetScrollOffset(0);

				UpdateStealingColor();
				RefreshItemDisplay();
				JLMRefresh.store(true);

				// trigger the OnOpen event
				ExtraScript* xScript = GetExtraType(newRef->extraDataList, Script);
				if (xScript && xScript->eventList)
				{
					ThisCall(0x5183C0, xScript->eventList, g_thePlayer, kEvent_OnOpen);
				}
			}
			return;
		}

		isVisible = false;
		JLMRefresh.store(isVisible);
		JLMVisible.store(isVisible);
		SetContainer(nullptr);
		mainTile->SetFloat(kTileValue_visible, isVisible, 1);
	}

	bool Init()
	{
		HUDMainMenu* hud = HUDMainMenu::GetSingleton();

		if (!InjectMenuXML(hud))
		{
			MessageBoxA(nullptr, "Loot Menu failed to initialize", "Tweaks", MB_ICONERROR);
			return false;
		}
		Hooks::Apply();

		Tile* JLMRect = hud->AddTileFromTemplate(mainTile, "JLMTemp", 0);

		ItemTiles = (Tile**)GameHeapAlloc(sizeof(Tile*) * JLMItems);
		char itemName[10] = "Item";
		for (int i = 0; i < JLMItems; ++i)
		{
			Tile* item = hud->AddTileFromTemplate(JLMRect, "JLMInjected", 0);
			sprintf(itemName, "Item%d", i);

			item->name.Set(itemName);
			SetTileComponentValue(item, "_ID", i);
			item->SetFloat(kTileValue_visible, 1, true);
			ItemTiles[i] = item;
		}

		const UInt8 SortPriorities[] = { kFormType_Key, kFormType_Note, kFormType_Book, kFormType_Ammo, kFormType_AlchemyItem, kFormType_Weapon, kFormType_Armor, kFormType_Clothing, kFormType_Misc };
		UInt8 priority = 255;
		for (auto item : SortPriorities)
		{
			ItemTypeToPriority[item] = priority--;
		}

		TextWidthTrait = mainTile->TraitNameToID("_JLMTextWidth");
		SetTileComponentValue(mainTile, "_JHMFont", JLMFont);
		SetTileComponentValue(mainTile, "_JLMItemHeightMin", JLMHeightMin);
		SetTileComponentValue(mainTile, "_JLMItemHeightMax", JLMHeightMax);
		SetTileComponentValue(mainTile, "_JLMItemWidthMin", JLMWidthMin);
		SetTileComponentValue(mainTile, "_JLMItemWidthMax", JLMWidthMax);

		SetTileComponentValue(mainTile, "_JLMOffsetX", JLMOffsetX);
		SetTileComponentValue(mainTile, "_JLMOffsetY", JLMOffsetY);

		SetTileComponentValue(mainTile, "_JLMItemIndent", JLMItemIndent);
		SetTileComponentValue(mainTile, "_JLMTextIndentX", JLMIndentTextX);
		SetTileComponentValue(mainTile, "_JLMItemIndentY", JLMIndentTextY);
		SetTileComponentValue(mainTile, "_JLMTextAdjust", JLMTextAdjust);

		SetTileComponentValue(mainTile, "_JLMJustifyX", JLMJustifyX);
		SetTileComponentValue(mainTile, "_JLMJustifyY", JLMJustifyY);

		SetButtonPromptTexts();

		SetTileComponentValue(mainTile, "_JLMButtonString3", "");
		SetTileComponentValue(mainTile, "_JLMInteract1", kPrompt_Take);
		SetTileComponentValue(mainTile, "_JLMInteract2", kPrompt_Open);
		SetTileComponentValue(mainTile, "_JLMInteract3", 0.0);

		SetTileComponentValue(mainTile, "_JLMItemHeightCur", 200);
		SetTileComponentValue(mainTile, "_JLMVisible", "2");

		SetTileComponentValue(mainTile, "_JLMIndex", 0.0F);
		SetTileComponentValue(mainTile, "_JLMItems", JLMItems);
		SetTileComponentValue(mainTile, "_JLMOffset", 0.0F);
		SetTileComponentValue(mainTile, "_JLMTotal", 0.0F);

		SetTileComponentValue(mainTile, "_JLMWeightVisible", JLMWeightVisible);
		if (JLMWeightVisible)
		{
			SetTileComponentValue(mainTile, "_JLMWeight", "0/0");
			SetTileComponentValue(mainTile, "_JLMWeightColor", 1.0F);
		}

		SetTileComponentValue(mainTile, "_JLMStealing", 0.0F);

		mainTile->SetFloat(kTileValue_visible, 1, true);

		SetVisible(false, true);
		GetScriptWhitelist();

		return true;
	}

	void UpdateCarryWeightText()
	{
		int playerInvWeight = g_thePlayer->avOwner.Fn_02(kAVCode_InventoryWeight);
		int playerCarryWeight = g_thePlayer->GetMaxCarryWeightPerkModified();

		char buf[50];
		sprintf(buf, "%d/%d", playerInvWeight, playerCarryWeight);

		SetTileComponentValue(mainTile, "_JLMWeight", buf);

		float selectedStackWeight = 0;
		if (auto selected = GetSelectedItem())
		{
			if (selected->entry && selected->entry->type)
			{
				float itemWeight = selected->entry->type->GetWeight();
				int countDelta = max(1, selected->entry->countDelta);
				selectedStackWeight = itemWeight * countDelta;
			}
		}

		if (playerInvWeight + selectedStackWeight > playerCarryWeight)
		{
			SetTileComponentValue(mainTile, "_JLMWeightColor", 2.0F);
		}
		else
		{
			SetTileComponentValue(mainTile, "_JLMWeightColor", 1.0F);
		}
	}

	signed int GetDpadScroll()
	{
		int iKeyRepeatInterval = 70;
		int iKeyRepeatTime = 300;

		static UInt32 lastDpadTime;
		static bool isScrolling;

		auto currentTime = timeGlobals->msPassed;
		auto deltaTime = currentTime - lastDpadTime;
		signed int result = 0;

		if (GetXIControlPressed(kXboxCtrl_DPAD_UP))
		{
			result = 1;
		}
		else if (GetXIControlPressed(kXboxCtrl_DPAD_DOWN))
		{
			result = -1;
		}
		else if (GetXIControlHeld(kXboxCtrl_DPAD_UP))
		{
			if (deltaTime > iKeyRepeatTime)
			{
				isScrolling = true;
				result = 1;
			}
			else if (isScrolling && (deltaTime > iKeyRepeatInterval))
			{
				result = 1;
			}
		}
		else if (GetXIControlHeld(kXboxCtrl_DPAD_DOWN))
		{
			if (deltaTime > iKeyRepeatTime)
			{
				isScrolling = true;
				result = -1;
			}
			else if (isScrolling && (deltaTime > iKeyRepeatInterval))
			{
				result = -1;
			}
		}
		else
		{
			isScrolling = false;
		}

		if (result)
		{
			lastDpadTime = currentTime;
		}

		return result;
	}

	bool IsAddScriptToWhiteList() {
		return IsShiftHeld() && IsAltHeld() && IsCtrlHeld() && IsKeyHeld(VK_RETURN);
	}

	bool IsAltEquip()
	{
		if (!JLMEnableQuickUse) return false;
		return IsShiftHeld() || GetXIControlHeld(kXboxCtrl_DPAD_LEFT) || GetXIControlHeld(kXboxCtrl_DPAD_RIGHT);
	}

	void TakeOrUseSelectedItem()
	{
		if (auto item = GetSelectedItem())
		{
			auto entry = item->entry;

			TESObjectREFR* itemRef = (TESObjectREFR*)entry->type;

			bool shouldKeepOwnership = !ref->IsActor();

			if (IsAltEquip() && NOT_ID(itemRef->baseForm, Ammo))
			{
				if (IsDroppedWeapon(item))
				{
					// nop call to PlaySound to prevent double pickup/equip sound
					NopFunctionCall(0x7766BD, 3);

					g_thePlayer->HandlePickupItem(itemRef, 1, false);
					g_thePlayer->EquipRef(itemRef);

					WriteRelCall(0x7766BD, 0x6F7D90);
				}
				else
				{
					ref->RemoveItemTarget(itemRef, g_thePlayer, 1, shouldKeepOwnership);
					g_thePlayer->EquipItemAlt(entry, false, true);
				}
			}
			else
			{
				if (IsDroppedWeapon(item))
				{
					g_thePlayer->HandlePickupItem(itemRef, 1, false);
					g_thePlayer->PlayPickupOrEquipSound(itemRef->baseForm, true, false);
				}
				else
				{
					ref->RemoveItemTarget(itemRef, g_thePlayer, item->count, shouldKeepOwnership);
					g_thePlayer->PlayPickupOrEquipSound(itemRef, true, false);
				}
			}

			if (JLMStealing)
			{
				// alert owners the item is stolen
				float value = itemRef->GetItemValue() * entry->countDelta;
				if (value > 0)
				{
					auto owner = ThisCall<TESObjectREFR*>(0x4F26C0, ref);
					ThisCall(0x70C030, g_thePlayer, ref, 0, 0, value, owner);
				}
			}

			UpdateItems();
			items.QuickSort(0, items.Size(), SortingFunctionLootMenu);

			if ((JLMIndex + ScrollOffset) >= NumContainerItems)
			{
				SetScrollOffset(0);
				ScrollToIndex(NumContainerItems - 1);
			}
			else if (ScrollOffset + JLMItems > NumContainerItems)
			{
				SetScrollOffset(ScrollOffset - 1);
				ScrollToIndex(JLMIndex);
			}

			shouldRunCloseContainerScript = true;
		}
	}

	void RunOnActivateBlock() {
		// prevent container menu showing
		NopFunctionCall(0x54FC37);
		NopFunctionCall(0x4B758F);
		NopFunctionCall(0x5572DA);

		// prevent the container playing the open animation, since the container menu would show when it ends
		SafeWrite8(0x44E120, 0xC3);

		ref->Activate(g_thePlayer, 0, 0, 1);

		SafeWrite8(0x44E120, 0x56);

		WriteRelCall(0x54FC37, 0x61D5A0);
		WriteRelCall(0x4B758F, 0x61D5A0);
		WriteRelCall(0x5572DA, 0x61D5A0);
	}

	void TakeAllItems()
	{
		int numItems = items.Size();
		float totalValueTaken = 0;

		bool shouldKeepOwnership = !ref->IsActor();

		for (int i = 0; i < numItems; ++i)
		{
			auto item = items.Get(i);
			auto entry = item->entry;
			TESObjectREFR* itemRef = (TESObjectREFR*)entry->type;

			if (IsDroppedWeapon(item))
			{
				g_thePlayer->HandlePickupItem(itemRef, 1, false);
			}
			else
			{
				ref->RemoveItemTarget(itemRef, g_thePlayer, item->count, shouldKeepOwnership);
			}

			if (JLMStealing)
			{
				totalValueTaken += itemRef->GetItemValue() * entry->countDelta;
			}
		}

		// alert owners the item is stolen
		if (totalValueTaken > 0)
		{
			auto owner = ThisCall<TESObjectREFR*>(0x4F26C0, ref);
			ThisCall(0x70C030, g_thePlayer, ref, 0, 0, totalValueTaken, owner);
		}

		UpdateItems();
		items.QuickSort(0, items.Size(), SortingFunctionLootMenu);
		SetScrollOffset(0);
		ScrollToIndex(0);
		shouldRunCloseContainerScript = true;
	}

	void Update()
	{
		HandlePrompt(JLMVisible.load());

		if (IsAddScriptToWhiteList()) {
			if (HUDMainMenu* hud = HUDMainMenu::GetSingleton(); hud && hud->crosshairRef) {
				TESObjectREFR* target = hud->crosshairRef->ResolveAshpile();
				if (target->GetContainer()) AddScriptToWhitelist(target);
			}
		}

		if (!ref) return;

		TESObjectREFR* hudRef = HUDMainMenu::GetSingleton()->crosshairRef;
		if (!hudRef || !(hudRef->ResolveAshpile()->GetContainer()) || hudRef->ResolveAshpile() != ref->ResolveAshpile())
		{
			ResetAndHideMenu();
			SetVisible(false);
			return;
		}

		auto inputGlobals = OSInputGlobals::GetSingleton();
		int scroll = inputGlobals->mouseWheelScroll / 120;
		if (!scroll)
		{
			scroll = GetDpadScroll();
		}
		if (scroll)
		{
			UInt32 previousIndex = JLMIndex;
			UInt32 previousOffset = ScrollOffset;
			ScrollToIndex(JLMIndex - scroll);

			if (JLMIndex != previousIndex || ScrollOffset != previousOffset)
			{
				PlayGameSound("UIPipBoyScroll");
				RefreshItemDisplay();
			}
		}

		if (NumContainerItems && mainTile->GetValueFloat(kTileValue_visible))
		{
			if (inputGlobals->GetControlState(TakeItemControlCode, isPressed))
			{
				RunOnActivateBlock();
				TakeOrUseSelectedItem();
				RefreshItemDisplay();
			}
			else if (inputGlobals->GetControlState(ActivateContainerCode, isPressed) && IsAltEquip())
			{
				RunOnActivateBlock();
				TakeAllItems();
				PlayGameSound("UIItemTakeAll");
				RefreshItemDisplay();
			}
		}

		if (JLMWeightVisible)
		{
			UpdateCarryWeightText();
		}

		SetTileComponentValue(mainTile, "_JLMInteract1", IsAltEquip() ? kPrompt_Equip : kPrompt_Take);
		SetTileComponentValue(mainTile, "_JLMInteract2", IsAltEquip() ? kPrompt_TakeAll : kPrompt_Open);

		if (!JLMRefresh.load())
			return;

		static bool isNextFrame = false;
		if (isNextFrame) {
			JLMVisible.store(JLMRefresh.load());
			mainTile->SetFloat(kTileValue_visible, JLMRefresh.load(), 1);
			RefreshItemDisplay();
			JLMRefresh.store(false);
			isNextFrame = false;
			return;
		}
		isNextFrame = true;
	}

	namespace Hooks
	{
		HUDMainMenu* HideLootMenu_WrapGetHUDMainMenu()
		{
			SetVisible(false);
			return HUDMainMenu::GetSingleton();
		}

		_declspec(naked) void ShowLootMenu_WrapSearchStrFromINI()
		{
			_asm
			{
				mov cl, 1
				call SetVisible

				push    1
				call    HandlePrompt
				add     esp, 4

				mov ecx, dword ptr ds : [0xF654A8]
				ret
			}
		}

		_declspec(naked) void __fastcall ShowLootMenu_ContainersHook(int promptType)
		{
			_asm
			{
				cmp esi, kOpen
				jne NotContainer

				mov cl, 1
				call SetVisible

				push    1
				call    HandlePrompt
				add     esp, 4

				NotContainer :
				mov eax, dword ptr ds : [esi * 4 + 0x10733C0]
					ret
			}
		}

		_declspec(naked) void __fastcall HideLootMenu_OnGrab() {
			constexpr static UInt32 origAddr = 0x00475686;
			_asm
			{
				pushad
				pushfd

				mov     ecx, 0
				mov     edx, 0
				call    SetVisible

				push    0
				call    SetContainer
				add     esp, 4

				push    0
				call    HandlePrompt
				add     esp, 4

				popfd
				popad

				sub     esp, 12
				fld     dword ptr[ecx + 4]
				jmp     origAddr
			}
		}

		void __fastcall OnContainerOpen_HideMenu(Menu* menu)
		{
			ThisCall(0x639DA0, menu);
			SetVisible(false);
			SetContainer(nullptr);
		}

		int __fastcall GetMouseScrollPreventIfLootMenuVisible(OSInputGlobals* input, void* edx, int _3)
		{
			if (JLMVisible.load()) return 0;
			return input->mouseWheelScroll;
		}

		int __fastcall GetActivateKeyPressedAndNotLootMenuVisible(OSInputGlobals* input, void* edx, ControlCode controlCode, KeyState isPressed)
		{
			if (JLMVisible.load() && IsAltEquip()) return false;
			return input->GetControlState(JLMVisible.load() ? ActivateContainerCode : ControlCode::Activate, isPressed);
		}

		int __fastcall GetKeyPressedAndNotLootMenuVisible(OSInputGlobals* input, void* edx, ControlCode controlCode, KeyState isPressed)
		{
			return !JLMVisible.load() && input->GetControlState(controlCode, isPressed);
		}

		signed int __cdecl GetControllerStateAndNotLootMenuVisible(XboxControlCode code, bool state)
		{
			if (JLMVisible.load()) return 1;
			return CdeclCall<signed int>(0x6211B0, code, state);
		}

		UInt32 startMenuCloseAddr;
		void __fastcall StartMenuClose_SetButtonPrompts(StartMenu* menu)
		{
			ThisCall(startMenuCloseAddr, menu);
			SetButtonPromptTexts();
		}

		void Apply()
		{
			WriteRelCall(0x64BF0E, UInt32(HideLootMenu_WrapGetHUDMainMenu));

			// actors
			WriteRelCall(0x64B01D, UInt32(ShowLootMenu_WrapSearchStrFromINI));
			SafeWrite8(0x64B01D + 5, 0x90);

			// containers
			WriteRelCall(0x64B259, UInt32(ShowLootMenu_ContainersHook));
			SafeWrite16(0x64B259 + 5, 0x9090);

			// prevent scrollwheel if menu visible
			WriteRelCall(0x77C269, UInt32(GetMouseScrollPreventIfLootMenuVisible));

			// prevent activate if menu visible
			WriteRelCall(0x78A603, UInt32(GetActivateKeyPressedAndNotLootMenuVisible));

			if (JLMReloadKeyActivates)
			{
				ActivateContainerCode = ControlCode::ReadyItem;
				// prevent reload key if menu visible
				for (UInt32 patchAddr : {0x788D0B, 0x77D1C7, 0x788D19, 0x788D28})
				{
					WriteRelCall(patchAddr, UInt32(GetKeyPressedAndNotLootMenuVisible));
				}
			}
			else
			{
				// prevent sleep wait key if menu visible
				WriteRelCall(0x789ED4, UInt32(GetKeyPressedAndNotLootMenuVisible));
			}

			// reset the menu when opening a container, so it refreshes if any items were taken
			WriteRelCall(0x63BCA4, UInt32(OnContainerOpen_HideMenu));

			// prevent DPAD while menu visible
			WriteRelCall(0x618964, UInt32(GetControllerStateAndNotLootMenuVisible)); // Down
			WriteRelCall(0x61896F, UInt32(GetControllerStateAndNotLootMenuVisible)); // Up
			WriteRelCall(0x61897A, UInt32(GetControllerStateAndNotLootMenuVisible)); // Right
			WriteRelCall(0x618985, UInt32(GetControllerStateAndNotLootMenuVisible)); // Left

			// hides loot menu when grabbing object/actor
			WriteRelJump(0x00475680, UInt32((HideLootMenu_OnGrab)));

			// set the button prompts when closing the start menu
			startMenuCloseAddr = DetourRelCall(0x680563, UInt32(StartMenuClose_SetButtonPrompts));
		}
	}
}