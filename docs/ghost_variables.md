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
5. **Not a port**: `varType() != VVarType::PORT` — submodule ports retain
   PORT type after inlining even though direction is cleared, and DPI context
   code can access them via raw data pointers that bypass ghost callbacks
6. **Not forced or forceable**: not subject to `force`/`release`
7. **Not DPI-written**: not modified via DPI calls
8. **Not a parameter**: not a `parameter` or `localparam`
9. **All inputs survive**: every signal read by the driving expression is itself
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

### Lazy-Eval Function Creation: V3GhostFunc (`src/V3GhostFunc.cpp`)

Runs after V3Gate in the pipeline. This pass moves un-pinned ghost
assignments out of the eval loop into a `_ghostEval` CFunc that is
registered as a VPI read callback.

1. **collectGhostAssigns**: Walks Active blocks for `AstAssignW` nodes
   whose LHS variable has `isGhost()` set.
2. **analyzeReaders**: Determines which ghost variables are read by
   non-ghost eval-loop code ("eval-loop readers") vs. only by other
   ghost assignments ("ghost-to-ghost" references).
3. **computePinned**: Iterative pinning — ghost variables with eval-loop
   readers are pinned. If a pinned ghost reads from another ghost, that
   dependency is also pinned transitively.
4. **clearGhost on pinned vars**: Pinned variables have their ghost flag
   cleared so V3EmitCSyms won't register callbacks for them. Their
   assignments stay in the eval loop.
5. **topoSort**: Un-pinned ghosts are topologically sorted so that
   dependencies are computed before dependents in the callback.
6. **createGhostFuncs**: Creates one `_ghostEval(void* voidSelf)` CFunc
   per scope, moves un-pinned ghost assignments into it, and removes
   them from the Active blocks.

### Symbol Callback Registration: V3EmitCSyms (`src/V3EmitCSyms.cpp`)

Emits code in the `__Vsymtab` constructor to register `_ghostEval` as
the ghost read callback for each un-pinned ghost variable:

```cpp
__Vscopep->varGhostCbs("varname", &ModClass___ghostEval, static_cast<void*>(&(TOP)));
```

Also emits a forward declaration for the `_ghostEval` function.

### Runtime: VerilatedVar (`include/verilated_sym_props.h`)

`VerilatedVar` has `VlGhostReadCb` and `m_ghostReadCtx` fields. When
`ghostRead()` is called, it invokes the callback with the context pointer
(the module instance), which lazily recomputes all ghost variables for
that scope.

The VPI layer (`include/verilated_vpi.cpp`) calls `varp->ghostRead()` in
`vl_vpi_get_value()` before reading the variable's data, ensuring ghost
variables return fresh values.

`VerilatedScope::varGhostCbs()` (`include/verilated.h`, `include/verilated.cpp`)
provides the API for registering ghost read callbacks on a per-variable basis,
taking a callback function pointer and a context pointer.

### AST Flag: `m_isGhost` (`src/V3AstNodeOther.h`)

A single bit flag on `AstVar` with `isGhost()` / `setGhost()` / `clearGhost()`
accessors. Set by V3Ghost, checked by V3Gate and V3EmitCSyms. Cleared by
V3GhostFunc for pinned variables that must remain in the eval loop.

## Design Decisions

### Why `public_flat_rd` only (not `public_flat_rw`)?

`public_flat_rw` signals can be written externally via VPI or DPI. Optimizing
these requires a write callback that propagates the written value to all
downstream consumers — significantly more complex. `public_flat_rd` signals
are read-only from the external perspective, making them safe to optimize
with expression inlining alone.

### Lazy eval vs. eval-loop assignment

V3GhostFunc performs pinning analysis to determine which ghost assignments
can safely be removed from the eval loop. Un-pinned ghosts (those with no
eval-loop readers after V3Gate inlining) have their assignments moved to a
lazy-eval CFunc that only runs when the variable is read via VPI. Pinned
ghosts (those still read by eval-loop code) remain in the eval loop with
their ghost flag cleared.

Direct C++ member access to a ghost variable does NOT trigger the callback
— it reads whatever stale value is in the backing storage. Only VPI access
calls `ghostRead()` to refresh the value.

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
| `src/V3GhostFunc.h` | New — declares `V3GhostFunc::ghostFuncAll()` |
| `src/V3GhostFunc.cpp` | New — lazy-eval CFunc creation, pinning analysis |
| `src/V3EmitCSyms.cpp` | Ghost callback registration and forward declarations |
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

- **No transitive ghost chains**: A ghost's inputs must all be non-ghost.
  Allowing ghost-to-ghost chains would require topological callback ordering.
- **No `public_flat_rw` support**: Write-path optimization is not implemented.
- **No wide signal support**: The callback infrastructure supports scalar types;
  `VlWide<N>` signals are not yet handled.
- **Direct C++ member access reads stale values**: Only VPI access triggers
  the ghost callback. Direct `model->rootp->varname` access reads whatever
  was last stored, which may be stale for un-pinned ghosts.
- **Chain-pattern performance**: When ghost variables form a linear dependency
  chain, V3Gate inlines expressions transitively, causing the pinning analysis
  to pin alternating stages. The optimization is most effective for
  independently-computed ghost variables.

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

### Transitive ghost dependencies and pinning

If signal `c = f(d)` and signal `d = g(a)` are both ghost-eligible, making
both ghosts creates a dependency chain. V3GhostFunc handles this via the
pinning analysis: if `c` is read by eval-loop code, both `c` AND `d` are
pinned (their assignments stay in the eval loop). Only ghosts with NO
eval-loop readers (after V3Gate inlining) are moved to the lazy-eval callback.

The `allInputsSurvive` check in V3Ghost prevents a ghost from depending on
another ghost at eligibility time. However, V3Gate's inlining may later
create ghost-to-ghost references when it substitutes expressions. The
V3GhostFunc pinning analysis handles these correctly by tracking which ghosts
read from other ghosts and pinning transitively.

### Submodule port exclusion

After V3Inline, submodule ports retain their `VVarType::PORT` type but lose
their `VDirection` — `isIO()` returns false. DPI context code accesses these
variables via `svGetScope()` + `varFind()` + raw `datap()` pointers, bypassing
the VPI ghost callback path. If such a port were ghosted and its assignment
moved to the lazy-eval callback, DPI reads would see stale values. The fix
is to exclude `VVarType::PORT` variables from ghost eligibility.
