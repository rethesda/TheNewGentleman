#pragma once

class SEEvents : public Singleton<SEEvents>,
                 public RE::BSTEventSink<RE::TESObjectLoadedEvent>,
                 public RE::BSTEventSink<RE::TESEquipEvent>,
                 public RE::BSTEventSink<RE::TESSwitchRaceCompleteEvent> {
  public:
    void RegisterEvents();

  protected:
    RE::BSEventNotifyControl ProcessEvent(const RE::TESEquipEvent* event, RE::BSTEventSource<RE::TESEquipEvent>*) override;
    RE::BSEventNotifyControl ProcessEvent(const RE::TESObjectLoadedEvent* event, RE::BSTEventSource<RE::TESObjectLoadedEvent>*) override;
    RE::BSEventNotifyControl ProcessEvent(const RE::TESSwitchRaceCompleteEvent* event, RE::BSTEventSource<RE::TESSwitchRaceCompleteEvent>*) override;
};

extern SEEvents* events;