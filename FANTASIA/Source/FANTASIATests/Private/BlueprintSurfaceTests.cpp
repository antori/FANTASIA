/**
 * @file BlueprintSurfaceTests.cpp
 * @brief Every component the FANTASIA module exposes is in the Blueprint palette, read off the
 *        engine's reflection (C-fantasia-types, C-build).
 *
 * The subject is the running editor's UClass registry, so nothing here reads the source tree and
 * nothing passes vacuously: a module that did not load yields no classes, which the count floor
 * turns into a failure. No class name is written in this file.
 */

#include "Components/ActorComponent.h"
#include "Misc/AutomationTest.h"
#include "UObject/Class.h"
#include "UObject/UObjectHash.h"
#include "UObject/UObjectIterator.h"

namespace FantasiaBlueprintSurface
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter;

	/** The package that holds every reflected class of the FANTASIA module. */
	const TCHAR* const ModulePackage = TEXT("/Script/FANTASIA");

	/**
	 * Lower bound on the components found in the module. Seven were observed when this test was
	 * written, and this floor sits one below that, so that an added component
	 * does not break the test; zero means the module did not load.
	 */
	constexpr int32 MinComponents = 6;

	/** Components reflected in Package, excluding abstract, deprecated and superseded classes. */
	TArray<const UClass*> FindComponents(const TCHAR* Package)
	{
		TArray<const UClass*> Found;
		for (TObjectIterator<UClass> It; It; ++It)
		{
			const UClass* Class = *It;
			const UPackage* Outer = Class->GetOutermost();
			if (Outer && Outer->GetName() == Package && Class->IsChildOf(UActorComponent::StaticClass()) &&
				!Class->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists))
			{
				Found.Add(Class);
			}
		}
		return Found;
	}

	/** True when Class or an ancestor declares the key (`meta = (BlueprintSpawnableComponent)` has an empty value, so a boolean read would say false). */
	bool HasMetaInHierarchy(const UClass* Class, const FName Key)
	{
		for (const UClass* Walk = Class; Walk; Walk = Walk->GetSuperClass())
		{
			if (Walk->HasMetaData(Key))
			{
				return true;
			}
		}
		return false;
	}

	/** Reports the scan as a test result: a floor on the count, then the palette specifier of each class. */
	void CheckSurface(FAutomationTestBase& Test, const TCHAR* Package)
	{
		const TArray<const UClass*> Components = FindComponents(Package);
		Test.TestTrue(*FString::Printf(TEXT("%s reflects at least %d components (found %d); fewer means the module did not load"), Package, MinComponents, Components.Num()),
			Components.Num() >= MinComponents);

		for (const UClass* Class : Components)
		{
			Test.TestTrue(*FString::Printf(TEXT("%s carries BlueprintSpawnableComponent"), *Class->GetName()),
				HasMetaInHierarchy(Class, TEXT("BlueprintSpawnableComponent")));
		}
	}
}

/** US-019: every non-abstract component in /Script/FANTASIA is Blueprint-spawnable. */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBlueprintSurfaceComponents, "FANTASIA.Blueprint.Surface.ComponentsAreSpawnable", FantasiaBlueprintSurface::Flags)

bool FBlueprintSurfaceComponents::RunTest(const FString& Parameters)
{
	FantasiaBlueprintSurface::CheckSurface(*this, FantasiaBlueprintSurface::ModulePackage);
	return true;
}
