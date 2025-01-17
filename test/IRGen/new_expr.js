/**
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// RUN: %hermesc -hermes-parser -dump-ir %s -O0 | %FileCheckOrRegen %s --match-full-lines
// RUN: %hermesc -hermes-parser -dump-ir %s -O

function Car(model, year) {
  this.model = model;
  this.year = year;
  return "wat";
}

function test_simple_new() {
  var ctor = new Car("Eagle", 1993);
}

function test_simple_call() {
  var call = Car(1,2)
}

// Auto-generated content below. Please do not modify manually.

// CHECK:function global(): any
// CHECK-NEXT:frame = []
// CHECK-NEXT:%BB0:
// CHECK-NEXT:       DeclareGlobalVarInst "Car": string
// CHECK-NEXT:       DeclareGlobalVarInst "test_simple_new": string
// CHECK-NEXT:       DeclareGlobalVarInst "test_simple_call": string
// CHECK-NEXT:  %3 = CreateFunctionInst (:object) %Car(): any
// CHECK-NEXT:       StorePropertyLooseInst %3: object, globalObject: object, "Car": string
// CHECK-NEXT:  %5 = CreateFunctionInst (:object) %test_simple_new(): any
// CHECK-NEXT:       StorePropertyLooseInst %5: object, globalObject: object, "test_simple_new": string
// CHECK-NEXT:  %7 = CreateFunctionInst (:object) %test_simple_call(): any
// CHECK-NEXT:       StorePropertyLooseInst %7: object, globalObject: object, "test_simple_call": string
// CHECK-NEXT:  %9 = AllocStackInst (:any) $?anon_0_ret: any
// CHECK-NEXT:        StoreStackInst undefined: undefined, %9: any
// CHECK-NEXT:  %11 = LoadStackInst (:any) %9: any
// CHECK-NEXT:        ReturnInst %11: any
// CHECK-NEXT:function_end

// CHECK:function Car(model: any, year: any): any
// CHECK-NEXT:frame = [model: any, year: any]
// CHECK-NEXT:%BB0:
// CHECK-NEXT:  %0 = LoadParamInst (:any) %<this>: any
// CHECK-NEXT:  %1 = CoerceThisNSInst (:object) %0: any
// CHECK-NEXT:  %2 = LoadParamInst (:any) %model: any
// CHECK-NEXT:       StoreFrameInst %2: any, [model]: any
// CHECK-NEXT:  %4 = LoadParamInst (:any) %year: any
// CHECK-NEXT:       StoreFrameInst %4: any, [year]: any
// CHECK-NEXT:  %6 = LoadFrameInst (:any) [model]: any
// CHECK-NEXT:       StorePropertyLooseInst %6: any, %1: object, "model": string
// CHECK-NEXT:  %8 = LoadFrameInst (:any) [year]: any
// CHECK-NEXT:       StorePropertyLooseInst %8: any, %1: object, "year": string
// CHECK-NEXT:        ReturnInst "wat": string
// CHECK-NEXT:%BB1:
// CHECK-NEXT:        UnreachableInst
// CHECK-NEXT:function_end

// CHECK:function test_simple_new(): any
// CHECK-NEXT:frame = [ctor: any]
// CHECK-NEXT:%BB0:
// CHECK-NEXT:       StoreFrameInst undefined: undefined, [ctor]: any
// CHECK-NEXT:  %1 = LoadPropertyInst (:any) globalObject: object, "Car": string
// CHECK-NEXT:  %2 = LoadPropertyInst (:any) %1: any, "prototype": string
// CHECK-NEXT:  %3 = CreateThisInst (:any) %2: any, %1: any
// CHECK-NEXT:  %4 = CallInst (:any) %1: any, empty: any, empty: any, %1: any, %3: any, "Eagle": string, 1993: number
// CHECK-NEXT:  %5 = GetConstructedObjectInst (:any) %3: any, %4: any
// CHECK-NEXT:       StoreFrameInst %5: any, [ctor]: any
// CHECK-NEXT:       ReturnInst undefined: undefined
// CHECK-NEXT:function_end

// CHECK:function test_simple_call(): any
// CHECK-NEXT:frame = [call: any]
// CHECK-NEXT:%BB0:
// CHECK-NEXT:       StoreFrameInst undefined: undefined, [call]: any
// CHECK-NEXT:  %1 = LoadPropertyInst (:any) globalObject: object, "Car": string
// CHECK-NEXT:  %2 = CallInst (:any) %1: any, empty: any, empty: any, undefined: undefined, undefined: undefined, 1: number, 2: number
// CHECK-NEXT:       StoreFrameInst %2: any, [call]: any
// CHECK-NEXT:       ReturnInst undefined: undefined
// CHECK-NEXT:function_end
