# C-elevenlabs-protocol: ElevenLabs request building and stream decoding
- As of: J-0027
- Version: 1
- Provider: M-tts
- Consumers: M-ace, M-tests
- Tests: FANTASIA/Source/FANTASIATests/Private/ElevenLabsProtocolTests.cpp
- Provenance: recovered

## Specification

Header: `ElevenLabsProtocol.h`. Plain C++, no reflection: a pure protocol layer that
turns configuration into a request and bytes into audio and timing. It is the one piece
of M-tts that M-ace reuses directly rather than by inheritance.

- `struct FElevenLabsVoiceSettings` — stability, similarity boost, style, speaker boost
  and speed, with the defaults pinned by `Types.FElevenLabsVoiceSettings.Defaults`
- `class FANTASIA_API FElevenLabsProtocol`:
  - `static FTTSSynthesisRequest BuildRequest(const FString& VoiceID, const FString& ModelID, ...)`
    — builds URL, headers and body, including the language code and the speed from the
    voice settings
  - `static FString ApplyPronunciations(const TMap<FString, FString>& Pronunciations, const FString& Text)`
  - `static TSharedPtr<FTTSStreamDecoder> MakeStreamDecoder()`
  - `static void ParseOffline(const TArray<uint8>& Json, FTTSData& OutResult)`
  - `static TArray<FTTSSegmentTiming> WordsFromCharacters(const TArray<FTTSSegmentTiming>& Characters)`

## Errors

- `ParseOffline` on malformed JSON leaves `OutResult` at its default and reports
  nothing; the caller checks the result rather than a status.
- The stream decoder holds an incomplete server-sent line until the rest arrives
  (pinned by `StreamDecoder.PartialLine`). A line that never completes is dropped at
  end of stream.
- `ApplyPronunciations` on empty text returns empty text (pinned by
  `ApplyPronunciations.EmptyText`).

## Invariants

- Every function is static and holds no state. The only stateful object is the stream
  decoder, which is owned by its caller and is not thread-safe.
- `ApplyPronunciations` matches **whole words only**, case-insensitively, and restores
  the original leading capitalisation on the replacement. Text is tokenised on
  `FChar::IsAlnum` boundaries, so a key occurring inside a longer word is not replaced.
  It is correct with several keys present (pinned by `ApplyPronunciations.MultipleKeys`).
  Accented glyphs are an unresolved edge case the implementation flags.
- `BuildRequest` puts the speed in the voice settings and the language code in the
  request body (pinned by `BuildRequest.Speed` and `BuildRequest.LanguageCode`), and
  targets the streaming endpoint for the given voice (pinned by `BuildRequest.URL`).
- Audio chunks arrive base64-encoded and are decoded to PCM16.
- `WordsFromCharacters` groups character timings into words on whitespace: an empty
  input gives an empty result, a single character gives one word, runs of spaces do not
  produce empty words, and a trailing space does not produce a trailing word (all four
  pinned by tests).
- Character and word timings are internal. Exposing them to Blueprints is a change
  request against ADR-0010, not an addition.
