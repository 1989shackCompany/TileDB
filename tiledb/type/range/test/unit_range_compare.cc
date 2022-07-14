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
    "Test range lower bound comparisons",
    "[range]",
    int8_t,
    uint8_t,
    int16_t,
    uint16_t,
    int32_t,
    uint32_t,
    int64_t,
    uint64_t,
    float,
    double) {
  SECTION("Lower bound 1 is less than") {
    TestType data1[2]{1, 10};
    TestType data2[2]{2, 10};
    Range r1{data1, 2 * sizeof(TestType)};
    Range r2{data2, 2 * sizeof(TestType)};
    CHECK(!lower_bound_equal<TestType>(r1, r2));
    CHECK(!lower_bound_greater_than<TestType>(r1, r2));
    CHECK(lower_bound_less_than<TestType>(r1, r2));
  }
  SECTION("Lower bound 1 is equal") {
    TestType data1[2]{2, 10};
    TestType data2[2]{2, 10};
    Range r1{data1, 2 * sizeof(TestType)};
    Range r2{data2, 2 * sizeof(TestType)};
    CHECK(lower_bound_equal<TestType>(r1, r2));
    CHECK(!lower_bound_greater_than<TestType>(r1, r2));
    CHECK(!lower_bound_less_than<TestType>(r1, r2));
  }
  SECTION("Lower bound 1 is greater than") {
    TestType data1[2]{1, 10};
    TestType data2[2]{0, 10};
    Range r1{data1, 2 * sizeof(TestType)};
    Range r2{data2, 2 * sizeof(TestType)};
    CHECK(!lower_bound_equal<TestType>(r1, r2));
    CHECK(lower_bound_greater_than<TestType>(r1, r2));
    CHECK(!lower_bound_less_than<TestType>(r1, r2));
  }
}

TEMPLATE_TEST_CASE(
    "Test range upper bound comparisons",
    "[range]",
    int8_t,
    uint8_t,
    int16_t,
    uint16_t,
    int32_t,
    uint32_t,
    int64_t,
    uint64_t,
    float,
    double) {
  SECTION("Upper bound 1 is less than") {
    TestType data1[2]{0, 9};
    TestType data2[2]{0, 10};
    Range r1{data1, 2 * sizeof(TestType)};
    Range r2{data2, 2 * sizeof(TestType)};
    CHECK(!upper_bound_equal<TestType>(r1, r2));
    CHECK(!upper_bound_greater_than<TestType>(r1, r2));
    CHECK(upper_bound_less_than<TestType>(r1, r2));
  }
  SECTION("Lower bound 1 is equal") {
    TestType data1[2]{0, 10};
    TestType data2[2]{0, 10};
    Range r1{data1, 2 * sizeof(TestType)};
    Range r2{data2, 2 * sizeof(TestType)};
    CHECK(upper_bound_equal<TestType>(r1, r2));
    CHECK(!upper_bound_greater_than<TestType>(r1, r2));
    CHECK(!upper_bound_less_than<TestType>(r1, r2));
  }
  SECTION("Lower bound 1 is greater than") {
    TestType data1[2]{0, 20};
    TestType data2[2]{0, 10};
    Range r1{data1, 2 * sizeof(TestType)};
    Range r2{data2, 2 * sizeof(TestType)};
    CHECK(!upper_bound_equal<TestType>(r1, r2));
    CHECK(upper_bound_greater_than<TestType>(r1, r2));
    CHECK(!upper_bound_less_than<TestType>(r1, r2));
  }
}
