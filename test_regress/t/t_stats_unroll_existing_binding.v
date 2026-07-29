// DESCRIPTION: Verilator: Test statistics for unroll const binding (existing binding)
// SPDX-License-Identifier: CC0-1.0

module t (/*AUTOARG*/);
   // Variable with binding created before loop
   logic [31:0] bound_var /*verilator public_flat_rw*/;
   
   initial begin
      // Create binding outside loop
      bound_var = 32'h1234;
      
      // Nested loop tries to create another binding for same variable
      // The existing-binding check prevents both performed and blocked counters
      for (int i = 0; i < 2; i++) begin
         for (int j = 0; j < 2; j++) begin
            bound_var = 32'h5678;
         end
      end
      
      $write("*-* All Finished *-*\n");
      $finish;
   end
endmodule
