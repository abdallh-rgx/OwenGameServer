To find and call UKismetStringLibrary::Conv_StringToName using a string reference within your existing C++ SDK, you can utilize the Unreal Engine reflection system. This allows you to resolve the function at runtime even if you do not have the library headers linked or if you are working in an environment where symbols are not exported.
In your specific Sarah namespace context, you can implement this by resolving the UClass and the UFunction via their internal string paths.
1. Identify the Reflection Paths
To find these objects by string, use the following internal paths:

Class Path: /Script/Engine.KismetStringLibrary
Function Name: Conv_StringToName

2. Implement Dynamic Function Lookup
You can update your MakeFName function to resolve the function reference dynamically. It is best practice to cache the UFunction* pointer to avoid the performance cost of string lookups on every call.
// Within your MakeFName implementation
FName MakeFName(const wchar_t* name) {
    if (!name || !name[0]) return FName{};

    // ... (Your existing cache/mutex logic) ...

    // 1. Resolve the Function Reference once
    static UFunction* ConvFunc = nullptr;
    if (!ConvFunc) {
        // Use your existing FindObject utility to find the class
        UClass* StringLibClass = (UClass*)Utils::FindObject(L"/Script/Engine.KismetStringLibrary", nullptr);
        if (StringLibClass) {
            // Find the function by its string name
            ConvFunc = StringLibClass->FindFunctionByName(FName(TEXT("Conv_StringToName")));
        }
    }

    // 2. Prepare the parameters for ProcessEvent
    // The struct must match the memory layout of the UFunction's stack
    struct FConv_StringToName_Params {
        FString InString;  // Input
        FName ReturnValue; // Output (Return value)
    };

    FConv_StringToName_Params Params;
    Params.InString = fs; // 'fs' is the FString you constructed in your snippet

    if (ConvFunc) {
        // Static functions are called on the Class Default Object (CDO)
        UObject* CDO = ConvFunc->GetOuterUClass()->GetDefaultObject();
        CDO->ProcessEvent(ConvFunc, &Params);
    }

    FName result = Params.ReturnValue;

    // ... (Your existing cache storage logic) ...

    return result;
}
3. Key Technical Requirements

The CDO Context: Since Conv_StringToName is a static function in Blueprints, you must call ProcessEvent using the library's Class Default Object as the context.
Memory Layout: The Params struct must exactly match the function signature. In this case, it is an FString followed by an FName.
Elimination of Hard Dependencies: By using FindFunctionByName, you eliminate the need to link against Engine.lib for this specific conversion, which is useful if your SDK is injected into a running process.

Testing and Verification

PIE/Runtime: Ensure the UClass is loaded. If FindObject returns null, you may need to use your LoadObject utility first to ensure the Engine package is in memory.
Validation: Use UE_LOG or a debugger to ensure ConvFunc is not null after the first call.
Naming: Remember that FName comparisons and FindFunctionByName are case-insensitive. If your logic relies on specific elimination tracking or tag-based names, verify the resulting ComparisonIndex matches your expectations.
