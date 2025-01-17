/**
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// RUN: %hermesc -hermes-parser -dump-ir %s -O0 | %FileCheckOrRegen %s --match-full-lines
// RUN: %hermesc -hermes-parser -dump-ir %s -O

function days_of_the_week(day, x) {
  switch (day) {
  default:
    day = "?";
  case 0:
    day = "Sunday";
    break;
  case 1:
    day = "Monday";
    break;
  case x:
    day = "Tuesday";
    break;
  case 3:
    day = "Wednesday";
    break;
  case 4:
    day = "Thursday";
    break;
  case 5:
    day = "Friday";
    break;
  case 6:
    day = "Saturday";
  }
  return day;
}

function simple_xor(b) {
  switch (b) {
  case 1: return 0;
  case 0: return 1;
  }
  return "invalid";
}

function simple_xor2(b) {
  switch (b) {
  case 1: return 0;
  case 0: return 1;
  default: return "invalid"
  }
}

function simple_test0(b) {
  switch (b) {
  case 1+2: return 4+5;
  case 2+3: return 6+7;
  default: return 8+9;
  }
}

function simple_test1(b) {
  switch (b) {
  case 1+2: return 4+5;
  case 2+3: break;
  default: return 8+9;
  }
}

function fallthrough(b) {
  switch (b) {
  case 0: null
  case 1: null
  default: null
  }
}

// Auto-generated content below. Please do not modify manually.

// CHECK:function global(): any
// CHECK-NEXT:frame = []
// CHECK-NEXT:%BB0:
// CHECK-NEXT:       DeclareGlobalVarInst "days_of_the_week": string
// CHECK-NEXT:       DeclareGlobalVarInst "simple_xor": string
// CHECK-NEXT:       DeclareGlobalVarInst "simple_xor2": string
// CHECK-NEXT:       DeclareGlobalVarInst "simple_test0": string
// CHECK-NEXT:       DeclareGlobalVarInst "simple_test1": string
// CHECK-NEXT:       DeclareGlobalVarInst "fallthrough": string
// CHECK-NEXT:  %6 = CreateFunctionInst (:object) %days_of_the_week(): any
// CHECK-NEXT:       StorePropertyLooseInst %6: object, globalObject: object, "days_of_the_week": string
// CHECK-NEXT:  %8 = CreateFunctionInst (:object) %simple_xor(): any
// CHECK-NEXT:       StorePropertyLooseInst %8: object, globalObject: object, "simple_xor": string
// CHECK-NEXT:  %10 = CreateFunctionInst (:object) %simple_xor2(): any
// CHECK-NEXT:        StorePropertyLooseInst %10: object, globalObject: object, "simple_xor2": string
// CHECK-NEXT:  %12 = CreateFunctionInst (:object) %simple_test0(): any
// CHECK-NEXT:        StorePropertyLooseInst %12: object, globalObject: object, "simple_test0": string
// CHECK-NEXT:  %14 = CreateFunctionInst (:object) %simple_test1(): any
// CHECK-NEXT:        StorePropertyLooseInst %14: object, globalObject: object, "simple_test1": string
// CHECK-NEXT:  %16 = CreateFunctionInst (:object) %fallthrough(): any
// CHECK-NEXT:        StorePropertyLooseInst %16: object, globalObject: object, "fallthrough": string
// CHECK-NEXT:  %18 = AllocStackInst (:any) $?anon_0_ret: any
// CHECK-NEXT:        StoreStackInst undefined: undefined, %18: any
// CHECK-NEXT:  %20 = LoadStackInst (:any) %18: any
// CHECK-NEXT:        ReturnInst %20: any
// CHECK-NEXT:function_end

// CHECK:function days_of_the_week(day: any, x: any): any
// CHECK-NEXT:frame = [day: any, x: any]
// CHECK-NEXT:%BB0:
// CHECK-NEXT:  %0 = LoadParamInst (:any) %day: any
// CHECK-NEXT:       StoreFrameInst %0: any, [day]: any
// CHECK-NEXT:  %2 = LoadParamInst (:any) %x: any
// CHECK-NEXT:       StoreFrameInst %2: any, [x]: any
// CHECK-NEXT:  %4 = LoadFrameInst (:any) [day]: any
// CHECK-NEXT:  %5 = BinaryStrictlyEqualInst (:any) 0: number, %4: any
// CHECK-NEXT:       CondBranchInst %5: any, %BB1, %BB2
// CHECK-NEXT:%BB3:
// CHECK-NEXT:  %7 = LoadFrameInst (:any) [day]: any
// CHECK-NEXT:       ReturnInst %7: any
// CHECK-NEXT:%BB4:
// CHECK-NEXT:       StoreFrameInst "?": string, [day]: any
// CHECK-NEXT:        BranchInst %BB1
// CHECK-NEXT:%BB1:
// CHECK-NEXT:        StoreFrameInst "Sunday": string, [day]: any
// CHECK-NEXT:        BranchInst %BB3
// CHECK-NEXT:%BB2:
// CHECK-NEXT:  %13 = BinaryStrictlyEqualInst (:any) 1: number, %4: any
// CHECK-NEXT:        CondBranchInst %13: any, %BB5, %BB6
// CHECK-NEXT:%BB5:
// CHECK-NEXT:        StoreFrameInst "Monday": string, [day]: any
// CHECK-NEXT:        BranchInst %BB3
// CHECK-NEXT:%BB6:
// CHECK-NEXT:  %17 = LoadFrameInst (:any) [x]: any
// CHECK-NEXT:  %18 = BinaryStrictlyEqualInst (:any) %17: any, %4: any
// CHECK-NEXT:        CondBranchInst %18: any, %BB7, %BB8
// CHECK-NEXT:%BB7:
// CHECK-NEXT:        StoreFrameInst "Tuesday": string, [day]: any
// CHECK-NEXT:        BranchInst %BB3
// CHECK-NEXT:%BB8:
// CHECK-NEXT:  %22 = BinaryStrictlyEqualInst (:any) 3: number, %4: any
// CHECK-NEXT:        CondBranchInst %22: any, %BB9, %BB10
// CHECK-NEXT:%BB9:
// CHECK-NEXT:        StoreFrameInst "Wednesday": string, [day]: any
// CHECK-NEXT:        BranchInst %BB3
// CHECK-NEXT:%BB10:
// CHECK-NEXT:  %26 = BinaryStrictlyEqualInst (:any) 4: number, %4: any
// CHECK-NEXT:        CondBranchInst %26: any, %BB11, %BB12
// CHECK-NEXT:%BB11:
// CHECK-NEXT:        StoreFrameInst "Thursday": string, [day]: any
// CHECK-NEXT:        BranchInst %BB3
// CHECK-NEXT:%BB12:
// CHECK-NEXT:  %30 = BinaryStrictlyEqualInst (:any) 5: number, %4: any
// CHECK-NEXT:        CondBranchInst %30: any, %BB13, %BB14
// CHECK-NEXT:%BB13:
// CHECK-NEXT:        StoreFrameInst "Friday": string, [day]: any
// CHECK-NEXT:        BranchInst %BB3
// CHECK-NEXT:%BB14:
// CHECK-NEXT:  %34 = BinaryStrictlyEqualInst (:any) 6: number, %4: any
// CHECK-NEXT:        CondBranchInst %34: any, %BB15, %BB16
// CHECK-NEXT:%BB15:
// CHECK-NEXT:        StoreFrameInst "Saturday": string, [day]: any
// CHECK-NEXT:        BranchInst %BB3
// CHECK-NEXT:%BB16:
// CHECK-NEXT:        BranchInst %BB4
// CHECK-NEXT:%BB17:
// CHECK-NEXT:        BranchInst %BB5
// CHECK-NEXT:%BB18:
// CHECK-NEXT:        BranchInst %BB7
// CHECK-NEXT:%BB19:
// CHECK-NEXT:        BranchInst %BB9
// CHECK-NEXT:%BB20:
// CHECK-NEXT:        BranchInst %BB11
// CHECK-NEXT:%BB21:
// CHECK-NEXT:        BranchInst %BB13
// CHECK-NEXT:%BB22:
// CHECK-NEXT:        BranchInst %BB15
// CHECK-NEXT:%BB23:
// CHECK-NEXT:        UnreachableInst
// CHECK-NEXT:function_end

// CHECK:function simple_xor(b: any): any
// CHECK-NEXT:frame = [b: any]
// CHECK-NEXT:%BB0:
// CHECK-NEXT:  %0 = LoadParamInst (:any) %b: any
// CHECK-NEXT:       StoreFrameInst %0: any, [b]: any
// CHECK-NEXT:  %2 = LoadFrameInst (:any) [b]: any
// CHECK-NEXT:       SwitchInst %2: any, %BB1, 1: number, %BB2, 0: number, %BB3
// CHECK-NEXT:%BB1:
// CHECK-NEXT:       ReturnInst "invalid": string
// CHECK-NEXT:%BB2:
// CHECK-NEXT:       ReturnInst 0: number
// CHECK-NEXT:%BB4:
// CHECK-NEXT:       BranchInst %BB3
// CHECK-NEXT:%BB3:
// CHECK-NEXT:       ReturnInst 1: number
// CHECK-NEXT:%BB5:
// CHECK-NEXT:       BranchInst %BB1
// CHECK-NEXT:%BB6:
// CHECK-NEXT:       UnreachableInst
// CHECK-NEXT:function_end

// CHECK:function simple_xor2(b: any): any
// CHECK-NEXT:frame = [b: any]
// CHECK-NEXT:%BB0:
// CHECK-NEXT:  %0 = LoadParamInst (:any) %b: any
// CHECK-NEXT:       StoreFrameInst %0: any, [b]: any
// CHECK-NEXT:  %2 = LoadFrameInst (:any) [b]: any
// CHECK-NEXT:       SwitchInst %2: any, %BB1, 1: number, %BB2, 0: number, %BB3
// CHECK-NEXT:%BB4:
// CHECK-NEXT:       UnreachableInst
// CHECK-NEXT:%BB2:
// CHECK-NEXT:       ReturnInst 0: number
// CHECK-NEXT:%BB5:
// CHECK-NEXT:       BranchInst %BB3
// CHECK-NEXT:%BB3:
// CHECK-NEXT:       ReturnInst 1: number
// CHECK-NEXT:%BB6:
// CHECK-NEXT:       BranchInst %BB1
// CHECK-NEXT:%BB1:
// CHECK-NEXT:       ReturnInst "invalid": string
// CHECK-NEXT:%BB7:
// CHECK-NEXT:        BranchInst %BB4
// CHECK-NEXT:function_end

// CHECK:function simple_test0(b: any): any
// CHECK-NEXT:frame = [b: any]
// CHECK-NEXT:%BB0:
// CHECK-NEXT:  %0 = LoadParamInst (:any) %b: any
// CHECK-NEXT:       StoreFrameInst %0: any, [b]: any
// CHECK-NEXT:  %2 = LoadFrameInst (:any) [b]: any
// CHECK-NEXT:  %3 = BinaryAddInst (:any) 1: number, 2: number
// CHECK-NEXT:  %4 = BinaryStrictlyEqualInst (:any) %3: any, %2: any
// CHECK-NEXT:       CondBranchInst %4: any, %BB1, %BB2
// CHECK-NEXT:%BB3:
// CHECK-NEXT:       UnreachableInst
// CHECK-NEXT:%BB1:
// CHECK-NEXT:  %7 = BinaryAddInst (:any) 4: number, 5: number
// CHECK-NEXT:       ReturnInst %7: any
// CHECK-NEXT:%BB2:
// CHECK-NEXT:  %9 = BinaryAddInst (:any) 2: number, 3: number
// CHECK-NEXT:  %10 = BinaryStrictlyEqualInst (:any) %9: any, %2: any
// CHECK-NEXT:        CondBranchInst %10: any, %BB4, %BB5
// CHECK-NEXT:%BB4:
// CHECK-NEXT:  %12 = BinaryAddInst (:any) 6: number, 7: number
// CHECK-NEXT:        ReturnInst %12: any
// CHECK-NEXT:%BB5:
// CHECK-NEXT:        BranchInst %BB6
// CHECK-NEXT:%BB6:
// CHECK-NEXT:  %15 = BinaryAddInst (:any) 8: number, 9: number
// CHECK-NEXT:        ReturnInst %15: any
// CHECK-NEXT:%BB7:
// CHECK-NEXT:        BranchInst %BB4
// CHECK-NEXT:%BB8:
// CHECK-NEXT:        BranchInst %BB6
// CHECK-NEXT:%BB9:
// CHECK-NEXT:        BranchInst %BB3
// CHECK-NEXT:function_end

// CHECK:function simple_test1(b: any): any
// CHECK-NEXT:frame = [b: any]
// CHECK-NEXT:%BB0:
// CHECK-NEXT:  %0 = LoadParamInst (:any) %b: any
// CHECK-NEXT:       StoreFrameInst %0: any, [b]: any
// CHECK-NEXT:  %2 = LoadFrameInst (:any) [b]: any
// CHECK-NEXT:  %3 = BinaryAddInst (:any) 1: number, 2: number
// CHECK-NEXT:  %4 = BinaryStrictlyEqualInst (:any) %3: any, %2: any
// CHECK-NEXT:       CondBranchInst %4: any, %BB1, %BB2
// CHECK-NEXT:%BB3:
// CHECK-NEXT:       ReturnInst undefined: undefined
// CHECK-NEXT:%BB1:
// CHECK-NEXT:  %7 = BinaryAddInst (:any) 4: number, 5: number
// CHECK-NEXT:       ReturnInst %7: any
// CHECK-NEXT:%BB2:
// CHECK-NEXT:  %9 = BinaryAddInst (:any) 2: number, 3: number
// CHECK-NEXT:  %10 = BinaryStrictlyEqualInst (:any) %9: any, %2: any
// CHECK-NEXT:        CondBranchInst %10: any, %BB4, %BB5
// CHECK-NEXT:%BB4:
// CHECK-NEXT:        BranchInst %BB3
// CHECK-NEXT:%BB5:
// CHECK-NEXT:        BranchInst %BB6
// CHECK-NEXT:%BB6:
// CHECK-NEXT:  %14 = BinaryAddInst (:any) 8: number, 9: number
// CHECK-NEXT:        ReturnInst %14: any
// CHECK-NEXT:%BB7:
// CHECK-NEXT:        BranchInst %BB4
// CHECK-NEXT:%BB8:
// CHECK-NEXT:        BranchInst %BB6
// CHECK-NEXT:%BB9:
// CHECK-NEXT:        BranchInst %BB3
// CHECK-NEXT:function_end

// CHECK:function fallthrough(b: any): any
// CHECK-NEXT:frame = [b: any]
// CHECK-NEXT:%BB0:
// CHECK-NEXT:  %0 = LoadParamInst (:any) %b: any
// CHECK-NEXT:       StoreFrameInst %0: any, [b]: any
// CHECK-NEXT:  %2 = LoadFrameInst (:any) [b]: any
// CHECK-NEXT:       SwitchInst %2: any, %BB1, 0: number, %BB2, 1: number, %BB3
// CHECK-NEXT:%BB4:
// CHECK-NEXT:       ReturnInst undefined: undefined
// CHECK-NEXT:%BB2:
// CHECK-NEXT:       BranchInst %BB3
// CHECK-NEXT:%BB3:
// CHECK-NEXT:       BranchInst %BB1
// CHECK-NEXT:%BB1:
// CHECK-NEXT:       BranchInst %BB4
// CHECK-NEXT:function_end
