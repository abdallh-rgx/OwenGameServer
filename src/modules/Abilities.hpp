#pragma once
#include "pch.h"

class Abilities {
public:
    static FGameplayAbilitySpecHandle GiveAbility(UAbilitySystemComponent* asc, UObject* ability);
    static void GiveAbilitySet(UAbilitySystemComponent* asc, UFortAbilitySet* set);
    static void Hook();
};
