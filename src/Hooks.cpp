#include <Base.h>
#include <Core.h>
#include <Events.h>
#include <Hooks.h>

void Hooks::Install() {
  //
  InstallHook<Load3D>();
}

RE::NiAVObject* Hooks::Load3D::thunk(Target* actor, bool backgroundLoading) {
  auto res = Load3D::func(actor, backgroundLoading);
  auto npc = actor ? actor->GetActorBase() : nullptr;
  auto canModify = core->CanModifyNPC(npc);
  if (canModify == Util::resOkRacePP || canModify < 0) return res;
  if (npc->HasKeyword(ut->Key(Util::kyProcessed))) return res;
  auto curSkin = actor->GetSkin();
  events->DoChecks(actor);
  if (actor->GetSkin() != curSkin) {
    SKSE::log::debug("\t{} hook fixed addon for actor [0x{:x}:{}].", Load3D::name, actor->GetFormID(), npc->GetName());
  }
  return res;
}
