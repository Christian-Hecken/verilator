# Remaining work for `vpi_handle_by_name` expression support

## Current state (as of Feb 2026)

The `vpi_handle_by_name` function now supports rich indexing expressions
in the name string. All parsing lives in `include/verilated_vpi.cpp`.

### What's implemented

| Feature | Example | Since |
|---|---|---|
| Multi-dim array indexing | `mem[0][3][2]` | Branch baseline |
| Bit-range part-select | `sig[15:8]` | This session |
| Indexed part-select `+:` | `sig[8+:8]` → `[15:8]` | This session |
| Indexed part-select `-:` | `sig[15-:8]` → `[15:8]` | This session |
| Arithmetic (`+`,`-`,`*`,`/`) | `sig[4*8-1:3*8]` | This session |
| Parentheses | `sig[(WIDTH-1):0]` | This session |
| Unary `+`/`-` | `sig[-1+32:0]` | This session |
| Identifier resolution | `sig[WIDTH-1:0]` | This session |
| Combined index + part-sel | `mem[N-1][BYTE+:BYTE]` | This session |

### What's NOT implemented (ordered by likely user impact)

#### 1. Hex / octal / binary number literals
**Impact: HIGH** — users commonly write `sig[8'hFF:0]` or `sig[0xFF:0]`.

**Where to change:** `vpi_eval_atom()` at line ~2254 of `verilated_vpi.cpp`.
Currently the number branch only handles decimal via `std::strtol(..., 10)`.

**Implementation sketch:**
```cpp
// In vpi_eval_atom, replace the decimal-only number parsing with:
if (isdigit(static_cast<unsigned char>(s[pos]))) {
    // Check for 0x/0X (hex), 0b/0B (binary), 0 (octal) prefixes
    if (s[pos] == '0' && pos + 1 < len) {
        if (s[pos+1] == 'x' || s[pos+1] == 'X') {
            // Parse hex: 0xFF
            result = std::strtol(s + pos, &endp, 16);
            ...
        } else if (s[pos+1] == 'b' || s[pos+1] == 'B') {
            // Parse binary: 0b1010
            result = std::strtol(s + pos + 2, &endp, 2);
            ...
        }
        // else: octal or just '0'
    }
    // Also consider SV-style sized literals: 8'hFF, 32'd10, 8'b1010
    // The size prefix (8') can be parsed, then dispatch on base letter.
}
```

**SV-style sized literals** (`8'hFF`, `32'd10`, `8'b1010`):
- Parse a decimal number, check for `'` followed by base letter (`h`,`d`,`o`,`b`)
- The size prefix constrains the width but for index purposes the value is what matters
- This is a nice-to-have beyond C-style `0x` prefixes

**Test to update:** `t_vpi_var.cpp` line ~1689 currently expects `mem_2d[0x2][3]`
to return NULL. Change it to verify the correct value when hex is supported.

#### 2. `$` (last-element / MSB) operator
**Impact: MEDIUM** — used in SV for `arr[$]`, `sig[$:16]`, `sig[$-1:0]`.

**Where to change:** `vpi_eval_atom()` — when encountering `$`, look up the
signal being indexed and return its MSB/last index.

**Challenge:** The evaluator doesn't currently know *which* signal is being
indexed. The scope prefix helps resolve identifiers, but `$` needs the
signal's own range. Options:
- Pass the signal name into the evaluator so it can look up the range
- Post-process: replace `$` tokens before evaluation begins
- Evaluate `$` lazily after the signal handle is obtained

This is more complex than the other items because it depends on context
(which dimension of which signal).

#### 3. Modulo operator (`%`)
**Impact: LOW** — occasionally useful in parameterized/generated contexts.

**Where to change:** `vpi_eval_term()` at line ~2310 — add `'%'` alongside
`'*'` and `'/'` in the operator check. Trivial change:
```cpp
if (op != '*' && op != '/' && op != '%') break;
// ...
if (op == '%') {
    if (rhs == 0) return false;
    result %= rhs;
}
```

#### 4. Bitwise and shift operators (`&`, `|`, `^`, `~`, `<<`, `>>`)
**Impact: LOW** — rarely used in indexing expressions.

**Where to change:** Extend the recursive descent grammar:
```
expr   → bitor_expr
bitor  → bitxor (('|') bitxor)*
bitxor → bitand (('^') bitand)*
bitand → shift  (('&') shift)*
shift  → add    (('<<' | '>>') add)*
add    → term   (('+' | '-') term)*
term   → factor (('*' | '/' | '%') factor)*
factor → ['+'|'-'|'~'] factor | atom
```
This is straightforward but adds ~40 more lines of parsing functions.

#### 5. Power operator (`**`)
**Impact: VERY LOW** — exists in SV constant expressions, almost never in indexing.

**Where to change:** Add between factor and atom levels in the grammar.

---

## Architecture reference

### Key files
- **`include/verilated_vpi.cpp`** — all parsing and VPI implementation
- **`test_regress/t/t_vpi_partsel.cpp`** — test for part-select, arithmetic, identifiers, +:/−:
- **`test_regress/t/t_vpi_partsel.v`** — Verilog test module (signals + parameters)
- **`test_regress/t/t_vpi_partsel.py`** — test driver
- **`test_regress/t/t_vpi_var.cpp`** — existing VPI test (has some index edge cases)

### Key functions (all in `verilated_vpi.cpp`)

| Function | Line | Purpose |
|---|---|---|
| `vpi_eval_skip_ws` | ~2247 | Skip whitespace in expression |
| `vpi_eval_is_ident_start` | ~2251 | Check if char starts an identifier |
| `vpi_eval_is_ident_char` | ~2252 | Check if char continues an identifier |
| `vpi_eval_atom` | ~2254 | Parse atom: `(expr)`, number, or identifier |
| `vpi_eval_factor` | ~2296 | Parse unary +/- and atoms |
| `vpi_eval_term` | ~2310 | Parse `*`, `/` |
| `vpi_eval_expr` | ~2332 | Parse `+`, `-` |
| `vpi_eval_const_expr` | ~2354 | Entry point: evaluate full string |
| `vpi_parse_single_index` | ~2368 | Parse `[expr]` as array index |
| `vpi_parse_bit_range` | ~2433 | Parse `[hi:lo]`, `[base+:width]`, `[base-:width]` |
| `vpi_parse_indices` | ~2509 | Parse all trailing brackets from a name |
| `VerilatedVpioVar::withPartSelect` | ~420 | Apply bit-range to a variable handle |
| `VerilatedVpioVar::withIndex` | ~459 | Apply array index to a variable handle |

### Data flow

```
vpi_handle_by_name("t.mem[WIDTH-1][BYTE+:BYTE]")
  │
  ├─ vpi_parse_indices() extracts scope="t.", indices={WIDTH-1}, bitRange={BYTE+:BYTE}
  │    ├─ vpi_parse_bit_range() → detects +: → base_str="BYTE", width_str="BYTE"
  │    │    ├─ vpi_eval_const_expr("BYTE", scope="t.") → vpi_eval_atom → ident "BYTE"
  │    │    │    → vpi_handle_by_name("t.BYTE") → vpi_get_value → 8
  │    │    └─ computes hi=15, lo=8
  │    └─ vpi_parse_single_index() → "WIDTH-1"
  │         └─ vpi_eval_const_expr("WIDTH-1", scope="t.") → 32-1=31
  │
  ├─ Look up "t.mem" in scope tables → VerilatedVpioVar
  ├─ Apply indices: withIndex(31) → narrowed handle
  └─ Apply bit range: withPartSelect(15, 8) → narrowed handle with m_partselBits=8
```

### `VerilatedVpioVar` fields relevant to part-select

- `m_bitOffset` — offset in bits from the start of the variable's storage
- `m_partselBits` — if ≥ 0, overrides `bitSize()` and `size()` with the part-select width
- `m_indexedDim` — which dimension has been indexed into (used by `isIndexedDimUnpacked()`)

### UVM HDL unification

The UVM HDL files (`test_regress/t/uvm/v2017_1_0/dpi/uvm_hdl_verilator.c` and
`v2020_3_1/dpi/uvm_hdl_verilator.c`) were simplified during this session.
They previously had their own bracket-parsing code (`uvm_hdl_parse_bitrange`,
`uvm_hdl_handle_by_name_partsel`) which duplicated VPI logic. This was removed;
they now delegate entirely to `vpi_handle_by_name` + `vpi_get_value`/`vpi_put_value`.

All features added to the expression evaluator (arithmetic, identifiers, +:/−:)
automatically benefit UVM HDL access paths as well.

### Test parameters available in `t_vpi_partsel.v`

```verilog
parameter WIDTH = 32;      // PUBLIC_FLAT_RD
parameter BYTE = 8;        // PUBLIC_FLAT_RD
parameter MEM_DEPTH = 4;   // PUBLIC_FLAT_RD
localparam HI_BYTE = 31;   // PUBLIC_FLAT_RD
localparam LO_BYTE = 24;   // PUBLIC_FLAT_RD
```

Signals: `sig_desc[31:0]`, `sig_asc[0:31]`, `mem[0:3]` (each 32-bit),
`wide_sig[63:0]`. See the .v file for initial values.
