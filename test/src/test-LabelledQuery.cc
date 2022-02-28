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
#include "tiledb/sm/c_api/tiledb_struct_def.h"
#include "tiledb/sm/dimension_label/dimension_label.h"
#include "tiledb/sm/dimension_label/dimension_label_query.h"
#include "tiledb/sm/enums/encryption_type.h"
#include "tiledb/sm/enums/label_order.h"
#include "tiledb/sm/enums/query_status.h"
#include "tiledb/sm/enums/query_type.h"
#include "tiledb/sm/query/query.h"
#include "tiledb/sm/subarray/subarray.h"

#ifdef _WIN32
#include "tiledb/sm/filesystem/win.h"
#else
#include "tiledb/sm/filesystem/posix.h"
#endif

#include <catch.hpp>
#include <iostream>
#include <string>
#include <unordered_map>

using namespace tiledb::sm;
using namespace tiledb::test;

/* ********************************* */
/*         STRUCT DEFINITION         */
/* ********************************* */

/**
 * Fixture for creating a temporary directory for a test case. This fixture
 * also manages the context and virtual file system for the test case.
 */
struct TemporaryDirectoryFixture {
 public:
  /** Fixture constructor. */
  TemporaryDirectoryFixture()
      : supported_filesystems_(vfs_test_get_fs_vec()) {
    // Initialize virtual filesystem and context.
    REQUIRE(vfs_test_init(supported_filesystems_, &ctx, &vfs_).ok());

    // Create temporary directory based on the supported filesystem
#ifdef _WIN32
    SupportedFsLocal windows_fs;
    temp_dir_ = windows_fs.file_prefix() + windows_fs.temp_dir();
#else
    SupportedFsLocal posix_fs;
    temp_dir_ = posix_fs.file_prefix() + posix_fs.temp_dir();
#endif
    create_dir(temp_dir_, ctx, vfs_);
  }

  /** Fixture destructor. */
  ~TemporaryDirectoryFixture() {
    remove_dir(temp_dir_, ctx, vfs_);
    tiledb_ctx_free(&ctx);
    tiledb_vfs_free(&vfs_);
  }

  /** Create a new array. */
  std::string fullpath(std::string&& name) {
    return temp_dir_ + name;
  }

 protected:
  /** TileDB context */
  tiledb_ctx_t* ctx;

  /** Name of the temporary directory to use for this test */
  std::string temp_dir_;

 private:
  /** Virtual file system */
  tiledb_vfs_t* vfs_;

  /** Vector of supported filesystems. Used to initialize ``vfs_``. */
  const std::vector<std::unique_ptr<SupportedFs>> supported_filesystems_;
};

/**
 * Create and write to a 1D array.
 *
 *  Domain: (dim0, [1, 16], uint64)
 *  Attrs: (a1, [0.1, 0.2, ...1.6], float)
 */
void create_main_array_1d(const std::string& name, tiledb_ctx_t* ctx) {
  uint64_t domain[2]{1, 16};
  uint64_t tile_extent{16};
  create_array(
      ctx,
      name,
      TILEDB_DENSE,
      {"dim0"},
      {TILEDB_UINT64},
      {domain},
      {&tile_extent},
      {"a1"},
      {TILEDB_FLOAT32},
      {1},
      {tiledb::test::Compressor(TILEDB_FILTER_LZ4, -1)},
      TILEDB_ROW_MAJOR,
      TILEDB_ROW_MAJOR,
      10000);
  tiledb::test::QueryBuffers buffers;
  std::vector<float> a1_data(16);
  for (uint64_t ii{0}; ii < 16; ++ii) {
    a1_data[ii] = 0.1 * (1 + ii);
  }
  buffers["a1"] = tiledb::test::QueryBuffer(
      {&a1_data[0], a1_data.size() * sizeof(float), nullptr, 0});
  write_array(ctx, name, TILEDB_ROW_MAJOR, buffers);
}

void create_uniform_label(
    const std::string& name_indexed,
    const std::string& name_labelled,
    tiledb_ctx_t* ctx) {
  // Create indexed array
  int64_t index_domain[2]{1, 16};
  int64_t index_tile_extent{16};
  create_array(
      ctx,
      name_indexed,
      TILEDB_DENSE,
      {"index"},
      {TILEDB_INT64},
      {index_domain},
      {&index_tile_extent},
      {"label"},
      {TILEDB_UINT64},
      {1},
      {tiledb::test::Compressor(TILEDB_FILTER_LZ4, -1)},
      TILEDB_ROW_MAJOR,
      TILEDB_ROW_MAJOR,
      10000);
  // Create labelled array
  int64_t label_domain[2]{-16, -1};
  int64_t label_tile_extent{16};
  create_array(
      ctx,
      name_labelled,
      TILEDB_SPARSE,
      {"label"},
      {TILEDB_UINT64},
      {label_domain},
      {&label_tile_extent},
      {"index"},
      {TILEDB_INT64},
      {1},
      {tiledb::test::Compressor(TILEDB_FILTER_LZ4, -1)},
      TILEDB_ROW_MAJOR,
      TILEDB_ROW_MAJOR,
      10000);
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
  write_array(ctx, name_indexed, TILEDB_GLOBAL_ORDER, label_buffer);
  // Set data in labelled array
  tiledb::test::QueryBuffers buffers;
  buffers["label"] = tiledb::test::QueryBuffer(
      {&label_data[0], label_data.size() * sizeof(int64_t), nullptr, 0});
  buffers["index"] = tiledb::test::QueryBuffer(
      {&index_data[0], index_data.size() * sizeof(uint64_t), nullptr, 0});
  write_array(ctx, name_labelled, TILEDB_GLOBAL_ORDER, buffers);
}

TEST_CASE_METHOD(
    TemporaryDirectoryFixture,
    "LabelledQuery: External label, 1D",
    "[Query][1d][DimensionLabel]") {
  // Create arrays for test cases.
  const std::string main_array_name = fullpath("main");
  const std::string label_array_name = fullpath("labelled");
  const std::string index_array_name = fullpath("indexed");
  create_main_array_1d(main_array_name, ctx);
  create_uniform_label(index_array_name, label_array_name, ctx);

  SECTION("Read range from a dimension label") {
    // Open the dimension label.
    URI indexed_uri{index_array_name.c_str()};
    URI labelled_uri{label_array_name.c_str()};
    auto dimension_label = make_shared<DimensionLabel>(
        HERE(),
        indexed_uri,
        labelled_uri,
        ctx->ctx_->storage_manager(),
        LabelOrder::INCREASING_LABELS);
    auto status = dimension_label->open(
        QueryType::READ, EncryptionType::NO_ENCRYPTION, nullptr, 0);
    if (!status.ok())
      INFO("Open dimension_label: " + status.to_string());
    REQUIRE(status.ok());

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
    auto close_status = dimension_label->close();
    CHECK(close_status.ok());
  }
  SECTION("Read label data from a dimension label") {
    // Open the dimension label.
    URI indexed_uri{index_array_name.c_str()};
    URI labelled_uri{label_array_name.c_str()};
    auto dimension_label = make_shared<DimensionLabel>(
        HERE(),
        indexed_uri,
        labelled_uri,
        ctx->ctx_->storage_manager(),
        LabelOrder::INCREASING_LABELS);
    auto status = dimension_label->open(
        QueryType::READ, EncryptionType::NO_ENCRYPTION, nullptr, 0);
    if (!status.ok())
      INFO("Open dimension alebl: " + status.to_string());
    REQUIRE(status.ok());

    // Create dimension label data query.
    OrderedLabelsQuery query{dimension_label, ctx->ctx_->storage_manager()};
    status = query.create_data_query();
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
    auto close_status = dimension_label->close();
    if (!close_status.ok())
      INFO("Close dimension label: " + status.to_string());
    CHECK(close_status.ok());

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
    int rc = tiledb_array_alloc(ctx, main_array_name.c_str(), &main_array);
    CHECK(rc == TILEDB_OK);
    rc = tiledb_array_open(ctx, main_array, TILEDB_READ);
    CHECK(rc == TILEDB_OK);

    // Open the dimension label.
    URI indexed_uri{index_array_name.c_str()};
    URI labelled_uri{label_array_name.c_str()};
    auto dimension_label = make_shared<DimensionLabel>(
        HERE(),
        indexed_uri,
        labelled_uri,
        ctx->ctx_->storage_manager(),
        LabelOrder::INCREASING_LABELS);
    auto status = dimension_label->open(
        QueryType::READ, EncryptionType::NO_ENCRYPTION, nullptr, 0);
    if (!status.ok())
      INFO("Open dimension label: " + status.to_string());
    REQUIRE(status.ok());

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

    auto close_status = dimension_label->close();
    CHECK(close_status.ok());

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
