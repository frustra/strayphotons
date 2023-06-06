/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include <tests.hh>

namespace ExampleTests {
    using namespace testing;

    void ExampleTest() {
        AssertTrue(2 == (1 + 1), "This is an example of a test");
        AssertEqual(2, 1 + 1, "This is an example of a test");
    }

    // Register the test
    Test test(&ExampleTest);
} // namespace ExampleTests
