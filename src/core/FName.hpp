#pragma once
#include <cstdint>
#include <string>
#include "Offsets.hpp"
#include "AndroidBase.hpp"

FName MakeFName(const wchar_t* name);
uint32_t MakeFNameIndex(const wchar_t* name);
