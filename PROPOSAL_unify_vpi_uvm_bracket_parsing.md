# Proposal: Unify VPI and UVM Bracket Parsing

## Problem Statement

There are two independent bracket-parsing systems that overlap:

1. **VPI layer** (`include/verilated_vpi.cpp`):
   - `vpi_parse_single_index()` — parses `[N]`, rejects `[hi:lo]`
   - `vpi_parse_indices()` — parses `[0][3][2]` right-to-left, fails entirely if ANY bracket has `:`
   - Used by `vpi_handle_by_name()` for array indexing

2. **UVM HDL layer** (`test_regress/t/uvm/v*/dpi/uvm_hdl_verilator.c`):
   - `uvm_hdl_parse_bitrange()` — parses trailing `[hi:lo]` or `[idx]`
   - `uvm_hdl_handle_by_name_partsel()` — tries `vpi_handle_by_name()`, if it fails strips the last bracket and retries
   - `uvm_hdl_set_vlog()` / `uvm_hdl_get_vlog()` — read-modify-write using `svGetPartselLogic`/`svPutPartselLogic`

These will fight each other. For `"mem[0][3][15:8]"`:
1. `vpi_parse_indices` tries `[15:8]` first (right-to-left), sees `:`, **fails entirely** (returns false, array indices lost)
2. `vpi_handle_by_name("mem[0][3][15:8]")` → fails
3. UVM layer retries: strips `[15:8]`, calls `vpi_handle_by_name("mem[0][3]")` → now works
4. UVM applies bit-range with `svGetPartselLogic`/`svPutPartselLogic`

This means **two VPI calls**, **two independent parsers**, and fragile coupling.

## Proposed Architecture

Move bit-range handling into `vpi_handle_by_name()`. The returned `VerilatedVpioVar` handle
carries the bit-range information natively via its existing `m_bitOffset` and a new `m_partselBits`
field. This is the same mechanism `withIndex()` already uses for packed dimension indexing.

### After unification, for `"mem[0][3][15:8]"`:
1. `vpi_parse_indices`: parses `[15:8]` as bit-range, `[3]` and `[0]` as array indices
2. `vpi_handle_by_name`: looks up `"mem"`, applies indices via `withIndex()`, applies bit-range via new `withPartSelect()`
3. Returns handle with `bitOffset()=8`, `bitSize()=8`
4. `vpi_get_value` / `vpi_put_value` automatically read/write only bits 15:8
5. UVM layer just calls `vpi_handle_by_name()` once — no partsel logic needed

## Detailed Changes

### 1. `vpi_parse_indices()` — extend to extract optional bit-range

```cpp
// New struct for bit-range info
struct VlVpiBitRange {
    int32_t hi = 0;
    int32_t lo = 0;
    bool valid = false;
};

// Updated signature — optional bitRange output parameter
static bool vpi_parse_indices(std::string& name,
                              std::vector<PLI_INT32>& indices,
                              VlVpiBitRange* bitRange = nullptr);
```

**Behavior change**: When the rightmost bracket contains `:`, instead of failing:
- Extract `[hi:lo]` into `bitRange` (if caller provided the pointer)
- Strip that bracket from the name
- Continue parsing remaining brackets as integer indices as before
- If `bitRange` is nullptr (backwards compat), fall back to current behavior (fail on `:`)

### 2. `VerilatedVpioVar` — add part-select support

```cpp
class VerilatedVpioVar VL_NOT_FINAL : public VerilatedVpioVarBase {
    // ... existing fields ...
    int32_t m_partselBits = -1;  // NEW: -1 means no part-select active

public:
    // NEW: Create a part-selected view of this variable
    VerilatedVpioVar* withPartSelect(int32_t hi, int32_t lo) const;

    // MODIFIED: Override to return narrowed width when part-select active
    uint32_t bitSize() const {
        if (m_partselBits >= 0) return m_partselBits;
        // ... existing logic ...
    }

    // MODIFIED: Override to return narrowed width when part-select active
    uint32_t size() const override {
        if (m_partselBits >= 0) return m_partselBits;
        // ... existing logic ...
    }
};
```

**`withPartSelect(hi, lo)` implementation**:
```cpp
VerilatedVpioVar* withPartSelect(int32_t hi, int32_t lo) const {
    // Must have a current range (packed dimension) to select from
    const VerilatedRange* range = get_range();
    if (!range) return nullptr;

    // Normalize the selection range
    int32_t sel_lo = std::min(hi, lo);
    int32_t sel_hi = std::max(hi, lo);
    int32_t decl_left = range->left();
    int32_t decl_right = range->right();
    int32_t decl_lo = std::min(decl_left, decl_right);
    int32_t decl_hi = std::max(decl_left, decl_right);

    // Range check
    if (sel_lo < decl_lo || sel_hi > decl_hi) return nullptr;

    int32_t width = sel_hi - sel_lo + 1;

    // Convert to storage bit position
    // For [31:0] (descending): bit N maps to storage position N - decl_lo
    // For [0:31] (ascending):  bit N maps to storage position decl_right - N
    int32_t normalized_lo;
    if (decl_left > decl_right)  // descending [31:0]
        normalized_lo = sel_lo - decl_lo;
    else  // ascending [0:31]
        normalized_lo = decl_right - sel_hi;

    auto ret = new VerilatedVpioVar{this};
    ret->m_bitOffset += normalized_lo;
    ret->m_partselBits = width;
    return ret;
}
```

This is exactly the normalization logic currently in `uvm_hdl_handle_by_name_partsel`, but
expressed using the `VerilatedRange` API that `VerilatedVpioVar` already has access to.

### 3. `vpi_handle_by_name()` — apply bit-range after indices

```cpp
vpiHandle vpi_handle_by_name(PLI_BYTE8* namep, vpiHandle scope) {
    // ... existing setup ...

    std::string scopeAndName = namep;
    std::vector<PLI_INT32> indices;
    VlVpiBitRange bitRange;
    bool has_indices = vpi_parse_indices(scopeAndName, indices, &bitRange);

    // ... existing scope/var lookup (unchanged) ...

    // Apply array indices (existing code)
    if (has_indices && !indices.empty()) {
        result_handle = vpi_handle_by_multi_index(result_handle, indices.size(), indices.data());
        if (!result_handle) return nullptr;
    }

    // NEW: Apply bit-range part-select
    if (bitRange.valid) {
        VerilatedVpioVar* varop = VerilatedVpioVar::castp(result_handle);
        if (!varop) return nullptr;
        VerilatedVpioVar* partsel = varop->withPartSelect(bitRange.hi, bitRange.lo);
        if (!partsel) return nullptr;
        result_handle = partsel->castVpiHandle();
    }

    return result_handle;
}
```

### 4. UVM HDL layer — simplify dramatically

**Delete**: `uvm_hdl_parse_bitrange()`, `uvm_hdl_handle_by_name_partsel()`

**Simplify `uvm_hdl_set_vlog()`**:
```c
static int uvm_hdl_set_vlog(char *path, p_vpi_vecval value, PLI_INT32 flag) {
    vpiHandle r = vpi_handle_by_name(path, 0);
    if (r == 0) {
        m_uvm_error("UVM/DPI/HDL_SET", "...", path);
        return 0;
    }
    // vpi_get(vpiSize) returns the narrowed width for part-selected handles
    int size = vpi_get(vpiSize, r);
    // ... max width check ...

    s_vpi_value value_s = {vpiVectorVal};
    value_s.value.vector = value;
    s_vpi_time time_s = {vpiSimTime, 0, 0, 0.0};
    vpi_put_value(r, &value_s, &time_s, flag);
    vpi_release_handle(r);
    return 1;
}
```

**Simplify `uvm_hdl_get_vlog()`**:
```c
static int uvm_hdl_get_vlog(char *path, p_vpi_vecval value, PLI_INT32 flag, int partsel) {
    vpiHandle r = vpi_handle_by_name(path, 0);
    if (r == 0) {
        m_uvm_error("UVM/DPI/VLOG_GET", "...", path);
        return 0;
    }
    int size = vpi_get(vpiSize, r);
    // ... max width check ...
    int chunks = (size - 1) / 32 + 1;

    s_vpi_value value_s = {vpiVectorVal};
    vpi_get_value(r, &value_s);
    for (int i = 0; i < chunks; ++i) {
        value[i].aval = value_s.value.vector[i].aval;
        value[i].bval = value_s.value.vector[i].bval;
    }
    vpi_release_handle(r);
    return 1;
}
```

No more `svGetPartselLogic`/`svPutPartselLogic`. No more `is_partsel` branches.

## Why This Works

The existing VPI value read/write infrastructure already handles `bitOffset()` and `bitSize()`:

- **`vl_vpi_var_access_info()`** (line ~2760): Computes masks using `vop->bitOffset()` and
  `vop->bitSize()`, handles word-straddling for WDATA. A narrowed handle simply produces
  tighter masks at the correct position.

- **`vl_vpi_get_word_gen()`** (line ~2817): Uses `bitCount = min(bitCount, varBits - addOffset)`
  where `varBits = vop->bitSize()`. A narrowed handle limits the read automatically.

- **`vl_vpi_put_word_gen()`** (line ~2826): Same pattern — masks ensure only selected bits
  are written, preserving surrounding bits. This is a read-modify-write at the word level.

- **`vpi_put_value` vpiBinStrVal path** (line ~3233): Iterates `for (i = 0; i < varBits; ++i)`
  with `pos = valueVop->bitOffset() + i`. A narrowed handle limits the loop and shifts
  positions correctly.

- **`vpi_get(vpiSize)`** (line ~2604): Returns `vop->size()`. With the override, part-selected
  handles return the narrowed width, so the UVM layer allocates correct buffer sizes.

## Edge Cases

| Case | Handling |
|------|----------|
| `signal[5]` (single bit) | Already works via `withIndex(5)` — treated as packed dim index |
| `signal[5:5]` (single bit range) | `withPartSelect(5,5)` → width=1, same result as `[5]` |
| `signal[15:8]` (simple range) | No array indices, just `withPartSelect(15,8)` |
| `mem[0][3][15:8]` (array + range) | Indices {0,3} via `withIndex`, then `withPartSelect(15,8)` |
| `signal[8+:8]` (indexed part-sel) | Not supported (neither system handles this today) |
| `mem[0:1][3]` (slice) | Not supported (unpacked array slice, separate issue) |
| `signal` (no brackets) | No change, works as before |
| Out-of-range `[99:90]` | `withPartSelect` returns nullptr → `vpi_handle_by_name` returns nullptr |

## Testing

All existing tests should continue to pass:
- `t_vpi_var.py` — comprehensive VPI test
- `t_uvm_dpi_v2017_1_0.py` / `t_uvm_dpi_v2020_3_1.py` — UVM HDL access
- `t_unpacked_array_slice.py` — expected-fail slice test
- `t_vpi_multi_index.py` — multi-index VPI test

New test coverage needed:
- `vpi_handle_by_name("signal[15:8]")` returns handle with correct `vpiSize` = 8
- `vpi_get_value` / `vpi_put_value` on part-selected handle reads/writes correct bits
- Ascending range `[0:31]` normalization works correctly
- Out-of-range part-select returns nullptr

## Summary of Files Changed

| File | Changes |
|------|---------|
| `include/verilated_vpi.cpp` | Add `VlVpiBitRange` struct, modify `vpi_parse_indices`, add `withPartSelect` to `VerilatedVpioVar`, modify `vpi_handle_by_name` |
| `test_regress/t/uvm/v2017_1_0/dpi/uvm_hdl_verilator.c` | Remove `uvm_hdl_parse_bitrange`, `uvm_hdl_handle_by_name_partsel`; simplify set/get functions |
| `test_regress/t/uvm/v2020_3_1/dpi/uvm_hdl_verilator.c` | Same changes (identical file) |

## Key Design Principle

**Single responsibility**: The VPI layer handles ALL name-to-handle resolution
(scopes, variables, array indices, and bit ranges). The UVM layer is just a thin
wrapper that calls `vpi_handle_by_name()` and `vpi_get_value()`/`vpi_put_value()`.

The `VerilatedVpioVar` handle is the single source of truth for what data is being
accessed — its `varDatap()`, `bitOffset()`, and `bitSize()` fully describe the view.
