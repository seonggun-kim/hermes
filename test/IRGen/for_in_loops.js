/**
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// RUN: %hermesc -hermes-parser -dump-ir %s -O0 | %FileCheckOrRegen %s --match-full-lines
// RUN: %hermesc -hermes-parser -dump-ir %s -O

function sink(a) { }

function simple_for_in_loop(obj) {
  var prop = 0;
  for (prop in obj) { sink(prop) }
}

function for_in_loop_with_break_continue(obj) {
  var prop = 0;
  for (prop in obj) { sink(prop); break; continue; }
}

function for_in_loop_with_named_break(obj) {
  var prop = 0;
goto1:
  for (prop in obj) { sink(prop); break goto1; }
}

function check_var_decl_for_in_loop(obj) {
  for (var prop in obj) { sink(prop) }
}

function loop_member_expr_lhs() {
  var x = {};

  for (x.y in [1,2,3]) { sink(x.y); }

  sink(x.y);
}

// Auto-generated content below. Please do not modify manually.

// CHECK:function global(): any
// CHECK-NEXT:frame = []
// CHECK-NEXT:%BB0:
// CHECK-NEXT:       DeclareGlobalVarInst "sink": string
// CHECK-NEXT:       DeclareGlobalVarInst "simple_for_in_loop": string
// CHECK-NEXT:       DeclareGlobalVarInst "for_in_loop_with_break_continue": string
// CHECK-NEXT:       DeclareGlobalVarInst "for_in_loop_with_named_break": string
// CHECK-NEXT:       DeclareGlobalVarInst "check_var_decl_for_in_loop": string
// CHECK-NEXT:       DeclareGlobalVarInst "loop_member_expr_lhs": string
// CHECK-NEXT:  %6 = CreateFunctionInst (:object) %sink(): any
// CHECK-NEXT:       StorePropertyLooseInst %6: object, globalObject: object, "sink": string
// CHECK-NEXT:  %8 = CreateFunctionInst (:object) %simple_for_in_loop(): any
// CHECK-NEXT:       StorePropertyLooseInst %8: object, globalObject: object, "simple_for_in_loop": string
// CHECK-NEXT:  %10 = CreateFunctionInst (:object) %for_in_loop_with_break_continue(): any
// CHECK-NEXT:        StorePropertyLooseInst %10: object, globalObject: object, "for_in_loop_with_break_continue": string
// CHECK-NEXT:  %12 = CreateFunctionInst (:object) %for_in_loop_with_named_break(): any
// CHECK-NEXT:        StorePropertyLooseInst %12: object, globalObject: object, "for_in_loop_with_named_break": string
// CHECK-NEXT:  %14 = CreateFunctionInst (:object) %check_var_decl_for_in_loop(): any
// CHECK-NEXT:        StorePropertyLooseInst %14: object, globalObject: object, "check_var_decl_for_in_loop": string
// CHECK-NEXT:  %16 = CreateFunctionInst (:object) %loop_member_expr_lhs(): any
// CHECK-NEXT:        StorePropertyLooseInst %16: object, globalObject: object, "loop_member_expr_lhs": string
// CHECK-NEXT:  %18 = AllocStackInst (:any) $?anon_0_ret: any
// CHECK-NEXT:        StoreStackInst undefined: undefined, %18: any
// CHECK-NEXT:  %20 = LoadStackInst (:any) %18: any
// CHECK-NEXT:        ReturnInst %20: any
// CHECK-NEXT:function_end

// CHECK:function sink(a: any): any
// CHECK-NEXT:frame = [a: any]
// CHECK-NEXT:%BB0:
// CHECK-NEXT:  %0 = LoadParamInst (:any) %a: any
// CHECK-NEXT:       StoreFrameInst %0: any, [a]: any
// CHECK-NEXT:       ReturnInst undefined: undefined
// CHECK-NEXT:function_end

// CHECK:function simple_for_in_loop(obj: any): any
// CHECK-NEXT:frame = [obj: any, prop: any]
// CHECK-NEXT:%BB0:
// CHECK-NEXT:  %0 = LoadParamInst (:any) %obj: any
// CHECK-NEXT:       StoreFrameInst %0: any, [obj]: any
// CHECK-NEXT:       StoreFrameInst undefined: undefined, [prop]: any
// CHECK-NEXT:       StoreFrameInst 0: number, [prop]: any
// CHECK-NEXT:  %4 = AllocStackInst (:any) $?anon_0_iter: any
// CHECK-NEXT:  %5 = AllocStackInst (:any) $?anon_1_base: any
// CHECK-NEXT:  %6 = AllocStackInst (:number) $?anon_2_idx: any
// CHECK-NEXT:  %7 = AllocStackInst (:number) $?anon_3_size: any
// CHECK-NEXT:  %8 = LoadFrameInst (:any) [obj]: any
// CHECK-NEXT:       StoreStackInst %8: any, %5: any
// CHECK-NEXT:  %10 = AllocStackInst (:any) $?anon_4_prop: any
// CHECK-NEXT:        GetPNamesInst %4: any, %5: any, %6: number, %7: number, %BB1, %BB2
// CHECK-NEXT:%BB1:
// CHECK-NEXT:        ReturnInst undefined: undefined
// CHECK-NEXT:%BB2:
// CHECK-NEXT:        GetNextPNameInst %10: any, %5: any, %6: number, %7: number, %4: any, %BB1, %BB3
// CHECK-NEXT:%BB3:
// CHECK-NEXT:  %14 = LoadStackInst (:any) %10: any
// CHECK-NEXT:        StoreFrameInst %14: any, [prop]: any
// CHECK-NEXT:  %16 = LoadPropertyInst (:any) globalObject: object, "sink": string
// CHECK-NEXT:  %17 = LoadFrameInst (:any) [prop]: any
// CHECK-NEXT:  %18 = CallInst (:any) %16: any, empty: any, empty: any, undefined: undefined, undefined: undefined, %17: any
// CHECK-NEXT:        BranchInst %BB2
// CHECK-NEXT:function_end

// CHECK:function for_in_loop_with_break_continue(obj: any): any
// CHECK-NEXT:frame = [obj: any, prop: any]
// CHECK-NEXT:%BB0:
// CHECK-NEXT:  %0 = LoadParamInst (:any) %obj: any
// CHECK-NEXT:       StoreFrameInst %0: any, [obj]: any
// CHECK-NEXT:       StoreFrameInst undefined: undefined, [prop]: any
// CHECK-NEXT:       StoreFrameInst 0: number, [prop]: any
// CHECK-NEXT:  %4 = AllocStackInst (:any) $?anon_0_iter: any
// CHECK-NEXT:  %5 = AllocStackInst (:any) $?anon_1_base: any
// CHECK-NEXT:  %6 = AllocStackInst (:number) $?anon_2_idx: any
// CHECK-NEXT:  %7 = AllocStackInst (:number) $?anon_3_size: any
// CHECK-NEXT:  %8 = LoadFrameInst (:any) [obj]: any
// CHECK-NEXT:       StoreStackInst %8: any, %5: any
// CHECK-NEXT:  %10 = AllocStackInst (:any) $?anon_4_prop: any
// CHECK-NEXT:        GetPNamesInst %4: any, %5: any, %6: number, %7: number, %BB1, %BB2
// CHECK-NEXT:%BB1:
// CHECK-NEXT:        ReturnInst undefined: undefined
// CHECK-NEXT:%BB2:
// CHECK-NEXT:        GetNextPNameInst %10: any, %5: any, %6: number, %7: number, %4: any, %BB1, %BB3
// CHECK-NEXT:%BB3:
// CHECK-NEXT:  %14 = LoadStackInst (:any) %10: any
// CHECK-NEXT:        StoreFrameInst %14: any, [prop]: any
// CHECK-NEXT:  %16 = LoadPropertyInst (:any) globalObject: object, "sink": string
// CHECK-NEXT:  %17 = LoadFrameInst (:any) [prop]: any
// CHECK-NEXT:  %18 = CallInst (:any) %16: any, empty: any, empty: any, undefined: undefined, undefined: undefined, %17: any
// CHECK-NEXT:        BranchInst %BB1
// CHECK-NEXT:%BB4:
// CHECK-NEXT:        BranchInst %BB2
// CHECK-NEXT:%BB5:
// CHECK-NEXT:        BranchInst %BB2
// CHECK-NEXT:function_end

// CHECK:function for_in_loop_with_named_break(obj: any): any
// CHECK-NEXT:frame = [obj: any, prop: any]
// CHECK-NEXT:%BB0:
// CHECK-NEXT:  %0 = LoadParamInst (:any) %obj: any
// CHECK-NEXT:       StoreFrameInst %0: any, [obj]: any
// CHECK-NEXT:       StoreFrameInst undefined: undefined, [prop]: any
// CHECK-NEXT:       StoreFrameInst 0: number, [prop]: any
// CHECK-NEXT:  %4 = AllocStackInst (:any) $?anon_0_iter: any
// CHECK-NEXT:  %5 = AllocStackInst (:any) $?anon_1_base: any
// CHECK-NEXT:  %6 = AllocStackInst (:number) $?anon_2_idx: any
// CHECK-NEXT:  %7 = AllocStackInst (:number) $?anon_3_size: any
// CHECK-NEXT:  %8 = LoadFrameInst (:any) [obj]: any
// CHECK-NEXT:       StoreStackInst %8: any, %5: any
// CHECK-NEXT:  %10 = AllocStackInst (:any) $?anon_4_prop: any
// CHECK-NEXT:        GetPNamesInst %4: any, %5: any, %6: number, %7: number, %BB1, %BB2
// CHECK-NEXT:%BB3:
// CHECK-NEXT:        ReturnInst undefined: undefined
// CHECK-NEXT:%BB1:
// CHECK-NEXT:        BranchInst %BB3
// CHECK-NEXT:%BB2:
// CHECK-NEXT:        GetNextPNameInst %10: any, %5: any, %6: number, %7: number, %4: any, %BB1, %BB4
// CHECK-NEXT:%BB4:
// CHECK-NEXT:  %15 = LoadStackInst (:any) %10: any
// CHECK-NEXT:        StoreFrameInst %15: any, [prop]: any
// CHECK-NEXT:  %17 = LoadPropertyInst (:any) globalObject: object, "sink": string
// CHECK-NEXT:  %18 = LoadFrameInst (:any) [prop]: any
// CHECK-NEXT:  %19 = CallInst (:any) %17: any, empty: any, empty: any, undefined: undefined, undefined: undefined, %18: any
// CHECK-NEXT:        BranchInst %BB1
// CHECK-NEXT:%BB5:
// CHECK-NEXT:        BranchInst %BB2
// CHECK-NEXT:function_end

// CHECK:function check_var_decl_for_in_loop(obj: any): any
// CHECK-NEXT:frame = [obj: any, prop: any]
// CHECK-NEXT:%BB0:
// CHECK-NEXT:  %0 = LoadParamInst (:any) %obj: any
// CHECK-NEXT:       StoreFrameInst %0: any, [obj]: any
// CHECK-NEXT:       StoreFrameInst undefined: undefined, [prop]: any
// CHECK-NEXT:  %3 = AllocStackInst (:any) $?anon_0_iter: any
// CHECK-NEXT:  %4 = AllocStackInst (:any) $?anon_1_base: any
// CHECK-NEXT:  %5 = AllocStackInst (:number) $?anon_2_idx: any
// CHECK-NEXT:  %6 = AllocStackInst (:number) $?anon_3_size: any
// CHECK-NEXT:  %7 = LoadFrameInst (:any) [obj]: any
// CHECK-NEXT:       StoreStackInst %7: any, %4: any
// CHECK-NEXT:  %9 = AllocStackInst (:any) $?anon_4_prop: any
// CHECK-NEXT:        GetPNamesInst %3: any, %4: any, %5: number, %6: number, %BB1, %BB2
// CHECK-NEXT:%BB1:
// CHECK-NEXT:        ReturnInst undefined: undefined
// CHECK-NEXT:%BB2:
// CHECK-NEXT:        GetNextPNameInst %9: any, %4: any, %5: number, %6: number, %3: any, %BB1, %BB3
// CHECK-NEXT:%BB3:
// CHECK-NEXT:  %13 = LoadStackInst (:any) %9: any
// CHECK-NEXT:        StoreFrameInst %13: any, [prop]: any
// CHECK-NEXT:  %15 = LoadPropertyInst (:any) globalObject: object, "sink": string
// CHECK-NEXT:  %16 = LoadFrameInst (:any) [prop]: any
// CHECK-NEXT:  %17 = CallInst (:any) %15: any, empty: any, empty: any, undefined: undefined, undefined: undefined, %16: any
// CHECK-NEXT:        BranchInst %BB2
// CHECK-NEXT:function_end

// CHECK:function loop_member_expr_lhs(): any
// CHECK-NEXT:frame = [x: any]
// CHECK-NEXT:%BB0:
// CHECK-NEXT:       StoreFrameInst undefined: undefined, [x]: any
// CHECK-NEXT:  %1 = AllocObjectInst (:object) 0: number, empty: any
// CHECK-NEXT:       StoreFrameInst %1: object, [x]: any
// CHECK-NEXT:  %3 = AllocStackInst (:any) $?anon_0_iter: any
// CHECK-NEXT:  %4 = AllocStackInst (:any) $?anon_1_base: any
// CHECK-NEXT:  %5 = AllocStackInst (:number) $?anon_2_idx: any
// CHECK-NEXT:  %6 = AllocStackInst (:number) $?anon_3_size: any
// CHECK-NEXT:  %7 = AllocArrayInst (:object) 3: number, 1: number, 2: number, 3: number
// CHECK-NEXT:       StoreStackInst %7: object, %4: any
// CHECK-NEXT:  %9 = AllocStackInst (:any) $?anon_4_prop: any
// CHECK-NEXT:        GetPNamesInst %3: any, %4: any, %5: number, %6: number, %BB1, %BB2
// CHECK-NEXT:%BB1:
// CHECK-NEXT:  %11 = LoadPropertyInst (:any) globalObject: object, "sink": string
// CHECK-NEXT:  %12 = LoadFrameInst (:any) [x]: any
// CHECK-NEXT:  %13 = LoadPropertyInst (:any) %12: any, "y": string
// CHECK-NEXT:  %14 = CallInst (:any) %11: any, empty: any, empty: any, undefined: undefined, undefined: undefined, %13: any
// CHECK-NEXT:        ReturnInst undefined: undefined
// CHECK-NEXT:%BB2:
// CHECK-NEXT:        GetNextPNameInst %9: any, %4: any, %5: number, %6: number, %3: any, %BB1, %BB3
// CHECK-NEXT:%BB3:
// CHECK-NEXT:  %17 = LoadStackInst (:any) %9: any
// CHECK-NEXT:  %18 = LoadFrameInst (:any) [x]: any
// CHECK-NEXT:        StorePropertyLooseInst %17: any, %18: any, "y": string
// CHECK-NEXT:  %20 = LoadPropertyInst (:any) globalObject: object, "sink": string
// CHECK-NEXT:  %21 = LoadFrameInst (:any) [x]: any
// CHECK-NEXT:  %22 = LoadPropertyInst (:any) %21: any, "y": string
// CHECK-NEXT:  %23 = CallInst (:any) %20: any, empty: any, empty: any, undefined: undefined, undefined: undefined, %22: any
// CHECK-NEXT:        BranchInst %BB2
// CHECK-NEXT:function_end
