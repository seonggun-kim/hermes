/**
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// RUN: %shermes --test262 -O0 -dump-ir %s | %FileCheckOrRegen --match-full-lines %s
// RUN: %shermes --test262 -custom-opt=simplestackpromotion -dump-ir %s > /dev/null

// Note that we are passing --test262 to both enable TDZ and to delay TDZ errors
// until runtime.

function check1() {
    return x + y;
    let x = 10;
    const y = 1;
}

function check2(p) {
    var b = a;
    let a;
    return a + b;
}

function check3() {
    let x = check3_inner();
    function check3_inner() {
        return x + 1;
    }
    return x;
}

function check4() {
    x = 10;
    let x;
    return x;
}

// Auto-generated content below. Please do not modify manually.

// CHECK:function global(): any
// CHECK-NEXT:frame = []
// CHECK-NEXT:%BB0:
// CHECK-NEXT:       DeclareGlobalVarInst "check1": string
// CHECK-NEXT:       DeclareGlobalVarInst "check2": string
// CHECK-NEXT:       DeclareGlobalVarInst "check3": string
// CHECK-NEXT:       DeclareGlobalVarInst "check4": string
// CHECK-NEXT:  %4 = CreateFunctionInst (:object) %check1(): any
// CHECK-NEXT:       StorePropertyLooseInst %4: object, globalObject: object, "check1": string
// CHECK-NEXT:  %6 = CreateFunctionInst (:object) %check2(): any
// CHECK-NEXT:       StorePropertyLooseInst %6: object, globalObject: object, "check2": string
// CHECK-NEXT:  %8 = CreateFunctionInst (:object) %check3(): any
// CHECK-NEXT:       StorePropertyLooseInst %8: object, globalObject: object, "check3": string
// CHECK-NEXT:  %10 = CreateFunctionInst (:object) %check4(): any
// CHECK-NEXT:        StorePropertyLooseInst %10: object, globalObject: object, "check4": string
// CHECK-NEXT:  %12 = AllocStackInst (:any) $?anon_0_ret: any
// CHECK-NEXT:        StoreStackInst undefined: undefined, %12: any
// CHECK-NEXT:  %14 = LoadStackInst (:any) %12: any
// CHECK-NEXT:        ReturnInst %14: any
// CHECK-NEXT:function_end

// CHECK:function check1(): any
// CHECK-NEXT:frame = [x: any|empty, y: any|empty]
// CHECK-NEXT:%BB0:
// CHECK-NEXT:       StoreFrameInst empty: empty, [x]: any|empty
// CHECK-NEXT:       StoreFrameInst empty: empty, [y]: any|empty
// CHECK-NEXT:  %2 = ThrowIfEmptyInst (:any) empty: empty
// CHECK-NEXT:  %3 = ThrowIfEmptyInst (:any) empty: empty
// CHECK-NEXT:  %4 = BinaryAddInst (:any) %2: any, %3: any
// CHECK-NEXT:       ReturnInst %4: any
// CHECK-NEXT:%BB1:
// CHECK-NEXT:       StoreFrameInst 10: number, [x]: any|empty
// CHECK-NEXT:       StoreFrameInst 1: number, [y]: any|empty
// CHECK-NEXT:       UnreachableInst
// CHECK-NEXT:function_end

// CHECK:function check2(p: any): any
// CHECK-NEXT:frame = [p: any, b: any, a: any|empty]
// CHECK-NEXT:%BB0:
// CHECK-NEXT:  %0 = LoadParamInst (:any) %p: any
// CHECK-NEXT:       StoreFrameInst %0: any, [p]: any
// CHECK-NEXT:       StoreFrameInst undefined: undefined, [b]: any
// CHECK-NEXT:       StoreFrameInst empty: empty, [a]: any|empty
// CHECK-NEXT:  %4 = ThrowIfEmptyInst (:any) empty: empty
// CHECK-NEXT:       StoreFrameInst %4: any, [b]: any
// CHECK-NEXT:       StoreFrameInst undefined: undefined, [a]: any|empty
// CHECK-NEXT:  %7 = LoadFrameInst (:any|empty) [a]: any|empty
// CHECK-NEXT:  %8 = UnionNarrowTrustedInst (:any) %7: any|empty
// CHECK-NEXT:  %9 = LoadFrameInst (:any) [b]: any
// CHECK-NEXT:  %10 = BinaryAddInst (:any) %8: any, %9: any
// CHECK-NEXT:        ReturnInst %10: any
// CHECK-NEXT:%BB1:
// CHECK-NEXT:        UnreachableInst
// CHECK-NEXT:function_end

// CHECK:function check3(): any
// CHECK-NEXT:frame = [x: any|empty, check3_inner: any]
// CHECK-NEXT:%BB0:
// CHECK-NEXT:       StoreFrameInst empty: empty, [x]: any|empty
// CHECK-NEXT:       StoreFrameInst undefined: undefined, [check3_inner]: any
// CHECK-NEXT:  %2 = CreateFunctionInst (:object) %check3_inner(): any
// CHECK-NEXT:       StoreFrameInst %2: object, [check3_inner]: any
// CHECK-NEXT:  %4 = LoadFrameInst (:any) [check3_inner]: any
// CHECK-NEXT:  %5 = CallInst (:any) %4: any, empty: any, empty: any, undefined: undefined, undefined: undefined
// CHECK-NEXT:       StoreFrameInst %5: any, [x]: any|empty
// CHECK-NEXT:  %7 = LoadFrameInst (:any|empty) [x]: any|empty
// CHECK-NEXT:  %8 = UnionNarrowTrustedInst (:any) %7: any|empty
// CHECK-NEXT:       ReturnInst %8: any
// CHECK-NEXT:%BB1:
// CHECK-NEXT:        UnreachableInst
// CHECK-NEXT:function_end

// CHECK:function check4(): any
// CHECK-NEXT:frame = [x: any|empty]
// CHECK-NEXT:%BB0:
// CHECK-NEXT:       StoreFrameInst empty: empty, [x]: any|empty
// CHECK-NEXT:  %1 = ThrowIfEmptyInst (:undefined) empty: empty
// CHECK-NEXT:       StoreFrameInst 10: number, [x]: any|empty
// CHECK-NEXT:       StoreFrameInst undefined: undefined, [x]: any|empty
// CHECK-NEXT:  %4 = LoadFrameInst (:any|empty) [x]: any|empty
// CHECK-NEXT:  %5 = UnionNarrowTrustedInst (:any) %4: any|empty
// CHECK-NEXT:       ReturnInst %5: any
// CHECK-NEXT:%BB1:
// CHECK-NEXT:       UnreachableInst
// CHECK-NEXT:function_end

// CHECK:function check3_inner(): any
// CHECK-NEXT:frame = []
// CHECK-NEXT:%BB0:
// CHECK-NEXT:  %0 = LoadFrameInst (:any|empty) [x@check3]: any|empty
// CHECK-NEXT:  %1 = ThrowIfEmptyInst (:any) %0: any|empty
// CHECK-NEXT:  %2 = BinaryAddInst (:any) %1: any, 1: number
// CHECK-NEXT:       ReturnInst %2: any
// CHECK-NEXT:%BB1:
// CHECK-NEXT:       UnreachableInst
// CHECK-NEXT:function_end
