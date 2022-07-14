/**
 * @file unit_range_compare.cc
 *
 * @section LICENSE
 *
 * The MIT License
 *
 * @copyright Copyright (c) 2022 TileDB, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 * @section DESCRIPTION
 *
 * Tests for comparing ranges
 */

#include <catch.hpp>
#include "tiledb/type/range/range.h"

using namespace tiledb::common;
using namespace tiledb::type;

TEMPLATE_TEST_CASE(
    "Test increase/decrease integer range bounds",
    "[range]",
    int8_t,
    uint8_t,
    int16_t,
    uint16_t,
    int32_t,
    uint32_t,
    int64_t,
    uint64_t) {
  TestType data[2]{1, 10};
  Range range{data, 2 * sizeof(TestType)};
  SECTION("Decrease lower bound") {
    decrease_lower_bound<TestType>(range);
    const auto* result = static_cast<const TestType*>(range.data());
    CHECK(result[0] == 0);
  }
  SECTION("Increase lower bound") {
    increase_lower_bound<TestType>(range);
    const auto* result = static_cast<const TestType*>(range.data());
    CHECK(result[0] == 2);
  }
  SECTION("Decrease upper bound") {
    decrease_upper_bound<TestType>(range);
    const auto* result = static_cast<const TestType*>(range.data());
    CHECK(result[1] == 9);
  }
  SECTION("Increase upper bound") {
    increase_upper_bound<TestType>(range);
    const auto* result = static_cast<const TestType*>(range.data());
    CHECK(result[1] == 11);
  }
}
