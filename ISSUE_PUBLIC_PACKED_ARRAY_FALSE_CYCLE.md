# Verilator Issue: False-Positive Cycle Detection with `--public-flat-rw` on Packed Arrays

**Date Discovered**: 2026-03-03  
**Verilator Version**: 5.045 devel (rev v5.044-229-g0d2fcfd49)  
**Severity**: Medium (False positive warning; blocks valid designs with global public signals)

---

## Summary

When using `--public-flat-rw` to mark all signals as VPI-accessible, Verilator's combinational cycle detector incorrectly identifies a **false-positive circular combinational logic** warning on a simple feedforward wire chain implemented as a packed array.

The same design compiles **without warnings** in the baseline case (without `--public-flat-rw`), proving there is no actual combinational loop in the design.

---

## The Problem

### Design (Simple Feedforward Chain)

```verilog
module DUT #(parameter int DEPTH = 256) (input logic in, output logic out);
  wire [DEPTH-1:0] tmp;

  genvar i;
  generate
    for (i = 0; i < DEPTH; i = i + 1) begin : CHAIN
      if (i == 0) begin
        assign tmp[i] = in & 1'b1;
      end else begin
        // Pure feedforward: tmp[i] depends only on tmp[i-1]
        // NO FEEDBACK LOOP
        assign tmp[i] = tmp[i-1] & 1'b1;
      end
    end
  endgenerate

  assign out = tmp[DEPTH-1];
endmodule

module top(input logic in, output logic out);
  DUT #(.DEPTH(256)) dut (.in(in), .out(out));
endmodule
```

### Expected Behavior

- Compilation succeeds; no warnings
- This is a classic shift-chain / ripple structure with zero combinational loops

### Observed Behavior (with `--public-flat-rw`)

```
%Warning-UNOPTFLAT: .../test_regress/t/t_aliasing.v:4:20: Signal unoptimizable: 
Circular combinational logic: 'top.dut.tmp'
    4 |   wire [DEPTH-1:0] tmp;
```

### Key Observation

- **Without `--public-flat-rw`**: Compiles cleanly ✓
- **With `--public-flat-rw`**: False-positive cycle warning ✗
- Conclusion: The issue is triggered **specifically by marking the packed array public**

---

## Root Cause Analysis

### Source Code Location

**File**: `src/V3SchedAcyclic.cpp`  
**Key Functions**:
- `buildGraph()` (line 128): Constructs dependency graph from combinational logic
- `reportCycles()` (line 357): Reports UNOPTFLAT warnings
- `breakCycles()` (line 488): Main entry point for cycle detection

**Detection Flow**:
1. `buildGraph()` creates a graph of logic vertices and variable vertices
2. For each `AstVarRef` in the logic, edges are added:
   - If written: `logic → var` (producer)
   - If read: `var → logic` (consumer)
3. `removeNonCyclic()` removes vertices not part of cycles
4. `graphp->acyclic()` cuts edges to make acyclic
5. `reportCycles()` emits warnings for remaining variable cut-vertices

### Hypothesis

When a signal is marked public with `--public-flat-rw`, it probably:
1. Gets added to an additional set of dependencies (for VPI accessibility tracking)
2. Gets included in the cycle graph even though it should not be
3. OR: The graph edge detection treats each bit of a packed array differently, creating artificial self-loops or feedback paths

**Need Investigation**:
- How does `isSigPublic()` affect the logic collection in `buildGraph()`?
- Does packed array bit-level tracking create spurious edges?
- Is there a user2/user3 flag issue in `buildGraph()` line 150-160 that causes re-processing?

---

## Reproduction Steps

### Files Involved

- Source: `test_regress/t/t_aliasing.v`
- Test (baseline): `test_regress/t/t_aliasing.py`
- Test (public): `test_regress/t/t_aliasing_public.py`
- Harness: `test_regress/t/t_aliasing_tb.cpp`

### Commands

```bash
cd test_regress

# No warning (baseline)
python3 driver.py t/t_aliasing.py

# False-positive warning (public)
python3 driver.py t/t_aliasing_public.py
```

### Expected Output

```
# Without --public-flat-rw:
[Success, no warnings]

# With --public-flat-rw:
%Warning-UNOPTFLAT: .../t_aliasing.v:4:20: 
Signal unoptimizable: Circular combinational logic: 'top.dut.tmp'
```

---

## Design Patterns Affected

Any design using:
- **Packed-array wire chains** (common for wide buses, shift registers, pipelines)
- **Generated for-loop assignments** to packed-array elements
- **Global `--public-flat-rw`** flag (for full VPI accessibility)

---

## Workarounds

### Workaround 1: Add `.vlt` Waiver

Create `test_regress/t/t_aliasing.vlt`:
```verilog
lint_off -rule UNOPTFLAT -file "*t_aliasing.v" -match "Signal unoptimizable*"
```

Then reference in Python test:
```python
test.compile(..., verilator_flags2=[
    "--vlt t/t_aliasing.vlt",
    "--public-flat-rw",
    "--exe", test.pli_filename
])
```

### Workaround 2: Use Explicit Wire Assignments

Instead of a packed array with a generate loop, use individual explicit assignments:
```verilog
wire w0 = in & 1'b1;
wire w1 = w0 & 1'b1;
wire w2 = w1 & 1'b1;
...
wire w31 = w30 & 1'b1;
assign out = w31;
```

This avoids packed-array logic entirely (but is less elegant for parameterized designs).

### Workaround 3: Selective Public Marking

Use `/*verilator public_flat_rw*/` on only the signals that need VPI access, instead of global `--public-flat-rw`.

---

## Next Steps for Fixing

### Investigation Phase

1. **Inspect `buildGraph()` more closely**: 
   - How are packed-array bits handled?
   - Is `user2` / `user3` being set correctly to prevent re-visitation?
   - Does `isSigPublic()` cause the logic collector to add extra edges?

2. **Trace the cycle detection**:
   - Add debug output to `buildGraph()` to see what edges are created for packed arrays
   - Compare edge lists: baseline vs. public variant
   - Use `--debug` and `dumpGraphLevel >= 6` to generate `.dot` files

3. **Check related code**:
   - `src/V3Gate.cpp` (lines 164-168): Uses `isSigPublic()` in gate reduction
   - `src/V3Dead.cpp` (line 415): Uses `isSigPublic()` in dead code elimination
   - Look for any special logic that treats public signals differently in cycle detection

### Fix Candidates

1. **If the issue is in edge creation**: Fix `buildGraph()` to exclude internal packed-array bits from cycle analysis when they form a simple chain
2. **If the issue is in flag persistence**: Ensure `user2` / `user3` flags are reset properly between iterations
3. **If the issue is in public signal handling**: Exclude public-signal-only dependency edges from the acyclicity check (since they're not real combinational loops)

### PR Preparation

Once the root cause is identified:
1. Create a minimal test case (already have: `t_aliasing.v` with `--public-flat-rw`)
2. Write a unit test or regression test
3. Document the fix in code comments (especially in `buildGraph()` or `reportCycles()`)
4. Ensure no regressions on existing tests: `make test` or run full test suite

---

## References

- **Verilator Source**: `src/V3SchedAcyclic.cpp` (cycle detection)
- **Verilator Source**: `src/V3AstNodes.cpp` lines 551-553 (`isSigPublic()` definition)
- **Test Files**:
  - `test_regress/t/t_aliasing.v` (reproducer)
  - `test_regress/t/t_aliasing.py` (baseline test)
  - `test_regress/t/t_aliasing_public.py` (public variant test)
  - `test_regress/obj_vlt/t_aliasing_public/vlt_compile.log` (build output)

---

## Additional Context

This issue was discovered while investigating the performance impact of `--public-flat-rw` on signal optimization. The false-positive warning prevents using packed-array-based microbenchmarks to measure the actual overhead of making internal signals VPI-accessible.

**Thesis Context**: [See `/workspaces/verilator/thesis_proposal.md`](thesis_proposal.md) section 3.A (ICO Loop Logic Replication) and section 5 (Optimization Opportunities).
