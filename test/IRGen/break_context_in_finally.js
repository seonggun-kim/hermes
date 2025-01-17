/**
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// RUN: %hermesc -O0 -dump-ir %s | %FileCheckOrRegen %s --match-full-lines

// Ensure that the "break" in the finally handler exits the correct
// loop.

function foo() {
    outer:
    for (;;) {
        try {
            for (;;) {
                bar2();
                break outer;
            }
            bar3();
        } finally {
            finally1();
            break;
        }
    }
}

// Auto-generated content below. Please do not modify manually.

// CHECK:function global(): any
// CHECK-NEXT:frame = []
// CHECK-NEXT:%BB0:
// CHECK-NEXT:       DeclareGlobalVarInst "foo": string
// CHECK-NEXT:  %1 = CreateFunctionInst (:object) %foo(): any
// CHECK-NEXT:       StorePropertyLooseInst %1: object, globalObject: object, "foo": string
// CHECK-NEXT:  %3 = AllocStackInst (:any) $?anon_0_ret: any
// CHECK-NEXT:       StoreStackInst undefined: undefined, %3: any
// CHECK-NEXT:  %5 = LoadStackInst (:any) %3: any
// CHECK-NEXT:       ReturnInst %5: any
// CHECK-NEXT:function_end

// CHECK:function foo(): any
// CHECK-NEXT:frame = []
// CHECK-NEXT:%BB0:
// CHECK-NEXT:       BranchInst %BB1
// CHECK-NEXT:%BB2:
// CHECK-NEXT:       ReturnInst undefined: undefined
// CHECK-NEXT:%BB1:
// CHECK-NEXT:       TryStartInst %BB3, %BB4
// CHECK-NEXT:%BB5:
// CHECK-NEXT:       BranchInst %BB2
// CHECK-NEXT:%BB6:
// CHECK-NEXT:       BranchInst %BB1
// CHECK-NEXT:%BB7:
// CHECK-NEXT:       BranchInst %BB6
// CHECK-NEXT:%BB3:
// CHECK-NEXT:  %6 = CatchInst (:any)
// CHECK-NEXT:  %7 = TryLoadGlobalPropertyInst (:any) globalObject: object, "finally1": string
// CHECK-NEXT:  %8 = CallInst (:any) %7: any, empty: any, empty: any, undefined: undefined, undefined: undefined
// CHECK-NEXT:       BranchInst %BB5
// CHECK-NEXT:%BB8:
// CHECK-NEXT:        BranchInst %BB7
// CHECK-NEXT:%BB4:
// CHECK-NEXT:        BranchInst %BB9
// CHECK-NEXT:%BB9:
// CHECK-NEXT:  %12 = TryLoadGlobalPropertyInst (:any) globalObject: object, "bar2": string
// CHECK-NEXT:  %13 = CallInst (:any) %12: any, empty: any, empty: any, undefined: undefined, undefined: undefined
// CHECK-NEXT:        BranchInst %BB10
// CHECK-NEXT:%BB11:
// CHECK-NEXT:  %15 = TryLoadGlobalPropertyInst (:any) globalObject: object, "bar3": string
// CHECK-NEXT:  %16 = CallInst (:any) %15: any, empty: any, empty: any, undefined: undefined, undefined: undefined
// CHECK-NEXT:        BranchInst %BB12
// CHECK-NEXT:%BB13:
// CHECK-NEXT:        BranchInst %BB9
// CHECK-NEXT:%BB14:
// CHECK-NEXT:        BranchInst %BB13
// CHECK-NEXT:%BB10:
// CHECK-NEXT:        TryEndInst
// CHECK-NEXT:  %21 = TryLoadGlobalPropertyInst (:any) globalObject: object, "finally1": string
// CHECK-NEXT:  %22 = CallInst (:any) %21: any, empty: any, empty: any, undefined: undefined, undefined: undefined
// CHECK-NEXT:        BranchInst %BB5
// CHECK-NEXT:%BB15:
// CHECK-NEXT:        BranchInst %BB5
// CHECK-NEXT:%BB16:
// CHECK-NEXT:        BranchInst %BB14
// CHECK-NEXT:%BB12:
// CHECK-NEXT:        TryEndInst
// CHECK-NEXT:  %27 = TryLoadGlobalPropertyInst (:any) globalObject: object, "finally1": string
// CHECK-NEXT:  %28 = CallInst (:any) %27: any, empty: any, empty: any, undefined: undefined, undefined: undefined
// CHECK-NEXT:        BranchInst %BB5
// CHECK-NEXT:%BB17:
// CHECK-NEXT:        BranchInst %BB8
// CHECK-NEXT:%BB18:
// CHECK-NEXT:        ThrowInst %6: any
// CHECK-NEXT:function_end
