/**
 * @file WhisperCaptureComponent.h
 * @brief Actor component for real-time microphone capture and Whisper transcription.
 */

// Copyright (c) 2024 WhisperASR Plugin. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "WhisperTypes.h"

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable: 4456 4457 4458 4459 4244 4267 4800 4756 4100 4127 4189 4706 4702 4310 4305 4242 4389)
#endif
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Weverything"
#endif

#include "AudioCapture.h"

#if defined(__clang__)
#pragma clang diagnostic pop
#endif
#if defined(_MSC_VER)
#pragma warning(pop)
#endif

#include "WhisperCaptureComponent.generated.h"

// Forward declarations
class UWhisperSubsystem;

/**
 * UWhisperCaptureComponent
 *
 * Actor component that opens the system microphone via UE 5.6's
 * Audio::FAudioCapture, resamples to 16 kHz mono float, buffers audio,
 * and feeds it to the Whisper subsystem for transcription.
 *
 * Drop onto any Actor. Call StartCapture / StopCapture from Blueprints.
 * Results arrive via OnCaptureTranscriptionComplete (and optionally
 * per-segment via OnStreamingSegment in streaming mode).
 */
UCLASS(ClassGroup = (FANTASIA), Config = Game, meta = (BlueprintSpawnableComponent,
	DisplayName = "Whisper Audio Capture"))
class FANTASIA_API UWhisperCaptureComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	UWhisperCaptureComponent();
	virtual ~UWhisperCaptureComponent();

	// ── Configuration ────────────────────────────────────────────────────

	/** Transcription config used for captured audio */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WhisperASR|Capture")
	FWhisperConfig TranscriptionConfig;

	/**
	 * If true, automatically transcribe every N seconds of buffered audio
	 * while capture is active (chunked streaming mode).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WhisperASR|Capture", Config)
	bool bStreamingMode = false;

	/** Seconds between streaming transcriptions (if bStreamingMode) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WhisperASR|Capture", Config,
		meta = (EditCondition = "bStreamingMode", ClampMin = "1.0", ClampMax = "30.0"))
	float StreamingIntervalSeconds = 5.0f;

	/**
	 * Overlap in seconds between consecutive chunks. The last N seconds
	 * of each chunk are kept and prepended to the next one. This prevents
	 * words at chunk boundaries from being cut in half and garbled.
	 * Recommended: 1.0–2.0 seconds.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WhisperASR|Capture", Config,
		meta = (EditCondition = "bStreamingMode", ClampMin = "0.0", ClampMax = "5.0"))
	float StreamingOverlapSeconds = 1.0f;

	/**
	 * Maximum buffer duration in seconds. Oldest samples are discarded
	 * when the buffer exceeds this length.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WhisperASR|Capture", Config,
		meta = (ClampMin = "1.0", ClampMax = "300.0"))
	float MaxBufferDurationSeconds = 30.0f;

	/**
	 * Minimum peak amplitude to consider as speech (voice activity gate).
	 * Typical values: 0.01–0.05 depending on microphone gain.
	 * Use CalibrateNoiseFloor() to set this automatically.
	 * Set to 0.0 to disable VAD filtering.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WhisperASR|Capture", Config,
		meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float VadEnergyThreshold = 0.01f;

	/**
	 * When true, the component continuously listens and automatically
	 * transcribes each utterance when silence is detected after speech.
	 * No manual StopCapture needed — stays active for the next utterance.
	 * Uses VadEnergyThreshold to distinguish speech from silence.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WhisperASR|Capture", Config)
	bool bAutoDetectSpeech = true;

	/**
	 * How long silence must persist after speech before the utterance
	 * is considered finished and sent for transcription (seconds).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WhisperASR|Capture", Config,
		meta = (EditCondition = "bAutoDetectSpeech", ClampMin = "0.3", ClampMax = "10.0"))
	float SilenceTimeoutSeconds = 1.5f;

	/**
	 * Index of the audio input device to use.
	 * -1 = system default microphone.
	 * Use GetAvailableInputDevices to enumerate options.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WhisperASR|Capture")
	int32 AudioDeviceIndex = -1;

	// ── Noise Suppression ────────────────────────────────────────────────

	/**
	 * Apply a high-pass filter to remove low-frequency rumble (AC hum,
	 * fan noise, breath thumps). Adds negligible CPU overhead.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WhisperASR|Noise", Config)
	bool bEnableHighPassFilter = true;

	/** High-pass cutoff frequency in Hz. 80-120 Hz works well for speech. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WhisperASR|Noise", Config,
		meta = (EditCondition = "bEnableHighPassFilter", ClampMin = "20.0", ClampMax = "300.0"))
	float HighPassCutoffHz = 100.0f;

	/**
	 * Apply a soft noise gate that attenuates audio below a multiple of
	 * the measured noise floor. Run CalibrateNoiseFloor first for best results.
	 * Smoothed attack/release prevents clicks.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WhisperASR|Noise", Config)
	bool bEnableNoiseGate = true;

	/**
	 * Audio below NoiseGateThresholdMultiplier * LastMeasuredNoiseFloor
	 * is attenuated. If no calibration was done, falls back to VadEnergyThreshold.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WhisperASR|Noise", Config,
		meta = (EditCondition = "bEnableNoiseGate", ClampMin = "1.0", ClampMax = "10.0"))
	float NoiseGateThresholdMultiplier = 1.5f;

	/**
	 * How much to attenuate noise (linear gain, 0.0 = silence, 1.0 = passthrough).
	 * 0.1 = -20 dB, a good balance between aggressive noise removal and
	 * preserving room ambience that helps Whisper detect end-of-speech.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WhisperASR|Noise", Config,
		meta = (EditCondition = "bEnableNoiseGate", ClampMin = "0.0", ClampMax = "1.0"))
	float NoiseGateAttenuation = 0.1f;

	/** Attack time in milliseconds (how fast gate opens for speech). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WhisperASR|Noise", Config,
		meta = (EditCondition = "bEnableNoiseGate", ClampMin = "1.0", ClampMax = "100.0"))
	float NoiseGateAttackMs = 5.0f;

	/** Release time in milliseconds (how fast gate closes after speech). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WhisperASR|Noise", Config,
		meta = (EditCondition = "bEnableNoiseGate", ClampMin = "10.0", ClampMax = "500.0"))
	float NoiseGateReleaseMs = 100.0f;

	// ── Events ───────────────────────────────────────────────────────────

	/** Fired when captured audio finishes transcription */
	UPROPERTY(BlueprintAssignable, Category = "WhisperASR|Capture|Events")
	FOnWhisperTranscriptionComplete OnCaptureTranscriptionComplete;

	/** Fired per-segment during streaming transcription */
	UPROPERTY(BlueprintAssignable, Category = "WhisperASR|Capture|Events")
	FOnWhisperSegmentReady OnStreamingSegment;

	/** Fired when noise floor calibration completes */
	UPROPERTY(BlueprintAssignable, Category = "WhisperASR|Capture|Events")
	FOnWhisperNoiseCalibrationComplete OnNoiseCalibrationComplete;

	// ── Controls ─────────────────────────────────────────────────────────

	/** Open the microphone and begin capturing audio. Returns false if device fails to open. */
	UFUNCTION(BlueprintCallable, Category = "WhisperASR|Capture")
	bool StartCapture();

	/** Stop the microphone and transcribe whatever is in the buffer */
	UFUNCTION(BlueprintCallable, Category = "WhisperASR|Capture")
	void StopCapture();

	/** Stop the microphone and discard the buffer (no transcription) */
	UFUNCTION(BlueprintCallable, Category = "WhisperASR|Capture")
	void CancelCapture();

	/** Manually trigger transcription of the current buffer */
	UFUNCTION(BlueprintCallable, Category = "WhisperASR|Capture")
	void TranscribeBuffer();

	/** Clear the audio buffer without transcribing */
	UFUNCTION(BlueprintCallable, Category = "WhisperASR|Capture")
	void ClearBuffer();

	/**
	 * Mute or unmute the capture. While muted, incoming audio is discarded,
	 * VAD is suppressed, and the buffer is cleared. The microphone stays open
	 * so capture resumes instantly when unmuted.
	 *
	 * Typical use: call SetMuted(true) when your agent starts speaking (TTS),
	 * then SetMuted(false) when it finishes, to prevent the agent's voice
	 * from being transcribed.
	 */
	UFUNCTION(BlueprintCallable, Category = "WhisperASR|Capture")
	void SetMuted(bool bMute);

	/** Returns true if capture is currently muted */
	UFUNCTION(BlueprintPure, Category = "WhisperASR|Capture")
	bool IsMuted() const { return bIsMuted; }

	/**
	 * Record background noise for DurationSeconds, then set VadEnergyThreshold
	 * to the measured peak amplitude times Multiplier.
	 * Fires OnNoiseCalibrationComplete when done.
	 * If the mic is already capturing, calibration runs alongside; otherwise
	 * the mic is opened temporarily and closed when calibration finishes.
	 */
	UFUNCTION(BlueprintCallable, Category = "WhisperASR|Capture")
	void CalibrateNoiseFloor(float DurationSeconds = 1.0f, float Multiplier = 2.0f);

	/** Returns the last measured noise floor (peak amplitude), or -1 if never calibrated */
	UFUNCTION(BlueprintPure, Category = "WhisperASR|Capture")
	float GetNoiseFloor() const { return LastMeasuredNoiseFloor; }

	/**
	 * Log detailed audio diagnostics to the output log:
	 * buffer size, peak amplitude, RMS level, device info, and threshold.
	 * Call this while capturing to diagnose VAD issues.
	 */
	UFUNCTION(BlueprintCallable, Category = "WhisperASR|Capture")
	void LogAudioDiagnostics();

	/** Check if the microphone is currently active */
	UFUNCTION(BlueprintPure, Category = "WhisperASR|Capture")
	bool IsCapturing() const { return bIsCapturing; }

	/** Current buffer duration in seconds */
	UFUNCTION(BlueprintPure, Category = "WhisperASR|Capture")
	float GetBufferDuration() const;

	/** Current RMS audio level (0–1) — useful for VU meter widgets */
	UFUNCTION(BlueprintPure, Category = "WhisperASR|Capture")
	float GetCurrentAudioLevel() const;

	/**
	 * Enumerate available audio input (microphone) devices.
	 * Returns display names; the array index is the DeviceIndex.
	 */
	UFUNCTION(BlueprintCallable, Category = "WhisperASR|Capture")
	static TArray<FString> GetAvailableInputDevices();

protected:

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

private:

	/**
	 * Test seam: lets the characterisation tests in WhisperCaptureComponentTests.cpp
	 * reach the private DSP steps (resampler, RMS, high-pass, noise gate).
	 * A friend declaration adds no exported or Blueprint-facing surface.
	 */
	friend struct FWhisperCaptureTestAccess;

	// ── Audio capture internals ──────────────────────────────────────────

	/**
	 * Monotonic session counter. Incremented on every StartCapture.
	 * Each in-flight TranscribeBuffer captures the value at submission time;
	 * HandleTranscriptionResult ignores results whose session no longer matches.
	 * Prevents stale background transcriptions from corrupting a fresh session.
	 */
	uint32 CaptureSessionId = 0;
	uint32 InFlightTranscriptionSessionId = 0;

	/** Platform audio capture handle (UE 5.6 AudioCaptureCore) */
	TUniquePtr<Audio::FAudioCapture> AudioCapture;

	/** Sample rate reported by the opened device */
	int32 DeviceSampleRate = 0;

	/** Channel count reported by the opened device */
	int32 DeviceNumChannels = 0;

	/** Opens the audio capture device; returns true on success */
	bool OpenCaptureDevice();

	/** Closes and releases the audio capture device */
	void CloseCaptureDevice();

	// ── Buffer management ────────────────────────────────────────────────

	/**
	 * The main audio buffer: 16 kHz mono float32.
	 * Written from the audio callback thread, read from the game thread.
	 * Access is serialized through BufferCritSection.
	 */
	TArray<float> AudioBuffer;

	/** Protects AudioBuffer from concurrent audio-thread / game-thread access */
	mutable FCriticalSection BufferCritSection;

	/** Tracks whether a transcription is currently in-flight */
	FThreadSafeBool bTranscriptionInFlight;

	bool bIsCapturing = false;
	bool bIsMuted = false;
	float StreamingTimer = 0.f;

	// ── Noise suppression state (audio thread) ──────────────────────────
	// Biquad high-pass filter state (Direct Form II Transposed)
	float HpfX1 = 0.f, HpfX2 = 0.f;
	float HpfY1 = 0.f, HpfY2 = 0.f;
	float HpfB0 = 1.f, HpfB1 = 0.f, HpfB2 = 0.f;
	float HpfA1 = 0.f, HpfA2 = 0.f;
	float HpfLastCutoff = -1.f;

	// Envelope follower (smoothed |sample|, tracks audio level over ~ms timescale)
	float NoiseGateEnvelope = 0.f;

	// Soft gate gain state (smoothed gain, 0..1)
	float NoiseGateGain = 1.f;

	/** Recompute high-pass filter coefficients for the current cutoff and 16 kHz. */
	void UpdateHighPassCoefficients();

	/** Apply noise suppression in-place on a buffer of 16 kHz mono samples. */
	void ApplyNoiseSuppression(float* Samples, int32 NumSamples);

	// ── VAD auto-detect state ───────────────────────────────────────────
	float SilenceTimer = 0.f;
	float VadCheckTimer = 0.f;
	bool bSpeechDetected = false;

	/** Peak amplitude updated atomically by the audio callback (fixed-point * 10000) */
	FThreadSafeCounter RecentPeakFixed;
	void SetRecentPeak(float Value);
	float GetRecentPeak() const;

	// ── Noise calibration state ─────────────────────────────────────────
	bool bIsCalibrating = false;
	bool bCalibrationOwnsMic = false; // true if we opened the mic just for calibration
	float CalibrationDuration = 1.0f;
	float CalibrationMultiplier = 2.0f;
	float CalibrationTimer = 0.f;
	float LastMeasuredNoiseFloor = -1.f;

	/** Whether the previous tick interval detected speech */
	bool bWasSpeaking = false;

	/** Accumulated result across streaming intervals during speech.
	 *  OnCaptureTranscriptionComplete fires with this when silence is detected. */
	FWhisperResult AccumulatedResult;

	/** Atomic so GetCurrentAudioLevel() is safe from any thread */
	FThreadSafeCounter CurrentRMSLevelFixed; // stored as int = value * 10000

	void SetRMSLevel(float Value);
	float GetRMSLevel() const;

	/** Enforce MaxBufferDurationSeconds (caller must hold BufferCritSection) */
	void TrimBufferLocked();

	/** Compute RMS energy of the last N samples */
	static float ComputeRMS(const float* Samples, int32 Count);

	// ── Resampling state ─────────────────────────────────────────────────

	/**
	 * Fractional sample accumulator for the linear resampler.
	 * Persists between callbacks so we don't drift.
	 */
	double ResamplePhase = 0.0;

	/**
	 * Resample + mix to mono in one pass.
	 * Appends output to OutBuffer. Caller must hold BufferCritSection.
	 */
	void ResampleAndMixToMono(const float* InAudio, int32 NumFrames,
		int32 NumChannels, int32 SrcRate, TArray<float>& OutBuffer);

	// ── Delegate handlers ────────────────────────────────────────────────

	UFUNCTION()
	void HandleTranscriptionResult(const FWhisperResult& Result);

	UFUNCTION()
	void HandleSegmentReady(const FWhisperSegment& Segment);
};
