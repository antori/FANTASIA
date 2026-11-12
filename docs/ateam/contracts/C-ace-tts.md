# C-ace-tts: Speech-driven facial animation
- As of: J-0027
- Version: 1
- Provider: M-ace
- Consumers: none
- Tests: FANTASIAACE/Source/FANTASIAACETests/Private/ACERESTTTSComponentTests.cpp
- Provenance: recovered

## Specification

This contract is also part of the plugin's public Blueprint surface: it is what projects using FANTASIA call. A change to it invalidates compiled Blueprint assets in user projects, so the consumer list below understates who is affected.

Headers: `ACERESTTTSComponent.h`, `ACEElevenLabsTTSComponent.h`, `FANTASIAACE.h`.
Both components extend C-rest-tts and keep its whole surface; what follows is what they
add.

`UACERESTTTSComponent : URESTTTSComponent`:

- `A2Fpointer` (`UACEAudioCurveSourceComponent*`) — the curve source on the character
- `A2FParameters` (`UAudio2FaceParameters*`)
- `A2FProvider` (`FName`) — the Audio2Face provider id, for example `LocalA2F-Mark`
- `EmotionParameters` (`FAudio2FaceEmotion`)

`UACEElevenLabsTTSComponent : UACERESTTTSComponent` — adds `VoiceID`, `ModelID`, `Key`,
`language_code`, `stability`, `similarity_boost`, `style`, `use_speaker_boost`, `speed`
(default 1.0) and `PronunciationMap`.

`class FFANTASIAACEModule : public IModuleInterface` — the companion plugin's entry.

## Errors

- With `A2Fpointer` unset or `A2FProvider` naming a provider that is not installed, no
  animation is produced; synthesis and playback still work. The failure is logged, not
  raised.
- If `NV_ACE_Reference` is not enabled, this module does not load and the core plugin
  is unaffected: core FANTASIA never references ACE (ADR-0005).
- An A2F session that fails to open leaves the audio audible and the face still.

## Invariants

- **End of turn is mandatory here.** `bEndTurn = true` on the last `TTSSynthesize` of a
  turn, or an explicit `EndTurn()`, closes the Audio2Face session. Omitting it cuts the
  tail of the current utterance short *and* lets it bleed into the next playback: the
  session stays open and the next turn's audio joins it. C-rest-tts tolerates omission;
  this contract does not. Projects migrating from the pre-split, non-ACE ElevenLabs
  component must add the call.
- Neither header includes `FANTASIATypes.h`, but `ACEElevenLabsTTSComponent` names
  `FTTSData` in a public override signature, so this module depends on C-fantasia-types
  directly as well as through C-rest-tts. The build agrees: `FANTASIAACE.Build.cs` lists
  `FANTASIA` as a public dependency.
- What this module supplies to its base class is the consumer that base class lacks:
  the drain loop, the pacing, `PlaybackComplete` and a non-empty `EndTurn()` are all
  here (F-009). A project using core FANTASIA without this plugin gets none of them.
- Audio samples reach Audio2Face as they are decoded, so animation is produced while
  the utterance is still streaming (NFR-003).
- The consumer task is joined before `EndPlay` returns, so no A2F callback outlives the
  component.
- In-flight samples are not discarded when the stream completes; the tail is played out.
- Provider ids are installation-specific and come from the characters installed in the
  engine's `Plugins/Marketplace/` folder, not from this plugin.
- `EmotionParameters` is Blueprint-accessible and applies to the next session opened,
  not retroactively to one in progress.
- `Key` and the other ElevenLabs settings here duplicate `UElevenLabsTTSComponent`
  field for field (F-003). This component inherits from `UACERESTTTSComponent`, not
  from `UElevenLabsTTSComponent`, so the duplication is structural and the two must be
  changed together until it is resolved.
- This module has no tests at adoption and no test module exists yet; the `Tests:` path
  above is where one belongs.
