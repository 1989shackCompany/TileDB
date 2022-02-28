/**
 * @file test-LabelledQuery.cc
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
 * Tests the `LabelledQuery` class.
 */

#include "test/src/helpers.h"
#include "test/src/vfs_helpers.h"
#include "tiledb.h"
#include "tiledb/sm/array_schema/dimension_label_reference.h"
#include "tiledb/sm/c_api/experimental/tiledb_dimension_label.h"
#include "tiledb/sm/c_api/experimental/tiledb_struct_def.h"
#include "tiledb/sm/c_api/tiledb.h"
#include "tiledb/sm/c_api/tiledb_experimental.h"
#include "tiledb/sm/c_api/tiledb_struct_def.h"
#include "tiledb/sm/dimension_label/dimension_label.h"
#include "tiledb/sm/dimension_label/dimension_label_query.h"
#include "tiledb/sm/enums/encryption_type.h"
#include "tiledb/sm/enums/label_order.h"
#include "tiledb/sm/enums/query_status.h"
#include "tiledb/sm/enums/query_type.h"
#include "tiledb/sm/query/query.h"
#include "tiledb/sm/subarray/subarray.h"

#include <catch.hpp>
#include <iostream>
#include <string>
#include <unordered_map>

using namespace tiledb::sm;
using namespace tiledb::test;

TEST_CASE_METHOD(
    TemporaryDirectoryFixture,
    "LabelledQuery: External label, 1D",
    "[Query][1d][DimensionLabel]") {
  // Create array and dimension label.
  const std::string array_name = fullpath("main");
  std::string dim_label_name{};
  std::string labelled_array_name{};
  std::string indexed_array_name{};
  // Set data for query.
  std::vector<uint64_t> index_data(16);
  std::vector<int64_t> label_data(16);
  for (uint32_t ii{0}; ii < 16; ++ii) {
    label_data[ii] = static_cast<int64_t>(ii) - 16;
    index_data[ii] = (ii + 1);
  }
  uint64_t index_data_size{16 * sizeof(uint64_t)};
  uint64_t label_data_size{16 * sizeof(uint64_t)};

  {
    int64_t index_domain[2]{1, 16};
    int64_t index_tile_extent{16};
    int64_t label_domain[2]{-16, -1};
    int64_t label_tile_extent{16};
    auto array_schema = create_array_schema(
        ctx,
        TILEDB_DENSE,
        {"dim0"},
        {TILEDB_UINT64},
        {index_domain},
        {&index_tile_extent},
        {"a1"},
        {TILEDB_FLOAT32},
        {1},
        {tiledb::test::Compressor(TILEDB_FILTER_LZ4, -1)},
        TILEDB_ROW_MAJOR,
        TILEDB_ROW_MAJOR,
        10000,
        false);
    tiledb_dimension_label_schema_t* dim_label_schema;
    REQUIRE_TILEDB_OK(tiledb_dimension_label_schema_alloc(
        ctx,
        TILEDB_INCREASING_LABELS,
        TILEDB_UINT64,
        index_domain,
        &index_tile_extent,
        TILEDB_INT64,
        label_domain,
        &label_tile_extent,
        &dim_label_schema));
    REQUIRE_TILEDB_OK(tiledb_array_schema_add_dimension_label(
        ctx, array_schema, 0, "x", dim_label_schema));
    {
      const auto& dim_label_ref =
          array_schema->array_schema_->dimension_label_reference(0);
      dim_label_name = array_name + "/" + dim_label_ref.uri().to_string();
      indexed_array_name = dim_label_name + "/indexed";
      labelled_array_name = dim_label_name + "/labelled";
    }
    tiledb_dimension_label_schema_free(&dim_label_schema);
    REQUIRE_TILEDB_OK(
        tiledb_array_create(ctx, array_name.c_str(), array_schema));
    tiledb_array_schema_free(&array_schema);
  }

  URI dim_label_uri{dim_label_name.c_str()};
  Status status{Status::Ok()};
  SECTION("Write to dimension label") {
    // Open the dimension label.
    auto dimension_label = make_shared<DimensionLabel>(
        HERE(), dim_label_uri, ctx->ctx_->storage_manager());
    dimension_label->open(
        QueryType::WRITE, EncryptionType::NO_ENCRYPTION, nullptr, 0);

    // Create dimension label data query.
    OrderedLabelsQuery query{dimension_label, ctx->ctx_->storage_manager()};
    status = query.initialize_data_query();
    REQUIRE_TILEDB_STATUS_OK(status);

    REQUIRE_TILEDB_STATUS_OK(
        query.set_index_data_buffer(index_data.data(), &index_data_size, true));
    REQUIRE_TILEDB_STATUS_OK(
        query.set_label_data_buffer(label_data.data(), &label_data_size, true));

    // Submit the query.
    REQUIRE_TILEDB_STATUS_OK(query.submit_data_query());

    REQUIRE(query.status_data_query() == QueryStatus::COMPLETED);

    // Close and clean-up the array
    dimension_label->close();

    // Read back data from indexed array.
    {
      std::vector<int64_t> indexed_array_label_data(16, 0);
      // 1. Allocate and open the indexed array.
      tiledb_array_t* indexed_array;
      REQUIRE_TILEDB_OK(
          tiledb_array_alloc(ctx, indexed_array_name.c_str(), &indexed_array));
      REQUIRE_TILEDB_OK(tiledb_array_open(ctx, indexed_array, TILEDB_READ));
      // 2. Allocate the query.
      tiledb_query_t* indexed_query;
      REQUIRE_TILEDB_OK(
          tiledb_query_alloc(ctx, indexed_array, TILEDB_READ, &indexed_query));
      // 3. Allocate and set the subarray.
      tiledb_subarray_t* indexed_subarray;
      REQUIRE_TILEDB_OK(
          tiledb_subarray_alloc(ctx, indexed_array, &indexed_subarray));
      uint64_t range_data[2]{1, 16};
      REQUIRE_TILEDB_OK(tiledb_subarray_add_range(
          ctx, indexed_subarray, 0, &range_data[0], &range_data[1], nullptr));
      REQUIRE_TILEDB_OK(
          tiledb_query_set_subarray_t(ctx, indexed_query, indexed_subarray));
      // 4. Set the data buffer for the label array.
      uint64_t output_label_data_size{16 * sizeof(uint64_t)};
      REQUIRE_TILEDB_OK(tiledb_query_set_buffer(
          ctx,
          indexed_query,
          "label",
          indexed_array_label_data.data(),
          &output_label_data_size));
      // 5. Submit the query.
      REQUIRE_TILEDB_OK(tiledb_query_submit(ctx, indexed_query));
      // 6. Release the resources
      tiledb_subarray_free(&indexed_subarray);
      tiledb_query_free(&indexed_query);
      REQUIRE_TILEDB_OK(tiledb_array_close(ctx, indexed_array));
      tiledb_array_free(&indexed_array);

      for (uint32_t ii{0}; ii < 16; ++ii) {
        CHECK(label_data[ii] == indexed_array_label_data[ii]);
      }
      // Check data is as expected.
    }

    // Read back data from labelled array.
    std::vector<int64_t> labelled_array_label_data(16, 0);
    std::vector<uint64_t> labelled_array_index_data(16, 0);
    {
      uint64_t output_index_data_size{16 * sizeof(uint64_t)};
      uint64_t output_label_data_size{16 * sizeof(uint64_t)};
      tiledb_array_t* labelled_array;
      REQUIRE_TILEDB_OK(tiledb_array_alloc(
          ctx, labelled_array_name.c_str(), &labelled_array));
      REQUIRE_TILEDB_OK(tiledb_array_open(ctx, labelled_array, TILEDB_READ));
      tiledb_query_t* labelled_query;
      REQUIRE_TILEDB_OK(tiledb_query_alloc(
          ctx, labelled_array, TILEDB_READ, &labelled_query));
      REQUIRE_TILEDB_OK(tiledb_query_set_buffer(
          ctx,
          labelled_query,
          "label",
          labelled_array_label_data.data(),
          &output_label_data_size));
      REQUIRE_TILEDB_OK(tiledb_query_set_buffer(
          ctx,
          labelled_query,
          "index",
          labelled_array_index_data.data(),
          &output_index_data_size));
      REQUIRE_TILEDB_OK(tiledb_query_submit(ctx, labelled_query));
      tiledb_query_free(&labelled_query);
      REQUIRE_TILEDB_OK(tiledb_array_close(ctx, labelled_array));
      tiledb_array_free(&labelled_array);
    }
    // Check data is as expected.
    for (uint32_t ii{0}; ii < 16; ++ii) {
      CHECK(label_data[ii] == labelled_array_label_data[ii]);
    }
    for (uint32_t ii{0}; ii < 16; ++ii) {
      CHECK(index_data[ii] == labelled_array_index_data[ii]);
    }
  }
  SECTION("Read range from manually written dimension label") {
    // Write main array.
    {
      tiledb::test::QueryBuffers buffers;
      std::vector<float> a1_data(16);
      for (uint64_t ii{0}; ii < 16; ++ii) {
        a1_data[ii] = 0.1 * (1 + ii);
      }
      buffers["a1"] = tiledb::test::QueryBuffer(
          {&a1_data[0], a1_data.size() * sizeof(float), nullptr, 0});
      write_array(ctx, array_name, TILEDB_ROW_MAJOR, buffers);
    }

    // Write dimension labels - TODO update to use API
    {
      // Define data
      std::vector<int64_t> label_data(16);
      std::vector<uint64_t> index_data(16);
      for (uint32_t ii{0}; ii < 16; ++ii) {
        label_data[ii] = static_cast<int64_t>(ii) - 16;
        index_data[ii] = (ii + 1);
      }
      // Set data in indexed array
      tiledb::test::QueryBuffers label_buffer;
      label_buffer["label"] = tiledb::test::QueryBuffer(
          {&label_data[0], label_data.size() * sizeof(int64_t), nullptr, 0});
      write_array(ctx, indexed_array_name, TILEDB_GLOBAL_ORDER, label_buffer);
      // Set data in labelled array
      tiledb::test::QueryBuffers buffers;
      buffers["label"] = tiledb::test::QueryBuffer(
          {&label_data[0], label_data.size() * sizeof(int64_t), nullptr, 0});
      buffers["index"] = tiledb::test::QueryBuffer(
          {&index_data[0], index_data.size() * sizeof(uint64_t), nullptr, 0});
      write_array(ctx, labelled_array_name, TILEDB_GLOBAL_ORDER, buffers);
    }

    SECTION("Read range from a dimension label") {
      // Open the dimension label.
      auto dimension_label = make_shared<DimensionLabel>(
          HERE(), dim_label_uri, ctx->ctx_->storage_manager());
      dimension_label->open(
          QueryType::READ, EncryptionType::NO_ENCRYPTION, nullptr, 0);

      // Create dimension label query.
      OrderedLabelsQuery query{dimension_label, ctx->ctx_->storage_manager()};

      // Set label range
      std::vector<int64_t> range{-8, -5};
      status = query.add_label_range(&range[0], &range[1], nullptr);
      if (!status.ok())
        INFO("Set label range: " + status.to_string());
      REQUIRE(status.ok());

      // Submit label query and check for success.
      status = query.resolve_labels();
      if (!status.ok())
        INFO("Resolve labels: " + status.to_string());
      REQUIRE(status.ok());

      // Submit main query and check for success.
      auto query_status = query.status_resolve_labels();
      INFO("Query status resolve labels: " + query_status_str(query_status));

      auto&& [range_status, index_range] = query.get_index_range();
      REQUIRE(range_status.ok());
      auto range_data = static_cast<const uint64_t*>(index_range.data());
      CHECK(range_data[0] == 9);
      CHECK(range_data[1] == 12);

      // Close and clean-up the array
      dimension_label->close();
    }
    SECTION("Read label data from a dimension label") {
      // Open the dimension label.
      auto dimension_label = make_shared<DimensionLabel>(
          HERE(), dim_label_uri, ctx->ctx_->storage_manager());
      dimension_label->open(
          QueryType::READ, EncryptionType::NO_ENCRYPTION, nullptr, 0);

      // Create dimension label data query.
      OrderedLabelsQuery query{dimension_label, ctx->ctx_->storage_manager()};
      status = query.initialize_data_query();
      if (!status.ok())
        INFO("Create data query: " + status.to_string());
      REQUIRE(status.ok());

      // Set index ranges.
      std::vector<uint64_t> index_range{9, 12};
      std::vector<Range> ranges{Range(&index_range[0], 2 * sizeof(uint64_t))};
      status = query.set_index_ranges(ranges);
      if (!status.ok())
        INFO("Set index ranges: " + status.to_string());
      REQUIRE(status.ok());

      // Set label data buffer
      std::vector<int64_t> label(16);
      uint64_t label_size{label.size() * sizeof(int64_t)};
      status = query.set_label_data_buffer(&label[0], &label_size, true);
      if (!status.ok())
        INFO("Set label data buffer: " + status.to_string());
      REQUIRE(status.ok());

      // Submit label query and check for success.
      status = query.submit_data_query();
      if (!status.ok())
        INFO("Submit data query: " + status.to_string());
      REQUIRE(status.ok());

      // Submit main query and check for success.
      auto query_status = query.status_data_query();
      INFO("Query status label data: " + query_status_str(query_status));

      // Close and clean-up the array
      dimension_label->close();

      // Check results
      std::vector<int64_t> expected_label{-8, -7, -6, -5};
      expected_label.resize(16, 0);
      for (size_t ii{0}; ii < 16; ++ii) {
        CHECK(label[ii] == expected_label[ii]);
      }
    }
    SECTION("Read label and set all buffer") {
      // Open the array
      tiledb_array_t* main_array;
      int rc = tiledb_array_alloc(ctx, array_name.c_str(), &main_array);
      CHECK(rc == TILEDB_OK);
      rc = tiledb_array_open(ctx, main_array, TILEDB_READ);
      CHECK(rc == TILEDB_OK);

      // Open the dimension label.
      auto dimension_label = make_shared<DimensionLabel>(
          HERE(), dim_label_uri, ctx->ctx_->storage_manager());
      dimension_label->open(
          QueryType::READ, EncryptionType::NO_ENCRYPTION, nullptr, 0);

      // Create query and set standard data.
      Query query{ctx->ctx_->storage_manager(), main_array->array_};
      query.set_layout(Layout::ROW_MAJOR);
      std::vector<float> a1(4);
      uint64_t a1_size{a1.size() * sizeof(float)};
      status = Status::Ok();
      status = query.set_data_buffer("a1", &a1[0], &a1_size);
      if (!status.ok())
        INFO(status.to_string());
      REQUIRE(status.ok());

      // Set ranges.
      query.set_external_label(0, "label0", dimension_label);
      std::vector<int64_t> range{-8, -5};
      status = query.add_label_range(0, &range[0], &range[1], nullptr);
      if (!status.ok())
        INFO("Set label range: " + status.to_string());
      REQUIRE(status.ok());

      // Set label data buffer
      std::vector<int64_t> label(4);
      uint64_t label_size{label.size() * sizeof(int64_t)};
      status = query.set_label_data_buffer("label0", &label[0], &label_size);
      if (!status.ok())
        INFO("Set label data buffer: " + status.to_string());
      REQUIRE(status.ok());

      // Submit label query and check for success.
      status = query.submit_labels();
      REQUIRE(status.ok());
      status = query.apply_labels();
      if (!status.ok())
        INFO("Apply labels: " + status.to_string());
      REQUIRE(status.ok());

      // Submit main query and check for success.
      status = query.submit();
      if (!status.ok())
        INFO("Submit query: " + status.to_string());
      REQUIRE(status.ok());
      INFO(query_status_str(query.status()));
      CHECK(query.status() == QueryStatus::COMPLETED);

      // Close and clean-up the arrayis
      rc = tiledb_array_close(ctx, main_array);
      CHECK(rc == TILEDB_OK);
      tiledb_array_free(&main_array);

      // Close and clean-up the array
      dimension_label->close();

      // Check results
      std::vector<int64_t> expected_label{-8, -7, -6, -5};
      std::vector<float> expected_a1{0.9f, 1.0f, 1.1f, 1.2f};

      for (size_t ii{0}; ii < 4; ++ii) {
        INFO("Label " << std::to_string(ii));
        CHECK(label[ii] == expected_label[ii]);
        CHECK(a1[ii] == expected_a1[ii]);
      }
    }
  }
}
