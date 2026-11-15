/**
 * @file OpenAICompatibleComponent.h
 * @brief Unified LLM chat-completion component supporting any OpenAI-compatible API.
 */

#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include <Json.h>
#include "Runtime/Online/HTTP/Public/Http.h"
#include "FANTASIATypes.h"
#include "OpenAICompatibleComponent.generated.h"

/**
 * @brief Enumerates well-known LLM providers with built-in presets.
 *
 * Select a value to auto-populate URL, model, timeout, and auth settings
 * via UOpenAICompatibleComponent::ApplyPreset. Use Custom for unlisted providers.
 */
UENUM(BlueprintType)
enum class ELLMProvider : uint8
{
	Custom		= 0 UMETA(DisplayName = "Custom"),
	OpenAI		= 1 UMETA(DisplayName = "OpenAI"),
	Groq		= 2 UMETA(DisplayName = "Groq"),
	Ollama		= 3 UMETA(DisplayName = "Ollama"),
	LMStudio	= 4 UMETA(DisplayName = "LM Studio"),
	vLLM		= 5 UMETA(DisplayName = "vLLM"),
};

/**
 * @brief Configuration struct for an OpenAI-compatible chat-completion endpoint.
 *
 * Holds the endpoint URL, API key, default model name, timeout, and whether
 * Bearer-token authentication is required. Exposed to Blueprint and INI config.
 */
USTRUCT(BlueprintType)
struct FOpenAICompatibleConfig
{
	GENERATED_BODY()

	/** @brief Full URL of the chat-completions endpoint. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LLM", Config)
	FString URL = TEXT("https://api.openai.com/v1/chat/completions");

	/** @brief Bearer token sent in the Authorization header (leave empty for local servers). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LLM", Config)
	FString APIKey;

	/** @brief Model identifier sent in the request when no override is specified. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LLM", Config)
	FString DefaultModel = TEXT("gpt-4-turbo-preview");

	/** @brief HTTP request timeout in seconds (clamped 1-300). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LLM", Config, meta = (ClampMin = "1", ClampMax = "300"))
	int32 TimeoutSeconds = 60;

	/** @brief When true, the APIKey is sent as a Bearer token; set false for local servers. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LLM", Config)
	bool bRequiresAuth = true;
};

/**
 * @brief Actor component that sends chat-completion requests to any OpenAI-compatible API.
 *
 * Replaces the provider-specific components (UOpenAIComponent, UGroqComponent,
 * UOllamaComponent) with a single configurable component. Supports both
 * non-streaming and streaming (SSE) responses, and provides sentence-level
 * buffering useful for text-to-speech pipelines.
 *
 * @see FOpenAICompatibleConfig, ELLMProvider
 */
UCLASS(ClassGroup = (LLM), meta = (BlueprintSpawnableComponent), config = Game)
class FANTASIA_API UOpenAICompatibleComponent : public UActorComponent
{
	GENERATED_BODY()

	// ── Public interface ────────────────────────────────────────────────

public:
	/** @brief Sets default values for this component's properties. */
	UOpenAICompatibleComponent();

	/**
	 * @brief Active endpoint and auth configuration.
	 *
	 * The API key is read from the host project's `DefaultGame.ini` and should be set
	 * there. A key typed into the Details panel is still saved into the Blueprint or
	 * level asset: this specifier adds an ini source, it does not make the property
	 * transient.
	 *
	 * @note  An asset that already holds a key still holds it. Clear the value in the
	 *        asset and re-save it for the key to leave the asset.
	 *
	 * @note  The Blueprint node "Apply Provider Preset" replaces this whole configuration,
	 *        APIKey included, and no preset carries a key — so a key read from
	 *        `DefaultGame.ini` is discarded by that call. To keep the key in configuration,
	 *        skip that node and set the endpoint fields in the same `DefaultGame.ini` entry
	 *        as the key.
	 */
	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "LLM|Configuration")
	FOpenAICompatibleConfig Config;

	// ── Events ──────────────────────────────────────────────────────────

	/** @brief Broadcast with the full response content when streaming is disabled. */
	UPROPERTY(BlueprintAssignable, BlueprintCallable, Category = "LLM|Events")
	FIncomingGPTResponseEvent IncomingGPTResponse;

	/** @brief Broadcast with each token chunk during streaming; the final call has bEndStream=true. */
	UPROPERTY(BlueprintAssignable, BlueprintCallable, Category = "LLM|Events")
	FIncomingGPTStreamResponseEvent IncomingGPTStreamResponse;

	/**
	 * @brief Broadcast with complete sentences accumulated during streaming.
	 *
	 * Useful for driving text-to-speech: tokens are buffered until sentence-ending
	 * punctuation is detected, then flushed as a single sentence string.
	 */
	UPROPERTY(BlueprintAssignable, BlueprintCallable, Category = "LLM|Events")
	FIncomingGPTStreamResponseEvent IncomingSentenceResponse;

	// ── Blueprint-callable methods ──────────────────────────────────────

	/**
	 * @brief Sends a chat-completion request using the current Config.
	 * @param messages      Ordered conversation turns to send.
	 * @param bStream       If true, tokens arrive incrementally via IncomingGPTStreamResponse.
	 * @param ModelOverride Optional model name that overrides Config.DefaultModel for this call.
	 */
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Get Chat Completion", AutoCreateRefTerm = "messages"), Category = "LLM")
	void GetChatCompletion(const TArray<FChatTurn>& messages, bool bStream = false, const FString& ModelOverride = TEXT(""));

	/**
	 * @brief Overwrites Config with the built-in preset for the given provider.
	 *
	 * The Blueprint node is "Apply Provider Preset". It replaces Config wholesale,
	 * APIKey included, and no preset carries a key: a key read from `DefaultGame.ini`
	 * is discarded by the call, and the next Get Chat Completion logs "API key is
	 * required but empty" while bRequiresAuth is true. Provider Custom is the
	 * exception and leaves Config untouched.
	 *
	 * Setting the key again after the preset makes it a literal in a Blueprint graph,
	 * which is committed. To keep the key in configuration, skip this node and set the
	 * endpoint fields in the same `DefaultGame.ini` entry as the key; the values are
	 * the ones Make OpenAI Config and its siblings return.
	 *
	 * @param Provider The LLM provider whose defaults should be applied.
	 */
	UFUNCTION(BlueprintCallable, Category = "LLM|Configuration", meta = (DisplayName = "Apply Provider Preset"))
	void ApplyPreset(ELLMProvider Provider);

	// ── Static preset factories ─────────────────────────────────────────

	/** @brief Returns a config preset for the OpenAI API. */
	UFUNCTION(BlueprintPure, Category = "LLM|Presets", meta = (DisplayName = "Make OpenAI Config"))
	static FOpenAICompatibleConfig MakeOpenAIConfig();

	/** @brief Returns a config preset for the Groq API. */
	UFUNCTION(BlueprintPure, Category = "LLM|Presets", meta = (DisplayName = "Make Groq Config"))
	static FOpenAICompatibleConfig MakeGroqConfig();

	/** @brief Returns a config preset for a local Ollama server. */
	UFUNCTION(BlueprintPure, Category = "LLM|Presets", meta = (DisplayName = "Make Ollama Config"))
	static FOpenAICompatibleConfig MakeOllamaConfig();

	/** @brief Returns a config preset for a local LM Studio server. */
	UFUNCTION(BlueprintPure, Category = "LLM|Presets", meta = (DisplayName = "Make LM Studio Config"))
	static FOpenAICompatibleConfig MakeLMStudioConfig();

	/** @brief Returns a config preset for a local vLLM server. */
	UFUNCTION(BlueprintPure, Category = "LLM|Presets", meta = (DisplayName = "Make vLLM Config"))
	static FOpenAICompatibleConfig MakeVLLMConfig();

	// ── Private implementation ──────────────────────────────────────────

private:
	/** @brief Byte offset into the SSE response body up to which parsing has completed. */
	int32 StreamParseOffset = 0;

	/** @brief Role carried forward across streaming delta chunks. */
	GPTRoleType StreamCurrentRole = GPTRoleType::ASSISTANT;

	/** @brief Accumulates tokens until a sentence boundary is detected. */
	FString SentenceBuffer;

	/** @brief True while a streaming request is in flight; prevents OnRequestCompleted from broadcasting the full response. */
	bool bStreamingActive = false;

	/** @brief Callback for completed HTTP responses (error handling + non-streaming results). */
	void OnRequestCompleted(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);

	/** @brief Callback invoked incrementally as streaming SSE data arrives. */
	void OnPartialResponseReceived(FHttpRequestPtr Request, uint64 BytesSent, uint64 BytesReceived);

	/**
	 * @brief Broadcasts any remaining text in SentenceBuffer and clears it.
	 * @param bEndStream If true, signals the end of the stream to listeners.
	 */
	void FlushSentenceBuffer(bool bEndStream);

	/**
	 * @brief Converts a role string (e.g. "assistant") to the GPTRoleType enum.
	 * @param RoleString The role string from the API JSON.
	 * @return Corresponding GPTRoleType value; defaults to ASSISTANT.
	 */
	static GPTRoleType ParseRole(const FString& RoleString);

	/**
	 * @brief Returns the lowercase string representation of a GPTRoleType.
	 * @param Role The role enum value.
	 * @return Reference to a static FString for the role name.
	 */
	static const FString& RoleToString(GPTRoleType Role);
};
