/**
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// RUN: %hermesc -O0 -dump-ir %s | %FileCheckOrRegen --match-full-lines %s

function foo() {
    return eval("1 + 1");
}

function bar() {
    return eval("2 + 2", Math, foo());
}

function baz() {
    return eval();
}

// Auto-generated content below. Please do not modify manually.

// CHECK:function global(): any
// CHECK-NEXT:frame = []
// CHECK-NEXT:%BB0:
// CHECK-NEXT:       DeclareGlobalVarInst "foo": string
// CHECK-NEXT:       DeclareGlobalVarInst "bar": string
// CHECK-NEXT:       DeclareGlobalVarInst "baz": string
// CHECK-NEXT:  %3 = CreateFunctionInst (:object) %foo(): any
// CHECK-NEXT:       StorePropertyLooseInst %3: object, globalObject: object, "foo": string
// CHECK-NEXT:  %5 = CreateFunctionInst (:object) %bar(): any
// CHECK-NEXT:       StorePropertyLooseInst %5: object, globalObject: object, "bar": string
// CHECK-NEXT:  %7 = CreateFunctionInst (:object) %baz(): any
// CHECK-NEXT:       StorePropertyLooseInst %7: object, globalObject: object, "baz": string
// CHECK-NEXT:  %9 = AllocStackInst (:any) $?anon_0_ret: any
// CHECK-NEXT:        StoreStackInst undefined: undefined, %9: any
// CHECK-NEXT:  %11 = LoadStackInst (:any) %9: any
// CHECK-NEXT:        ReturnInst %11: any
// CHECK-NEXT:function_end

// CHECK:function foo(): any
// CHECK-NEXT:frame = []
// CHECK-NEXT:%BB0:
// CHECK-NEXT:  %0 = TryLoadGlobalPropertyInst (:any) globalObject: object, "eval": string
// CHECK-NEXT:  %1 = GetBuiltinClosureInst (:object) [globalThis.eval]: number
// CHECK-NEXT:       CmpBrStrictlyEqualInst %0: any, %1: object, %BB1, %BB2
// CHECK-NEXT:%BB1:
// CHECK-NEXT:  %3 = DirectEvalInst (:any) "1 + 1": string, false: boolean
// CHECK-NEXT:       BranchInst %BB3
// CHECK-NEXT:%BB2:
// CHECK-NEXT:  %5 = CallInst (:any) %0: any, empty: any, empty: any, undefined: undefined, undefined: undefined, "1 + 1": string
// CHECK-NEXT:       BranchInst %BB3
// CHECK-NEXT:%BB3:
// CHECK-NEXT:  %7 = PhiInst (:any) %3: any, %BB1, %5: any, %BB2
// CHECK-NEXT:       ReturnInst %7: any
// CHECK-NEXT:%BB4:
// CHECK-NEXT:       UnreachableInst
// CHECK-NEXT:function_end

// CHECK:function bar(): any
// CHECK-NEXT:frame = []
// CHECK-NEXT:%BB0:
// CHECK-NEXT:  %0 = TryLoadGlobalPropertyInst (:any) globalObject: object, "eval": string
// CHECK-NEXT:  %1 = TryLoadGlobalPropertyInst (:any) globalObject: object, "Math": string
// CHECK-NEXT:  %2 = LoadPropertyInst (:any) globalObject: object, "foo": string
// CHECK-NEXT:  %3 = CallInst (:any) %2: any, empty: any, empty: any, undefined: undefined, undefined: undefined
// CHECK-NEXT:  %4 = GetBuiltinClosureInst (:object) [globalThis.eval]: number
// CHECK-NEXT:       CmpBrStrictlyEqualInst %0: any, %4: object, %BB1, %BB2
// CHECK-NEXT:%BB1:
// CHECK-NEXT:  %6 = DirectEvalInst (:any) "2 + 2": string, false: boolean
// CHECK-NEXT:       BranchInst %BB3
// CHECK-NEXT:%BB2:
// CHECK-NEXT:  %8 = CallInst (:any) %0: any, empty: any, empty: any, undefined: undefined, undefined: undefined, "2 + 2": string, %1: any, %3: any
// CHECK-NEXT:       BranchInst %BB3
// CHECK-NEXT:%BB3:
// CHECK-NEXT:  %10 = PhiInst (:any) %6: any, %BB1, %8: any, %BB2
// CHECK-NEXT:        ReturnInst %10: any
// CHECK-NEXT:%BB4:
// CHECK-NEXT:        UnreachableInst
// CHECK-NEXT:function_end

// CHECK:function baz(): any
// CHECK-NEXT:frame = []
// CHECK-NEXT:%BB0:
// CHECK-NEXT:  %0 = TryLoadGlobalPropertyInst (:any) globalObject: object, "eval": string
// CHECK-NEXT:  %1 = GetBuiltinClosureInst (:object) [globalThis.eval]: number
// CHECK-NEXT:       CmpBrStrictlyEqualInst %0: any, %1: object, %BB1, %BB2
// CHECK-NEXT:%BB1:
// CHECK-NEXT:  %3 = DirectEvalInst (:any) undefined: undefined, false: boolean
// CHECK-NEXT:       BranchInst %BB3
// CHECK-NEXT:%BB2:
// CHECK-NEXT:  %5 = CallInst (:any) %0: any, empty: any, empty: any, undefined: undefined, undefined: undefined
// CHECK-NEXT:       BranchInst %BB3
// CHECK-NEXT:%BB3:
// CHECK-NEXT:  %7 = PhiInst (:any) %3: any, %BB1, %5: any, %BB2
// CHECK-NEXT:       ReturnInst %7: any
// CHECK-NEXT:%BB4:
// CHECK-NEXT:       UnreachableInst
// CHECK-NEXT:function_end
