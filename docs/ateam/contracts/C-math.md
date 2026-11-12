# C-math: Density estimation and support vector machines
- As of: J-0027
- Version: 1
- Provider: M-ml
- Consumers: M-probabilistic, M-demo, M-tests
- Tests: FANTASIA/Source/FANTASIATests/Private/MathUtilitiesTests.cpp; FANTASIA/Source/FANTASIATests/Private/LibSVMTests.cpp
- Provenance: recovered

## Specification

This contract is also part of the plugin's public Blueprint surface: it is what projects using FANTASIA call. A change to it invalidates compiled Blueprint assets in user projects, so the consumer list below understates who is affected.

Headers: `MathUtilities.h`, `LibSVM.h`.

`UKernelDensityEstimator : UObject`:

- `setData(TArray<float> newData)`
- `evaluate(float point) -> float`
- `getPdf(float min, float max, int points) -> TMap<float, float>`

`Linspace` is a free function at global scope, not a class member and not a
`UFUNCTION`:

- `FANTASIA_API std::vector<float> Linspace(float Start, float End, int Points)`

It returns `std::vector<float>`, so it is reachable only from C++ and crosses the module
boundary in a standard-library type (F-010). `UKernelDensityEstimator` is the only
`UCLASS` in `MathUtilities.h`, so the Blueprint-facing half of this contract is the
estimator and `ULibSVM` alone.

`ULibSVM : UObject`:

- Types: `FCLFSample` (`sampleID`, `CLFclass`, `features`), `FCLFResult` (`sampleID`,
  `probabilities`), `LibSVMTypes`, `LibSVMKernels`, `FSVMParameters` (`SVMType` =
  `C_SVC`, `KernelType` = `RBF`, `C` = 1, `gamma` = 0.1, `degree` = 3, `coef0` = 0,
  `nu` = 0.5, `svrEpsilon` = 0.1, `cachesize` = 100, `epsilon` = 0.001, `shrinking` =
  true)
- State: `trainingSet`, `SVMParameters`
- Calls: `train()`, `predict(const TArray<FCLFSample>& samples) -> TArray<FCLFResult>`,
  `load(FString path)`

## Errors

- `predict` with no trained or loaded model returns an empty array and logs the reason
  (pinned by `PredictNoModel`).
- `predict` with an empty sample array returns an empty array (pinned by
  `PredictEmptySamples`).
- `load` on a missing or malformed model file leaves the estimator without a model and
  logs; the next `predict` then behaves as above.
- `evaluate` before `setData` returns zero.

## Invariants

- `getPdf` returns exactly `points` entries spanning `[min, max]` inclusive, keyed by
  sample position.
- `Linspace` returns exactly the requested number of points, inclusive of both bounds,
  and handles a negative range (all four cases pinned: two, three and five points, and
  a negative range). It is C++-only; a Blueprint cannot call it.
- `train()` is synchronous and replaces any previous model.
- `FCLFResult::probabilities` is indexed by class in libsvm's label order, not by the
  order classes appear in `trainingSet`.
- The default `FSVMParameters` are pinned by `Types.FSVMParameters.Defaults`; changing
  one silently changes every model trained without explicit parameters.
- Features must be the same length in every sample of a training set; libsvm does not
  check this and a ragged set produces a wrong model rather than an error.
- Determinism: for the same training set and parameters, `train()` then `predict()`
  gives the same labels (pinned by `TrainAndPredict`).
- The libsvm sources themselves (`svm.cpp`, `svm.h`) are vendored and unmapped. This
  module owns only the wrapper; a libsvm upgrade is a dependency change under P-06.
