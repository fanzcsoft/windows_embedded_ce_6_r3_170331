//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of the Microsoft shared
// source or premium shared source license agreement under which you licensed
// this source code. If you did not accept the terms of the license agreement,
// you are not authorized to use this source code. For the terms of the license,
// please see the license agreement between you and Microsoft or, if applicable,
// see the SOURCE.RTF on your install media or the root of your tools installation.
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES.
//
//------------------------------------------------------------------------------
//      TITLE("Interlock memory operations")
//------------------------------------------------------------------------------
//
//
// Module Name:
//
//    intrlock.s
//
// Abstract:
//
//    This module implements the InterlockedIncrement, I...Decrement,
// I...Exchange and I...TestExchange APIs.
//
//    WARNING: This module makes special use of register K1 to inform the
//  GeneralException handler in except.s that an interlocked operation
//  is being performed. Because the exception handler code has detailed
//  knowledge of this code, extreme care must be exercised when modifying
//  this module. For example, the store instruction must be followed
//  immediately by a "j ra" instruction.
//
// Environment:
//
//    Kernel mode or User mode.
//
//------------------------------------------------------------------------------
#include "ksmips.h"

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
LEAF_ENTRY(InterlockedExchange)

#if NO_LL
        la      t0, IExRestart
        .set    noreorder
IExRestart:
        addiu   k1, t0, 4 * 4           // (k1) = &IExDone, indicate interlocked
                                        //        operation in progress
        lw      v0, (a0)                // (v0) = original contents
        nop                             
        sw      a1, (a0)                // store new contents
IExDone:        
        j       ra                      
        move    k1, zero                // interlocked operation complete
#else                                   
        .set    noreorder               
        move    t0, a1                  
IExRestart:                             
        ll      v0, (a0)                // (v0) = original contents
        sc      a1, (a0)                // store new contents
        beq     a1,zero,IExRestart      // check if sc was successful
        move    a1, t0
        j       ra
        nop
#endif
        .end   InterlockedExchange


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
LEAF_ENTRY(InterlockedCompareExchange)

#if NO_LL
        la      t0, ICExRestart
        .set    noreorder
ICExRestart:
        addiu   k1, t0, 5 * 4           // (k1) = &ICExDone, indicate interlocked
                                        //        operation in progress
        lw      v0, (a0)                // (v0) = original contents
        bne     v0, a2, 10f             
        nop                             
        sw      a1, (a0)                // store new contents
ICExDone:        
10:     j       ra                      
        move    k1, zero                // interlocked operation complete
#else                                   
        move    t0, a1                  // (t0) = original Arg1 value
        .set    noreorder               
ICExRestart:                            
        ll      v0, (a0)                // (v0) = original contents
        bne     v0, a2, 10f             
        nop                             
        sc      a1, (a0)                // store new contents
        beq     a1,zero,ICExRestart     // check if sc was successful
        move    a1, t0                  // restore Arg1 value in case of restart
10:     j       ra
        nop
#endif
        .end    InterlockedCompareExchange


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
LEAF_ENTRY(InterlockedTestExchange)

#if NO_LL
        la      t0, ITExRestart
        .set    noreorder
ITExRestart:
        addiu   k1, t0, 6 * 4           // (k1) = &ITExDone, indicate interlocked
                                        //        operation in progress
        lw      v0, (a0)                // (v0) = original contents
        nop                             
        bne     v0, a1, 10f             
        nop                             
        sw      a2, (a0)                // store new contents
ITExDone:        
10:     j       ra                      
        move    k1, zero                // interlocked operation complete
#else                                   
        move    t0, a2                  
        .set    noreorder               
ITExRestart:                            
        ll      v0, (a0)                // (v0) = original contents
        bne     v0, a1, 10f             
        nop                             
        sc      a2, (a0)                // store new contents
        beq     a2,zero,ITExRestart     // check if sc was successful
        move    a2, t0
10:     j       ra
        nop
#endif
        .end    InterlockedTestExchange



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
LEAF_ENTRY(InterlockedIncrement)

#if NO_LL
        la      t0, IIncRestart
        .set    noreorder
IIncRestart:
        addiu   k1, t0, 5 * 4           // (k1) = &IIncDone, indicate interlocked
                                        //        operation in progress
        lw      v0, (a0)                // (v0) = original contents
        nop                             
        addu    v0, 1        
        sw      v0, (a0)                // store new contents
IIncDone:
        j       ra                      
        move    k1, zero                // interlocked operation complete
#else
        .set    noreorder
IIncRestart:
        ll      v0, (a0)                // (v0) = original contents
        addu    v0, 1
        move    t0, v0
        sc      v0, (a0)                // store new contents
        beq     v0,zero,IIncRestart     // check if sc was successful
        move    v0, t0
        j       ra
        nop
#endif
        .end    InterlockedIncrement



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
LEAF_ENTRY(InterlockedDecrement)

#if NO_LL
        la      t0, IDecRestart
        .set    noreorder
IDecRestart:
        addiu   k1, t0, 5 * 4           // (k1) = &IDeccDone, indicate interlocked
                                        //        operation in progress
        lw      v0, (a0)                // (v0) = original contents
        nop                             
        subu    v0, 1                   
        sw      v0, (a0)                // store new contents
IDecDone:
        j       ra                      
        move    k1, zero                // interlocked operation complete
#else
        .set    noreorder
IDecRestart:
        ll      v0, (a0)                // (v0) = original contents
        subu    v0, 1
        move    t0, v0
        sc      v0, (a0)                // store new contents
        beq     v0,zero,IDecRestart     // check if sc was successful
        move    v0, t0
        j       ra
        nop
#endif
        .end    InterlockedDecrement



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
LEAF_ENTRY(InterlockedExchangeAdd)

#if NO_LL
        la      t0, IXAddRestart
        .set    noreorder
IXAddRestart:
        addiu   k1, t0, 4 * 4           // (k1) = &IXAddDone, indicate interlocked
                                        //        operation in progress
        lw      v0, (a0)                // (v0) = original contents
        addu    t1, v0, a1              // (t1) = incremented value
        sw      t1, (a0)                // store new contents
IXAddDone:
        j       ra                      
        move    k1, zero                // interlocked operation complete
#else
        .set    noreorder
IXAddRestart:
        ll      v0, (a0)                // (v0) = original contents
        addu    t0, v0, a1              // (t0) = incremented value
        sc      t0, (a0)                // store new contents
        beq     t0,zero,IXAddRestart    // check if sc was successful
        nop
        j       ra
        nop
#endif
        .end    InterlockedExchangeAdd

