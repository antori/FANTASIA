# C-asr: Local speech recognition
- As of: J-0027
- Version: 1
- Provider: M-asr
- Consumers: M-demo, M-tests
- Tests: FANTASIA/Source/FANTASIATests/Private/WhisperBlueprintLibraryTests.cpp
- Provenance: recovered

## Specification

This contract is also part of the plugin's public Blueprint surface: it is what projects using FANTASIA call. A change to it invalidates compiled Blueprint assets in user projects, so the consumer list below understates who is affected.

Headers: `WhisperTypes.h`, `WhisperSubsystem.h`, `WhisperCaptureComponent.h`,
`WhisperBlueprintLibrary.h`. `WhisperBackend.h` is private and not part of this
contract.

Types (`WhisperTypes.h`):

- `EWhisperLanguage`, `EWhisperModelSize`, `EWhisperTask`, `EWhisperBackend`,
  `EWhisperStatus`
- `FWhisperSegment` (`StartTime`, `EndTime`, `Text`, `Confidence`)
- `FWhisperResult` (`bSuccess`, `FullText`, `Language`, `Segments`,
  `ProcessingTimeSeconds`, `ErrorMessage`)
- `FWhisperConfig` (`Language` = `Auto`, `Task` = `Transcribe`, `NumThreads` = 0,
  `MaxSegmentLength` = 0, `bTokenTimestamps` = false, `InitialPrompt`,
  `bSuppressNonSpeech` = true, `Temperature` = 0)
- `FWhisperModelInfo` (`FileName`, `DisplayName`, `FilePath`, `Size`, `Version`,
  `Quantization`, `bEnglishOnly`)
- Delegates: `FOnWhisperModelLoaded(bool bSuccess)`,
  `FOnWhisperTranscriptionComplete(const FWhisperResult&)`,
  `FOnWhisperSegmentReady(const FWhisperSegment&)`,
  `FOnWhisperStatusChanged(EWhisperStatus)`,
  `FOnWhisperNoiseCalibrationComplete(float NoiseLevel, float NewThreshold)`

`UWhisperSubsystem : UGameInstanceSubsystem` — one per game instance:

- Events: `OnModelLoaded`, `OnTranscriptionComplete`, `OnSegmentReady`, `OnStatusChanged`
- Model: `LoadModel(const FString& ModelFilePath)`,
  `LoadModelBySize(EWhisperModelSize)`, `LoadModelByName(const FString&)`,
  `UnloadModel()`, `IsModelLoaded() const`, `GetStatus() const`
- Backend selection: `Backend` (`EWhisperBackend`, default `CPU`), `GPUDeviceIndex`
  (default 0), and a Blueprint-pure query for whether a GPU backend library is present
- Transcription: `TranscribeAudioFile(const FString& WavFilePath, FWhisperConfig)`,
  `TranscribeAudioData(const TArray<float>& PCMData, FWhisperConfig)`
- Audio helpers: `LoadWavFile(const FString&, TArray<float>& OutSamples)`,
  `static ConvertInt16ToFloat(const TArray<uint8>&, TArray<float>& OutSamples)`

`UWhisperCaptureComponent : UActorComponent` — live microphone capture:

- Configuration: `TranscriptionConfig`, `bStreamingMode`, window and overlap seconds,
  maximum capture seconds, `bAutoDetectSpeech` with a silence duration,
  `AudioDeviceIndex` (default -1), `bEnableHighPassFilter` with cutoff,
  `bEnableNoiseGate` with threshold, ratio, attack and release
- Events: `OnCaptureTranscriptionComplete`, `OnStreamingSegment`,
  `OnNoiseCalibrationComplete`
- Control: `StartCapture()`, `StopCapture()`, `CancelCapture()`, `TranscribeBuffer()`,
  `ClearBuffer()`, `SetMuted(bool)`, `IsMuted() const`, `IsCapturing() const`
- Diagnostics: noise calibration, `GetNoiseFloor() const`, `GetCurrentAudioLevel()`,
  a buffer-duration query, a diagnostics log call, and
  `static GetAvailableInputDevices()`

`UWhisperBlueprintLibrary : UBlueprintFunctionLibrary` — pure helpers over a result:

- `GetFullText`, `GetSegmentCount(const FWhisperResult&)`,
  `GetSegment(const FWhisperResult&, int32 Index)`,
  `GetTotalDuration(const FWhisperResult&)`
- `FormatAsSRT(const FWhisperResult&)`, `FormatAsVTT(const FWhisperResult&)`
- `MakeDefaultConfig()`, `MakeRealtimeConfig()`
- `GetModelFileName(EWhisperModelSize, bool bEnglishOnly = false)`,
  `DoesModelFileExist(const FString&)`, `GetAvailableModels()`,
  `ParseModelFileName(const FString&)`

## Errors

- A failed transcription is reported through `FWhisperResult`: `bSuccess` false and
  `ErrorMessage` set. No exception crosses this boundary.
- A failed model load fires `OnModelLoaded(false)` and leaves `GetStatus()` reporting
  the error state; `IsModelLoaded()` stays false.
- If `Backend` is `GPU` and the CUDA library is absent, a warning is logged and
  inference proceeds on CPU. Callers are not told; they see a normal result (ADR-0006,
  ADR-0011). Only the library's *presence* is checked: if it loads but the CUDA runtime
  does not resolve, `use_gpu` is passed through and no warning is logged, which the
  subsystem header says of itself.
- `StartCapture()` returns false when the audio device cannot be opened, and also when
  capture is already running.
- `GetSegment` with an out-of-range index returns a default-constructed segment.

## Invariants

- Audio is 16 kHz mono float PCM in `TranscribeAudioData`. Callers convert.
- `Backend` is read at `LoadModel` time. Changing it afterwards has no effect until the
  model is reloaded.
- Transcription runs off the game thread; every delegate is broadcast on the game
  thread.
- Segment times are seconds from the start of the submitted audio, and
  `StartTime <= EndTime`.
- SRT and VTT timestamps carry hours correctly past one hour (pinned by
  `SRT.HourTimestamp`).
- `AudioDeviceIndex` of -1 means the operating system's default input device.
- `bEnableHighPassFilter` and `bEnableNoiseGate` both default to **true**, so the audio
  reaching recognition is filtered unless a caller turns them off.
- No network call is made by this module, ever. That is the point of it (G-4, US-001).
- The subsystem holds the single model instance; concurrent transcription requests are
  serialised by it.
- Four further public subsystem members are outside this contract because nothing in
  the repository calls them: `TranscribeSync`, `GetWhisperSampleRate()`,
  `LanguageToString` and `GetDefaultModelDirectory`.
- `UWhisperCaptureComponent` and the backend dispatcher have no tests at adoption
  (F-007). Every invariant above that no test name is cited for is read from the code,
  not pinned.
