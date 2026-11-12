# C-llm: Language model and agent-graph access
- As of: J-0027
- Version: 1
- Provider: M-llm
- Consumers: M-demo, M-tests
- Tests: FANTASIA/Source/FANTASIATests/Private/OpenAICompatibleConfigTests.cpp
- Provenance: recovered

## Specification

This contract is also part of the plugin's public Blueprint surface: it is what projects using FANTASIA call. A change to it invalidates compiled Blueprint assets in user projects, so the consumer list below understates who is affected.

Headers: `OpenAICompatibleComponent.h`, `LangGraphComponent.h`. Both take and return
`FChatTurn` and `GPTRoleType` from C-fantasia-types.

`UOpenAICompatibleComponent : UActorComponent`:

- `enum class ELLMProvider : uint8` — the supported presets
- `struct FOpenAICompatibleConfig` — `URL` (default
  `https://api.openai.com/v1/chat/completions`), `APIKey`, `DefaultModel` (default
  `gpt-4-turbo-preview`), `TimeoutSeconds` (default 60), `bRequiresAuth` (default true)
- `Config` — the property holding the struct above. It is **not** marked Config, and
  `LoadConfig` walks only the class's own Config properties, so the `Config` specifiers
  on the struct's fields never fire: `APIKey` is stored in the owning asset's defaults
  and is committed with it. Every other secret-bearing property in the plugin carries
  Config on the owning property; this one does not (F-008)
- Events: `IncomingGPTResponse`, `IncomingGPTStreamResponse`, `IncomingSentenceResponse`
- `GetChatCompletion(const TArray<FChatTurn>& messages, bool bStream = false, const FString& ModelOverride = TEXT(""))`
- `ApplyPreset(ELLMProvider Provider)`
- `static MakeOpenAIConfig()`, `MakeGroqConfig()`, `MakeOllamaConfig()`,
  `MakeLMStudioConfig()`, `MakeVLLMConfig()`

`ULangGraphComponent : UActorComponent`:

- `endpoint`, `port`
- Events: `IncomingLangGraphResponse(FString, GPTRoleType)`,
  `IncomingLangGraphStreamResponse(FString, GPTRoleType, bool endStream)`,
  `IncomingLangGraphSentenceResponse(same signature)`,
  `LangGraphThreadCreateResponse(FString threadID)`
- `createLangGraphThread(FString threadID)`, `deleteLangGraphThread(FString threadID)`
- `langGraphQuery(const TArray<FChatTurn>& messages, FString threadID, FString assistantID, FString model = "openai/gpt-4o", int recursionLimit = 10, bool stream = false)`

## Errors

- An HTTP failure or a non-success status is logged; no response event fires. Callers
  must not assume a reply for every request.
- A malformed stream chunk is skipped. The stream ends with `endStream = true` whether
  or not content arrived.
- `ApplyPreset` overwrites `Config` wholesale, `APIKey` included; a key set before the
  call is lost.

## Invariants

- The five preset factories return exactly their provider's endpoint, default model and
  authentication requirement, each pinned by a test
  (`Preset.OpenAI`, `Preset.Groq`, `Preset.Ollama`, `Preset.LMStudio`, `Preset.vLLM`).
  `MakeOllamaConfig` and `MakeLMStudioConfig` are the local ones and do not require
  auth.
- Requests are issued off the game thread; every event is broadcast on the game thread.
- Stream deltas arrive in order on `IncomingGPTStreamResponse` /
  `IncomingLangGraphStreamResponse`, and the sentence events carry whole sentences
  assembled from those deltas.
- LangGraph stream chunks are parsed in both shapes the server emits, including
  Anthropic content-block arrays, and are broadcast once the turn is complete rather
  than per fragment. No ADR records this; it is recovered from
  `LangGraphComponent.cpp`.
- Conversation history is the caller's: neither component keeps it between calls.
- `APIKey` is never logged. It is **not** safe from being committed, contrary to P-15,
  until F-008 is fixed.
- Only the preset configuration is tested. Request construction, streaming and the
  LangGraph thread lifecycle have no tests at adoption; the behaviour above is read
  from the code.
