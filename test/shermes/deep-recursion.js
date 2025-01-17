/**
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// Make sure to run with a large register stack so we exhaust the native stack first.
// RUN: %shermes -exec %s -Wx,-max-register-stack=1048576 | %FileCheck --match-full-lines %s

function f() {
    f.apply();
}

try {
    f();
    print("recursion not caught")
} catch (e) {
    print("recursion caught", e.name, e.message);
}

// CHECK: recursion caught RangeError Maximum call stack size exceeded (native stack depth)
