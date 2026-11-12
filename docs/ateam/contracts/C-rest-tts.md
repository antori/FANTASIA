# C-rest-tts: Streaming speech synthesis over HTTP
- As of: J-0027
- Version: 1
- Provider: M-tts
- Consumers: M-ace, M-demo, M-tests
- Tests: FANTASIA/Source/FANTASIATests/Private/Pcm16StreamDecoderTests.cpp
- Provenance: recovered

## Specification

Headers: `RESTTTSComponent.h`, `ElevenLabsTTSComponent.h`. This contract is both the
application-facing surface and the extension point M-ace derives from, which is why it
is the only contract in the project whose subclassing behaviour is part of the
agreement.

Read the division of labour first, because it is not what the class names suggest:
`URESTTTSComponent` synthesizes, decodes, resamples, buffers per call and records
submission order. It does **not** consume those buffers. Ordering, pacing,
`PlaybackComplete` and a non-empty `EndTurn()` all come from a subclass that supplies a
consumer, and the only one in the repository is in M-ace (F-009).

`URESTTTSComponent : UActorComponent` — the generic base:

- Configuration: `SampleRate` (default 16000), `bStreaming` (default false), `URL`
  (Config), `BodyTemplate` (Config), and `Headers`, a string map that is **not** marked
  Config even though it is where an API key goes (F-008)
- Events: `SynthesisReady(FString id)`, `PartialSynthesisReady(FString id)`,
  `PlaybackComplete(FString id)`
- Calls: `TTSSynthesize(FString text, FString id, bool bEndTurn = false)`,
  `virtual EndTurn()`, `TTSGetSound(FString id) -> USoundWaveProcedural*`,
  `TTSGetRawSound(FString id) -> TArray<uint8>`

`UElevenLabsTTSComponent : URESTTTSComponent` — the shipped implementation, adding
`VoiceID`, `ModelID`, `Key`, `language_code`, `stability`, `similarity_boost`, `style`,
`use_speaker_boost`, `speed` (default 1.0) and `PronunciationMap`.

Extension points for a subclass, five virtuals and the protected state they work on:

- `virtual FTTSSynthesisRequest BuildSynthesisRequest(const FString& Text, const FString& ID)`
- `virtual void ProcessResponse(const TArray<uint8>& RawResponse, FTTSData& OutResult)`
- `virtual void WakeConsumer()` — empty in core; a subclass with a consumer uses it to be
  woken when samples land
- `virtual TSharedPtr<FTTSStreamDecoder> CreateStreamDecoder()`
- `virtual void EndTurn()` — empty in core
- protected: `BuffersByCaller`, `BuffersMutex`, `PlaybackOrder`, `CurrentPlayback`, the
  `FAudioBuffer`, `FPlaybackOrderEntry` and `FTTSStreamDecoder` types, and
  `A2F_STREAMING_BATCH_SIZE`

M-ace overrides five of them and drains the protected buffers directly. A subclass that
overrides none gets synthesis and buffering and no playback behaviour.

## Errors

- An HTTP failure is logged and nothing is handed to the audio pipeline. No event
  fires for the failed call, so a caller waiting on `SynthesisReady` for that id waits
  forever; it must not treat the event as guaranteed.
- `PlaybackComplete` never fires at all without a subclass consumer, however the call
  succeeded. A core-only application must drive playback from `TTSGetSound` and the
  synthesis events (F-009).
- `TTSGetSound` / `TTSGetRawSound` with an unknown id return null / an empty array.
- A malformed or partial stream chunk is carried over to the next read rather than
  discarded; a chunk that never completes is dropped silently at end of stream.
- An odd trailing byte in a PCM16 stream is carried, not dropped (pinned by
  `Pcm16StreamDecoder.OddByteCarry`).

## Invariants

- Each `TTSSynthesize` call gets its own buffer, keyed by `id`. Buffers are never
  shared or reused across calls (ADR-0002).
- Submission order is recorded, in `PlaybackOrder`, in request order. Whether playback
  follows that order depends entirely on the consumer, because this component has none.
- Pacing is the consumer's. M-ace sleeps for 95% of each batch's duration; nothing in
  this component touches an audio device or a clock.
- `PlaybackComplete` is declared here and broadcast only by M-ace, once per id, after
  the last chunk of that id has been drained. The delegate's own doc comment claims core
  fires it immediately after synthesis; it does not (F-009).
- **A turn must be closed.** Either pass `bEndTurn = true` on the last
  `TTSSynthesize` of the turn, or call `EndTurn()`. Omitting it truncates the tail of
  the current utterance and lets it bleed into the next one. The base component
  tolerates omission; M-ace does not, because an open turn leaks the Audio2Face
  session (see C-ace-tts).
- Output is always mono 16 kHz. A WAV header is auto-detected and the audio resampled
  by linear interpolation when it is at another rate; `SampleRate` is the assumed input
  rate for a headerless stream only, and `TTSGetSound` hard-codes 16 kHz regardless.
- Word and character timing is captured but not exposed here (ADR-0010).
- `Headers` and `Key` are Config UPROPERTYs. No key is ever defaulted to a real value
  or logged (P-15).
- The ElevenLabs configuration surface listed above is duplicated verbatim in
  `UACEElevenLabsTTSComponent` and kept in step by hand. That is a recorded defect
  (F-003); until it is fixed, a change to any of those ten properties is a change to
  two modules.
