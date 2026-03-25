# VPI Alias Optimization -- Architecture & Design Notes

## Safety Contract

Alias-reduced signals are handled in one of two ways:

1. If an alias signal still has references after `GateInline`, it keeps C++
   storage and remains accessible to generated logic.
2. If an alias signal has no remaining references, its C++ struct member is
   removed. Any direct C++ access then fails at compile time.

For eliminated aliases, access is VPI-only via redirected `varInsert` address.

---

## Overview

This optimization detects trivial aliases (`assign A = B`) in public signals,
re-enables substitution, and redirects VPI registration to the canonical
variable's storage.

To avoid stale C++ members, storage elimination is selective:

- eliminate only aliases proven unreferenced after inlining
- keep aliases still referenced by generated logic/testbench code

This prevents runtime stale-value bugs while still allowing compile-time
failures for truly eliminated signals.

---

## Architecture

### 1. Alias Detection in [src/V3Gate.cpp](src/V3Gate.cpp)

`GateTrivialAliasReduction` runs before `GateInline` and marks safe trivial
aliases with `vpiAlias(target)`.

Key guards:

- only in `--public-flat-rw` mode
- source signal must be user-public (`isSigUserRdPublic()`)
- driver must be public or IO (survives dead-code passes)
- source and target must share the same scope

### 2. Reference-Aware Elimination Marking in [src/V3Gate.cpp](src/V3Gate.cpp)

After `GateInline`, the pass scans all `AstNodeVarRef` and records referenced
variables. For each alias var:

- if still referenced: keep storage
- if unreferenced and not IO: mark `vpiAliasElim(true)` and `noCReset(true)`

`vpiAliasElim` is a dedicated flag for this feature.

### 3. Header Emission in [src/V3EmitCHeaders.cpp](src/V3EmitCHeaders.cpp)

Struct members are skipped only when both are true:

- `vpiAlias()` is set
- `vpiAliasElim()` is true

This avoids accidental removals from unrelated `noCReset` uses.

### 4. VPI Address Redirect in [src/V3EmitCSyms.cpp](src/V3EmitCSyms.cpp)

`EmitCSyms` builds an alias map (`source -> canonical`) and emits `varInsert`
with the canonical variable's address expression, so VPI reads/writes hit live
storage.

---

## Data Fields

In [src/V3AstNodeOther.h](src/V3AstNodeOther.h):

- `vpiAlias`: canonical alias target pointer
- `vpiAliasElim`: dedicated "remove C++ member" marker for this optimization

`vpiAliasElim` exists because `noCReset` is reused elsewhere and is not
specific enough to control struct-member removal safely.

---

## Limitations and Tradeoffs

- Eliminated aliases are intentionally inaccessible from direct C++ API.
- If generated logic still references an alias, it is kept and not eliminated.
- Some aliases that are theoretically removable may remain due to conservative
  reference tracking.
- Optimization is limited to safe same-scope trivial aliases.

---

## Regression Coverage

- `t_aliasing_public`: selective elimination and compile success
- `t_vpi_alias`: VPI read path through alias redirection
- `t_vpi_public_depthn_2`: cross-scope aliases are not misredirected
- `t_export_packed_struct2`: per-signal public paths keep required storage
- `t_dpi_var`, `t_inst_tree_inl1_pub1`: no dangling alias targets
