# C-probabilistic: Bayesian networks and influence diagrams
- As of: J-0027
- Version: 1
- Provider: M-probabilistic
- Consumers: M-demo, M-tests
- Tests: FANTASIA/Source/FANTASIATests/Private/BayesianNetworkTests.cpp; FANTASIA/Source/FANTASIATests/Private/InfluenceDiagTests.cpp
- Provenance: recovered

## Specification

This contract is also part of the plugin's public Blueprint surface: it is what projects using FANTASIA call. A change to it invalidates compiled Blueprint assets in user projects, so the consumer list below understates who is affected.

Headers: `BayesianNetwork.h`, `InfluenceDiag.h`. The node-type and algorithm enums come
from C-fantasia-types; the kernel density estimator comes from C-math.

`UBayesianNetwork : UObject`:

- Types: `FMapContainer` (`Map`), `FBayesianArcStruct` (`Tail`, `Head`),
  `FBayesianNodeStruct` (`name`, `variables`, `values`, `parents`)
- State: `serializedNodes`, `nodeNames`, `nodeDescriptions`, `arcs`,
  `InferenceAlgorithm` (default `ShaferShenoy`)
- Two completion events, not one: the Blueprint delegate `InferenceReady`, and the
  C++-only `FSimpleMulticastDelegate OnInferenceReady`. M-demo binds the C++ one.
- Build: `setBN(const FString& Filename)`, `Init()`,
  `addDiscretizedVariable(variable, description, minTick, maxTick, nPoints, BayesianNodeType)`,
  `addLabelizedVariable(variable, description, labels)`, `addArc(parent, child)`,
  `fillWith(variable, value)`, `fillCPT(variable, values)`, `erase(variable)`
- Inference: `makeInference()`, `getPosterior(variable) -> TMap<FString, float>`,
  `setEvidence(variable, data)`, `eraseEvidence(variable)`, `eraseAllEvidence()`
- Query: `getEntropy(variable)`, `idFromName(variable)`,
  `getMarkovBlanketNodes(variable)`, `writeBIF(file)`

`UInfluenceDiag : UObject`:

- Types: `FInfluenceDiagArcStruct` (`Tail`, `Head`), `FInfluenceDiagNodeStruct`
  (`name`, `variables`, `values`, `parents`, `nodeType` default `CHANCE`),
  `FArrayFloat` (`arrayFloat`)
- State: `serializedNodes`, `nodeNames`, `nodeDescriptions`, `arcs`
- Two completion events, as above: `InferenceReady` for Blueprints and the C++-only
  `OnInferenceReady`.
- Build: `Init()`, `addDiscretizedVariable(...)`,
  `addLabelizedVariable(variable, description, labels, InfluenceNodeType)`,
  `addArc(parent, child)`, `fillCPT(variable, values)`,
  `fillUtility(variable, values)`, `erase(variable)`
- Inference: `makeInference()`, `getPosterior(variable)`,
  `getPosteriorUtility(variable)`, `getMEU()`, `optimalDecision(variable) -> TMap<FString, FArrayFloat>`,
  `addEvidence(variable, data)`, `eraseEvidence(variable)`, `eraseAllEvidence()`
- Query: `getEntropy(variable)`, `idFromName(variable)`, `decisionOrderExists()`,
  `decisionOrder()`, `writeBIFXML(file)`

## Errors

- A duplicate variable name or a duplicate arc is rejected and logged; the model is
  left unchanged (pinned by `DuplicateVariable`, `DuplicateArc`, `DuplicateNode`).
- `erase` on an absent variable is a no-op; erasing a present one removes its arcs with
  it and leaves the model consistent (pinned by `EraseNode`).
- `idFromName` on an unknown name returns a negative id.
- `getPosterior` before `makeInference()` returns an empty map.
- `decisionOrderExists()` false means the diagram has no valid decision order and
  `optimalDecision` is meaningless.
- aGrUM exceptions are caught at the boundary and logged; they do not propagate into
  Blueprint code. Exceptions are enabled in this module for exactly that reason.

## Invariants

- `Init()` must be called before any inference, and again after the structure changes.
- `makeInference()` is synchronous but dispatched off the game thread; both completion
  events are broadcast on the game thread when the posteriors are available. A C++
  consumer must bind `OnInferenceReady`, not the Blueprint delegate.
- Inference is exact: Lazy Propagation, Shafer-Shenoy or Variable Elimination for
  networks, Shafer-Shenoy LIMID for diagrams. Given the same model and evidence the
  result is deterministic.
- A posterior map is keyed by variable label and sums to one over the variable's domain.
- `fillCPT` takes values in the row-major order aGrUM expects over the node's parents;
  a wrong-length array is rejected.
- Discretised variables are defined by `minTick`, `maxTick` and `nPoints`; ticks are the
  bin edges, so a variable has `nPoints - 1` bins.
- aGrUM 2.x is linked as per-module static libraries in the order ID, BN, BASE
  (ADR-0009). The module consumes `C-build` for that.
- `WITH_AGRUM_BINDING` is defined by M-dist and honoured by the tests and by M-demo,
  but **not** by this module's own sources, which include aGrUM headers
  unconditionally. The define therefore cannot disable the binding (F-005).
