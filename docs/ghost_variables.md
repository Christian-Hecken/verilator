# Ghost Variables: Architecture and Design

## Overview

Ghost variables are an optimization for Verilator that targets `public_flat_rd`
signals. These signals exist solely for external observability (VPI reads, direct
C++ access) but block important optimizations because Verilator normally treats
all public signals as optimization barriers.

Ghost variables allow the compiler to **inline the ghost's driving expression
into downstream consumers** (via V3Gate), while preserving the ghost variable's
own assignment so that external reads (VPI, direct C++ member access) still
return correct values. This means internal logic benefits from expression
inlining, while external observers see no behavioral change.

## What It Optimizes

Consider a design with many `public_flat_rd` intermediate signals:

```systemverilog
logic [7:0] a;
logic [7:0] c /* verilator public_flat_rd */;
logic [7:0] z;

assign c = ~a;
assign z = c & b;
```

Without ghost variables, `c` being public prevents V3Gate from inlining `~a`
into the computation of `z`. The signal `c` acts as an optimization barrier
— it must be computed, stored, and then read back by `z`'s logic.

With ghost variables, V3Gate recognizes that `c` is a ghost and inlines `~a`
directly into `z`'s computation (`z = (~a) & b`), while still computing
`c = ~a` separately so VPI/C++ reads work. In designs with many such
intermediate public signals, this can significantly reduce the critical path
and improve eval() performance.

## Eligibility Criteria

A signal is ghost-eligible if **all** of the following hold:

1. **Read-only public**: `public_flat_rd` but NOT `public_flat_rw`
2. **Exactly one combinational driver**: a single `assign` or combinational
   `always @*` assignment
3. **No clocked drivers**: not written in any `always_ff` or clocked block
4. **Not a primary I/O**: not a top-level port
5. **Not forced or forceable**: not subject to `force`/`release`
6. **Not DPI-written**: not modified via DPI calls
7. **Not a parameter**: not a `parameter` or `localparam`
8. **All inputs survive**: every signal read by the driving expression is itself
   a non-ghost signal (no transitive ghost chains in the current implementation)

## Architecture

### Compiler Pass: V3Ghost (`src/V3Ghost.cpp`)

Runs after `V3Force` and before `V3DfgOptimizer`/`V3Gate` in the compilation
pipeline (`src/Verilator.cpp`).

**Pass 1** walks the AST and counts combinational vs. clocked write references
for each `AstVarScope`, recording the RHS expression for single-driver signals.

**Pass 2** iterates all `AstVarScope` nodes and applies the eligibility criteria.
Eligible signals get `AstVar::setGhost()` called, setting the `m_isGhost` bit
flag on the AST node.

### Gate Optimization: V3Gate (`src/V3Gate.cpp`)

Two changes enable ghost optimization:

1. **Reducibility** (line ~164): Normal public signals are marked
   `clearReducibleAndDedupable("SigPublic")`, preventing V3Gate from inlining
   their expression. Ghost signals instead keep `reducible=true` while being
   marked `consumed` — this allows V3Gate to inline the ghost's expression into
   consumers while preventing the ghost from being removed as unused.

2. **Assignment preservation** (line ~766): When V3Gate has inlined a signal's
   expression into all consumers, it normally deletes the signal and its driver.
   For ghosts, the deletion is skipped — the assignment `c = ~a` is preserved
   so the backing storage stays correct for external reads.

### Runtime: VerilatedVar (`include/verilated_sym_props.h`)

`VerilatedVar` gains a `VlGhostReadCb` function pointer field. When non-null,
`ghostRead()` calls this callback to lazily recompute the variable's value
before it is read. This is the hook for future work where the eval-loop
assignment could be removed entirely, with the callback taking over as the
sole computation path.

The VPI layer (`include/verilated_vpi.cpp`) calls `varp->ghostRead()` in
`vl_vpi_get_value()` before reading the variable's data, ensuring ghost
variables return fresh values.

`VerilatedScope::varGhostCb()` (`include/verilated.h`, `include/verilated.cpp`)
provides the API for registering ghost read callbacks on a per-variable basis.

### AST Flag: `m_isGhost` (`src/V3AstNodeOther.h`)

A single bit flag on `AstVar` with `isGhost()` / `setGhost()` accessors.
Checked by V3Gate and V3EmitCSyms.

## Design Decisions

### Why `public_flat_rd` only (not `public_flat_rw`)?

`public_flat_rw` signals can be written externally via VPI or DPI. Optimizing
these requires a write callback that propagates the written value to all
downstream consumers — significantly more complex. `public_flat_rd` signals
are read-only from the external perspective, making them safe to optimize
with expression inlining alone.

### Why keep the assignment in eval()?

The current implementation preserves the ghost variable's assignment in the
eval loop (`c = ~a` still runs every cycle). This is the conservative approach
— it guarantees correctness for direct C++ member access without requiring
any callback infrastructure. The runtime callback hooks exist as scaffolding
for a future optimization that would remove the eval-loop assignment entirely
and compute the value only on demand.

### Why no transitive ghosts?

If signal `c` depends on signal `d`, and both are ghost-eligible, making both
ghosts would require `c`'s lazy-eval callback to trigger `d`'s callback first.
This creates ordering dependencies and potential cycles. The current
implementation requires all inputs to be non-ghost, avoiding this complexity.

## Files Modified

| File | Change |
|------|--------|
| `src/V3Ghost.h` | New — declares `V3Ghost::ghostAll()` |
| `src/V3Ghost.cpp` | New — ghost eligibility analysis pass |
| `src/V3AstNodeOther.h` | `m_isGhost` bit flag on `AstVar` |
| `src/V3Gate.cpp` | Ghost-aware reducibility and assignment preservation |
| `src/V3EmitCSyms.cpp` | Placeholder for ghost callback registration |
| `src/V3Dead.cpp` | Comment clarifying ghosts aren't eliminated |
| `src/V3Localize.cpp` | Comment cleanup |
| `src/Verilator.cpp` | Pipeline integration — calls `V3Ghost::ghostAll()` |
| `src/CMakeLists.txt` | Build system — adds V3Ghost.cpp |
| `src/Makefile_obj.in` | Build system — adds V3Ghost.o |
| `include/verilated_sym_props.h` | `VlGhostReadCb` and ghost methods on `VerilatedVar` |
| `include/verilated.h` | `VlGhostReadCb` typedef, `varGhostCb()` on `VerilatedScope` |
| `include/verilated.cpp` | `VerilatedScope::varGhostCb()` implementation |
| `include/verilated_vpi.cpp` | `ghostRead()` call in `vl_vpi_get_value()` |
| `test_regress/t/t_ghost_var.v` | Regression test — Verilog |
| `test_regress/t/t_ghost_var.cpp` | Regression test — C++ driver |
| `test_regress/t/t_ghost_var.py` | Regression test — test runner |

## Known Limitations and Future Work

- **Eval-loop assignment not yet removed**: The ghost variable is still computed
  every cycle. The full optimization (lazy eval only on read) requires emitting
  the ghost read callback in V3EmitCSyms, which is scaffolded but not yet wired.
- **No transitive ghost chains**: A ghost's inputs must all be non-ghost.
- **No `public_flat_rw` support**: Write-path optimization is not implemented.
- **No wide signal support**: The callback infrastructure supports scalar types;
  `VlWide<N>` signals are not yet handled.
- **Single-scope only**: Cross-scope ghost dependencies are not analyzed.

## Pitfalls and Difficulties

This section documents problems encountered during implementation and their
resolutions, as a reference for future work.

### VPI and DPI write-back (resolved by scoping to `public_flat_rd`)

The original design targeted `public_flat_rw` signals. This required a write
callback so that VPI `vpi_put_value()` and DPI writes could propagate changes
back to downstream logic. Implementing this correctly required:

- A `VlGhostVar<T>` proxy template with `operator=` triggering write callbacks
- Write callback code generation in V3EmitCSyms
- Correct interaction with `evalNeeded` / re-evaluation triggers

The write path proved too complex for an initial implementation. Signals that
can be externally written (`public_flat_rw`, DPI-writable, forceable) interact
with scheduling, force/release semantics, and the DFG optimizer in ways that
are difficult to get right without extensive testing. Scoping to read-only
signals eliminated the entire write-path problem.

### Force/release implementation signals

Verilator's V3Force pass generates internal signals like `__VforceEn` and
`__VforceVal` that are marked `sigUserRWPublic(true)` for VPI access but are
NOT themselves marked `isForceable()`. An early eligibility check that only
excluded `isForceable()` signals missed these, causing them to be incorrectly
ghosted. The fix was to exclude all `isSigUserRWPublic()` signals (which is
now implicit since we require `!isSigUserRWPublic()`).

### VerilatedVar ABI sensitivity

Adding fields to `VerilatedVar` changes the struct's size and layout. Early
iterations added two pointer fields (`m_ghostReadCb`, `m_ghostWriteCb`) between
existing members, which broke `VerilatedVarNameMap` (a `std::map<string,
VerilatedVar>`) — the map's value type changed size, causing DPI `varFind()`
lookups to return garbage. The current implementation adds only one pointer
field (`m_ghostReadCb`), which is sufficient for read-only ghosts. Any future
additions must be careful about ABI compatibility with existing compiled
testbenches.

### V3Gate interaction: consumed vs. reducible

V3Gate uses two properties to decide what to do with a signal:
- **reducible**: can the signal's expression be inlined into consumers?
- **consumed**: is the signal used by something that prevents removal?

Normal public signals are `consumed=true, reducible=false` — they can't be
inlined and can't be removed. Ghost signals need `consumed=true,
reducible=true` — they CAN be inlined but should NOT be removed (because the
assignment must persist for VPI reads). Getting this combination wrong in
either direction causes either missed optimization (if not reducible) or
incorrect removal (if not consumed).

Additionally, when V3Gate has inlined a ghost's expression into all consumers
and the ghost's output edge list is empty, V3Gate normally deletes the signal
and its driver. Ghost signals must skip this deletion to preserve their
eval-loop assignment.

### Optimization pass interactions

Multiple optimization passes needed investigation:
- **V3DfgOptimizer**: Safe — ghost vars keep their assignment via `sigPublic`
  protection in V3Dead, so DFG optimization of consumers is fine.
- **V3Life**: Safe — doesn't affect ghost variables since they have public
  visibility.
- **V3Localize**: Safe — `isSigPublic()` check already prevents localization.
- **V3SplitVar**: Safe — doesn't split public signals.
- **V3Inline**: Safe — module inlining preserves variable properties.
- **V3Sched**: Safe — ghost variables are still computed in the ICO/combo
  eval loop since their assignments are preserved.

Only V3Gate required ghost-specific changes. The other passes' existing
protections for public signals also protect ghost variables.

### Transitive ghost dependencies

If signal `c = f(d)` and signal `d = g(a)` are both ghost-eligible, making
both ghosts creates a dependency chain. When `c`'s lazy-eval callback fires,
it needs `d`'s current value, but `d` is also a ghost whose assignment might
have been removed. This requires either:
1. Topological ordering of ghost callbacks, or
2. Each ghost callback triggering its input ghosts recursively

Both approaches add complexity and risk of cycles. The current implementation
avoids this entirely by requiring all inputs to be non-ghost (`allInputsSurvive`
check). This limits the number of eligible signals but guarantees correctness.
A future improvement could process signals in topological order and allow
chains, using the dependency graph to sequence callback registration.
