#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include "Offsets.hpp"
#include "AndroidBase.hpp"
#include "FName.hpp"

struct FVectorLocal {
    double X, Y, Z;
};

inline FQuat RotatorToQuat(const FRotator& r) {
    constexpr double RAD = 0.017453292519943295;
    double cp = r.Pitch * RAD * 0.5;
    double cy = r.Yaw * RAD * 0.5;
    double cr = r.Roll * RAD * 0.5;
    double sp = sin(cp), sy = sin(cy), sr = sin(cr);
    double cph = cos(cp), cyh = cos(cy), crh = cos(cr);
    FQuat q;
    q.X = crh * sp * cyh + sr * cph * sy;
    q.Y = crh * sp * sy - sr * cph * cyh;
    q.Z = crh * cph * sy + sr * sp * cyh;
    q.W = crh * cph * cyh - sr * sp * sy;
    return q;
}

inline FRotator QuatToRotator(const FQuat& q) {
    constexpr double RAD_TO_DEG = 57.29577951308232;
    const double singularityTest = q.Z * q.X - q.W * q.Y;
    const double yawY = 2.0 * (q.W * q.Z + q.X * q.Y);
    const double yawX = 1.0 - 2.0 * ((q.Y * q.Y) + (q.Z * q.Z));

    const double SINGULARITY_THRESHOLD = 0.4999995;
    FRotator rotator;

    if (singularityTest < -SINGULARITY_THRESHOLD) {
        rotator.Pitch = -90.0;
        rotator.Yaw = atan2(yawY, yawX) * RAD_TO_DEG;
        rotator.Roll = FRotator::NormalizeAxis(-rotator.Yaw - (2.0 * atan2(q.X, q.W) * RAD_TO_DEG));
    } else if (singularityTest > SINGULARITY_THRESHOLD) {
        rotator.Pitch = 90.0;
        rotator.Yaw = atan2(yawY, yawX) * RAD_TO_DEG;
        rotator.Roll = FRotator::NormalizeAxis(rotator.Yaw - (2.0 * atan2(q.X, q.W) * RAD_TO_DEG));
    } else {
        rotator.Pitch = asin(2.0 * singularityTest) * RAD_TO_DEG;
        rotator.Yaw = atan2(yawY, yawX) * RAD_TO_DEG;
        rotator.Roll = atan2(-2.0 * (q.W * q.X + q.Y * q.Z), (1.0 - 2.0 * ((q.X * q.X) + (q.Y * q.Y)))) * RAD_TO_DEG;
    }

    return rotator;
}

inline CoreUObject::FTransform MakeTransform(const FVector& loc, const FRotator& rot = FRotator()) {
    CoreUObject::FTransform t;
    t.Rotation = RotatorToQuat(rot);
    t.Translation = loc;
    t.Scale3D = FVector(1.0, 1.0, 1.0);
    return t;
}

template <typename T, typename U>
inline T* CastSDK(U* obj) {
    return (obj && obj->IsA(T::StaticClass())) ? (T*)obj : nullptr;
}

template <typename T>
using UEAllocatedVector = std::vector<T>;
using UEAllocatedString = std::string;
using UEAllocatedWString = std::wstring;

inline bool operator==(const FGuid& a, const FGuid& b) {
    return a.A == b.A && a.B == b.B && a.C == b.C && a.D == b.D;
}
inline bool operator!=(const FGuid& a, const FGuid& b) {
    return !(a == b);
}

void MakeWeakPtrInto(FWeakObjectPtr& out, void* obj);

template<typename T>
inline TWeakObjectPtr<T> MakeWeakPtr(T* obj) {
    TWeakObjectPtr<T> wp{};
    MakeWeakPtrInto(wp, obj);
    return wp;
}

inline std::wstring FNameToWString(const FName& name) {
    std::string s = name.ToString();
    return std::wstring(s.begin(), s.end());
}

class Utils {
public:
    static UObject* FindObject(const wchar_t* path, UClass* cls = nullptr);
    static UObject* LoadObject(const wchar_t* path, UClass* cls = nullptr);
    static UObject* FindOrLoad(const wchar_t* path, UClass* cls = nullptr);

    template <typename T = UObject>
    static T* Find(const wchar_t* path) {
        return (T*)FindObject(path, nullptr);
    }

    template <typename T = UObject>
    static T* Load(const wchar_t* path) {
        return (T*)LoadObject(path, nullptr);
    }

    template <typename T = UObject>
    static T* Get(const wchar_t* path) {
        return (T*)FindOrLoad(path, nullptr);
    }

    static AActor* SpawnActor(UClass* cls, const FVector& loc, const FRotator& rot = FRotator(), AActor* owner = nullptr);

    template <typename T = AActor>
    static T* SpawnActor(UClass* cls, const FVector& loc, const FRotator& rot = FRotator(), AActor* owner = nullptr) {
        return (T*)SpawnActor(cls, loc, rot, owner);
    }

    template <typename T = AActor>
    static T* SpawnActor(const FVector& loc, const FRotator& rot = FRotator(), AActor* owner = nullptr) {
        return (T*)SpawnActor(T::StaticClass(), loc, rot, owner);
    }

    static std::vector<AActor*> GetAllActors(UClass* cls);

    template <typename T = AActor>
    static std::vector<T*> GetAll() {
        std::vector<T*> out;
        for (AActor* a : GetAllActors(T::StaticClass())) out.push_back((T*)a);
        return out;
    }

    template <typename T = AActor>
    static std::vector<T*> GetAll(UClass* cls) {
        std::vector<T*> out;
        for (AActor* a : GetAllActors(cls)) out.push_back((T*)a);
        return out;
    }

    static float EvaluateScalableFloat(FScalableFloat& value);

    static float EvaluateCurve(FCurveTableRowHandle& handle, float inTime);

    static FString ToFString(const std::wstring& s);

    static std::wstring FromFString(const FString& s);

    static bool TagContainerHasTag(const FGameplayTagContainer& container, const wchar_t* tagName);

    static bool TagContainerHasAll(const FGameplayTagContainer& container, const FGameplayTagContainer& required);

    static void MarkItemDirty(FFastArraySerializer& serializer, FFastArraySerializerItem& item);

    static void MarkArrayDirty(FFastArraySerializer& serializer);
};

namespace Sarah {
void* EngineRealloc(void* ptr, int64_t newLen, uint32_t alignment);
}
