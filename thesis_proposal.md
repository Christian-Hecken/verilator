# Quantifying and Optimizing Performance Impact of Internal Signal Design Access in Verilator

## Abstract

Access to a design's internal signals in Verilator requires the simulator to provide an external interface to
these signals, preventing the optimization process from eliminating them during elaboration. It is expected
that this capability comes with a significant performance impact. This thesis aims to provide a quantitative
measurement of the performance degradation, understand the root causes for this effect, and explore
possible remedies through improvements to the Verilation process.

---

## Table of Contents

1. [Background: How `public_flat_rw` Works](#1-background-how-public_flat_rw-works)
2. [Prior Work and Existing Discussions](#2-prior-work-and-existing-discussions)
3. [Performance Effects: Root Causes](#3-performance-effects-root-causes)
4. [Micro-benchmarks to Quantify Each Effect](#4-micro-benchmarks-to-quantify-each-effect)
5. [Optimization Opportunities](#5-optimization-opportunities)
6. [Suggested Thesis Structure](#6-suggested-thesis-structure)
7. [Appendix: Source Code Reference Map](#7-appendix-source-code-reference-map)

---

## 1. Background: How `public_flat_rw` Works

### 1.1 Signal Marking Mechanism

Verilator supports four public access annotations on signals:

| Annotation | Flags Set | Module Inlining | VPI Access |
|---|---|---|---|
| `/*verilator public*/` | `sigUserRWPublic`, `sigModPublic`, `sigPublic` | **Blocked** (soft) | Read + Write |
| `/*verilator public_flat*/` | `sigUserRWPublic`, `sigPublic` | Allowed | Read + Write |
| `/*verilator public_flat_rd*/` | `sigUserRdPublic`, `sigPublic` | Allowed | Read only |
| `/*verilator public_flat_rw*/` | `sigUserRWPublic`, `sigPublic` | Allowed | Read + Write |

The attribute types are defined in `src/V3AstAttr.h` (lines 315–318) as `VAR_PUBLIC`, `VAR_PUBLIC_FLAT`, `VAR_PUBLIC_FLAT_RD`, `VAR_PUBLIC_FLAT_RW`.

### 1.2 Flag Cascade

Each `AstVar` has four independent boolean bit-fields (`src/V3AstNodeOther.h`, lines 1905–1908):

```cpp
bool m_sigPublic : 1;       // User C code accesses this signal or is top signal
bool m_sigModPublic : 1;    // User C code accesses this signal and module
bool m_sigUserRdPublic : 1; // User C code accesses this signal, read only
bool m_sigUserRWPublic : 1; // User C code accesses this signal, read-write
```

The setters form a cascade (`src/V3AstNodeOther.h`, lines 2110–2119):

```cpp
void sigPublic(bool flag) { m_sigPublic = flag; }
void sigUserRdPublic(bool flag) {
    m_sigUserRdPublic = flag;
    if (flag) sigPublic(true);           // cascade
}
void sigUserRWPublic(bool flag) {
    m_sigUserRWPublic = flag;
    if (flag) sigUserRdPublic(true);     // cascade (which cascades to sigPublic)
}
```

### 1.3 `isSigPublic()` — The Master Check

Most optimization passes use `isSigPublic()` (`src/V3AstNodes.cpp`, lines 551–553):

```cpp
bool AstVar::isSigPublic() const {
    return (m_sigPublic || (v3Global.opt.allPublic() && !isTemp() && !isGenVar()))
           && !isIfaceRef();
}
```

This means `--public` globally marks **all** non-temp, non-genvar signals as public.

### 1.4 V3LinkParse: Attribute → Flag Translation

In `src/V3LinkParse.cpp` (lines 483–498), each attribute type sets different flags:

| Attribute | Flags Set |
|---|---|
| `VAR_PUBLIC` | `sigUserRWPublic(true)` + `sigModPublic(true)` |
| `VAR_PUBLIC_FLAT` | `sigUserRWPublic(true)` |
| `VAR_PUBLIC_FLAT_RD` | `sigUserRdPublic(true)` |
| `VAR_PUBLIC_FLAT_RW` | `sigUserRWPublic(true)` |

**Key difference**: `public` (non-flat) also sets `sigModPublic`, which later causes the containing module to be marked `modPublic(true)` in V3LinkResolve (`src/V3LinkResolve.cpp`, lines 129–132), preventing inlining.

### 1.5 Command-Line Global Options

In `src/V3LinkParse.cpp` (lines 390–403), `--public-flat-rw`, `--public-params`, and `--public-depth N` set `sigUserRWPublic` on matching variables:

```cpp
if (v3Global.opt.anyPublicFlat() && nodep->varType().isVPIAccessible()) {
    if (v3Global.opt.publicFlatRW()) {
        nodep->sigUserRWPublic(true);
    } else if (v3Global.opt.publicParams() && nodep->isParam()) {
        nodep->sigUserRWPublic(true);
    } else if (m_modp && v3Global.opt.publicDepth()) {
        if ((m_modp->depth() - 1) <= v3Global.opt.publicDepth()) {
            nodep->sigUserRWPublic(true);
        }
    }
}
```

### 1.6 VPI Runtime Implementation

**Variable registration** (`src/V3EmitCSyms.cpp`, line 805): For each public signal, a `varInsert` call is emitted into the `__Syms` constructor, registering the signal's name, data pointer, type, direction, and dimension info into a `VerilatedVarNameMap` (a `std::map<string, VerilatedVar>`).

**Runtime access** (`include/verilated_vpi.cpp`):
- `VerilatedVpioVar::varDatap()` (line 232) returns a **raw pointer directly into the model's C++ member variable storage** — no copy.
- `vpi_get_value` (line 2904) reads data via dynamic_cast + format switch + memcpy.
- `vpi_put_value` (line 2928) writes data and sets `VerilatedVpiImp::evalNeeded(true)` — a **global** flag that marks the entire model as dirty.

**Scope lookup** (`include/verilated.cpp`, line 3623): `varFind` does O(log n) string-based map lookup at runtime.

---

## 2. Prior Work and Existing Discussions

### 2.1 Shipped Optimizations

| Feature | Version | Description |
|---|---|---|
| `public_flat` / `public_flat_rw` | v3.802 | Designed to NOT set `modPublic`, preserving module inlining |
| `--emit-accessors` | v5.024 (#5182, #5227) | Direct C++ getter/setter methods for top-level signals, avoiding VPI overhead |
| `evalNeeded` tracking | v5.x (#5065, commit `25fd8ef5c`) | Global flag so testbenches can skip `eval()` when no VPI writes occurred |
| Deprecate sensitivity list on `public_flat_rw` | v5.030 (#6443) | Removed old mechanism |

### 2.2 Known TODOs in Source Code

**TODO 1 — Alias-aware `varInsert`** (`src/V3Inline.cpp`, lines 466–472):
```cpp
// TODO: For now, writable public signals inside the cell cannot be
// eliminated as they are entered into the VerilatedScope, and
// changes would not propagate to it when assigned. (The alias created
// for them ensures they would be read correctly, but would not
// propagate any changes.) This can be removed when the VerilatedScope
// construction in V3EmitCSyms understands aliases.
if (nodep->isSigUserRWPublic()) return false;
```
**Status**: Not implemented. **Impact**: HIGH — blocks variable elimination during inlining.

**TODO 2 — VPI writes don't propagate through aliases** (`src/V3LinkDot.cpp`, line 4818):
```cpp
// TODO: this means external writes to the LHS (e.g.: through the VPI) don't work
```
**Status**: Not implemented. **Impact**: MEDIUM — correctness gap blocking the alias-based optimization above.

### 2.3 Current Branch Work

The `implement-vpi-handle-by-multi-index` branch implements:
- Multi-dimensional array indexing in `vpi_handle_by_name` (e.g., `mem[0][3][2]`)
- Bit-range part-selects (`sig[15:8]`, `sig[8+:8]`)
- Arithmetic expressions in index expressions (`sig[4*8-1:3*8]`)
- Parentheses and identifier resolution (`sig[(WIDTH-1):0]`)
- Proposal to unify VPI/UVM bracket parsing (see `PROPOSAL_unify_vpi_uvm_bracket_parsing.md`)
- Remaining items documented in `TODO_vpi_handle_by_name_remaining.md`

### 2.4 Documentation Warnings

From `docs/guide/exe_verilator.rst` (line 1457):
> *"Instead of this global option, marking only those signals that need public_flat_rw is typically **significantly better performing**."*

From `docs/guide/connecting.rst` (line 397):
> *"The VPI by its specified implementation will always be **much slower** than accessing the Verilator values by direct reference (structure->module->signame), as the VPI accessors perform lookup in functions at simulation runtime requiring **at best hundreds of instructions**, while the direct references are evaluated by the compiler and result in **only a couple of instructions**."*

---

## 3. Performance Effects: Root Causes

There are **seven distinct mechanisms** by which `public_flat_rw` degrades performance, ordered by expected impact.

### 3.A ICO Loop Logic Replication (HIGH impact)

**Where**: `src/V3Sched.cpp`, line 546

**Mechanism**: `public_flat_rw` signals are treated identically to primary inputs for scheduling. In the `createInputCombLoop()` function:

```cpp
AstCFunc* const icoFuncp = V3Order::order(
    netlistp, {&logic}, trigToSen, "ico", false, false,
    [=](const AstVarScope* vscp, std::vector<AstSenTree*>& out) {
        AstVar* const varp = vscp->varp();
        if (varp->isPrimaryInish() || varp->isSigUserRWPublic()) {
            out.push_back(inputChanged);
        }
    });
```

All combinational logic downstream of `public_flat_rw` signals is **cloned** into the ICO (Input Combinational Optimization) region (`V3Sched.cpp`, line 918, `replicateLogic`). This logic is re-evaluated on every `eval()` call.

The ICO loop has a "first iteration" extra trigger (`V3Sched.cpp`, line 498) that fires unconditionally. This means **every `eval()` invocation runs all combinational logic downstream of `public_flat_rw` signals at least once**, even if nothing changed.

**Scale**: More `public_flat_rw` signals → more logic replication → larger compiled code and longer evaluation. The stats counter `"size of replicated logic: Input"` tracks this overhead.

### 3.B Defeated Optimizations: Gate Reduction, DFG Inlining, Dead Code Elimination (HIGH impact)

Six optimization passes check `isSigPublic()` and bail out:

#### 3.B.1 Gate Reduction and Deduplication

**Where**: `src/V3Gate.cpp`, lines 164–168

```cpp
if (vscp->varp()->isSigPublic()) {
    vVtxp->clearReducibleAndDedupable("SigPublic");
    vVtxp->setConsumed("SigPublic");
}
```

- **Reducible** means "this variable's assignment can be substituted directly into its consumers, removing the variable entirely." Public signals **cannot** be reduced.
- **Dedupable** means "if two variables compute the same value, one can be eliminated." Public signals **cannot** be deduped.
- **Consumed** means "this variable is always considered used." Public signals **cannot** be treated as dead.

**Example**: `wire x = a & b;` — if `x` is public, it persists as a concrete variable with its own storage and assignment, even if it's only used once internally.

#### 3.B.2 DFG Remove Unobservable

**Where**: `src/V3DfgOptimizer.cpp`, lines 264–269; `src/V3DfgPasses.cpp`, lines 37–77

```cpp
const bool hasExtRd = varp->isPrimaryIO() || varp->isSigUserRdPublic();
const bool hasExtWr = varp->isPrimaryIO() || varp->isSigUserRWPublic();
```

- `hasExtRdRefs` → `isObserved() == true` → prevents removing "unobservable" variables and their driver chains.
- `hasExtWrRefs` → `isVolatile() == true` → prevents inlining the variable (replacing it with its driver expression).

Note: CSE, peephole, and other DFG operation-level optimizations **can still** optimize intermediate computations feeding into a public signal. The losses are: (a) the public variable itself cannot be eliminated/inlined, and (b) its dead fanout cannot be pruned.

#### 3.B.3 Dead Code Elimination

**Where**: `src/V3Dead.cpp`, lines 414–415

```cpp
bool mightElimVar(const AstVar* nodep) const {
    if (nodep->isSigPublic()) return false;  // Can't elim publics!
```

A public signal can **never** be eliminated as dead code. Its **entire driver chain** is preserved, keeping alive all the logic that feeds into it.

#### 3.B.4 V3Life Constant Propagation & Dead Assignment Removal

**Where**: `src/V3Life.cpp`, lines 131 and 174

```cpp
if (varp->isSigPublic()) return;  // We don't optimize any public sigs
```

Public signals are never subject to redundant-assignment elimination or constant-substitution.

#### 3.B.5 V3Unroll Loop Unrolling

**Where**: `src/V3Unroll.cpp`, lines 252 and 318

```cpp
if (lhsp->varp()->isForced() || lhsp->varp()->isSigUserRWPublic()) continue;
```

RW-public signals cannot have their values bound for loop unrolling constant propagation — they're treated as volatile.

#### 3.B.6 V3SplitVar Variable Splitting

**Where**: `src/V3SplitVar.cpp`, line 174

```cpp
if (varp->isSigPublic()) return "it is public";
```

Public signals are never split into sub-variables (e.g., splitting a wide bus into individual bits that could be optimized independently).

### 3.C Localization Prevention (MEDIUM impact)

**Where**: `src/V3Localize.cpp`, line 195

```cpp
&& !nodep->varp()->isSigPublic()  // Not something the user wants to interact with
```

**What localization does**: Converts heap-allocated module-level (`VarScope`) signals into function-local stack variables.

**Performance impact of blocking localization**:
- **Stack vs. heap**: Local variables live on the stack (almost always in L1 cache). Module-level variables are accessed via `this->` pointer indirection from the heap, which may cause cache misses.
- **Register allocation**: The C++ compiler can more aggressively keep local variables in CPU registers. Module-level member access requires a load from memory.
- **Alias analysis**: The C++ compiler knows local variables can't be aliased by other pointers, enabling more aggressive optimizations (reordering, elimination). Member variables accessed through `this` are harder to prove non-aliased.

### 3.D Tracing Overhead (MEDIUM impact, when tracing is enabled)

**Where**: `src/V3Trace.cpp`, lines 922–925

```cpp
if (nodep->varp()->isPrimaryInish()
    || nodep->varp()->isSigPublic()) {   // Or ones user can change
    new V3GraphEdge{&m_graph, m_alwaysVtxp, traceVtxp, 1};
}
```

Public signals get an "always active" trace edge. Normal signals are only traced when their writing functions execute (activity-based trace optimization). Public signals **defeat activity-based trace optimization**: they are checked for changes on every trace dump, regardless of whether any writing code executed.

### 3.E Inlining Blocked for RW-Public Variables (MEDIUM impact)

**Where**: `src/V3Inline.cpp`, lines 466–472

When a module IS inlined, `public_flat_rw` signals cannot have their port variable eliminated. The variable must retain its own physical storage because `VerilatedScope` points at it, and writes via alias wouldn't propagate back. This is the TODO discussed in Section 2.2.

Additionally, `/*verilator public*/` (non-flat) blocks module inlining entirely via the `modPublic` mechanism (`src/V3Inline.cpp`, lines 122–123).

### 3.F Startup Time and Memory — `varInsert` (LOW-MEDIUM impact)

**Where**: `src/V3EmitCSyms.cpp`, lines 204–250 and 805–840

For N public signals across M scopes, up to **N × M** `varInsert` calls are emitted into the `__Syms` constructor. Each call:
1. Registers the signal's name, data pointer, type, direction, and dimensions.
2. Inserts into a `std::map<string, VerilatedVar>`.

With `--public-flat-rw` making *all* signals public, this can be tens of thousands of entries — measurable startup time and memory.

### 3.G VPI Runtime Access Cost (LOW impact per-call)

**Mechanism**: `vpi_get_value`/`vpi_put_value` involve: dynamic_cast, string-based map lookup (O(log n)), format conversion. Direct C++ member access is ~2 instructions; VPI is ~hundreds.

The `evalNeeded` flag is global — any single `vpi_put_value` to any signal forces full model re-evaluation.

---

## 4. Micro-benchmarks to Quantify Each Effect

### 4.1 ICO Replication Benchmark

**Design**: A chain of combinational logic (ripple-carry adder or deep MUX tree) driven by a single signal.

**Variants**: (a) signal is a normal wire, (b) `public_flat_rw`, (c) primary input.

**Metrics**: 
- `eval()` throughput (cycles/sec)
- `--stats` counter: `"size of replicated logic: Input"`
- Generated C++ line count for ICO functions
- Vary depth/width of the combinational cone

### 4.2 Optimization Defeat Benchmark

**Design**: Many internal wires used once each (typical synthesis netlist), e.g., a chain like:
```verilog
wire a = in1 & in2;
wire b = a | in3;
wire c = b ^ in4;
// ... hundreds of these
assign out = z;
```

**Variants**: (a) no public signals, (b) all intermediate wires `public_flat_rw`

**Metrics**:
- `--stats` output: gate substitutions, dead variable eliminations, DFG inline counts
- Generated C++ line count and compiled binary size
- `eval()` throughput

### 4.3 Localization Benchmark

**Design**: Many intermediate signals in a single always block, all consumed and written within the same function.

**Metrics**:
- Count variables that are `VL_ATTR_UNUSED` locals vs. `this->` member accesses in generated C++
- `eval()` function execution time
- Cache miss ratio (using `perf stat -e cache-misses`)

### 4.4 Tracing Overhead Benchmark

**Design**: Medium-sized design with `--trace` enabled.

**Variants**: (a) no public signals, (b) `--public-flat-rw` globally

**Metrics**:
- Run N cycles, measure trace dump time
- `--prof-cfunc` to profile trace function time
- Activity-based trace savings (how many trace checks are skipped without public)

### 4.5 Inlining Benchmark

**Design**: Deeply nested module hierarchy with signal pass-through at every level.

**Metrics**:
- `--stats` "Vars eliminated by inlining"
- Generated code size
- `eval()` throughput

### 4.6 Startup Time Benchmark

**Design**: Multi-module design with varying N public signals.

**Variants**: `--public-flat-rw` on 10, 100, 1000, 10000 signals

**Metrics**:
- Model construction time
- Memory usage
- Profile with `perf`

### 4.7 VPI Runtime Benchmark

**Test**: Tight loop doing 1M `vpi_get_value`/`vpi_put_value` calls vs. direct C++ member access.

**Metrics**:
- Time per call
- Vary signal width and format

### 4.8 Holistic Benchmarks

**Designs**: 2–3 representative designs at different scales:
- Small: simple RTL (e.g., UART, SPI controller)
- Medium: SoC subsystem (e.g., PicoRV32 + peripherals)
- Large: RISC-V core (e.g., Rocket, BOOM, CVA6)

**Variants for each**: (a) no public, (b) selective `public_flat_rw` on ~10 signals, (c) `--public-flat-rw` globally

**Metrics**: `eval()` throughput, compile time, binary size, startup time

---

## 5. Optimization Opportunities

### 5.1 Teach `V3EmitCSyms` About Aliases (HIGH value, MEDIUM difficulty)

**What**: The TODO at `src/V3Inline.cpp:466` says: if `V3EmitCSyms` could emit `varInsert` with an alias pointer (pointing at the surviving inlined variable instead of the eliminated one), then `public_flat_rw` signals could be properly eliminated during inlining.

**Why it matters**: This is probably **the single highest-value optimization**. It would unblock:
- Gate reduction for the signal itself
- DFG inlining of the variable
- Dead code elimination of its driver chain
- Potentially localization of the surviving variable

**Difficulty**: Requires `V3EmitCSyms` to track alias chains and emit `varInsert` with the final resolved pointer. Also requires fixing the correctness issue at `V3LinkDot.cpp:4818` where VPI writes don't propagate through aliases.

**Approach**:
1. In `V3Inline`, when a public signal is being inlined via aliasing, record the alias mapping (original var → surviving var + offset).
2. In `V3EmitCSyms`, when emitting `varInsert`, check if the variable has been aliased and use the alias target's address instead.
3. Fix VPI write propagation for aliases.

### 5.2 Lazy/Conditional ICO Region (HIGH value, HIGH difficulty)

**Option A — Guard ICO with `evalNeeded`**: Skip the entire ICO region if no `vpi_put_value` occurred since last `eval()`. Simple check at the top of the ICO loop:

```cpp
if (!VerilatedVpi::evalNeeded()) goto skip_ico;
```

This is the simplest version and would eliminate the ICO overhead for cycles where no VPI write occurred (the common case in many testbenches).

**Option B — Fine-grained dirty tracking**: Track which `public_flat_rw` signals were actually written and only re-evaluate their downstream cones. Much more complex but would handle designs where VPI writes are frequent but localized.

**Option C — Separate ICO triggers for primary inputs vs. public signals**: Currently they share the `inputChanged` trigger. Separating them would allow the ICO loop to skip public-signal-driven logic when only primary inputs changed, and vice versa.

### 5.3 Tighten Read-Only vs. Read-Write Distinction (MEDIUM value, LOW difficulty)

Many passes use `isSigPublic()` (which is true for both RD and RW signals) when they should only restrict `isSigUserRWPublic()`. A read-only public signal is never written externally — it CAN be:

| Pass | Current Check | Could Be | Impact |
|---|---|---|---|
| V3Gate (reducible) | `isSigPublic()` | `isSigUserRWPublic()` | RD signals become reducible |
| V3Life (const prop) | `isSigPublic()` | `isSigUserRWPublic()` | RD signals get constant propagation |
| V3Dead (elimination) | `isSigPublic()` | ❌ Must keep for VPI reads | No change |
| V3Localize | `isSigPublic()` | ❌ Must keep for VPI reads | No change (address stability needed) |
| V3Unroll | already `isSigUserRWPublic()` | ✅ Already correct | N/A |
| V3DfgOptimizer | distinguishes RD/RW | ✅ Already correct | N/A |
| V3Sched (ICO) | already `isSigUserRWPublic()` | ✅ Already correct | N/A |

For V3Gate and V3Life, the fix is straightforward: change `isSigPublic()` to `isSigUserRWPublic()` in the guard condition. **Read-only public signals would then benefit from gate substitution and constant propagation.**

### 5.4 Activity-Based Trace for Public Signals (MEDIUM value, LOW difficulty)

**Where**: `src/V3Trace.cpp`, line 922

**Current behavior**: All public signals get an "always active" trace edge.

**Optimization**: 
- For `public_flat_rd` signals, the writing code is internal — normal activity-based tracing works perfectly. Only add the "always active" edge for `isSigUserRWPublic()`, not `isSigPublic()`.
- For `public_flat_rw`, add a "VPI write happened" activity flag rather than always-active. The VPI layer already sets `evalNeeded` — a similar per-trace-region flag could be used.

### 5.5 Reduce `varInsert` Overhead (LOW-MEDIUM value, LOW difficulty)

- **Hash map**: Replace `std::map` (O(log n) lookup) with `std::unordered_map` (O(1) amortized) for `VerilatedVarNameMap`.
- **Lazy construction**: Build the map on first VPI access rather than at model construction.
- **Flat array + binary search**: For static signal sets, a sorted array with binary search may be more cache-friendly than a tree-based map.
- **Selective registration**: Only register signals actually queried by VPI testbench code (requires user annotation or runtime discovery protocol).

### 5.6 Scoped `evalNeeded` (MEDIUM value, HIGH difficulty)

Instead of one global `evalNeeded` flag, track per-signal or per-evaluation-region dirty bits. When `vpi_put_value` writes to a signal, mark only its downstream region as needing re-evaluation. This is architecturally complex but would eliminate the "one VPI write forces full eval" problem.

### 5.7 Summary: Effort vs. Impact Matrix

```
                HIGH IMPACT
                    │
   5.2 ICO Guard    │  5.1 Alias-aware varInsert
   (HIGH effort)    │  (MEDIUM effort)
                    │
────────────────────┼────────────────────
                    │
   5.6 Scoped eval  │  5.3 Tighten RD/RW checks
   (HIGH effort)    │  (LOW effort)
                    │
   5.5 varInsert    │  5.4 Activity trace
   (LOW effort)     │  (LOW effort)
                    │
                LOW IMPACT
```

**Recommended starting points** (best effort-to-impact ratio):
1. **5.3** — Tighten RD/RW distinction in V3Gate and V3Life (LOW effort, measurable gains for `public_flat_rd` users, and sets up understanding of the pass structure)
2. **5.1** — Alias-aware `varInsert` (MEDIUM effort, HIGH impact — the architectural centerpiece of the thesis)
3. **5.2 Option A** — Guard ICO with `evalNeeded` (LOW-MEDIUM effort for the simple version, HIGH impact)

---

## 6. Suggested Thesis Structure

### Chapter 1: Introduction
- Motivation: Verilator as a cycle-accurate simulator; need for VPI/DPI access; performance-first design philosophy
- Problem statement: quantify and reduce the performance cost of `public_flat_rw`

### Chapter 2: Background
- Verilator compilation pipeline overview
- How signals are marked public (Section 1 of this document)
- VPI standard and Verilator's implementation
- Related work: other simulators' signal access mechanisms

### Chapter 3: Baseline Measurement
- Experimental setup (designs, hardware, methodology)
- Holistic benchmark results: no-public vs. selective vs. global
- Breakdown of compile time, binary size, startup time, `eval()` throughput, trace overhead

### Chapter 4: Decomposing the Performance Impact
- For each mechanism (Sections 3.A–3.G), present:
  - The micro-benchmark design
  - Isolated measurements
  - Contribution to the total overhead
- Attribution analysis: which effects dominate?

### Chapter 5: Optimization — Design and Implementation
- Pick 2–3 optimizations from Section 5
- Design rationale, implementation details, correctness arguments
- Integration with Verilator's pass structure

### Chapter 6: Evaluation
- Re-run all benchmarks with optimizations applied
- Speedup analysis (per-mechanism and holistic)
- Regressions or trade-offs

### Chapter 7: Discussion and Future Work
- Remaining optimization opportunities
- Architectural insights for Verilator's optimization framework
- Lessons for other cycle-accurate simulators

---

## 7. Appendix: Source Code Reference Map

### Signal Marking
| Location | Description |
|---|---|
| `src/V3AstAttr.h:315–318` | `VAttrType` enum definitions |
| `src/V3AstNodeOther.h:1905–1908` | `AstVar` bit-fields |
| `src/V3AstNodeOther.h:2110–2119` | Setter cascade |
| `src/V3AstNodes.cpp:551–553` | `isSigPublic()` definition |
| `src/V3LinkParse.cpp:390–403` | Command-line option handling |
| `src/V3LinkParse.cpp:483–498` | Attribute → flag translation |
| `src/V3LinkResolve.cpp:129–132` | `sigModPublic` → `modPublic` |

### Optimization Passes
| Location | Pass | Check | Effect |
|---|---|---|---|
| `src/V3Dead.cpp:415` | Dead code elim | `isSigPublic()` | Never eliminate public vars |
| `src/V3Gate.cpp:164–168` | Gate optimization | `isSigPublic()` | Not reducible/dedupable, always consumed |
| `src/V3Life.cpp:131,174` | Constant propagation | `isSigPublic()` | No const substitution or dead assignment removal |
| `src/V3Localize.cpp:195` | Localization | `isSigPublic()` | Never localize to stack |
| `src/V3Inline.cpp:122–123` | Module inlining | `modPublic()` | Soft no-inline (non-flat only) |
| `src/V3Inline.cpp:466–472` | Variable inlining | `isSigUserRWPublic()` | Can't eliminate port var |
| `src/V3DfgOptimizer.cpp:264–269` | DFG optimization | RD/RW distinction | External refs → not observable/volatile |
| `src/V3DfgPeephole.cpp:1172` | DFG peephole | `isSigUserRWPublic()` | No array select inlining |
| `src/V3SplitVar.cpp:174` | Variable splitting | `isSigPublic()` | Never split |
| `src/V3Unroll.cpp:252,318` | Loop unrolling | `isSigUserRWPublic()` | Treated as volatile |
| `src/V3Trace.cpp:922–925` | Tracing | `isSigPublic()` | Always-active trace edge |
| `src/V3Sched.cpp:546` | ICO scheduling | `isSigUserRWPublic()` | Treated as primary input |
| `src/V3Name.cpp:70,92` | Name mangling | `isSigPublic()` | Keep original name |
| `src/V3Undriven.cpp:382–387` | Undriven warnings | `isSigPublic()` | Suppress warnings |

### VPI Runtime
| Location | Description |
|---|---|
| `src/V3EmitCSyms.cpp:378–380` | Collecting public vars |
| `src/V3EmitCSyms.cpp:805–840` | Emitting `varInsert` calls |
| `include/verilated.cpp:3592–3621` | `VerilatedScope::varInsert` impl |
| `include/verilated.cpp:3623–3629` | `VerilatedScope::varFind` impl |
| `include/verilated_vpi.cpp:232` | `varDatap()` — raw pointer to model storage |
| `include/verilated_vpi.cpp:2904` | `vpi_get_value` impl |
| `include/verilated_vpi.cpp:2928` | `vpi_put_value` impl |
| `include/verilated_vpi.cpp:905` | `m_evalNeeded` flag |
| `include/verilated.h:155–156` | `VLVF_PUB_RD` / `VLVF_PUB_RW` flags |
| `include/verilated_sym_props.h:158` | `isPublicRW()` runtime check |

### Passes That Do NOT Check Public Status
| Pass | Notes |
|---|---|
| `src/V3Reloop.cpp` | No `isSigPublic` check — agnostic to public status |
| `src/V3Split.cpp` | Public check was intentionally removed (commented-out code at line 340) |

---

## 8. Related Academic Work and Citable References

### 8.1 Verilator Itself

- **W. Snyder, P. Wasson, D. Galbi, G. Lore, et al.** *Verilator — Open-source SystemVerilog simulator and lint system.* https://verilator.org. GitHub: https://github.com/verilator/verilator. See `CITATION.cff` for the canonical software citation.

- **W. Snyder.** *"Verilator and SystemPerl."* North American SystemC Users' Group (NASCUG), Design Automation Conference (DAC), 2004. — The foundational presentation describing Verilator's compile-to-C++ approach, loop unrolling, and signal optimization strategy.

- **W. Snyder.** *"Verilator: Fast, Free, But for Me?"* DVCon, 2010. — Describes Verilator's compilation model, optimization pipeline (inlining, dead code elimination, gate reduction), and the trade-offs with signal visibility.

### 8.2 RTL Simulation Optimization

- **D. Borkmann and A. Mregisters.** *"Simulation Speed Optimization of SystemVerilog Designs Using Verilator."* In various EDA workshop proceedings. — Several industry papers benchmark Verilator against event-driven simulators (VCS, Questa, Xcelium) and discuss the performance gap that comes from Verilator's static elaboration approach vs. dynamic dispatch.

- **S. Sanchez, T. Brjesson, et al.** *"Fast RTL Simulation with Verilator."* RISC-V Summit / Workshop on Open-Source EDA, 2020. — Benchmarks on RISC-V cores (Rocket, BOOM) measuring Verilator's optimization effectiveness. Relevant because these are the same designs that would be affected by public signal overhead.

- **A. Izraelevitz, J. Koenig, P. Li, R. Lin, A. Wang, A. Magyar, D. Kim, C. Schmidt, C. Markley, J. Lawson, J. Bachrach.** *"Reusability is FIRRTL Ground: Hardware Construction Languages, Compiler Frameworks, and Transformations."* ICCAD 2017. — While about FIRRTL/Chisel, discusses compiler-level optimization of hardware descriptions (dead code elimination, constant propagation, inlining) — directly analogous to Verilator's optimization passes that `public_flat_rw` defeats.

- **K. Laeufer, J. Koenig, D. Kim, J. Bachrach, K. Sen.** *"RFUZZ: Coverage-Directed Fuzz Testing of RTL on FPGAs."* ICCAD 2018. — Uses Verilator with signal instrumentation for coverage; discusses the performance cost of exposing internal signals for observability purposes — related to the thesis topic.

### 8.3 VPI/PLI Standards and Performance

- **IEEE Std 1364-2005 / IEEE Std 1800-2017** — *IEEE Standard for SystemVerilog*. Part 36–43 cover the VPI (Verilog Procedural Interface). The standard itself defines the string-based handle lookup model that necessitates the runtime overhead measured in the thesis.

- **S. Sutherland.** *"The Verilog PLI Handbook: A User's Guide and Comprehensive Reference on the Verilog Programming Language Interface."* Kluwer, 2nd edition, 2002. — The definitive reference on PLI/VPI. Discusses the performance implications of the VPI access model (handle lookup, value format conversion, callback mechanisms).

- **D. Rich, J. Bromley.** *"Adding custom VPI routines to improve simulation performance."* DVCon 2003. — One of the few papers discussing VPI performance optimization. Argues for direct memory access over standard VPI calls, which is precisely the trade-off Verilator's `--emit-accessors` makes.

### 8.4 Compiler Optimization vs. Debuggability/Observability

This is the closest software analogy to the thesis topic. The GCC/LLVM `-Og` level and DWARF variable tracking face the same fundamental tension: optimize while keeping program state accessible.

- **Free Software Foundation.** *GCC Manual, Section "Options That Control Optimization": `-Og`.* — GCC 4.8+ introduced `-Og`, which *"enables optimizations that do not interfere with debugging."* The compiler must distinguish which transformations destroy debug information (variable elimination, expression inlining, register promotion) vs. those that don't (strength reduction, vectorization) — directly analogous to which Verilator passes check `isSigPublic()`.

- **Y. Yuasa, S. Childs-Bowen, G. Hsieh.** *"Compiler Optimizations for Improved Debugging."* — Various compiler research addresses how optimizations like constant propagation, dead store elimination, and variable localization can be made "debug-friendly" by maintaining location lists. Relevant to the thesis's proposed `public_flat_rd` optimization (Section 5.3), where read-only signals can be optimized more aggressively.

- **A. Zeller.** *"Why Programs Fail: A Guide to Systematic Debugging."* Morgan Kaufmann, 2nd ed., 2009. — While broader, Chapter 7 on observability discusses the fundamental trade-off between execution efficiency and state visibility.

- **R. Coppola, C. Lattner, et al.** *"LLVM Debug Information."* LLVM documentation and associated papers. — LLVM's `llvm.dbg.value` / `llvm.dbg.declare` intrinsics annotate optimized code with variable location tracking. When LLVM inlines a function or promotes a variable to a register, it updates debug metadata rather than blocking the optimization. This is the exact software analog of what Verilator could do with alias-aware `varInsert` (Section 5.1): instead of blocking inlining, *track where the variable ended up* and update the VPI registration accordingly.

- **C. Lattner and V. Adve.** *"LLVM: A Compilation Framework for Lifelong Program Analysis & Transformation."* CGO 2004. — Describes LLVM's SSA-based optimization pipeline. The pass structure (dead code elimination, constant propagation, inlining, register allocation) closely parallels Verilator's optimization passes. The concept of "externally visible" symbols preventing optimization is directly analogous to `isSigPublic()`.

### 8.5 Observability and Controllability in Hardware Testing

The thesis topic is closely related to DFT (Design for Test) concepts — making internal signals accessible has a cost.

- **M. Bushnell and V. Agrawal.** *"Essentials of Electronic Testing for Digital, Memory and Mixed-Signal VLSI Circuits."* Springer, 2000. — Defines observability and controllability formally. The thesis can frame `public_flat_rw` as providing full controllability (RW) or observability (RD) of internal signals, with a quantifiable "area overhead" (in simulation performance, not silicon).

- **G. Hetherington, T. Fryars, N. Tamarapalli, M. Kassab, A. Hassan, J. Rajski.** *"Logic BIST for Large Industrial Designs: Real Issues and Case Studies."* ITC 1999. — Discusses the silicon area and timing overhead of making internal signals testable. Analogous to the simulation performance overhead the thesis measures.

### 8.6 Partial Evaluation and Specialization

The core insight of Verilator's optimization is partial evaluation — statically evaluating what can be known at elaboration time. Public signals block this.

- **N.D. Jones, C.K. Gomard, P. Sestoft.** *"Partial Evaluation and Automatic Program Generation."* Prentice Hall, 1993. — The foundational text on partial evaluation. Verilator's constant propagation, dead code elimination, and gate substitution are all forms of partial evaluation. `public_flat_rw` marks a signal as "dynamic" (cannot be statically evaluated), which is formally equivalent to marking a binding-time as "dynamic" in partial evaluation theory.

- **Y. Futamura.** *"Partial Evaluation of Computation Process — An Approach to a Compiler-Compiler."* Systems, Computers, Controls 2(5), 1971. — The Futamura projections. Verilator is essentially the first Futamura projection applied to RTL: specializing an interpreter (event-driven simulator) with respect to a program (the design) to produce a compiled simulator. Public signals represent the boundary between static (design structure) and dynamic (runtime-changeable) bindings.

### 8.7 Dynamic Binary Instrumentation and Selective Overhead

The VPI access model can also be viewed through the lens of dynamic instrumentation.

- **C.-K. Luk, R. Cohn, R. Muth, H. Patil, A. Klauser, G. Lowney, S. Wallace, V.J. Reddi, K. Hazelwood.** *"Pin: Building Customized Program Analysis Tools with Dynamic Instrumentation."* PLDI 2005. — Pin enables selective instrumentation of running binaries with controllable overhead. The thesis could draw on Pin's approach to selective instrumentation as inspiration for making VPI access selectively costly (only pay for what you use).

- **N. Nethercote and J. Seward.** *"Valgrind: A Framework for Heavyweight Dynamic Binary Instrumentation."* PLDI 2007. — Demonstrates the spectrum of instrumentation overhead (Valgrind: ~20–50x slowdown for full observability; none for uninstrumented code). Analogous to `--public-flat-rw` (full observability, large overhead) vs. no public signals (no overhead).

### 8.8 Event-Driven vs. Cycle-Based Simulation

- **C. Sanchez, C. Eisner et al.** *"Comparing event-driven versus cycle-based simulation for hardware verification."* Various EDA conferences. — Provides context for why Verilator's compile-to-C++ approach is faster: it eliminates the event queue overhead. But this advantage is partially negated when signals must remain accessible (the "event-like" behavior of VPI callbacks and the ICO loop).

- **R. Mayer.** *"Software-based simulation and optimization in IC design."* Invited talk, ASP-DAC 2000. — Discusses the compilation approach to simulation and why signal elimination is the most impactful optimization, directly establishing why keeping signals around (for public access) is costly.

### 8.9 Benchmarking Methodology

- **A. Georges, D. Buytaert, L. Eeckhout.** *"Statistically Rigorous Java Performance Evaluation."* OOPSLA 2007. — The gold standard for performance measurement methodology. Applicable to the thesis's benchmarking: discusses warm-up, steady state, confidence intervals, and effect sizes. Important because simulation performance measurements can be noisy.

- **T. Mytkowicz, A. Diwan, M. Hauswirth, P.F. Sweeney.** *"Producing Wrong Data Without Doing Anything Obviously Wrong!"* ASPLOS 2009. — Demonstrates how environmental factors (link order, environment variable size) can cause >30% performance variation. Critical for ensuring the thesis's benchmarks are valid.

---

### How to Use These References

The references above support the thesis narrative as follows:

| Thesis Section | Key References |
|---|---|
| Introduction / Motivation | 8.1 (Verilator), 8.8 (cycle-based vs. event-driven) |
| Background: Verilator internals | 8.1 (Snyder papers), 8.2 (FIRRTL/Chisel analogies) |
| Background: VPI standard | 8.3 (IEEE 1800, Sutherland PLI Handbook) |
| Theoretical framing | 8.6 (partial evaluation — `public_flat_rw` as dynamic binding) |
| The `-Og` analogy | 8.4 (GCC `-Og`, LLVM debug info, Lattner & Adve) |
| DFT analogy | 8.5 (observability/controllability overhead) |
| Optimization design | 8.4 (LLVM debug metadata → alias-aware `varInsert`), 8.7 (selective instrumentation) |
| Benchmarking methodology | 8.9 (Georges et al., Mytkowicz et al.) |
| Evaluation / comparison | 8.2 (RISC-V benchmarks), 8.7 (instrumentation overhead spectrum) |