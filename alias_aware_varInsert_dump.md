# Alias-aware `varInsert` Discussion Dump

Date: 2026-03-03

## User Prompt

Thanks! Can you dump everything you just said (including my prompt) into a markdown file for later review, please? (See <attachments> above for file contents. You may not need to search or read the file again.)

## Assistant Response (copied)

Progress: plan created and marked done — now the details.

**What "aliasing" means (concise)**
- **Alias:** a pointer/forwarding that makes two variable names refer to the same physical storage. In Verilator: when a variable inside an inlined module is merged into a surviving variable, the compiler may keep an alias so the original name still "points" at the surviving storage.
- **Why it matters:** `varInsert` registers a pointer for the VPI layer. If registration uses the original variable's address but that variable was eliminated or moved during inlining, the registered address is wrong. To support eliminating the original var we must register the alias target (the actual final address), not the eliminated name.

**ELI5 (kid-friendly) explanation + tiny analogy**
- Think of every signal as a mailbox labeled with a name (e.g., "inner.r"). The simulator used to always keep that mailbox as a separate box. Aliasing is like forwarding: when we combine mailboxes to save space, we put a forwarding note on the old mailbox that points to the new mailbox. If the post office (VPI) tries to deliver mail using the old label but we registered the old mailbox location, the mail goes to an empty room. Alias-aware registration means the post office should record the forwarding target address so mail arrives at the real box.

**Short technical demo (pseudocode / small example)**

Verilog (conceptual):
```verilog
module Inner(input logic a, b, output logic y);
  /*verilator public_flat_rw*/ logic tmp; // originally an explicit variable
  always_comb tmp = a & b;
  assign y = tmp;
endmodule

module Top(input logic a, b, output logic y);
  Inner in(.a(a), .b(b), .y(y));
endmodule
```
- Without alias-aware `varInsert`: to keep VPI correctness, the compiler refuses to eliminate `tmp` (keeps separate storage), increasing memory/logic.
- With alias-aware `varInsert`: compiler can remove `tmp` and register VPI pointing at the surviving storage (e.g., `Top::inlined_tmp`), so elimination is safe.

Concrete C++ pseudocode of current problem:
```cpp
// BEFORE inlining: varInsert(&Inner::tmp, "Inner.tmp")
varInsert(addr_of_tmp, "Inner.tmp");

// AFTER inlining + elimination: tmp removed, its expression folded into some other member
// but varInsert still points at the old address -> wrong
```
Alias-aware approach:
```cpp
// Determine alias target: &Top::survivor
varInsert(addr_of_alias_target, "Inner.tmp"); // register the final address
```

**Simple reproducible micro-benchmark (copyable)**
Goal: show impact on eval throughput and code size when many internal signals are forced public vs. when they are allowed to be eliminated.

Files to create:
- `bench.v` — generates many internal signals (that would normally be eliminated)
- `tb.cpp` — a minimal Verilator main that toggles inputs and calls `eval()` in a tight loop and measures runtime.

bench.v:
```verilog
module DUT(input logic clk, input logic in, output logic out);
  // Create a chain of N intermediate signals
  logic [0:0] a0;
  // annotate the ones we want public with the comment form, or run global --public-flat-rw
  /*verilator public_flat_rw*/ logic [0:0] a1;
  // ... replicate many similar statements to create work for optimization
  // final assignment
  assign out = a1 ^ a0; // simple combination
endmodule

module top;
  logic clk = 0;
  logic in = 0;
  logic out;
  DUT dut(.clk(clk), .in(in), .out(out));
  initial begin
    // nothing
  end
endmodule
```

tb.cpp (minimal):
```cpp
#include "Vtop.h"
#include "verilated.h"
#include <chrono>
int main(int argc, char **argv) {
  Verilated::commandArgs(argc, argv);
  Vtop* top = new Vtop;
  const int ITERS = 10'000'000;
  auto t0 = std::chrono::high_resolution_clock::now();
  for (int i=0;i<ITERS;i++) {
    top->in = (i & 1);
    top->eval();
  }
  auto t1 = std::chrono::high_resolution_clock::now();
  double s = std::chrono::duration<double>(t1 - t0).count();
  printf("time: %f s\n", s);
  delete top;
}
```

How to run:
```bash
# Generate C++ model and build
verilator --cc bench.v --exe tb.cpp -O3 --build

# Run and measure
./obj_dir/Vtop   # or ./obj_dir/Vtop + args
# Or use time for more detail:
time ./obj_dir/Vtop
```

Experimental variants:
- Variant A: no `/*verilator public_flat_rw*/` on internal signals (baseline).
- Variant B: add `/*verilator public_flat_rw*/` to many internal signals (or compile with `--public-flat-rw` to make them all public).

Expected observation:
- Variant B runs measurably slower (more cycles per eval), larger binary, longer compile/startup time.
- The relative slowdown increases with the number of public internals and with depth of logic they keep alive.

What to measure:
- Wall-clock time for the `ITERS` loop (as in `tb.cpp`).
- Binary size (`ls -lh obj_dir/Vtop`).
- Optionally `perf stat` for CPU cycles and cache-misses.

Notes:
- Make `bench.v` large enough so elimination would normally remove many temporaries (e.g., 1000 small local nets).
- If you annotate only a few signals, the effect is small; the worst-case is global `--public-flat-rw`.

**Very simple ELI5 explanation of the fix (one-paragraph)**
- Right now Verilator registers public signals using the variable's original storage address. If later optimization merges/eliminates that variable, the registration would point to the wrong place, so the compiler prevents the elimination. The fix is: when a variable is aliased (forwarded) to another variable during inlining, record that mapping and, at VPI registration time (`varInsert` in `V3EmitCSyms`), register the alias target's address instead of the eliminated name's address. That way you get both correct VPI behavior and the freedom to eliminate redundant storage — like making the post office record the forwarding address instead of the old mailbox.

**High-level implementation steps (minimal, non-technical list)**
- While doing inlining, build a small mapping original_var -> final_target_var (and any offset for bit-slices).
- In the code that emits `varInsert`, look up the mapping and use the final address when registering the variable with VPI.
- Fix VPI write propagation so writes via handles properly update through any alias offsets (so that partial-bit writes still land correctly).
- Add tests: a VPI test that writes to an aliased signal and verifies the model sees the write.

If you'd like, I can:
- Produce the concrete `bench.v` and `tb.cpp` files tuned to show a clear timing delta.
- Draft the minimal code-changes (patch) sketch for `V3Inline` → alias mapping and `V3EmitCSyms` → alias-aware `varInsert`.
- Which would you prefer next?
