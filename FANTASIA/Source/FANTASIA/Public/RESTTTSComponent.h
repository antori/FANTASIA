/**
 * @file RESTTTSComponent.h
 * @brief Generic REST-based text-to-speech actor component with streaming
 *        and a protected playback-buffer surface for ACE subclasses.
 *
 * This is the core, ACE-agnostic class. The NVIDIA Audio2Face integration
 * lives in the optional FANTASIAACE plugin's UACERESTTTSComponent subclass.
 * The buffer scaffolding here (per-call FAudioBuffer objects, submission-
 * order PlaybackOrder queue) is shaped so the ACE consumer task in the
 * subclass can drain it without core having to know anything about ACE.
 */

#pragma once

#include "FANTASIA.h"
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "FANTASIATypes.h"
#include "Http.h"
#include "Async/Future.h"
#include <atomic>
#include "Runtime/Engine/Classes/Sound/SoundWaveProcedural.h"

#include "RESTTTSComponent.generated.h"

/**
 * @brief Describes a single HTTP request for TTS synthesis.
 *
 * Built by URESTTTSComponent::BuildSynthesisRequest (or subclass overrides)
 * and consumed by IssueHttpRequest.
 */
struct FTTSSynthesisRequest
{
	FString URL;                        /**< Target endpoint URL. */
	TMap<FString, FString> Headers;     /**< HTTP headers to include. */
	FString Body;                       /**< Request body. Empty triggers a GET; non-empty triggers a POST. */
	FString ID;                         /**< Caller-assigned request identifier. */
	FString OriginalText;               /**< Original text submitted for synthesis. */
	bool bStreaming = false;            /**< If true, process audio chunks as they arrive. */
};

/** Number of float samples batched before each Audio2Face streaming call. */
static constexpr int32 A2F_STREAMING_BATCH_SIZE = 100;

/**
 * @brief Per-request audio buffer used for serial playback.
 *
 * One buffer is created per TTSSynthesize call and lives in a per-caller
 * list (BuffersByCaller) until both synthesis is complete and the
 * consumer has drained it. HTTP callbacks capture the buffer pointer
 * directly, so there is no by-id lookup that could clobber an in-flight
 * slot when the same caller id is reused (e.g. BP always passing
 * "Answer" for every sentence in a turn).
 *
 * Synthesis runs in parallel across many calls; playback is serial,
 * driven by PlaybackOrder (FIFO of buffer pointers in submission order).
 */
struct FAudioBuffer
{
	TArray<float> Samples;                          /**< Mono float samples at 16 kHz, in time order. */
	int32 ConsumedSamples = 0;                      /**< Count already pushed to A2F. */
	std::atomic<bool> bSynthesisComplete{false};    /**< Set once HTTP response is fully processed. */
	FCriticalSection Mutex;                         /**< Protects Samples, ConsumedSamples, Words, Characters. */

	/** Per-word timing for this utterance (provider alignment), captured by a
	 *  provider stream decoder and snapshotted into the stored FTTSData. Not
	 *  yet consumed for events -- kept for when ACE delivers native segment
	 *  animation events. */
	TArray<FTTSSegmentTiming> Words;

	/** Per-character timing for this utterance (provider alignment). */
	TArray<FTTSSegmentTiming> Characters;
};

/** Submission-order entry: caller id (for delegate broadcasts) + raw
 *  pointer to the audio buffer. The buffer's lifetime is owned by the
 *  TUniquePtr inside BuffersByCaller; we only remove it (and free it)
 *  AFTER bSynthesisComplete is observed AND the consumer has drained
 *  it, so the raw pointer is stable for as long as the consumer or HTTP
 *  callback could be holding it. */
struct FPlaybackOrderEntry
{
	FString CallerId;
	FAudioBuffer* Buffer;
};

/**
 * @brief Per-request decoder turning raw streamed HTTP body bytes into PCM
 *        float samples (appended to the per-call buffer) and optional
 *        word/character timing.
 *
 * One decoder is created per streaming request via
 * URESTTTSComponent::CreateStreamDecoder and owns any partial-byte /
 * partial-line buffering across Feed calls, so the core can hand it every
 * newly-arrived byte without alignment concerns. The base PCM16 decoder
 * reproduces the raw-PCM streaming behaviour; provider subclasses (e.g.
 * ElevenLabs with timestamps) return a decoder that parses a JSON envelope.
 */
struct FANTASIA_API FTTSStreamDecoder
{
	virtual ~FTTSStreamDecoder() = default;

	/** Decode newly-arrived raw bytes and append the resulting samples (and
	 *  any timing) to Target. Must lock Target->Mutex around mutations. */
	virtual void Feed(TArrayView<const uint8> NewBytes, FAudioBuffer* Target) = 0;

	/** Flush any buffered tail once the response is complete. */
	virtual void Finish(FAudioBuffer* Target) {}
};

/** Default decoder: interprets the byte stream as little-endian 16-bit mono
 *  PCM, carrying a trailing odd byte between Feed calls. Reproduces the
 *  pre-timing streaming behaviour for generic REST providers. */
struct FANTASIA_API FPcm16StreamDecoder final : public FTTSStreamDecoder
{
	virtual void Feed(TArrayView<const uint8> NewBytes, FAudioBuffer* Target) override;

private:
	uint8 PendingByte = 0;
	bool bHasPendingByte = false;
};

/** Broadcast when a request's audio has finished playing (drained by a
 *  subclass-installed consumer such as ACE Audio2Face). For the bare
 *  core component (no subclass / no consumer), this fires immediately
 *  after synthesis completes. SEPARATE from SynthesisReady, which fires
 *  when the HTTP response arrives. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FPlaybackCompleteEvent, FString, Id);

/**
 * @brief Generic REST-based text-to-speech actor component.
 *
 * Sends HTTP GET or POST requests to any TTS endpoint, receives PCM
 * audio, auto-detects WAV headers, resamples to 16 kHz, and exposes
 * the audio via TTSGetSound / TTSGetRawSound. The component allocates
 * a per-call FAudioBuffer and stages each call in a global
 * submission-order queue (PlaybackOrder) so subclasses can install a
 * consumer task that drains buffers in order.
 *
 * Subclasses (e.g. UElevenLabsTTSComponent) override BuildSynthesisRequest
 * and ProcessResponse to handle provider-specific protocols.
 *
 * The optional FANTASIAACE plugin's UACERESTTTSComponent subclass
 * installs an Audio2Face consumer that drains the buffer scaffolding
 * and animates a strongly-typed UACEAudioCurveSourceComponent.
 *
 * @see UElevenLabsTTSComponent, FTTSSynthesisRequest
 */
UCLASS(meta = (BlueprintSpawnableComponent), Config = Game)
class FANTASIA_API URESTTTSComponent : public UActorComponent
{
	GENERATED_BODY()

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/**
	 * @brief Build the HTTP request for a synthesis call.
	 *
	 * Base implementation uses URL, Headers, and BodyTemplate properties.
	 * Subclasses override to build provider-specific requests.
	 *
	 * @param Text  Plain text to synthesize.
	 * @param ID    Caller-assigned request identifier.
	 * @return Populated synthesis request descriptor.
	 */
	virtual FTTSSynthesisRequest BuildSynthesisRequest(const FString& Text, const FString& ID);

	/**
	 * @brief Extract additional data (word timing, lipsync, notifies) from the raw HTTP response.
	 *
	 * Called in HandleResult after WAV detection and resampling.
	 * Base implementation is a no-op.
	 *
	 * @param RawResponse  Raw HTTP response bytes.
	 * @param OutResult    TTS data to populate with extracted metadata.
	 */
	virtual void ProcessResponse(const TArray<uint8>& RawResponse, FTTSData& OutResult);

	/**
	 * @brief Hook called whenever new audio samples are pushed into a
	 *        per-call FAudioBuffer.
	 *
	 * Called from HandleResult (after the final PCM landed in Target) and
	 * from HandlePartialResult (after each streaming chunk landed). Default
	 * implementation is a no-op; the FANTASIAACE subclass overrides it
	 * to trigger its ConsumerWakeEvent so the Audio2Face consumer task
	 * picks up the new samples.
	 */
	virtual void WakeConsumer() {}

	/**
	 * @brief Factory for the per-request streaming decoder.
	 *
	 * Default returns an FPcm16StreamDecoder (raw 16-bit PCM, current
	 * behaviour). Provider subclasses override to parse their streaming
	 * envelope (e.g. ElevenLabs with-timestamps NDJSON) and populate the
	 * per-call buffer's Words/Characters timing.
	 */
	virtual TSharedPtr<FTTSStreamDecoder> CreateStreamDecoder();

	/** Expected sample rate of the incoming audio (used when no WAV header is present). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration")
	int32 SampleRate = 16000;

	/** If true, process audio chunks as they arrive (streaming mode). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration")
	bool bStreaming = false;

	// ── Per-call playback scaffolding (protected for subclass consumers) ──

	/** Per-caller-id list of audio buffers, in submission order for that
	 *  id. TTSSynthesize appends a new buffer to the list for the given
	 *  id; a subclass consumer drains from the head and pops once a buffer
	 *  is fully played. The same id can have many in-flight buffers (e.g.
	 *  when BP passes "Answer" for every sentence in a turn) without
	 *  collisions, because each buffer is its own object accessed via
	 *  raw pointer captured in the HTTP callback. */
	TMap<FString, TArray<TUniquePtr<FAudioBuffer>>> BuffersByCaller;
	FCriticalSection BuffersMutex;

	/** Global submission-order FIFO of (caller id, buffer pointer).
	 *  Cross-id ordering: if BP submits ("Answer", S1) then ("Answer", S2)
	 *  then ("Greeting", G1), the consumer plays S1 -> S2 -> G1.
	 *  SPSC: game thread is producer, consumer task is consumer. */
	TQueue<FPlaybackOrderEntry, EQueueMode::Spsc> PlaybackOrder;

	/** Entry currently being drained. Touched only by a subclass consumer. */
	TOptional<FPlaybackOrderEntry> CurrentPlayback;

private:

	// ── Result Buffering ───────────────────────────────────────────────

	/** Maps request IDs to completed TTS data (thread-safe via BufferMutex). */
	TMap<FString, FTTSData> Buffer;

	/** Mutex protecting Buffer access from HTTP callback and game threads. */
	FCriticalSection BufferMutex;

	/**
	 * @brief Issue an HTTP request from the given synthesis descriptor.
	 *
	 * The HTTP callbacks capture `Target` directly so they push samples
	 * into the right buffer without any by-id lookup. This is what makes
	 * same-id-reuse safe.
	 *
	 * @param Request  Fully populated synthesis request.
	 * @param Target   Buffer to receive samples for this request. Owned
	 *                 by BuffersByCaller; pointer is stable for the
	 *                 buffer's lifetime.
	 */
	void IssueHttpRequest(FTTSSynthesisRequest Request, FAudioBuffer* Target);

	/**
	 * @brief Process a complete synthesis response (WAV detection, resampling, buffering).
	 * @param Response  Raw TTS data from the HTTP response.
	 * @param Id        Caller's request identifier (used for Buffer storage and SynthesisReady broadcast).
	 * @param Target    Per-call audio buffer to populate (non-streaming) and mark complete.
	 */
	void HandleResult(FTTSData Response, FString Id, FAudioBuffer* Target);

	/**
	 * @brief Parse a WAV header. Returns true if data starts with RIFF/WAVE magic.
	 * @param Data             Raw audio data to inspect.
	 * @param OutSampleRate    Parsed sample rate.
	 * @param OutNumChannels   Parsed channel count.
	 * @param OutBitsPerSample Parsed bits per sample.
	 * @param OutDataOffset    Byte offset where PCM data begins.
	 * @return True if a valid WAV header was found.
	 */
	static bool TryParseWavHeader(const TArray<uint8>& Data, int32& OutSampleRate, int32& OutNumChannels, int32& OutBitsPerSample, int32& OutDataOffset);

	/**
	 * @brief Resample 16-bit PCM audio from SourceRate to 16000 Hz using linear interpolation.
	 * @param PCMData      Pointer to raw PCM data.
	 * @param NumBytes     Number of bytes in PCMData.
	 * @param SourceRate   Original sample rate of the input.
	 * @param NumChannels  Number of audio channels in the input.
	 * @return Resampled mono 16-bit PCM data at 16 kHz.
	 */
	static TArray<uint8> ResampleTo16kHz(const uint8* PCMData, int32 NumBytes, int32 SourceRate, int32 NumChannels);

public:
	URESTTTSComponent();

	// ── Events ─────────────────────────────────────────────────────────

	/** Broadcast when a full synthesis result is ready, carrying the request ID. */
	UPROPERTY(BlueprintAssignable, Category = "TTS")
	FSynthesizedEvent SynthesisReady;

	/** Broadcast when a partial (streaming) synthesis chunk arrives. */
	UPROPERTY(BlueprintAssignable, Category = "TTS")
	FPartialSynthesizedEvent PartialSynthesisReady;

	/** Broadcast when a request's audio has finished playing (drained by
	 *  a subclass-installed consumer such as ACE Audio2Face). For the bare
	 *  core component with no consumer attached, this is fired immediately
	 *  after synthesis completes. */
	UPROPERTY(BlueprintAssignable, Category = "TTS")
	FPlaybackCompleteEvent PlaybackComplete;

	// ── HTTP Configuration ─────────────────────────────────────────────

	/** Base URL for the TTS endpoint. May contain {text} placeholder for GET mode. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration", Config)
	FString URL;

	/**
	 * @brief HTTP headers to include in every synthesis request. This is where an API
	 *        key belongs (for example `xi-api-key`), so the value is read from the
	 *        host project's `DefaultGame.ini` and should be set there. A key typed
	 *        into the Details panel is still saved into the Blueprint or level asset:
	 *        this specifier adds an ini source, it does not make the property
	 *        transient.
	 *
	 *        `LoadConfig` reads the ini section of the instance's **own** class, so a
	 *        `Headers` line under `[/Script/FANTASIA.RESTTTSComponent]` reaches only a
	 *        plain `URESTTTSComponent`; a subclass that uses this property needs its
	 *        own section. The one shipped subclass that uses it is
	 *        `UACERESTTTSComponent`, whose section is
	 *        `[/Script/FANTASIAACE.ACERESTTTSComponent]`. The ElevenLabs subclasses
	 *        build their request headers from their own `Key` property and never read
	 *        this one.
	 *
	 * @note  A project that already typed a key into a Blueprint or actor asset still
	 *        has that key stored in the asset: this specifier does not clean it
	 *        retroactively. Clear the value in the asset and re-save it for the key to
	 *        leave the asset.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration", Config)
	TMap<FString, FString> Headers;

	/**
	 * @brief JSON body template. Use {text} as placeholder for the synthesis text.
	 *        If empty, a GET request is made (with {text} URL-encoded in the URL).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration", Config, meta = (MultiLine = true))
	FString BodyTemplate;

	// ── Public Methods ─────────────────────────────────────────────────

	/**
	 * @brief Start a text-to-speech synthesis request.
	 *
	 * @param text       Plain text to synthesize.
	 * @param id         Unique identifier to retrieve results later.
	 * @param bEndTurn   Subclass-defined "this is the last sentence of the
	 *                   turn" signal. For the bare core component it has
	 *                   no effect. The FANTASIAACE subclass uses it to
	 *                   close the Audio2Face stream cleanly on this
	 *                   buffer's tail (OnAnimationEnded then fires so the
	 *                   BP can unmute Whisper).
	 *
	 * Subclasses that implement end-of-turn semantics should override
	 * EndTurn() (called by this method when bEndTurn is true) rather
	 * than this method.
	 */
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "TTS Start", Keywords = "TTS"), Category = "TTS")
	void TTSSynthesize(FString text, FString id, bool bEndTurn = false);

	/**
	 * @brief Signal end-of-turn without synthesizing any new text.
	 *
	 * Default (core) implementation is a no-op. Subclasses with a
	 * playback consumer override this to close their stream cleanly.
	 */
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "TTS End Turn", Keywords = "TTS"), Category = "TTS")
	virtual void EndTurn();

	/**
	 * @brief Retrieve the synthesized audio as a USoundWaveProcedural.
	 * @param id  Identifier of a completed synthesis request.
	 * @return Procedural SoundWave containing 16 kHz mono PCM audio, or nullptr if not found.
	 */
	UFUNCTION(BlueprintPure, meta = (DisplayName = "Get Sound", Keywords = "TTS"), Category = "TTS")
	USoundWaveProcedural* TTSGetSound(FString id);

	/**
	 * @brief Retrieve raw PCM audio bytes for a completed synthesis.
	 * @param id  Identifier of a completed synthesis request.
	 * @return Raw 16-bit PCM byte array, or empty if not found.
	 */
	UFUNCTION(BlueprintPure, meta = (DisplayName = "Get Raw Sound", Keywords = "TTS"), Category = "TTS")
	TArray<uint8> TTSGetRawSound(FString id);

	// ── Captured timing (internal; not Blueprint-exposed) ──────────────
	// Provider word/character timing is decoded and stored, but intentionally
	// not surfaced to Blueprints: the realtime "segment played" signal is
	// expected from ACE's native animation events. These C++ accessors keep
	// the captured data reachable until that lands.

	/** Per-word timing for a completed synthesis (empty if provider unsupported). */
	TArray<FTTSSegmentTiming> GetWordTimings(FString id);

	/** Per-character timing for a completed synthesis (empty if provider unsupported). */
	TArray<FTTSSegmentTiming> GetCharacterTimings(FString id);
};
