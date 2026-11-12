# C-prolog: Embedded Prolog reasoning
- As of: J-0027
- Version: 1
- Provider: M-prolog
- Consumers: M-demo, M-tests
- Tests: FANTASIA/Source/FANTASIATests/Private/SWIPrologHelpersTests.cpp
- Provenance: recovered

## Specification

This contract is also part of the plugin's public Blueprint surface: it is what projects using FANTASIA call. A change to it invalidates compiled Blueprint assets in user projects, so the consumer list below understates who is affected.

Headers: `SWIPrologComponent.h`, `SWIPrologHelpers.h`, `SWIPrologThread.h`. The term
objects themselves live in C-fantasia-types.

`USWIPrologComponent : UActorComponent`:

- Events: `SolutionAvailable(USWIPrologSolution*)`,
  `AllSolutionsReady(const TArray<USWIPrologSolution*>&)`,
  `QueryFailed(FString ErrorMessage)`
- Query: `SWIPLsubmitQuery(USWIPrologTerm* inRuleOrFact)`,
  `SWIPLqueryString(const FString& Query)`, `SWIPLfindAll(const FString& Query)`,
  `nextSolution()`
- Knowledge base: `SWIPLassert(USWIPrologObject* prologObject, bool asFirstClause)`,
  `SWIPLretract(USWIPrologTerm* ruleOrFact)`,
  `openPrologFile(const FString& filename)`,
  `SWIPLconsultString(const FString& PrologCode)`

`USWIPrologHelpers : UBlueprintFunctionLibrary` — term construction:

- `MakeAtom(const FString& Value) -> USWIPrologAtom*`
- `MakeVariable(const FString& Name) -> USWIPrologVariable*`
- `MakeCompound(const FString& Name, const TArray<USWIPrologTerm*>& Arguments) -> USWIPrologCompound*`
- `MakeInteger(int32 Value) -> USWIPrologInteger*`
- `MakeFloat(float Value) -> USWIPrologFloat*`
- `MakeList(const TArray<USWIPrologTerm*>& Elements) -> USWIPrologList*`

`SWIPrologThread.h` declares the worker that owns the engine. It is public only because
the component holds one; applications do not use it directly.

## Errors

- A query that cannot be parsed or run fires `QueryFailed` with the engine's message.
  No exception crosses the boundary.
- `nextSolution()` past the last solution fires `AllSolutionsReady` rather than
  `SolutionAvailable`.
- If the SWI-Prolog runtime DLLs are not deployed next to the plugin binaries, engine
  initialisation fails with a logged diagnostic at editor startup and every subsequent
  query fails. This is the single most common installation failure
  (README troubleshooting).
- `openPrologFile` on a missing path fails with a logged error.

## Invariants

- One Prolog engine per component, running on its own thread. SWI-Prolog is not
  re-entrant, so queries are serialised in submission order.
- Queries are submitted from the game thread and solutions are broadcast on the game
  thread; the engine never runs on the game thread.
- `SWIPLsubmitQuery` is iterative: solutions arrive one at a time through
  `SolutionAvailable`, each pull driven by `nextSolution()`. `SWIPLfindAll` is the
  batch form and fires `AllSolutionsReady` once.
- `USWIPrologSolution::verified` says whether the solution holds; `resultSet` carries
  the bindings as strings in the order the variables appear in the query.
- Term construction is pure and allocation-only: building a term touches no engine.
  Rendering a built term gives its Prolog form, pinned for atoms, variables, integers,
  floats, compounds, lists and nested compounds.
- Asserting with `asFirstClause` true places the clause before existing ones
  (`asserta`), false after them (`assertz`).
- Only the helper library is tested. The component, the thread and the engine lifecycle
  have no tests at adoption.
