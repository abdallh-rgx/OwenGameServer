#pragma once
#include "pch.h"

enum class EAmmoType : uint8_t {
    Assault = 0,
    Shotgun = 1,
    Submachine = 2,
    Rocket = 3,
    Sniper = 4
};

struct FLategameItem {
    int32_t Count;
    void* Pad;
    UFortItemDefinition* Item;
    FLategameItem(int c, void* p, UFortItemDefinition* i) : Count(c), Pad(p), Item(i) {}
};

class Lategame {
public:
    static FLategameItem GetShotguns();
    static FLategameItem GetAssaultRifles();
    static FLategameItem GetSnipers();
    static FLategameItem GetHeals();
    static UFortAmmoItemDefinition* GetAmmo(EAmmoType ammoType);
    static UFortResourceItemDefinition* GetResource(EFortResourceType resourceType);
};
