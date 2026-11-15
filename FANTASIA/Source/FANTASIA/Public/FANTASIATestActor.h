/**
 * @file FANTASIATestActor.h
 * @brief Blueprint-friendly test actor that exercises every FANTASIA component.
 *
 * Drop this actor into any level, put the credentials it needs in the host project's
 * DefaultGame.ini (typing them into the Details panel saves them into the level or
 * Blueprint asset), and call RunAllTests (or individual Test* functions) from Blueprint.
 * Results appear as on-screen messages and are broadcast via OnTestCompleted.
 */

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FANTASIATypes.h"
#include "OpenAICompatibleComponent.h"
#include "FANTASIATestActor.generated.h"

class UOpenAICompatibleComponent;
class UNeo4jComponent;
class USWIPrologComponent;
class UWhisperCaptureComponent;
class URESTTTSComponent;
class UElevenLabsTTSComponent;
class ULangGraphComponent;
class UBayesianNetwork;
class UInfluenceDiag;
class ULibSVM;
class UKernelDensityEstimator;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnFANTASIATestCompleted, FString, TestName, bool, bSuccess, FString, Message);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnFANTASIAAllTestsCompleted, int32, Passed, int32, Failed);

UCLASS(Blueprintable, Config = Game, meta = (DisplayName = "FANTASIA Test Actor"))
class FANTASIA_API AFANTASIATestActor : public AActor
{
	GENERATED_BODY()

public:
	AFANTASIATestActor();

	// ── Events ──────────────────────────────────────────────────────────

	/** Fired after each individual test completes. */
	UPROPERTY(BlueprintAssignable, Category = "FANTASIA Tests|Events")
	FOnFANTASIATestCompleted OnTestCompleted;

	/** Fired after RunAllTests finishes all tests. */
	UPROPERTY(BlueprintAssignable, Category = "FANTASIA Tests|Events")
	FOnFANTASIAAllTestsCompleted OnAllTestsCompleted;

	// ── Components (hidden from Details panel to avoid duplicate configuration) ──
	// Use the "FANTASIA Tests" category properties below to configure tests.
	// The components themselves are still accessible from Blueprint via their accessors.

	UPROPERTY(BlueprintReadOnly, Category = "FANTASIA Tests|Components")
	UOpenAICompatibleComponent* LLMComponent;

	UPROPERTY(BlueprintReadOnly, Category = "FANTASIA Tests|Components")
	UNeo4jComponent* Neo4jComponent;

	UPROPERTY(BlueprintReadOnly, Category = "FANTASIA Tests|Components")
	USWIPrologComponent* PrologComponent;

	UPROPERTY(BlueprintReadOnly, Category = "FANTASIA Tests|Components")
	UWhisperCaptureComponent* WhisperCapture;

	UPROPERTY(BlueprintReadOnly, Category = "FANTASIA Tests|Components")
	URESTTTSComponent* TTSComponent;

	UPROPERTY(BlueprintReadOnly, Category = "FANTASIA Tests|Components")
	UElevenLabsTTSComponent* ElevenLabsComponent;

	UPROPERTY(BlueprintReadOnly, Category = "FANTASIA Tests|Components")
	ULangGraphComponent* LangGraphComponent;

	// ── Online Test Configuration ───────────────────────────────────────

	/** Which LLM provider preset to apply before running the test. Select Custom to use LLMCustomURL. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FANTASIA Tests|LLM")
	ELLMProvider LLMProvider = ELLMProvider::OpenAI;

	/** Full chat-completions endpoint URL. Only used when LLMProvider == Custom. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FANTASIA Tests|LLM",
		meta = (EditCondition = "LLMProvider == ELLMProvider::Custom"))
	FString LLMCustomURL;

	/** Whether the custom endpoint requires an Authorization: Bearer header. Only used when LLMProvider == Custom. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FANTASIA Tests|LLM",
		meta = (EditCondition = "LLMProvider == ELLMProvider::Custom"))
	bool bLLMCustomRequiresAuth = true;

	/**
	 * @brief API key for the selected provider (leave blank for providers that don't
	 *        require auth, e.g. Ollama).
	 *
	 * Read from the host project's `DefaultGame.ini` and should be set there. A key
	 * typed into the Details panel is still saved into the Blueprint or level asset:
	 * this specifier adds an ini source, it does not make the property transient.
	 *
	 * @note  An asset that already holds a key still holds it. Clear the value in the
	 *        asset and re-save it for the key to leave the asset.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FANTASIA Tests|LLM", Config)
	FString LLMAPIKey;

	/** Model to use. Leave blank to keep the preset default. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FANTASIA Tests|LLM")
	FString LLMModel;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FANTASIA Tests|Neo4j")
	FString Neo4jEndpoint = TEXT("localhost");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FANTASIA Tests|Neo4j")
	int32 Neo4jPort = 7687;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FANTASIA Tests|Neo4j")
	FString Neo4jUser = TEXT("neo4j");

	/**
	 * @brief Password for the Neo4j user above.
	 *
	 * Read from the host project's `DefaultGame.ini` and should be set there. A password
	 * typed into the Details panel is still saved into the Blueprint or level asset:
	 * this specifier adds an ini source, it does not make the property transient.
	 *
	 * @note  An asset that already holds a password still holds it. Clear the value in
	 *        the asset and re-save it for the password to leave the asset.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FANTASIA Tests|Neo4j", Config)
	FString Neo4jPassword;

	/** Database to query. Leave blank to use the server's default ('neo4j' on most installations). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FANTASIA Tests|Neo4j")
	FString Neo4jDatabase;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FANTASIA Tests|TTS")
	FString TTSEndpoint;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FANTASIA Tests|TTS")
	FString TTSBody;

	/**
	 * @brief ElevenLabs API key. Leave blank to skip the ElevenLabs test.
	 *
	 * Read from the host project's `DefaultGame.ini` and should be set there. A key
	 * typed into the Details panel is still saved into the Blueprint or level asset:
	 * this specifier adds an ini source, it does not make the property transient.
	 *
	 * @note  An asset that already holds a key still holds it. Clear the value in the
	 *        asset and re-save it for the key to leave the asset.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FANTASIA Tests|ElevenLabs", Config)
	FString ElevenLabsKey;

	/** ElevenLabs voice ID to use (e.g. "21m00Tcm4TlvDq8ikWAM" for Rachel). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FANTASIA Tests|ElevenLabs")
	FString ElevenLabsVoiceID = TEXT("21m00Tcm4TlvDq8ikWAM");

	/** ElevenLabs model ID. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FANTASIA Tests|ElevenLabs")
	FString ElevenLabsModelID = TEXT("eleven_multilingual_v2");

	/**
	 * Whisper model to load. Can be either:
	 *   - An absolute path to a .bin file (e.g. "D:/Models/ggml-base.bin"), OR
	 *   - A bare filename (e.g. "ggml-base.bin") — the subsystem will search
	 *     Plugins/FANTASIA/Resources, Plugins/FANTASIA/Content/Models, and
	 *     Content/WhisperModels in that order.
	 * Leave blank to skip the Whisper model test.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FANTASIA Tests|Whisper")
	FString WhisperModelPath = TEXT("ggml-large-v3-turbo.bin");

	/** LangGraph server hostname (e.g. "http://localhost"). Leave blank to skip the test. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FANTASIA Tests|LangGraph")
	FString LangGraphEndpoint;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FANTASIA Tests|LangGraph")
	int32 LangGraphPort = 2024;

	/**
	 * Thread ID to create and delete during the LangGraph test.
	 * LangGraph Platform requires this to be a valid UUID (e.g.
	 * "550e8400-e29b-41d4-a716-446655440000"). If left blank or not a valid
	 * UUID, the test generates a fresh UUID at runtime.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FANTASIA Tests|LangGraph")
	FString LangGraphTestThreadID;

	// ── Test Functions (all BlueprintCallable) ──────────────────────────

	/** Run all tests. Offline tests always run; online tests are skipped if credentials are empty. */
	UFUNCTION(BlueprintCallable, Category = "FANTASIA Tests")
	void RunAllTests();

	// Offline tests — no credentials needed

	/** Build a Bayesian Network, run inference, verify posteriors. */
	UFUNCTION(BlueprintCallable, Category = "FANTASIA Tests|Offline")
	void TestBayesianNetwork();

	/** Build an Influence Diagram, run inference, verify MEU. */
	UFUNCTION(BlueprintCallable, Category = "FANTASIA Tests|Offline")
	void TestInfluenceDiag();

	/** Train SVM on separable data, predict, verify results. */
	UFUNCTION(BlueprintCallable, Category = "FANTASIA Tests|Offline")
	void TestSVM();

	/** Set KDE data, evaluate density, verify PDF normalization. */
	UFUNCTION(BlueprintCallable, Category = "FANTASIA Tests|Offline")
	void TestKDE();

	/** Create Prolog terms (atoms, compounds, lists), verify properties. */
	UFUNCTION(BlueprintCallable, Category = "FANTASIA Tests|Offline")
	void TestPrologHelpers();

	/** Exercise Whisper formatting (SRT/VTT) and model name parsing. */
	UFUNCTION(BlueprintCallable, Category = "FANTASIA Tests|Offline")
	void TestWhisperFormatting();

	/** Enumerate audio input devices; verifies the audio subsystem is functional. */
	UFUNCTION(BlueprintCallable, Category = "FANTASIA Tests|Offline")
	void TestWhisperCapture();

	// Online tests — need credentials / services

	/** Send a chat completion to the LLM, verify response arrives. */
	UFUNCTION(BlueprintCallable, Category = "FANTASIA Tests|Online")
	void TestLLM();

	/** Submit a RETURN 1 AS n query to Neo4j, verify response. */
	UFUNCTION(BlueprintCallable, Category = "FANTASIA Tests|Online")
	void TestNeo4j();

	/** Consult Prolog code and query for solutions. */
	UFUNCTION(BlueprintCallable, Category = "FANTASIA Tests|Online")
	void TestProlog();

	/** Load a Whisper model and check status. */
	UFUNCTION(BlueprintCallable, Category = "FANTASIA Tests|Online")
	void TestWhisperModel();

	/** Synthesize text via the generic REST TTS endpoint. */
	UFUNCTION(BlueprintCallable, Category = "FANTASIA Tests|Online")
	void TestTTS();

	/** Synthesize text via the ElevenLabs TTS API. */
	UFUNCTION(BlueprintCallable, Category = "FANTASIA Tests|Online")
	void TestElevenLabsTTS();

	/** Create a LangGraph thread against the configured server, verify the ID comes back. */
	UFUNCTION(BlueprintCallable, Category = "FANTASIA Tests|Online")
	void TestLangGraph();

private:
	void ReportTest(const FString& Name, bool bSuccess, const FString& Msg);

	// Callbacks for online test delegates (must be UFUNCTION for dynamic binding)
	UFUNCTION()
	void OnLLMResponse(FString Response, GPTRoleType gptRole);

	UFUNCTION()
	void OnNeo4jResponse(FNeo4jResponse Response);

	UFUNCTION()
	void OnPrologAllSolutions(const TArray<USWIPrologSolution*>& Solutions);

	UFUNCTION()
	void OnWhisperModelLoaded(bool bSuccess);

	UFUNCTION()
	void OnTTSSynthesisReady(FString Id);

	UFUNCTION()
	void OnElevenLabsSynthesisReady(FString Id);

	UFUNCTION()
	void OnLangGraphThreadCreated(FString ThreadID);

	UPROPERTY()
	UBayesianNetwork* BayesianNet = nullptr;

	UPROPERTY()
	UInfluenceDiag* InfluenceDiagram = nullptr;

	UPROPERTY()
	ULibSVM* SVMClassifier = nullptr;

	UPROPERTY()
	UKernelDensityEstimator* KDE = nullptr;

	int32 PassCount = 0;
	int32 FailCount = 0;
	int32 TotalExpected = 0;
};
