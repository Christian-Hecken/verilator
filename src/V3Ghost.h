// -*- mode: C++; c-file-style: "cc-mode" -*-
//*************************************************************************
// DESCRIPTION: Verilator: Ghost variable analysis
//
// Mark public signals as "ghosts" when they can be safely optimized away
// and lazily reconstructed on external access (VPI / direct C++).
//
// Code available from: https://verilator.org
//
//*************************************************************************
//
// This program is free software; you can redistribute it and/or modify it
// under the terms of either the GNU Lesser General Public License Version 3
// or the Perl Artistic License Version 2.0.
// SPDX-FileCopyrightText: 2003-2026 Wilson Snyder
// SPDX-License-Identifier: LGPL-3.0-only OR Artistic-2.0
//
//*************************************************************************

#ifndef VERILATOR_V3GHOST_H_
#define VERILATOR_V3GHOST_H_

#include "config_build.h"
#include "verilatedos.h"

class AstNetlist;

//============================================================================

class V3Ghost final {
public:
    /// Analyze the netlist and mark ghost-eligible public signals
    static void ghostAll(AstNetlist* nodep) VL_MT_DISABLED;
};

#endif  // Guard
