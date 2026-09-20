#include "pch.h"
#include "Abilities.hpp"
#include "Dumper.hpp"

FGameplayAbilitySpecHandle Abilities::GiveAbility(UAbilitySystemComponent* asc, UObject* ability) {
    if (!asc || !ability) return;

    FGameplayAbilitySpec spec{};
    spec.Ability = (UGameplayAbility*)ability;
    spec.Level = 1;
    spec.InputID = -1;
    spec.SourceObject = nullptr;

    using GiveAbility_t = FGameplayAbilitySpecHandle* (*)(UAbilitySystemComponent*, FGameplayAbilitySpecHandle*, FGameplayAbilitySpec&&);
    static GiveAbility_t giveAbility = nullptr;
    if (!giveAbility) giveAbility = (GiveAbility_t)(Sarah::ImageBase + Off::GiveAbility);

    FGameplayAbilitySpecHandle outHandle{};
    giveAbility(asc, &outHandle, std::move(spec));
    return outHandle;
}

void Abilities::GiveAbilitySet(UAbilitySystemComponent* asc, UFortAbilitySet* set) {
    if (!set || !asc) return;
    for (auto& ability : set->GameplayAbilities) {
        UClass* cls = ability.Get();
        if (cls && cls->ClassDefaultObject) {
            GiveAbility(asc, cls->ClassDefaultObject);
        }
    }
}

void Abilities::Hook() {
}
