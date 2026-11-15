/**
 * @file WhisperCaptureComponentTests.cpp
 * @brief US-029 / F-007: characterisation of the microphone capture path of
 *        UWhisperCaptureComponent (C-asr).
 *
 * These tests pin what the code does today, bugs included. Each test says in its comment
 * whether a value is "believed correct" (the code and its documentation agree, and a change
 * would be a regression) or "merely current" (the code does it, nothing says it should, and a
 * deliberate change is expected to update the test). A value that looks wrong is pinned as it
 * is and carries the finding that reports it; fixing it is not in US-029.
 *
 * Nothing here opens an audio device, loads a model or touches the network. StartCapture is
 * deliberately not driven against a device, see F-018. The private DSP steps are reached through
 * FWhisperCaptureTestAccess, the friend declared in WhisperCaptureComponent.h; it only forwards.
 *
 * Test names are FANTASIA.WhisperCapture.<Area>.<Case>.
 */

#include "Misc/AutomationTest.h"
#include "UObject/Package.h"
#include "WhisperCaptureComponent.h"

// Global scope on purpose: the header's `friend struct FWhisperCaptureTestAccess;` declares the
// name in the enclosing (global) namespace, so a definition inside a namespace would not match.
struct FWhisperCaptureTestAccess
{
	struct FHighPass
	{
		float B0, B1, B2, A1, A2, LastCutoff;
	};

	struct FFilterState
	{
		float X1, X2, Y1, Y2, Envelope, Gain;
	};

	static void Resample(UWhisperCaptureComponent& C, const float* In, int32 Frames,
		int32 Channels, int32 SrcRate, TArray<float>& Out)
	{
		C.ResampleAndMixToMono(In, Frames, Channels, SrcRate, Out);
	}

	/** Appends 16 kHz mono samples to the capture buffer through the resampler's fast path. */
	static void AppendToBuffer(UWhisperCaptureComponent& C, const TArray<float>& Mono16k)
	{
		FScopeLock Lock(&C.BufferCritSection);
		C.ResampleAndMixToMono(Mono16k.GetData(), Mono16k.Num(), 1, 16000, C.AudioBuffer);
	}

	static double ResamplePhase(const UWhisperCaptureComponent& C) { return C.ResamplePhase; }
	static float ComputeRMS(const float* Samples, int32 Count) { return UWhisperCaptureComponent::ComputeRMS(Samples, Count); }
	static void UpdateHighPass(UWhisperCaptureComponent& C) { C.UpdateHighPassCoefficients(); }
	static void ApplyNoise(UWhisperCaptureComponent& C, TArray<float>& Samples) { C.ApplyNoiseSuppression(Samples.GetData(), Samples.Num()); }
	static void SetNoiseFloor(UWhisperCaptureComponent& C, float Floor) { C.LastMeasuredNoiseFloor = Floor; }
	static void SetRMSLevel(UWhisperCaptureComponent& C, float Level) { C.SetRMSLevel(Level); }
	static void ForceCapturing(UWhisperCaptureComponent& C, bool bCapturing) { C.bIsCapturing = bCapturing; }

	static FHighPass HighPass(const UWhisperCaptureComponent& C)
	{
		return {C.HpfB0, C.HpfB1, C.HpfB2, C.HpfA1, C.HpfA2, C.HpfLastCutoff};
	}

	static FFilterState Filter(const UWhisperCaptureComponent& C)
	{
		return {C.HpfX1, C.HpfX2, C.HpfY1, C.HpfY2, C.NoiseGateEnvelope, C.NoiseGateGain};
	}
};

namespace
{
using FAccess = FWhisperCaptureTestAccess;

constexpr EAutomationTestFlags CaptureTestFlags =
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter;

constexpr int32 CaptureRate = 16000;

#define WHISPER_CAPTURE_TEST(Id, Name) \
	IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWhisperCapture##Id, "FANTASIA.WhisperCapture." Name, CaptureTestFlags) \
	bool FWhisperCapture##Id::RunTest(const FString& Parameters)

/** A component with no owner, no world, no device and no model. */
UWhisperCaptureComponent* MakeComponent()
{
	return NewObject<UWhisperCaptureComponent>(GetTransientPackage());
}

TArray<float> MakeConstant(int32 Count, float Value)
{
	TArray<float> Out;
	Out.Init(Value, Count);
	return Out;
}

/** Count samples rising from 0 by Step per sample. */
TArray<float> MakeRamp(int32 Count, float Step)
{
	TArray<float> Out;
	Out.SetNumUninitialized(Count);
	for (int32 i = 0; i < Count; ++i)
	{
		Out[i] = i * Step;
	}
	return Out;
}

/** A sine of the given frequency and amplitude, sampled at the capture rate. */
TArray<float> MakeSine(int32 Count, double FrequencyHz, double Amplitude)
{
	TArray<float> Out;
	Out.SetNumUninitialized(Count);
	for (int32 i = 0; i < Count; ++i)
	{
		Out[i] = static_cast<float>(Amplitude * FMath::Sin(2.0 * UE_DOUBLE_PI * FrequencyHz * i / CaptureRate));
	}
	return Out;
}

/** Largest absolute sample at or after From. */
float PeakAbs(const TArray<float>& Samples, int32 From)
{
	float Peak = 0.f;
	for (int32 i = From; i < Samples.Num(); ++i)
	{
		Peak = FMath::Max(Peak, FMath::Abs(Samples[i]));
	}
	return Peak;
}

/** Sets which of the two noise-suppression stages, high-pass and gate, run. */
void IsolateStages(UWhisperCaptureComponent& C, bool bHighPass, bool bGate)
{
	C.bEnableHighPassFilter = bHighPass;
	C.bEnableNoiseGate = bGate;
}
} // namespace

// ============================================================================
// Defaults and initial state (C-asr)
// ============================================================================

// C-asr's two stated invariants. Believed correct: the contract and the header both say so.
WHISPER_CAPTURE_TEST(DefaultsInvariants, "Defaults.ContractInvariants")
{
	const UWhisperCaptureComponent* C = MakeComponent();
	TestEqual(TEXT("AudioDeviceIndex is -1 (system default)"), C->AudioDeviceIndex, -1);
	TestTrue(TEXT("High-pass filter is on by default"), C->bEnableHighPassFilter);
	TestTrue(TEXT("Noise gate is on by default"), C->bEnableNoiseGate);
	return true;
}

// Every Config property default as the header declares it. Believed correct for the booleans
// and for the values the tooltips quote (100 Hz cutoff, 0.1 = -20 dB, 5 ms and 100 ms);
// merely current for the rest, which are tuning choices.
WHISPER_CAPTURE_TEST(DefaultsConfig, "Defaults.Config")
{
	const UWhisperCaptureComponent* C = MakeComponent();

	TestFalse(TEXT("bStreamingMode"), C->bStreamingMode);
	TestEqual(TEXT("StreamingIntervalSeconds"), C->StreamingIntervalSeconds, 5.0f, 1e-6f);
	TestEqual(TEXT("StreamingOverlapSeconds"), C->StreamingOverlapSeconds, 1.0f, 1e-6f);
	TestEqual(TEXT("MaxBufferDurationSeconds"), C->MaxBufferDurationSeconds, 30.0f, 1e-6f);
	TestEqual(TEXT("VadEnergyThreshold"), C->VadEnergyThreshold, 0.01f, 1e-6f);
	TestTrue(TEXT("bAutoDetectSpeech"), C->bAutoDetectSpeech);
	TestEqual(TEXT("SilenceTimeoutSeconds"), C->SilenceTimeoutSeconds, 1.5f, 1e-6f);
	TestEqual(TEXT("HighPassCutoffHz"), C->HighPassCutoffHz, 100.0f, 1e-6f);
	TestEqual(TEXT("NoiseGateThresholdMultiplier"), C->NoiseGateThresholdMultiplier, 1.5f, 1e-6f);
	TestEqual(TEXT("NoiseGateAttenuation"), C->NoiseGateAttenuation, 0.1f, 1e-6f);
	TestEqual(TEXT("NoiseGateAttackMs"), C->NoiseGateAttackMs, 5.0f, 1e-6f);
	TestEqual(TEXT("NoiseGateReleaseMs"), C->NoiseGateReleaseMs, 100.0f, 1e-6f);

	// FWhisperConfig defaults, as C-asr lists them.
	const FWhisperConfig& Whisper = C->TranscriptionConfig;
	TestTrue(TEXT("Language is Auto"), Whisper.Language == EWhisperLanguage::Auto);
	TestTrue(TEXT("Task is Transcribe"), Whisper.Task == EWhisperTask::Transcribe);
	TestEqual(TEXT("NumThreads"), Whisper.NumThreads, 0);
	TestEqual(TEXT("MaxSegmentLength"), Whisper.MaxSegmentLength, 0);
	TestFalse(TEXT("bTokenTimestamps"), Whisper.bTokenTimestamps);
	TestTrue(TEXT("InitialPrompt is empty"), Whisper.InitialPrompt.IsEmpty());
	TestTrue(TEXT("bSuppressNonSpeech"), Whisper.bSuppressNonSpeech);
	TestEqual(TEXT("Temperature"), Whisper.Temperature, 0.0f, 1e-6f);
	return true;
}

// A freshly constructed component is idle with identity filters. Believed correct: the audio
// callback relies on all of this (HpfLastCutoff = -1 forces the first coefficient computation,
// gate gain 1 means the first buffer is not attenuated).
WHISPER_CAPTURE_TEST(DefaultsInitialState, "Defaults.InitialState")
{
	UWhisperCaptureComponent* C = MakeComponent();
	TestFalse(TEXT("Not capturing"), C->IsCapturing());
	TestFalse(TEXT("Not muted"), C->IsMuted());
	TestEqual(TEXT("Buffer is empty"), C->GetBufferDuration(), 0.0f, 1e-9f);
	TestEqual(TEXT("Noise floor is -1 until calibrated"), C->GetNoiseFloor(), -1.0f, 1e-6f);
	TestEqual(TEXT("Audio level is 0"), C->GetCurrentAudioLevel(), 0.0f, 1e-9f);

	// Merely current: ticking is off until StartCapture enables it, at 10 Hz.
	TestTrue(TEXT("Can tick"), C->PrimaryComponentTick.bCanEverTick);
	TestFalse(TEXT("Tick starts disabled"), C->PrimaryComponentTick.bStartWithTickEnabled);
	TestEqual(TEXT("Tick interval"), C->PrimaryComponentTick.TickInterval, 0.1f, 1e-6f);

	const FAccess::FHighPass Hp = FAccess::HighPass(*C);
	TestEqual(TEXT("B0"), Hp.B0, 1.0f, 1e-9f);
	TestEqual(TEXT("B1"), Hp.B1, 0.0f, 1e-9f);
	TestEqual(TEXT("B2"), Hp.B2, 0.0f, 1e-9f);
	TestEqual(TEXT("A1"), Hp.A1, 0.0f, 1e-9f);
	TestEqual(TEXT("A2"), Hp.A2, 0.0f, 1e-9f);
	TestEqual(TEXT("LastCutoff forces first recompute"), Hp.LastCutoff, -1.0f, 1e-9f);

	const FAccess::FFilterState F = FAccess::Filter(*C);
	TestEqual(TEXT("Envelope"), F.Envelope, 0.0f, 1e-9f);
	TestEqual(TEXT("Gate gain starts open"), F.Gain, 1.0f, 1e-9f);
	return true;
}

// ============================================================================
// Controls that need no device
// ============================================================================

// StartCapture's only path that returns false without touching hardware. Believed correct
// (the log text says so): a second StartCapture neither restarts the stream nor clears what was
// captured. The story's other StartCapture case, an unopenable device, cannot be driven
// without opening the real microphone, see F-018.
WHISPER_CAPTURE_TEST(StartWhileCapturing, "Controls.StartCaptureWhileCapturing")
{
	UWhisperCaptureComponent* C = MakeComponent();
	FAccess::AppendToBuffer(*C, MakeConstant(8000, 0.1f));
	FAccess::ForceCapturing(*C, true);

	AddExpectedMessage(TEXT("already capturing"), ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 1);
	TestFalse(TEXT("StartCapture returns false"), C->StartCapture());
	TestTrue(TEXT("Still capturing"), C->IsCapturing());
	TestEqual(TEXT("Buffer untouched"), C->GetBufferDuration(), 0.5f, 1e-6f);

	FAccess::ForceCapturing(*C, false);
	return true;
}

// SetMuted and ClearBuffer driven in sequence on an idle component.
// Believed correct: IsMuted reports what was set; muting and ClearBuffer empty the buffer and
// zero the level meter (the header says muting clears the buffer).
// Merely current: ClearBuffer also zeroes the resampler phase; SetMuted(false) resets the
// filter and gate state (stale history from before the mute), SetMuted(true) does not.
WHISPER_CAPTURE_TEST(MuteAndClear, "Controls.MuteAndClearSequence")
{
	UWhisperCaptureComponent* C = MakeComponent();

	FAccess::AppendToBuffer(*C, MakeConstant(8000, 0.1f));
	FAccess::SetRMSLevel(*C, 0.5f);
	TestEqual(TEXT("Half a second buffered"), C->GetBufferDuration(), 0.5f, 1e-6f);
	TestEqual(TEXT("Level meter reads what was set"), C->GetCurrentAudioLevel(), 0.5f, 1e-4f);

	// Dirty the filters: a loud buffer moves the high-pass history, a long quiet one closes the gate.
	TArray<float> Loud = MakeSine(256, 1000.0, 0.5);
	FAccess::ApplyNoise(*C, Loud);
	TArray<float> Quiet = MakeConstant(8000, 0.001f);
	FAccess::ApplyNoise(*C, Quiet);
	const FAccess::FFilterState Dirty = FAccess::Filter(*C);
	TestTrue(TEXT("Precondition: filter history is dirty"), Dirty.X1 != 0.f && Dirty.Gain < 0.9f);

	C->SetMuted(true);
	TestTrue(TEXT("Muted"), C->IsMuted());
	TestEqual(TEXT("Mute empties the buffer"), C->GetBufferDuration(), 0.0f, 1e-9f);
	TestEqual(TEXT("Mute zeroes the level"), C->GetCurrentAudioLevel(), 0.0f, 1e-9f);
	TestEqual(TEXT("Mute leaves the filter history alone"), FAccess::Filter(*C).Gain, Dirty.Gain, 1e-9f);

	C->SetMuted(false);
	TestFalse(TEXT("Unmuted"), C->IsMuted());
	const FAccess::FFilterState Reset = FAccess::Filter(*C);
	TestEqual(TEXT("Unmute resets X1"), Reset.X1, 0.0f, 1e-9f);
	TestEqual(TEXT("Unmute resets X2"), Reset.X2, 0.0f, 1e-9f);
	TestEqual(TEXT("Unmute resets Y1"), Reset.Y1, 0.0f, 1e-9f);
	TestEqual(TEXT("Unmute resets Y2"), Reset.Y2, 0.0f, 1e-9f);
	TestEqual(TEXT("Unmute resets the envelope"), Reset.Envelope, 0.0f, 1e-9f);
	TestEqual(TEXT("Unmute reopens the gate"), Reset.Gain, 1.0f, 1e-9f);

	// ClearBuffer: 44.1 kHz in 480-frame chunks leaves a non-zero resampler phase to be reset.
	const TArray<float> Chunk = MakeConstant(481, 0.2f); // one frame of slack, see F-017
	TArray<float> Sink;
	FAccess::Resample(*C, Chunk.GetData(), 480, 1, 44100, Sink);
	TestTrue(TEXT("Precondition: resampler phase carried"), FAccess::ResamplePhase(*C) > 0.0);
	FAccess::AppendToBuffer(*C, MakeConstant(16000, 0.1f));
	FAccess::SetRMSLevel(*C, 0.25f);

	C->ClearBuffer();
	TestEqual(TEXT("ClearBuffer empties the buffer"), C->GetBufferDuration(), 0.0f, 1e-9f);
	TestEqual(TEXT("ClearBuffer zeroes the level"), C->GetCurrentAudioLevel(), 0.0f, 1e-9f);
	TestEqual(TEXT("ClearBuffer zeroes the phase"), static_cast<float>(FAccess::ResamplePhase(*C)), 0.0f, 1e-12f);
	TestFalse(TEXT("ClearBuffer does not change mute"), C->IsMuted());
	return true;
}

// ============================================================================
// Resample and mix to mono
// ============================================================================

// 16 kHz mono is appended sample for sample, after what is already buffered.
// Believed correct: this is the documented fast path.
WHISPER_CAPTURE_TEST(ResamplePassThrough, "Resample.PassThrough16kMono")
{
	UWhisperCaptureComponent* C = MakeComponent();
	const TArray<float> In = {0.1f, -0.2f, 0.3f, -0.4f, 0.5f, 0.0f, -1.0f};
	TArray<float> Out = {9.0f};
	FAccess::Resample(*C, In.GetData(), In.Num(), 1, 16000, Out);

	TestEqual(TEXT("Appended, not replaced"), Out.Num(), In.Num() + 1);
	TestEqual(TEXT("Existing content kept"), Out[0], 9.0f, 0.0f);
	for (int32 i = 0; i < In.Num(); ++i)
	{
		TestEqual(*FString::Printf(TEXT("Sample %d identical"), i), Out[i + 1], In[i], 0.0f);
	}
	TestEqual(TEXT("Fast path leaves no phase"), static_cast<float>(FAccess::ResamplePhase(*C)), 0.0f, 1e-12f);
	return true;
}

// Output is mono and its frame count is frames * 16000 / rate within one frame, for the
// common device rates, stereo included. Believed correct (US-029's own tolerance).
WHISPER_CAPTURE_TEST(ResampleFrameCount, "Resample.FrameCountScalesWithRate")
{
	struct FCase
	{
		int32 Rate, Frames, Channels;
	};
	const FCase Cases[] = {
		{48000, 480, 1}, {48000, 480, 2}, {44100, 441, 1}, {44100, 441, 2},
		{22050, 441, 1}, {32000, 320, 1}, {96000, 960, 1}, {48000, 512, 1},
		{8000, 80, 1},   // up-sampling
	};
	for (const FCase& Case : Cases)
	{
		UWhisperCaptureComponent* C = MakeComponent();
		// One frame of slack after the input, so a read past the end (F-017) stays in the allocation.
		const TArray<float> In = MakeConstant((Case.Frames + 1) * Case.Channels, 0.25f);
		TArray<float> Out;
		FAccess::Resample(*C, In.GetData(), Case.Frames, Case.Channels, Case.Rate, Out);

		const double Expected = static_cast<double>(Case.Frames) * CaptureRate / Case.Rate;
		TestTrue(*FString::Printf(TEXT("%d Hz x%d ch, %d frames -> %d samples, expected %.2f +/- 1"),
					 Case.Rate, Case.Channels, Case.Frames, Out.Num(), Expected),
			FMath::Abs(Out.Num() - Expected) <= 1.0);
	}
	return true;
}

// Linear interpolation between neighbouring source frames, phase starting at zero.
// 48 kHz is an exact 3:1 decimation, so each output is the source ramp at 3k. 24 kHz is 1.5:1,
// so every odd output is a midpoint. Believed correct: this is what "linear" means.
WHISPER_CAPTURE_TEST(ResampleInterpolation, "Resample.LinearInterpolation")
{
	{
		UWhisperCaptureComponent* C = MakeComponent();
		const TArray<float> Ramp = MakeRamp(481, 0.001f);
		TArray<float> Out;
		FAccess::Resample(*C, Ramp.GetData(), 480, 1, 48000, Out);
		TestEqual(TEXT("48 kHz output count"), Out.Num(), 160);
		for (int32 k = 0; k < FMath::Min(Out.Num(), 160); ++k)
		{
			TestEqual(*FString::Printf(TEXT("48 kHz out[%d]"), k), Out[k], 3 * k * 0.001f, 1e-5f);
		}
	}
	{
		UWhisperCaptureComponent* C = MakeComponent();
		const TArray<float> Ramp = MakeRamp(101, 1.0f);
		TArray<float> Out;
		FAccess::Resample(*C, Ramp.GetData(), 100, 1, 24000, Out);
		TestEqual(TEXT("24 kHz output count"), Out.Num(), 67);
		for (int32 k = 0; k < FMath::Min(Out.Num(), 67); ++k)
		{
			TestEqual(*FString::Printf(TEXT("24 kHz out[%d] sits at source 1.5k"), k), Out[k], 1.5f * k, 1e-4f);
		}
	}
	return true;
}

// Multi-channel input is not averaged: each tap takes the channel with the larger magnitude,
// signed, and the first channel wins a tie. Merely current: the source comment explains the
// intent (mic arrays may carry speech on any channel), but the name and the header say "mix",
// and opposite-phase channels do not cancel as a sum would. Reported in F-019.
WHISPER_CAPTURE_TEST(ResampleLoudestChannel, "Resample.LoudestChannelWins")
{
	const auto Run = [](float Left, float Right)
	{
		UWhisperCaptureComponent* C = MakeComponent();
		TArray<float> In;
		for (int32 i = 0; i < 481; ++i) { In.Add(Left); In.Add(Right); }
		TArray<float> Out;
		FAccess::Resample(*C, In.GetData(), 480, 2, 48000, Out);
		return Out;
	};

	const TArray<float> Unequal = Run(0.5f, -0.8f);
	TestEqual(TEXT("Mono count"), Unequal.Num(), 160);
	TestEqual(TEXT("Larger magnitude wins, sign kept"), Unequal.Num() ? Unequal[80] : 0.f, -0.8f, 1e-6f);
	TestEqual(TEXT("Not the average"), PeakAbs(Unequal, 0), 0.8f, 1e-6f);

	const TArray<float> Tie = Run(0.8f, -0.8f);
	TestEqual(TEXT("Tie keeps the first channel"), Tie.Num() ? Tie[80] : 0.f, 0.8f, 1e-6f);
	return true;
}

// The fractional phase carries across callbacks, so chunked input yields the same total as one
// long input to within one frame, with no drift. Believed correct: this is why ResamplePhase
// persists. 480-frame chunks at 44.1 kHz do not divide evenly, so the carry matters.
WHISPER_CAPTURE_TEST(ResamplePhaseCarry, "Resample.PhaseCarryHasNoDrift")
{
	UWhisperCaptureComponent* C = MakeComponent();
	const TArray<float> Chunk = MakeConstant(481, 0.25f); // one frame of slack, see F-017
	TArray<float> Out;
	for (int32 i = 0; i < 100; ++i)
	{
		FAccess::Resample(*C, Chunk.GetData(), 480, 1, 44100, Out);
	}
	const double Expected = 100.0 * 480 * CaptureRate / 44100;
	TestTrue(*FString::Printf(TEXT("%d samples after 100 chunks, expected %.2f +/- 1"), Out.Num(), Expected),
		FMath::Abs(Out.Num() - Expected) <= 1.0);
	const double Phase = FAccess::ResamplePhase(*C);
	TestTrue(TEXT("Carried phase stays within one output step"), Phase > -3.0 && Phase < 3.0);
	return true;
}

// No input, no output, no state change. Believed correct.
WHISPER_CAPTURE_TEST(ResampleEmptyInput, "Resample.EmptyInputIsNoOp")
{
	UWhisperCaptureComponent* C = MakeComponent();
	const float One = 1.0f;
	TArray<float> Out = {0.5f};
	FAccess::Resample(*C, &One, 0, 1, 48000, Out);
	FAccess::Resample(*C, &One, -3, 1, 48000, Out);
	FAccess::Resample(*C, nullptr, 480, 1, 48000, Out);
	TestEqual(TEXT("Output untouched"), Out.Num(), 1);
	TestEqual(TEXT("Phase untouched"), static_cast<float>(FAccess::ResamplePhase(*C)), 0.0f, 1e-12f);
	return true;
}

// F-017: at 44.1 kHz with 4410-frame callbacks the loop condition (double Phase < NumFrames)
// admits a tap whose float-rounded index equals NumFrames, one frame past the input. The last
// output sample is then whatever sits after the buffer; here a guard frame of 7.0 stands in for
// it, and it reaches the output. Merely current, and wrong: when F-017 is fixed this test must
// change to expect 1600 samples, all 0.25, with the guard never read.
WHISPER_CAPTURE_TEST(ResampleReadsPastInput, "Resample.ReadsOneFramePastInput")
{
	UWhisperCaptureComponent* C = MakeComponent();
	TArray<float> In = MakeConstant(4410, 0.25f);
	In.Add(7.0f); // guard frame, outside the 4410 the callback reports
	TArray<float> Out;
	FAccess::Resample(*C, In.GetData(), 4410, 1, 44100, Out);

	TestEqual(TEXT("One output more than 4410 * 16000 / 44100"), Out.Num(), 1601);
	if (Out.Num() == 1601)
	{
		TestEqual(TEXT("Samples before the last are the signal"), Out[1599], 0.25f, 1e-6f);
		TestEqual(TEXT("Last sample is the guard frame past the input (F-017)"), Out[1600], 7.0f, 1e-3f);
	}
	return true;
}

// ============================================================================
// RMS
// ============================================================================

// RMS equals the arithmetic root mean square to within 1e-4 on known buffers. Believed correct.
WHISPER_CAPTURE_TEST(RmsKnownValues, "Rms.KnownValues")
{
	const float Pair[] = {0.3f, 0.4f};
	TestEqual(TEXT("{0.3, 0.4}"), FAccess::ComputeRMS(Pair, 2), FMath::Sqrt(0.125f), 1e-4f);

	const float Square[] = {1.f, -1.f, 1.f, -1.f};
	TestEqual(TEXT("+/-1 square wave"), FAccess::ComputeRMS(Square, 4), 1.0f, 1e-4f);

	const float Single[] = {-0.25f};
	TestEqual(TEXT("Single negative sample"), FAccess::ComputeRMS(Single, 1), 0.25f, 1e-4f);

	const TArray<float> Constant = MakeConstant(480, 0.5f);
	TestEqual(TEXT("Constant 0.5"), FAccess::ComputeRMS(Constant.GetData(), Constant.Num()), 0.5f, 1e-4f);

	// Whole cycles of a unit sine: amplitude / sqrt(2).
	const TArray<float> Sine = MakeSine(16000, 440.0, 1.0);
	TestEqual(TEXT("Unit sine, 440 whole cycles"), FAccess::ComputeRMS(Sine.GetData(), Sine.Num()), 0.70710678f, 1e-4f);
	return true;
}

// Nothing to measure reads as silence. Believed correct (a VU meter must not read NaN).
WHISPER_CAPTURE_TEST(RmsEmpty, "Rms.EmptyAndNull")
{
	const float Sample = 0.9f;
	TestEqual(TEXT("Zero count"), FAccess::ComputeRMS(&Sample, 0), 0.0f, 0.0f);
	TestEqual(TEXT("Negative count"), FAccess::ComputeRMS(&Sample, -4), 0.0f, 0.0f);
	TestEqual(TEXT("Null pointer"), FAccess::ComputeRMS(nullptr, 100), 0.0f, 0.0f);
	return true;
}

// The sum of squares is accumulated in float. On a full 30 s buffer (LogAudioDiagnostics measures
// the whole buffer) a steady 0.1 reads about 0.0999, an error near 1.3e-4 by sequential
// simulation, past the 1e-4 held on short buffers (F-019). The bound here is 1e-3 so the test
// holds whether or not the compiler vectorises the loop. Merely current.
WHISPER_CAPTURE_TEST(RmsLongBuffer, "Rms.LongBufferStaysWithinOneThousandth")
{
	const TArray<float> Constant = MakeConstant(30 * CaptureRate, 0.1f);
	TestEqual(TEXT("30 s of 0.1"), FAccess::ComputeRMS(Constant.GetData(), Constant.Num()), 0.1f, 1e-3f);
	return true;
}

// ============================================================================
// High-pass filter
// ============================================================================

// The coefficients at the default 100 Hz cutoff, pinned at what the float implementation yields
// (RBJ cookbook high-pass, Q = 0.7071, 16 kHz). Merely current to ~1e-6; believed correct that
// B0 + B1 + B2 = 0 (zero gain at DC) and B0 = B2.
WHISPER_CAPTURE_TEST(HighPassCoefficients, "HighPass.CoefficientsAt100Hz")
{
	UWhisperCaptureComponent* C = MakeComponent();
	FAccess::UpdateHighPass(*C);
	const FAccess::FHighPass Hp = FAccess::HighPass(*C);

	TestEqual(TEXT("B0"), Hp.B0, 0.97261363f, 2e-6f);
	TestEqual(TEXT("B1"), Hp.B1, -1.94522727f, 2e-6f);
	TestEqual(TEXT("B2"), Hp.B2, 0.97261363f, 2e-6f);
	TestEqual(TEXT("A1"), Hp.A1, -1.94447720f, 2e-6f);
	TestEqual(TEXT("A2"), Hp.A2, 0.94597745f, 2e-6f);
	TestEqual(TEXT("LastCutoff is the cutoff used"), Hp.LastCutoff, 100.0f, 1e-6f);

	TestEqual(TEXT("Numerator sums to zero (DC rejected)"), Hp.B0 + Hp.B1 + Hp.B2, 0.0f, 1e-6f);
	TestEqual(TEXT("B2 mirrors B0"), Hp.B2, Hp.B0, 1e-9f);
	return true;
}

// Cutoffs outside [20 Hz, 0.45 * 16 kHz = 7200 Hz] are clamped for the coefficients, but
// HpfLastCutoff records the requested value. Merely current: the property's ClampMin is 20,
// yet Blueprint can write any float and this is what then happens.
WHISPER_CAPTURE_TEST(HighPassClamp, "HighPass.CutoffIsClamped")
{
	UWhisperCaptureComponent* C = MakeComponent();

	C->HighPassCutoffHz = 10.0f;
	FAccess::UpdateHighPass(*C);
	FAccess::FHighPass Hp = FAccess::HighPass(*C);
	TestEqual(TEXT("10 Hz uses the 20 Hz B0"), Hp.B0, 0.99446177f, 2e-6f);
	TestEqual(TEXT("10 Hz uses the 20 Hz A1"), Hp.A1, -1.98889279f, 2e-6f);
	TestEqual(TEXT("10 Hz uses the 20 Hz A2"), Hp.A2, 0.98895413f, 2e-6f);
	TestEqual(TEXT("LastCutoff keeps the requested 10"), Hp.LastCutoff, 10.0f, 1e-6f);

	C->HighPassCutoffHz = 10000.0f;
	FAccess::UpdateHighPass(*C);
	Hp = FAccess::HighPass(*C);
	TestEqual(TEXT("10 kHz uses the 7200 Hz B0"), Hp.B0, 0.02008333f, 2e-6f);
	TestEqual(TEXT("10 kHz uses the 7200 Hz B1"), Hp.B1, -0.04016666f, 2e-6f);
	TestEqual(TEXT("10 kHz uses the 7200 Hz A1"), Hp.A1, 1.56101539f, 2e-6f);
	TestEqual(TEXT("10 kHz uses the 7200 Hz A2"), Hp.A2, 0.64134872f, 2e-6f);
	TestEqual(TEXT("LastCutoff keeps the requested 10000"), Hp.LastCutoff, 10000.0f, 1e-3f);
	return true;
}

// The filter's response, gate off. Believed correct: a 100 Hz Butterworth high-pass removes DC,
// is 3 dB down (0.7071) at its cutoff, about 0.24 an octave below, and passes speech.
// The impulse response pins the Direct Form I wiring (the header says DF II transposed, F-019).
// Gain figures are sampled peaks, deterministic here, hence the ranges.
WHISPER_CAPTURE_TEST(HighPassResponse, "HighPass.ResponseOfTheDefaultFilter")
{
	{
		UWhisperCaptureComponent* C = MakeComponent();
		IsolateStages(*C, true, false);
		TArray<float> Impulse = {1.f, 0.f, 0.f};
		FAccess::ApplyNoise(*C, Impulse);
		TestEqual(TEXT("h[0] = B0"), Impulse[0], 0.97261363f, 1e-5f);
		TestEqual(TEXT("h[1]"), Impulse[1], -0.05400229f, 1e-5f);
		TestEqual(TEXT("h[2]"), Impulse[2], -0.05246317f, 1e-5f);
	}
	{
		UWhisperCaptureComponent* C = MakeComponent();
		IsolateStages(*C, true, false);
		TArray<float> Dc = MakeConstant(CaptureRate, 0.5f);
		FAccess::ApplyNoise(*C, Dc);
		TestTrue(TEXT("DC decays to nothing within the second half"), PeakAbs(Dc, CaptureRate / 2) < 1e-3f);
	}

	struct FBand
	{
		double Hz;
		float MinGain, MaxGain;
	};
	const FBand Bands[] = {{50.0, 0.22f, 0.27f}, {100.0, 0.69f, 0.72f}, {1000.0, 0.97f, 1.01f}, {4000.0, 0.98f, 1.01f}};
	for (const FBand& Band : Bands)
	{
		UWhisperCaptureComponent* C = MakeComponent();
		IsolateStages(*C, true, false);
		TArray<float> Tone = MakeSine(CaptureRate, Band.Hz, 0.5);
		FAccess::ApplyNoise(*C, Tone);
		const float Gain = PeakAbs(Tone, CaptureRate / 2) / 0.5f;
		TestTrue(*FString::Printf(TEXT("%.0f Hz gain %.4f in [%.2f, %.2f]"), Band.Hz, Gain, Band.MinGain, Band.MaxGain),
			Gain >= Band.MinGain && Gain <= Band.MaxGain);
	}
	return true;
}

// With both stages off the buffer is untouched and no coefficient work is done. Believed
// correct. With only the gate on, the high-pass state still does not move.
WHISPER_CAPTURE_TEST(HighPassBypass, "HighPass.DisabledIsBypass")
{
	UWhisperCaptureComponent* C = MakeComponent();
	IsolateStages(*C, false, false);
	const TArray<float> Original = MakeSine(1000, 300.0, 0.7);
	TArray<float> Samples = Original;
	FAccess::ApplyNoise(*C, Samples);
	TestTrue(TEXT("Samples bit-identical"), Samples == Original);
	TestEqual(TEXT("Coefficients never computed"), FAccess::HighPass(*C).LastCutoff, -1.0f, 0.0f);

	IsolateStages(*C, false, true);
	FAccess::ApplyNoise(*C, Samples);
	TestEqual(TEXT("Gate alone leaves the high-pass history"), FAccess::Filter(*C).X1, 0.0f, 0.0f);
	TestEqual(TEXT("Gate alone leaves the coefficients"), FAccess::HighPass(*C).LastCutoff, -1.0f, 0.0f);
	return true;
}

// Coefficients are recomputed when the cutoff moves by more than 0.01 Hz, and not for less.
// Believed correct (the tolerance avoids recomputing every callback); the figures are merely current.
WHISPER_CAPTURE_TEST(HighPassRecompute, "HighPass.RecomputesOnCutoffChange")
{
	UWhisperCaptureComponent* C = MakeComponent();
	IsolateStages(*C, true, false);
	TArray<float> One = {0.f};

	FAccess::ApplyNoise(*C, One);
	TestEqual(TEXT("First use computes at 100 Hz"), FAccess::HighPass(*C).B0, 0.97261363f, 2e-6f);

	C->HighPassCutoffHz = 100.005f;
	FAccess::ApplyNoise(*C, One);
	TestEqual(TEXT("A 0.005 Hz change is ignored"), FAccess::HighPass(*C).LastCutoff, 100.0f, 1e-6f);

	C->HighPassCutoffHz = 300.0f;
	FAccess::ApplyNoise(*C, One);
	TestEqual(TEXT("300 Hz B0"), FAccess::HighPass(*C).B0, 0.92006552f, 2e-6f);
	TestEqual(TEXT("300 Hz A1"), FAccess::HighPass(*C).A1, -1.83373141f, 2e-6f);
	TestEqual(TEXT("300 Hz A2"), FAccess::HighPass(*C).A2, 0.84653062f, 2e-6f);
	return true;
}

// ============================================================================
// Noise gate
// ============================================================================

// A signal below the threshold (VadEnergyThreshold 0.01 when uncalibrated) settles at the
// attenuation gain, 0.1 by default; one above it is not attenuated. Believed correct.
WHISPER_CAPTURE_TEST(NoiseGateQuiet, "NoiseGate.AttenuatesBelowThreshold")
{
	UWhisperCaptureComponent* C = MakeComponent();
	IsolateStages(*C, false, true);
	TArray<float> Quiet = MakeConstant(CaptureRate, 0.001f);
	FAccess::ApplyNoise(*C, Quiet);
	TestEqual(TEXT("After 1 s the gain is the attenuation"), Quiet.Last() / 0.001f, 0.1f, 1e-3f);

	UWhisperCaptureComponent* Half = MakeComponent();
	IsolateStages(*Half, false, true);
	Half->NoiseGateAttenuation = 0.5f;
	TArray<float> QuietAgain = MakeConstant(CaptureRate, 0.001f);
	FAccess::ApplyNoise(*Half, QuietAgain);
	TestEqual(TEXT("NoiseGateAttenuation sets the settled gain"), QuietAgain.Last() / 0.001f, 0.5f, 1e-3f);
	return true;
}

WHISPER_CAPTURE_TEST(NoiseGateLoud, "NoiseGate.PassesAboveThreshold")
{
	UWhisperCaptureComponent* C = MakeComponent();
	IsolateStages(*C, false, true);
	TArray<float> Loud = MakeConstant(1000, 0.5f);
	FAccess::ApplyNoise(*C, Loud);
	TestEqual(TEXT("First sample untouched"), Loud[0], 0.5f, 1e-6f);
	TestEqual(TEXT("Last sample untouched"), Loud.Last(), 0.5f, 1e-6f);
	TestEqual(TEXT("Gate stays open"), FAccess::Filter(*C).Gain, 1.0f, 1e-6f);
	TestTrue(TEXT("Envelope follows the signal up"), FAccess::Filter(*C).Envelope > 0.4f);
	return true;
}

// The gate ramps, it does not cut: the gain starts open, the first quiet sample is attenuated by
// well under 1 %, and 200 ms later the gain is still above 0.2 (release 100 ms is a time constant,
// not a delay). Believed correct that it is smooth (the header promises no clicks); the exact
// figures (0.9994 and 0.222) are merely current.
WHISPER_CAPTURE_TEST(NoiseGateRamp, "NoiseGate.RampsRatherThanCuts")
{
	UWhisperCaptureComponent* C = MakeComponent();
	IsolateStages(*C, false, true);
	TArray<float> Quiet = MakeConstant(3200, 0.001f);
	FAccess::ApplyNoise(*C, Quiet);
	const float First = Quiet[0] / 0.001f;
	const float After200ms = Quiet[3199] / 0.001f;
	TestTrue(*FString::Printf(TEXT("First sample gain %.5f in [0.998, 1)"), First), First >= 0.998f && First < 1.0f);
	TestTrue(*FString::Printf(TEXT("Gain after 200 ms %.4f in [0.20, 0.24]"), After200ms),
		After200ms >= 0.20f && After200ms <= 0.24f);
	return true;
}

// A calibrated noise floor replaces the VAD threshold: threshold = floor * multiplier, 0.03 here.
// A 0.02 signal is above VadEnergyThreshold (passes uncalibrated) and below 0.03 (gated once
// calibrated). Believed correct: the header documents exactly this.
WHISPER_CAPTURE_TEST(NoiseGateCalibrated, "NoiseGate.CalibratedFloorOverridesVadThreshold")
{
	UWhisperCaptureComponent* Uncalibrated = MakeComponent();
	IsolateStages(*Uncalibrated, false, true);
	TArray<float> A = MakeConstant(CaptureRate, 0.02f);
	FAccess::ApplyNoise(*Uncalibrated, A);
	TestEqual(TEXT("Uncalibrated: 0.02 > 0.01 passes"), A.Last(), 0.02f, 1e-6f);

	UWhisperCaptureComponent* Calibrated = MakeComponent();
	IsolateStages(*Calibrated, false, true);
	FAccess::SetNoiseFloor(*Calibrated, 0.02f);
	TArray<float> B = MakeConstant(CaptureRate, 0.02f);
	FAccess::ApplyNoise(*Calibrated, B);
	TestEqual(TEXT("Calibrated: 0.02 < 0.02 * 1.5 is gated"), B.Last() / 0.02f, 0.1f, 1e-3f);
	return true;
}

// VadEnergyThreshold = 0 ("disable VAD filtering" per the header) still leaves the gate a
// threshold of 0.0001, not zero. Merely current: the gate still attenuates anything below 1e-4.
WHISPER_CAPTURE_TEST(NoiseGateZeroVad, "NoiseGate.ThresholdIsFlooredAtOneTenThousandth")
{
	UWhisperCaptureComponent* Above = MakeComponent();
	IsolateStages(*Above, false, true);
	Above->VadEnergyThreshold = 0.f;
	TArray<float> A = MakeConstant(CaptureRate, 0.001f);
	FAccess::ApplyNoise(*Above, A);
	TestEqual(TEXT("0.001 is above the 1e-4 floor and passes"), A.Last(), 0.001f, 1e-7f);

	UWhisperCaptureComponent* Below = MakeComponent();
	IsolateStages(*Below, false, true);
	Below->VadEnergyThreshold = 0.f;
	TArray<float> B = MakeConstant(CaptureRate, 0.00005f);
	FAccess::ApplyNoise(*Below, B);
	TestEqual(TEXT("0.00005 is below it and is gated"), B.Last() / 0.00005f, 0.1f, 1e-3f);
	return true;
}
