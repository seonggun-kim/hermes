/**
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// RUN: %shermes -typed -dump-ir %s | %FileCheckOrRegen --match-full-lines %s

'use strict';

class C {
  method(): number {
    return 1;
  }
}

print(new C().method());

// Auto-generated content below. Please do not modify manually.

// CHECK:function global(): undefined
// CHECK-NEXT:frame = []
// CHECK-NEXT:%BB0:
// CHECK-NEXT:  %0 = CreateFunctionInst (:object) %""(): undefined
// CHECK-NEXT:  %1 = CallInst [njsf] (:undefined) %0: object, %""(): undefined, empty: any, undefined: undefined, 0: number, 0: number
// CHECK-NEXT:       ReturnInst undefined: undefined
// CHECK-NEXT:function_end

// CHECK:function ""(exports: number): undefined [allCallsitesKnownInStrictMode]
// CHECK-NEXT:frame = []
// CHECK-NEXT:%BB0:
// CHECK-NEXT:  %0 = CreateFunctionInst (:object) %C(): undefined
// CHECK-NEXT:  %1 = CreateFunctionInst (:object) %method(): number
// CHECK-NEXT:  %2 = AllocObjectLiteralInst (:object) "method": string, %1: object
// CHECK-NEXT:       StorePropertyStrictInst %2: object, %0: object, "prototype": string
// CHECK-NEXT:  %4 = TryLoadGlobalPropertyInst (:any) globalObject: object, "print": string
// CHECK-NEXT:  %5 = LoadPropertyInst (:any) %0: object, "prototype": string
// CHECK-NEXT:  %6 = AllocObjectInst (:object) 0: number, %5: any
// CHECK-NEXT:  %7 = LoadParentInst (:object) %6: object
// CHECK-NEXT:  %8 = PrLoadInst (:object) %7: object, 0: number, "method": string
// CHECK-NEXT:  %9 = CallInst [njsf] (:any) %8: object, empty: any, empty: any, undefined: undefined, %6: object
// CHECK-NEXT:  %10 = CallInst (:any) %4: any, empty: any, empty: any, undefined: undefined, undefined: undefined, %9: any
// CHECK-NEXT:        ReturnInst undefined: undefined
// CHECK-NEXT:function_end

// CHECK:function C(): undefined
// CHECK-NEXT:frame = []
// CHECK-NEXT:%BB0:
// CHECK-NEXT:       ReturnInst undefined: undefined
// CHECK-NEXT:function_end

// CHECK:function method(): number [typed]
// CHECK-NEXT:frame = []
// CHECK-NEXT:%BB0:
// CHECK-NEXT:       ReturnInst 1: number
// CHECK-NEXT:function_end
