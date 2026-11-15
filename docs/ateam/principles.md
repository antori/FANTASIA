# Design principles
- As of: J-0112
- Approved: yes

## Method principles

These are ATeam's own, seeded at adoption and not specific to FANTASIA.

### P-01: Dependencies only through contracts
- Statement: A module uses another only through a contract in docs/ateam/contracts/.
- Rationale: Undeclared interfaces are where multi-agent work drifts.
- Enforced by: checker, reflexion, planner
- Provenance: stated

### P-02: One owner per file
- Statement: Each file belongs to exactly one module and one owner; a change outside your paths is a request, not an edit.
- Rationale: Concurrent agents editing the same file produce work nobody can review.
- Enforced by: hook, checker
- Provenance: stated

### P-03: No silent design change
- Statement: Contracts, modules, principles and the team change only through a change request.
- Rationale: A design that changes without a record cannot be verified against the goal.
- Enforced by: checker, reviewer
- Provenance: stated

### P-04: Done means verified
- Statement: Nothing is done until its acceptance tests and contract tests pass and a reviewer confirms it meets the request.
- Rationale: Passing tests without review admits work that solves the wrong problem; review without tests admits work that does not run.
- Enforced by: gate, tests, reviewer
- Provenance: stated

### P-05: Fast, tidy and lean
- Statement: The cleanest implementation that is fast and efficient by design, readable, and carries no more machinery than it needs. Applied from the first draft, checked per task, verified by each sprint's cleanup pass.
- Rationale: Cleanliness and speed are design properties, not a later pass.
- Enforced by: reviewer, lint
- Provenance: stated

### P-06: No new external dependency without an ADR
- Statement: Adding a third-party library, service or model requires an architecture decision record.
- Rationale: Every dependency here is also a licence obligation and a Win64 build-time artifact.
- Enforced by: reviewer, user
- Provenance: stated

### P-07: Implement the story, not the neighbourhood
- Statement: No unrequested refactors or features. The one exception is the sprint-end cleanup pass over code the coder changed in that sprint, which changes no behaviour.
- Rationale: Unrequested change is unreviewable and breaks the trace from goal to code.
- Enforced by: reviewer, gate
- Provenance: stated

## Project principles

One per non-goal in the brief, plus the standing rules this project already works by.

Every principle in this section is enforced by judgement. The project has no CI, so
nothing here has a mechanical check behind it: P-12 (a DLL rebuild plus a dumpbin import
check), P-15 (no committed key), P-16 (licence completeness) and P-17 (no heavy artifact
in git) are all mechanizable and none is mechanized. Until they are, they hold only as
far as a reviewer remembers them, which is the weakest form this project's most
expensive mistakes take.

### P-08: Components, not a dialogue manager
- Statement: FANTASIA exposes capabilities as independent Blueprint-facing components. It does not decide turn-taking, dialogue state, or how an agent is structured.
- Rationale: NG-1. Every project that uses FANTASIA manages dialogue differently; a built-in policy would be wrong for most of them and would couple all components to it.
- Enforced by: planner, reviewer
- Provenance: stated

### P-09: Win64 is the platform
- Statement: Features are designed and verified for Win64. Linux and macOS paths may exist but never constrain a design, and no feature waits on them.
- Rationale: NG-2. aGrUM, gRPC, OpenSSL, the whisper static libs and every prebuilt DLL exist only for Win64.
- Enforced by: planner, reviewer
- Provenance: stated

### P-10: Non-commercial scope
- Statement: No work is accepted whose justification is commercial distribution, paid support or storefront compliance. Redistribution stays inside what each upstream licence permits.
- Rationale: NG-3. The project is academic and unfunded for development; obligations it cannot meet must not be taken on.
- Enforced by: planner, user
- Provenance: stated

### P-11: Call models, do not host them
- Statement: FANTASIA integrates inference endpoints and local inference libraries. It does not train, fine-tune, serve or ship model weights of its own.
- Rationale: NG-4. Hosting would add licence, bandwidth and maintenance obligations the project cannot carry.
- Enforced by: planner, reviewer
- Provenance: stated

### P-12: The shipped DLL is the user's build
- Statement: Whenever runtime source changes land on a branch users install from, the Win64 prebuilt binaries are rebuilt, verified with dumpbin for unexpected imports, and committed as their own commit before the change is considered delivered.
- Rationale: G-3 and the fourth success criterion. Blueprint-only users never recompile, so stale binaries ship broken behaviour with correct-looking source.
- Enforced by: reviewer, user
- Provenance: recovered
- Confidence: high

### P-13: main is promoted, never pushed over
- Statement: Promotion to main is a cherry-pick or a fast-forward, decided by the user. Never a reset plus force-push; main is protected. Development and the per-project branches are the working branches.
- Rationale: main is what users clone. A rewritten history there breaks every clone and the release assets that reference it.
- Enforced by: user
- Provenance: recovered
- Confidence: high

### P-14: Newest released engine on main, previous engine branched first
- Statement: main carries the newest supported engine version. main moves to the next engine line only when that engine has been released — never on a preview — and only once the plugin builds clean against it and the FANTASIATests suite passes on it. Before the switch, the outgoing version is branched under its version number and kept, so earlier lines stay downloadable without becoming development targets.
- Rationale: Users are on a spread of engine versions and cannot all move at once, but maintaining several lines in parallel is beyond the project's capacity. The trigger is named rather than judged each time because a preview engine moves underneath the line, and a line nobody has built and tested is not a line users can be sent to.
- Enforced by: user, planner
- Provenance: stated

### P-15: Secrets live in configuration, never in the repository
- Statement: API keys, endpoints and request headers are Config UPROPERTYs read from DefaultGame.ini. No key, token or private endpoint is committed, defaulted to a real value, or logged.
- Rationale: The repository is public and the components all talk to paid services.
- Enforced by: reviewer, user
- Provenance: recovered
- Confidence: high

### P-16: Every third-party component carries its licence
- Statement: Anything bundled in the repository or fetched by bootstrap appears in THIRD_PARTY_LICENSES.md with its upstream licence text, and anything whose licence forbids redistribution is not shipped.
- Rationale: NG-3 and the project's standing as academic infrastructure. The plugin redistributes a large third-party surface.
- Enforced by: reviewer, user
- Provenance: recovered
- Confidence: high

### P-17: Heavy runtime artifacts live in releases, not in git
- Statement: Models, GPU stacks, build-time static libraries and vendor bundles are published as release assets and fetched by bootstrap. They are gitignored, and bootstrap is the single place that knows where they are.
- Rationale: A clone must stay small enough to be worth making, and release assets keep it that way.
- Enforced by: reviewer, user
- Provenance: recovered
- Confidence: high

### P-18: Code says what it is, not what it was
- Statement: Doc comments, headers and state documents describe present behaviour. Historical narration — a component's former behaviour, a superseded choice, a dated note — belongs in the journal.
- Rationale: Agents and readers load the comment, not the commit log; stale narration is read as current fact.
- Enforced by: reviewer
- Provenance: recovered
- Confidence: high

### P-19: A shipped engine line admits only minor fixes and security fixes, and minor is measured by surface
- Statement: Once an engine line ships it admits only minor fixes; everything larger targets the next line. A fix is minor when it touches no contract, no module boundary, no ADR and no dependency, changes no Blueprint-facing signature, and is estimated S or M. **A security fix is admitted whatever its surface**, on one test: the defect is recorded in the journal as a finding with `Kind=security`, opened by the security engineer, at `blocker` or `major`. Nothing else qualifies — not a minor security finding, not a security-adjacent fix, not a finding any other author rated. What the carve-out admits is the fix and nothing travelling with it: the contract edit is the minimum that describes the new behaviour, and the refactor that would be convenient to carry alongside is still refused (P-07). Every clause is decidable from the state documents and the journal, so a request is admitted or deferred at intake, before any code is written. Bringing the line into being — porting to the engine version, rebuilding the prebuilt binaries — is the line's own work and is not a fix against it; this bar does not measure it.
- Rationale: A maintenance line with no stated bar admits everything, and "minor fixes only" becomes an intention with no mechanism to refuse anything. The bar is surface rather than risk so that it can be applied at intake without a severity judgement, and the price is explicit: a two-line fix that touches a contract waits for the next line however much a maintainer wants it. F-012 is the live case — nothing in `BoltProtocol.h` carries `FANTASIA_API`, so the Bolt message layer cannot be linked from the test module at all, and exporting it widens C-neo4j's surface; for the life of the 5.8 line the handshake, HELLO, LOGON, RUN, PULL and the dechunker are not merely untested but untestable, and that is a price the bar charges deliberately. The carve-out for security exists because the surface test turned out to filter by the wrong thing in exactly one direction: a security fix in a module whose behaviour a contract describes almost always touches that contract **precisely because** it changes what crosses a trust boundary, so measuring surface refused the fixes by where the defect lived rather than by what it cost the user. Three credential-or-memory defects in one module refused by one clause is a mechanism selecting against the findings it most needs to admit, not a price being paid. The carve-out's own test is provenance rather than opinion, because an exception reading "unless it is important" would restore the severity judgement this principle was written to remove; `Kind=security`, author, and severity are facts in the journal, checkable at intake, and a coder who wants their change admitted cannot grant themselves any of the three. F-012 is minor and quality and sits in the same module as all three admitted findings, which is the test working: the module is not the criterion.
- Enforced by: planner, reviewer
- Provenance: stated
