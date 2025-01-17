/**
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// RUN: %hermesc -O0 -dump-ir %s | %FileCheckOrRegen --match-full-lines %s

var func1 = () => 10;

var func2 = () => { return 11; }

// Auto-generated content below. Please do not modify manually.

// CHECK:function global(): any
// CHECK-NEXT:frame = [?anon_0_this: any, ?anon_1_new.target: undefined|object]
// CHECK-NEXT:%BB0:
// CHECK-NEXT:  %0 = LoadParamInst (:any) %<this>: any
// CHECK-NEXT:  %1 = CoerceThisNSInst (:object) %0: any
// CHECK-NEXT:       StoreFrameInst %1: object, [?anon_0_this]: any
// CHECK-NEXT:  %3 = GetNewTargetInst (:undefined|object) %new.target: undefined|object
// CHECK-NEXT:       StoreFrameInst %3: undefined|object, [?anon_1_new.target]: undefined|object
// CHECK-NEXT:       DeclareGlobalVarInst "func1": string
// CHECK-NEXT:       DeclareGlobalVarInst "func2": string
// CHECK-NEXT:  %7 = AllocStackInst (:any) $?anon_2_ret: any
// CHECK-NEXT:       StoreStackInst undefined: undefined, %7: any
// CHECK-NEXT:  %9 = CreateFunctionInst (:object) %func1(): any
// CHECK-NEXT:        StorePropertyLooseInst %9: object, globalObject: object, "func1": string
// CHECK-NEXT:  %11 = CreateFunctionInst (:object) %func2(): any
// CHECK-NEXT:        StorePropertyLooseInst %11: object, globalObject: object, "func2": string
// CHECK-NEXT:  %13 = LoadStackInst (:any) %7: any
// CHECK-NEXT:        ReturnInst %13: any
// CHECK-NEXT:function_end

// CHECK:arrow func1(): any
// CHECK-NEXT:frame = []
// CHECK-NEXT:%BB0:
// CHECK-NEXT:       ReturnInst 10: number
// CHECK-NEXT:%BB1:
// CHECK-NEXT:       ReturnInst undefined: undefined
// CHECK-NEXT:function_end

// CHECK:arrow func2(): any
// CHECK-NEXT:frame = []
// CHECK-NEXT:%BB0:
// CHECK-NEXT:       ReturnInst 11: number
// CHECK-NEXT:%BB1:
// CHECK-NEXT:       ReturnInst undefined: undefined
// CHECK-NEXT:function_end
