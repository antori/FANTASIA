/**
 * @file Neo4jTransportQualityTests.cpp
 * @brief S-02 tester pass over US-031 (TLS verification), US-034 (two tooltips) and NFR-004.
 *
 * `Private/Security/Neo4jTlsVerificationTests.cpp` builds certificates and checks what the
 * component wrote on an `SSL_CTX`; this file asks what it leaves alone.
 *
 *   - `Neo4j.Hostname.*`: the name a certificate is checked against is `ExtractHostname`'s answer.
 *     Cases the security matrix does not carry: a bracketed IPv6 literal, a userinfo prefix, a
 *     scheme in capitals. Pinned as they are; F-058 is the IPv6 one.
 *   - `Tooltip.*` (US-034): the story's first criterion is about the metadata of `Config`; the
 *     coders' test reads `ApplyPreset`. These pin where the sentence is and what it says. F-059.
 *   - `Modularity.NoComponentIncludesAnother`: NFR-004's Measure is "no component module depends
 *     on another component module in the dependency view". The sprint touched M-neo4j and added
 *     OpenSSL to it, so the include graph is read and the cross-component edges are counted.
 */

#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "UObject/Class.h"

#include "Neo4jComponent.h"
#include "OpenAICompatibleComponent.h"
#include "RESTTTSComponent.h"
#include "../Security/Neo4jTestAccess.h"

namespace FantasiaQualityTransport
{
constexpr EAutomationTestFlags TestFlags =
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter;
} // namespace FantasiaQualityTransport

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FQualityNeo4jHostnameEdges,
	"FANTASIA.Quality.Neo4j.Hostname.EdgesOfTheName", FantasiaQualityTransport::TestFlags)
bool FQualityNeo4jHostnameEdges::RunTest(const FString& Parameters)
{
	// A scheme in capitals still means what it says: FString::StartsWith ignores case, so an
	// upper-case `BOLT+S://` is verified and `BOLT+SSC://` is the documented opt-out. Pinned both
	// ways so that neither a case-sensitive rewrite nor a verification gap opens quietly.
	{
		const FNeo4jTlsDecision Upper = FNeo4jTestAccess::DecideTransportSecurity(
			TEXT("BOLT+S://db.example.com:7687"), TEXT("db.example.com"), EBoltTransport::PlainTCP, false);
		TestTrue(TEXT("BOLT+S:// encrypts"), Upper.bEncrypt);
		TestTrue(TEXT("BOLT+S:// verifies the chain"), Upper.bVerifyChain);
		TestTrue(TEXT("BOLT+S:// verifies the name"), Upper.bVerifyHostname);

		const FNeo4jTlsDecision UpperSsc = FNeo4jTestAccess::DecideTransportSecurity(
			TEXT("BOLT+SSC://db.example.com:7687"), TEXT("db.example.com"), EBoltTransport::PlainTCP, false);
		TestTrue(TEXT("BOLT+SSC:// encrypts"), UpperSsc.bEncrypt);
		TestFalse(TEXT("BOLT+SSC:// verifies nothing, as lower case does"), UpperSsc.bVerifyChain || UpperSsc.bVerifyHostname);
	}

	// A bracketed IPv6 literal. ExtractHostname cuts at the first colon, which is inside the
	// brackets, so the host it returns is "[" and that is the name a certificate would be checked
	// against and the host the socket is asked to reach (F-058). It fails closed: no certificate
	// carries that name and no host answers to it, so no credential goes anywhere.
	TestEqual(TEXT("an IPv6 literal is cut at its first colon (F-058, pinned)"),
		FNeo4jTestAccess::ExtractHostname(TEXT("bolt+s://[::1]:7687")), FString(TEXT("[")));
	TestEqual(TEXT("and so is one without a scheme (F-058, pinned)"),
		FNeo4jTestAccess::ExtractHostname(TEXT("[2001:db8::1]")), FString(TEXT("[2001")));

	// Userinfo is not part of a Bolt URI, and is not stripped: it becomes part of the host.
	TestEqual(TEXT("a userinfo prefix stays in the host (pinned)"),
		FNeo4jTestAccess::ExtractHostname(TEXT("bolt+s://someone@db.example.com:7687")),
		FString(TEXT("someone@db.example.com")));

	// The routing case the security matrix pins with a bare host also holds with a port, a path
	// and a query attached to what the server named.
	TestEqual(TEXT("a path ends the host"),
		FNeo4jTestAccess::ExtractHostname(TEXT("neo4j+s://cluster.example.com/db/neo4j")),
		FString(TEXT("cluster.example.com")));
	TestEqual(TEXT("a query is not stripped when no path precedes it (pinned)"),
		FNeo4jTestAccess::ExtractHostname(TEXT("bolt+s://db.example.com?x=1")),
		FString(TEXT("db.example.com?x=1")));
	return true;
}

#if WITH_METADATA

namespace
{
FString ToolTipOf(const FProperty* Property) { return Property ? Property->GetMetaData(TEXT("ToolTip")) : FString(); }
FString ToolTipOf(const UField* Field) { return Field ? Field->GetMetaData(TEXT("ToolTip")) : FString(); }
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FQualityTooltipConfigAndPreset,
	"FANTASIA.Quality.Tooltip.ConfigAndPreset", FantasiaQualityTransport::TestFlags)
bool FQualityTooltipConfigAndPreset::RunTest(const FString& Parameters)
{
	// US-034's first criterion reads: given UOpenAICompatibleComponent::Config, when its metadata is
	// read, then it names `Apply Provider Preset` as a call that replaces the whole configuration
	// including the key, and says the key must be set again after it. Config's own metadata carries
	// the first two clauses, asserted below. Neither it nor ApplyPreset's says to set the key again
	// as advice: ApplyPreset says that doing so commits a literal, and both give the ini route
	// instead. That is a decision (ADR-0001), and the criterion's wording is pending a change
	// request, so the absence on Config is pinned under F-059.
	const UClass* Class = UOpenAICompatibleComponent::StaticClass();
	const FProperty* Config = Class->FindPropertyByName(TEXT("Config"));
	const UFunction* Preset = Class->FindFunctionByName(TEXT("ApplyPreset"));
	if (!TestNotNull(TEXT("Config is reflected"), Config) || !TestNotNull(TEXT("ApplyPreset is reflected"), Preset))
	{
		return false;
	}

	const FString OnConfig = ToolTipOf(Config);
	const FString OnPreset = ToolTipOf(Preset);

	TestTrue(TEXT("Config's tooltip tells the user the key belongs in DefaultGame.ini"), OnConfig.Contains(TEXT("DefaultGame.ini")));
	TestTrue(TEXT("Config's tooltip names the node that replaces the configuration"), OnConfig.Contains(TEXT("Apply Provider Preset")));
	TestTrue(TEXT("and says it replaces the whole configuration, the key included"),
		OnConfig.Contains(TEXT("replaces this whole configuration")) && OnConfig.Contains(TEXT("APIKey")));
	TestTrue(TEXT("and that a key read from DefaultGame.ini is discarded by the call"), OnConfig.Contains(TEXT("discarded by that call")));
	TestTrue(TEXT("and gives the ini route that keeps the key in configuration"), OnConfig.Contains(TEXT("skip that node")));
	TestFalse(TEXT("Config's tooltip does not tell the user to set the key again (F-059, pinned: the criterion's clause, not a defect)"),
		OnConfig.Contains(TEXT("again")));

	TestTrue(TEXT("ApplyPreset's tooltip names the node a user calls"), OnPreset.Contains(TEXT("Apply Provider Preset")));
	TestTrue(TEXT("and says the call replaces the whole configuration"), OnPreset.Contains(TEXT("wholesale")));
	TestTrue(TEXT("and that the key is set again afterwards"), OnPreset.Contains(TEXT("again")));
	TestTrue(TEXT("and that Custom leaves the configuration alone"), OnPreset.Contains(TEXT("Custom")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FQualityTooltipHeadersSection,
	"FANTASIA.Quality.Tooltip.HeadersSectionIsTheClassPath", FantasiaQualityTransport::TestFlags)
bool FQualityTooltipHeadersSection::RunTest(const FString& Parameters)
{
	// The tooltip names the ini section of the plain component and of the ACE subclass. Whether
	// a section name is right is a fact about the class path, so it is checked against the class
	// path: the base class here, and the ACE class when FANTASIAACE is loaded (the gate's host
	// project does not load it, so that half is reported rather than asserted).
	const FString ToolTip = ToolTipOf(URESTTTSComponent::StaticClass()->FindPropertyByName(TEXT("Headers")));
	const FString BasePath = FString::Printf(TEXT("[%s]"), *URESTTTSComponent::StaticClass()->GetPathName());
	TestTrue(TEXT("the tooltip names the base class's real ini section"), ToolTip.Contains(BasePath));
	AddInfo(FString::Printf(TEXT("the base section the tooltip must name: %s"), *BasePath));

	if (const UClass* Ace = FindObject<UClass>(nullptr, TEXT("/Script/FANTASIAACE.ACERESTTTSComponent")))
	{
		TestTrue(TEXT("the tooltip names the ACE class's real ini section"),
			ToolTip.Contains(FString::Printf(TEXT("[%s]"), *Ace->GetPathName())));
	}
	else
	{
		AddInfo(TEXT("FANTASIAACE is not loaded in this host, so the ACE section is not checked against a class path"));
		TestTrue(TEXT("the tooltip names the ACE section as the plugin's module and class would spell it"),
			ToolTip.Contains(TEXT("[/Script/FANTASIAACE.ACERESTTTSComponent]")));
	}
	return true;
}

#endif // WITH_METADATA

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FQualityModularityNoComponentIncludesAnother,
	"FANTASIA.Quality.Modularity.NoComponentIncludesAnother", FantasiaQualityTransport::TestFlags)
bool FQualityModularityNoComponentIncludesAnother::RunTest(const FString& Parameters)
{
	// File name to component family. M-core is every file not listed (FANTASIA.*, FANTASIATypes.h,
	// paramObject.h, the two gRPC guards); FANTASIATestActor is M-demo, which reaches every
	// component by design and whose coupling is already F-004.
	struct FFamily { const TCHAR* Name; TArray<FString> Prefixes; };
	const TArray<FFamily> Families = {
		{ TEXT("neo4j"),  { TEXT("Neo4jComponent"), TEXT("BoltProtocol"), TEXT("BoltPackStream") } },
		{ TEXT("llm"),    { TEXT("OpenAICompatibleComponent"), TEXT("LangGraphComponent") } },
		{ TEXT("tts"),    { TEXT("RESTTTSComponent"), TEXT("ElevenLabsTTSComponent"), TEXT("ElevenLabsProtocol") } },
		{ TEXT("asr"),    { TEXT("Whisper") } },
		{ TEXT("prolog"), { TEXT("SWIProlog") } },
	};
	auto FamilyOf = [&Families](const FString& FileName) -> int32
	{
		const FString Base = FPaths::GetBaseFilename(FileName);
		for (int32 i = 0; i < Families.Num(); ++i)
		{
			for (const FString& Prefix : Families[i].Prefixes)
			{
				if (Base.StartsWith(Prefix)) return i;
			}
		}
		return INDEX_NONE;
	};

	const FString Here = FPaths::GetPath(UTF8_TO_TCHAR(__FILE__));
	const FString Root = FPaths::ConvertRelativePathToFull(FPaths::Combine(Here, TEXT("../../../FANTASIA")));
	if (!IFileManager::Get().DirectoryExists(*Root))
	{
		AddInfo(FString::Printf(TEXT("Source root not reachable from this build, not scanned: %s"), *Root));
		return true;
	}

	TArray<FString> Files;
	for (const TCHAR* Dir : { TEXT("Public"), TEXT("Private") })
	{
		for (const TCHAR* Pattern : { TEXT("*.h"), TEXT("*.cpp") })
		{
			TArray<FString> Found;
			IFileManager::Get().FindFilesRecursive(Found, *FPaths::Combine(Root, Dir), Pattern, true, false);
			Files.Append(Found);
		}
	}
	if (!TestTrue(TEXT("the module's sources were found"), Files.Num() > 20))
	{
		return true;
	}

	int32 Edges = 0;
	int32 Cross = 0;
	for (const FString& File : Files)
	{
		const int32 From = FamilyOf(File);
		if (From == INDEX_NONE) continue;

		TArray<FString> Lines;
		FFileHelper::LoadFileToStringArray(Lines, *File);
		for (const FString& Line : Lines)
		{
			const FString Trimmed = Line.TrimStart();
			if (!Trimmed.StartsWith(TEXT("#include \""))) continue;
			int32 Close = INDEX_NONE;
			Trimmed.RightChop(10).FindChar(TEXT('"'), Close);
			if (Close == INDEX_NONE) continue;
			const int32 To = FamilyOf(Trimmed.Mid(10, Close));
			if (To == INDEX_NONE) continue;
			++Edges;
			if (To != From)
			{
				++Cross;
				AddError(FString::Printf(TEXT("%s (%s) includes %s (%s)"), *FPaths::GetCleanFilename(File),
					Families[From].Name, *Trimmed.Mid(10, Close), Families[To].Name));
			}
		}
	}
	AddInfo(FString::Printf(TEXT("%d include edges among the five component families, %d across families"), Edges, Cross));
	TestTrue(TEXT("the scan saw the includes inside each family"), Edges > 5);
	return true;
}
