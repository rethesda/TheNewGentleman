#include <Base.h>
#include <Core.h>
#include <Events.h>
#include <Inis.h>

void Events::RegisterEvents() {
  coverKeys.clear();
  coverKeys.push_back(Tng::ArmoKey(Tng::akeyCover));
  coverKeys.push_back(Tng::ArmoKey(Tng::akeyRevFem));
  coverKeys.push_back(Tng::ArmoKey(Tng::akeyRevMal));
  showErrMessage = true;
  const auto sesh = RE::ScriptEventSourceHolder::GetSingleton();
  sesh->AddEventSink<RE::TESEquipEvent>(GetSingleton());
  sesh->AddEventSink<RE::TESObjectLoadedEvent>(GetSingleton());
  sesh->AddEventSink<RE::TESSwitchRaceCompleteEvent>(GetSingleton());

  SKSE::log::info("Registered for necessary events.");
}

RE::BSEventNotifyControl Events::ProcessEvent(const RE::TESEquipEvent* event, RE::BSTEventSource<RE::TESEquipEvent>*) {
  if (!event) return RE::BSEventNotifyControl::kContinue;
  const auto actor = event->actor->As<RE::Actor>();
  auto npc = actor ? actor->GetActorBase() : nullptr;
  auto armor = RE::TESForm::LookupByID<RE::TESObjectARMO>(event->baseObject);
  if (Core::CanModifyNPC(npc) < 0 || !armor || !armor->HasKeywordInArray(coverKeys, false)) return RE::BSEventNotifyControl::kContinue;
  if (npc->race->HasKeyword(Tng::RaceKey(Tng::rkeyPreprocessed)) && !Base::ReevaluateRace(npc->race, actor)) return RE::BSEventNotifyControl::kContinue;
  if (armor->HasKeyword(Tng::ArmoKey(Tng::akeyCover)) || (armor->HasKeyword(Tng::ArmoKey(Tng::akeyRevFem)) && !npc->IsFemale()) || (armor->HasKeyword(Tng::ArmoKey(Tng::akeyRevMal)) && npc->IsFemale()))
    DoChecks(actor, armor, event->equipped);
  return RE::BSEventNotifyControl::kContinue;
}

RE::BSEventNotifyControl Events::ProcessEvent(const RE::TESObjectLoadedEvent* event, RE::BSTEventSource<RE::TESObjectLoadedEvent>*) {
  if (!event) return RE::BSEventNotifyControl::kContinue;
  const auto actor = RE::TESForm::LookupByID<RE::Actor>(event->formID);
  const auto npc = actor ? actor->GetActorBase() : nullptr;
  if (!npc) return RE::BSEventNotifyControl::kContinue;
  if (Core::CanModifyNPC(npc) < 0) return RE::BSEventNotifyControl::kContinue;
  if (npc->race->HasKeyword(Tng::RaceKey(Tng::rkeyPreprocessed)) && !Base::ReevaluateRace(npc->race, actor)) return RE::BSEventNotifyControl::kContinue;
  if (actor->IsPlayerRef() && Base::HasPlayerChanged(actor)) {
    Base::SetPlayerInfo(actor, Tng::cDef);
  }
  DoChecks(actor);
  return RE::BSEventNotifyControl::kContinue;
}

RE::BSEventNotifyControl Events::ProcessEvent(const RE::TESSwitchRaceCompleteEvent* event, RE::BSTEventSource<RE::TESSwitchRaceCompleteEvent>*) {
  auto actor = event->subject.get()->As<RE::Actor>();
  auto npc = actor ? actor->GetActorBase() : nullptr;
  if (!actor || !npc || !npc->skin) return RE::BSEventNotifyControl::kContinue;
  if (npc->race->HasKeyword(Tng::RaceKey(Tng::rkeyPreprocessed)) && !Base::ReevaluateRace(npc->race, actor)) return RE::BSEventNotifyControl::kContinue;
  if (npc->skin->HasKeyword(Tng::ArmoKey(Tng::akeyGenSkin)) && !npc->race->HasKeyword(Tng::RaceKey(Tng::rkeyProcessed))) {
    oldSkins.insert_or_assign(npc->GetFormID(), npc->skin);
    npc->skin = nullptr;
    return RE::BSEventNotifyControl::kContinue;
  }
  if (oldSkins.find(npc->GetFormID()) != oldSkins.end() && npc->race->HasKeyword(Tng::RaceKey(Tng::rkeyProcessed))) {
    npc->skin = oldSkins[npc->GetFormID()];
    oldSkins.erase(npc->GetFormID());
  }
  return RE::BSEventNotifyControl::kContinue;
}

void Events::DoChecks(RE::Actor* actor, RE::TESObjectARMO* armor, bool isEquipped) {
  CheckForAddons(actor);
  CheckCovering(actor, armor, isEquipped);
}

void Events::CheckCovering(RE::Actor* actor, RE::TESObjectARMO* armor, bool isEquipped) {
  if (!actor) return;
  auto down = actor->GetWornArmor(Tng::cSlotGenital);
  auto cover = armor && isEquipped ? armor : GetCoveringItem(actor, isEquipped ? nullptr : armor);
  bool needsCover = NeedsCover(actor);
  if (!needsCover || (down && FormToLocView(down) != Tng::cCover) || (!cover && down && FormToLocView(down) == Tng::cCover)) {
    if (down && FormToLocView(down) == Tng::cCover) {
      RE::ActorEquipManager::GetSingleton()->UnequipObject(actor, down, nullptr, 1, nullptr, false, true, false, true);
    }
    actor->RemoveItem(Tng::Block(), 10, RE::ITEM_REMOVE_REASON::kRemove, nullptr, nullptr);
    return;
  }
  auto tngCover = ForceTngCover(actor, false);
  if ((cover && down) || (!cover && !down)) return;
  if (!tngCover && showErrMessage) {
    showErrMessage = false;
    ShowSkyrimMessage("TNG faced an error when trying to cover genitalia. The New Gentleman won't function properly!");
  }
  if (down && FormToLocView(down) == Tng::cCover) {
    RE::ActorEquipManager::GetSingleton()->UnequipObject(actor, down, nullptr, 1, nullptr, false, true, false, true);
  }
  if (cover && !down) {
    RE::ActorEquipManager::GetSingleton()->EquipObject(actor, tngCover);
  }
}

RE::TESObjectARMO* Events::GetCoveringItem(RE::Actor* actor, RE::TESObjectARMO* armor) {
  auto npc = actor ? actor->GetActorBase() : nullptr;
  if (!npc) return nullptr;
  auto inv = actor->GetInventory([=](RE::TESBoundObject& a_object) { return a_object.IsArmor() && a_object.HasKeywordInArray(coverKeys, false); }, true);
  for (const auto& [item, invData] : inv) {
    const auto& [count, entry] = invData;
    if (count > 0 && entry && entry->IsWorn() && item != armor) {
      auto res = item->As<RE::TESObjectARMO>();
      if (res->HasKeyword(Tng::ArmoKey(Tng::akeyCover)) || (res->HasKeyword(Tng::ArmoKey(Tng::akeyRevFem)) && !npc->IsFemale()) || (res->HasKeyword(Tng::ArmoKey(Tng::akeyRevMal)) && npc->IsFemale()))
        return res;
    }
  }
  return nullptr;
}

bool Events::NeedsCover(RE::Actor* actor) {
  const auto npc = actor ? actor->GetActorBase() : nullptr;
  if (Base::CanModifyNPC(npc) < 0) return false;
  if (npc->IsFemale()) {
    return npc->HasKeyword(Tng::NPCKey(Tng::npckeyGentlewoman)) || (Base::GetNPCAddon(npc).second >= 0);
  } else {
    return (!npc->HasKeyword(Tng::NPCKey(Tng::npckeyExclude)));
  }
}

RE::TESBoundObject* Events::ForceTngCover(RE::Actor* actor, bool ifUpdate) {
  auto inv = actor->GetInventory([=](RE::TESBoundObject& a_object) { return a_object.IsArmor() && FormToLocView(a_object.As<RE::TESObjectARMO>()) == Tng::cCover; }, ifUpdate);
  if (!Tng::Block()) return nullptr;
  for (const auto& [item, invData] : inv) {
    const auto& [count, entry] = invData;
    if (count == 1 && entry) {
      return item;
    }
    if (count == 0 && entry) {
      actor->AddObjectToContainer(Tng::Block(), nullptr, 1, nullptr);
      return ForceTngCover(actor, true);
    }
    if (count > 1 && entry) {
      actor->RemoveItem(Tng::Block(), count - 1, RE::ITEM_REMOVE_REASON::kRemove, nullptr, nullptr);
      return item;
    }
  }
  if (ifUpdate) return nullptr;
  actor->AddObjectToContainer(Tng::Block(), nullptr, 1, nullptr);
  return ForceTngCover(actor, true);
}

void Events::CheckForAddons(RE::Actor* actor) {
  const auto npc = actor ? actor->GetActorBase() : nullptr;
  if (!npc) return;
  if (!npc->IsPlayer() || !Base::GetBoolSetting(Tng::bsExcludePlayerSize)) Core::SetActorSize(actor, Tng::cNul);
  auto addnPair = Base::GetNPCAddon(npc);
  if (addnPair.second == Tng::pgErr) {
    SKSE::log::critical("Faced an issue retrieving information for {}!", npc->GetName());
    return;
  }
  switch (addnPair.second) {
    case Tng::cNul:
      if (npc->IsFemale()) {
        if (!npc->skin || !npc->skin->HasKeyword(Tng::ArmoKey(Tng::akeyGenSkin))) return;
      } else {
        if (npc->skin && !npc->skin->HasKeyword(Tng::ArmoKey(Tng::akeyGenSkin))) return;
      }
      Core::SetNPCAddon(npc, Tng::cNul, false);
      return;
    case Tng::cDef:
      if (Base::GetRgAddon(npc->race) < 0) return;
      if (npc->IsFemale()) {
        auto addnIdx = GetNPCAutoAddon(npc);
        if ((addnIdx < 0 && npc->skin && npc->skin->HasKeyword(Tng::ArmoKey(Tng::akeyGenSkin))) || (addnIdx >= 0 && (!npc->skin || !npc->skin->HasKeyword(Tng::ArmoKey(Tng::akeyGenSkin)))))
          Core::SetNPCAddon(npc, addnIdx, false);
      } else {
        auto addnIdx = Base::GetBoolSetting(Tng::bsRandomizeMaleAddon) ? GetNPCAutoAddon(npc) : Tng::cDef;
        if (addnIdx < 0 && (!npc->skin || npc->skin->HasKeyword(Tng::ArmoKey(Tng::akeyGenSkin)))) return;
        Core::SetNPCAddon(npc, addnIdx, false);
      }
      return;
    default:
      if (npc->skin && npc->skin->HasKeyword(Tng::ArmoKey(Tng::akeyGenSkin))) return;
      Core::SetNPCAddon(npc, addnPair.second, addnPair.first);
      break;
  }
}

int Events::GetNPCAutoAddon(RE::TESNPC* npc) {
  auto list = Base::GetRgAddonList(npc->race, npc->IsFemale(), true);
  const auto count = list.size();
  const size_t chance = npc->IsFemale() ? static_cast<size_t>(std::floor(Tng::WRndGlb()->value + 0.1)) : Tng::cMalRandomPriority;
  if (count == 0 || chance == 0) return Tng::cDef;
  return npc->GetFormID() % 100 < chance ? static_cast<int>(list[npc->GetFormID() % count]) : Tng::cDef;
}
