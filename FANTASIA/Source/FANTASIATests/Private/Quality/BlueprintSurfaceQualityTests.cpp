/**
 * @file BlueprintSurfaceQualityTests.cpp
 * @brief Quality checks on the Blueprint palette claim of US-019, read against the predicate the
 *        engine itself applies, and a control for the premise of the count floor in
 *        BlueprintSurfaceTests.cpp.
 *
 * The engine decides palette membership in FKismetEditorUtilities::IsClassABlueprintSpawnableComponent:
 * the class is not abstract, is an ActorComponent, and carries the BlueprintSpawnableComponent
 * metadata ITSELF. UClass::HasMetaData does not look at parents, so a component that only inherits
 * the specifier from a FANTASIA parent is absent from the palette. BlueprintSurfaceTests.cpp walks
 * the hierarchy, which is looser than the engine, so this file asks the engine's question directly.
 * No class name is written here.
 */

#include "Components/ActorComponent.h"
#include "Misc/AutomationTest.h"
#include "UObject/Class.h"
#include "UObject/UObjectHash.h"
#include "UObject/UObjectIterator.h"

namespace FantasiaQualitySurface
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter;

	const TCHAR* const ModulePackage = TEXT("/Script/FANTASIA");
	const FName SpawnableKey(TEXT("BlueprintSpawnableComponent"));

	/** Non-abstract, non-deprecated, current components reflected in Package. */
	TArray<const UClass*> FindComponents(const TCHAR* Package)
	{
		TArray<const UClass*> Found;
		for (TObjectIterator<UClass> It; It; ++It)
		{
			const UClass* Class = *It;
			const UPackage* Outer = Class->GetOutermost();
			if (Outer && Outer->GetName() == Package && Class->IsChildOf(UActorComponent::StaticClass())
				&& !Class->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists))
			{
				Found.Add(Class);
			}
		}
		return Found;
	}

	/** True when an ancestor, and not the class itself, carries the key. */
	bool InheritsKeyOnly(const UClass* Class)
	{
		if (Class->HasMetaData(SpawnableKey))
		{
			return false;
		}
		for (const UClass* Walk = Class->GetSuperClass(); Walk; Walk = Walk->GetSuperClass())
		{
			if (Walk->HasMetaData(SpawnableKey))
			{
				return true;
			}
		}
		return false;
	}
}

// ============================================================================
// US-019: the class itself carries the specifier, which is what the palette reads
// ============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FQualityPaletteEnginePredicate, "FANTASIA.Quality.Blueprint.Palette.ClassCarriesSpecifierItself", FantasiaQualitySurface::Flags)
bool FQualityPaletteEnginePredicate::RunTest(const FString& Parameters)
{
	const TArray<const UClass*> Components = FantasiaQualitySurface::FindComponents(FantasiaQualitySurface::ModulePackage);
	TestTrue(*FString::Printf(TEXT("the scan finds components (found %d); none means the module did not load"), Components.Num()), Components.Num() >= 6);

	int32 Direct = 0;
	for (const UClass* Class : Components)
	{
		const bool bDirect = Class->HasMetaData(FantasiaQualitySurface::SpawnableKey);
		Direct += bDirect ? 1 : 0;
		TestTrue(*FString::Printf(TEXT("%s carries BlueprintSpawnableComponent on itself (an inherited specifier does not put it in the palette)"), *Class->GetName()), bDirect);
		TestFalse(*FString::Printf(TEXT("%s is not palette-visible only through a parent"), *Class->GetName()), FantasiaQualitySurface::InheritsKeyOnly(Class));
	}
	AddInfo(FString::Printf(TEXT("%d of %d components carry the specifier directly."), Direct, Components.Num()));
	return true;
}

// ============================================================================
// Control: the premise of the count floor in BlueprintSurfaceTests.cpp
// ============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FQualityPaletteFloorPremise, "FANTASIA.Quality.Blueprint.Palette.AbsentPackageYieldsNoComponents", FantasiaQualitySurface::Flags)
bool FQualityPaletteFloorPremise::RunTest(const FString& Parameters)
{
	// A scan of a package that does not exist finds nothing, so a floor above zero turns "the
	// module did not load" into a failure rather than an empty pass.
	TestEqual(TEXT("a package that does not exist reflects no components"),
		FantasiaQualitySurface::FindComponents(TEXT("/Script/FANTASIA_DoesNotExist")).Num(), 0);
	TestEqual(TEXT("an empty package name reflects no components"), FantasiaQualitySurface::FindComponents(TEXT("")).Num(), 0);
	TestTrue(TEXT("the real package reflects components, so the control is not satisfied by an empty registry"),
		FantasiaQualitySurface::FindComponents(FantasiaQualitySurface::ModulePackage).Num() > 0);
	return true;
}
