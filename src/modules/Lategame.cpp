#include "pch.h"
#include "Lategame.hpp"
#include "Dumper.hpp"

static FLategameItem PickFromList(std::vector<FLategameItem>& list) {
    if (list.empty()) return FLategameItem(0, nullptr, nullptr);
    return list[rand() % list.size()];
}

FLategameItem Lategame::GetShotguns() {
    static std::vector<FLategameItem> list{
        FLategameItem(1, {}, Utils::Find<UFortWeaponRangedItemDefinition>(L"/FlipperGameplay/Items/Weapons/BurstShotgun/WID_Shotgun_CoreBurst_Athena_SR.WID_Shotgun_CoreBurst_Athena_SR")),
        FLategameItem(1, {}, Utils::Find<UFortWeaponRangedItemDefinition>(L"/FlipperGameplay/Items/Weapons/BurstShotgun/WID_Shotgun_CoreBurst_Athena_VR.WID_Shotgun_CoreBurst_Athena_VR")),
        FLategameItem(1, {}, Utils::Find<UFortWeaponRangedItemDefinition>(L"/Game/Athena/Items/Weapons/WID_Shotgun_Standard_Athena_VR_Ore_T03.WID_Shotgun_Standard_Athena_VR_Ore_T03")),
        FLategameItem(1, {}, Utils::Find<UFortWeaponRangedItemDefinition>(L"/Game/Athena/Items/Weapons/WID_Shotgun_Standard_Athena_SR_Ore_T03.WID_Shotgun_Standard_Athena_SR_Ore_T03")),
    };
    return PickFromList(list);
}

FLategameItem Lategame::GetAssaultRifles() {
    static std::vector<FLategameItem> list{
        FLategameItem(1, {}, Utils::Find<UFortWeaponRangedItemDefinition>(L"/FlipperGameplay/Items/Weapons/CoreAR/WID_Assault_CoreAR_Athena_SR.WID_Assault_CoreAR_Athena_SR")),
        FLategameItem(1, {}, Utils::Find<UFortWeaponRangedItemDefinition>(L"/FlipperGameplay/Items/Weapons/CoreAR/WID_Assault_CoreAR_Athena_VR.WID_Assault_CoreAR_Athena_VR")),
        FLategameItem(1, {}, Utils::Find<UFortWeaponRangedItemDefinition>(L"/FlipperGameplay/Items/Weapons/RedDotAR/WID_Assault_RedDotAR_Athena_SR.WID_Assault_RedDotAR_Athena_SR")),
        FLategameItem(1, {}, Utils::Find<UFortWeaponRangedItemDefinition>(L"/FlipperGameplay/Items/Weapons/RedDotAR/WID_Assault_RedDotAR_Athena_VR.WID_Assault_RedDotAR_Athena_VR")),
        FLategameItem(1, {}, Utils::Find<UFortWeaponRangedItemDefinition>(L"/Game/Athena/Items/Weapons/WID_Assault_AutoHigh_Athena_SR_Ore_T03.WID_Assault_AutoHigh_Athena_SR_Ore_T03")),
        FLategameItem(1, {}, Utils::Find<UFortWeaponRangedItemDefinition>(L"/Game/Athena/Items/Weapons/WID_Assault_AutoHigh_Athena_VR_Ore_T03.WID_Assault_AutoHigh_Athena_VR_Ore_T03")),
    };
    return PickFromList(list);
}

FLategameItem Lategame::GetSnipers() {
    static std::vector<FLategameItem> list{
        FLategameItem(1, {}, Utils::Find<UFortWeaponRangedItemDefinition>(L"/FlipperGameplay/Items/Weapons/CoreSniper/WID_Sniper_CoreSniper_Athena_SR.WID_Sniper_CoreSniper_Athena_SR")),
        FLategameItem(1, {}, Utils::Find<UFortWeaponRangedItemDefinition>(L"/FlipperGameplay/Items/Weapons/CoreSMG/WID_SMG_CoreSMG_Athena_SR.WID_SMG_CoreSMG_Athena_SR")),
        FLategameItem(6, {}, Utils::Find<UFortWeaponRangedItemDefinition>(L"/Game/Athena/Items/Consumables/ShockwaveGrenade/Athena_ShockGrenade.Athena_ShockGrenade")),
        FLategameItem(1, {}, Utils::Find<UFortWeaponRangedItemDefinition>(L"/ParallelGameplay/Items/WestSausage/WID_WestSausage_Parallel_L_M.WID_WestSausage_Parallel_L_M")),
    };
    return PickFromList(list);
}

FLategameItem Lategame::GetHeals() {
    static std::vector<FLategameItem> list{
        FLategameItem(3, {}, Utils::Find<UFortWeaponRangedItemDefinition>(L"/Game/Athena/Items/Consumables/Shields/Athena_Shields.Athena_Shields")),
        FLategameItem(6, {}, Utils::Find<UFortWeaponRangedItemDefinition>(L"/Game/Athena/Items/Consumables/ShieldSmall/Athena_ShieldSmall.Athena_ShieldSmall")),
        FLategameItem(6, {}, Utils::Find<UFortWeaponRangedItemDefinition>(L"/Game/Athena/Items/Consumables/ChillBronco/Athena_ChillBronco.Athena_ChillBronco")),
        FLategameItem(2, {}, Utils::Find<UFortWeaponRangedItemDefinition>(L"/Game/Athena/Items/Consumables/PurpleStuff/Athena_PurpleStuff.Athena_PurpleStuff")),
        FLategameItem(6, {}, Utils::Find<UFortWeaponRangedItemDefinition>(L"/Game/Athena/Items/Consumables/ShockwaveGrenade/Athena_ShockGrenade.Athena_ShockGrenade")),
    };
    return PickFromList(list);
}

UFortAmmoItemDefinition* Lategame::GetAmmo(EAmmoType ammoType) {
    static std::vector<UFortAmmoItemDefinition*> ammos{
        Utils::Find<UFortAmmoItemDefinition>(L"/Game/Athena/Items/Ammo/AthenaAmmoDataBulletsLight.AthenaAmmoDataBulletsLight"),
        Utils::Find<UFortAmmoItemDefinition>(L"/Game/Athena/Items/Ammo/AthenaAmmoDataShells.AthenaAmmoDataShells"),
        Utils::Find<UFortAmmoItemDefinition>(L"/Game/Athena/Items/Ammo/AthenaAmmoDataBulletsMedium.AthenaAmmoDataBulletsMedium"),
        Utils::Find<UFortAmmoItemDefinition>(L"/Game/Athena/Items/Ammo/AmmoDataRockets.AmmoDataRockets"),
        Utils::Find<UFortAmmoItemDefinition>(L"/Game/Athena/Items/Ammo/AthenaAmmoDataBulletsHeavy.AthenaAmmoDataBulletsHeavy")
    };
    int idx = (int)ammoType;
    if (idx < 0 || idx >= (int)ammos.size()) return nullptr;
    return ammos[idx];
}

UFortResourceItemDefinition* Lategame::GetResource(EEFortResourceType resourceType) {
    static std::vector<UFortResourceItemDefinition*> res{
        Utils::Find<UFortResourceItemDefinition>(L"/Game/Items/ResourcePickups/WoodItemData.WoodItemData"),
        Utils::Find<UFortResourceItemDefinition>(L"/Game/Items/ResourcePickups/StoneItemData.StoneItemData"),
        Utils::Find<UFortResourceItemDefinition>(L"/Game/Items/ResourcePickups/MetalItemData.MetalItemData")
    };
    int idx = (int)resourceType;
    if (idx < 0 || idx > 2) return nullptr;
    return res[idx];
}
