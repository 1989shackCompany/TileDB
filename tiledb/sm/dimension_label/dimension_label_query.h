/**
 * @file <TODO: tiledb/path/to/example.h>
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
 * TODO: Add description
 */

#ifndef TILEDB_DIMENSION_LABEL_QUERY_H
#define TILEDB_DIMENSION_LABEL_QUERY_H

#include <string>
#include "tiledb/sm/dimension_label/dimension_label.h"
#include "tiledb/sm/dimension_label/range_query.h"
#include "tiledb/sm/enums/query_status.h"
#include "tiledb/sm/query/query.h"

namespace tiledb::sm {

class Array;
class DimensionLabel;
class Query;

/**
 * Returns a Status_DimensionLabelQueryError error class Status with a given
 * message. Note: currently set to return Query error.
 **/
inline Status Status_DimensionLabelQueryError(const std::string& msg) {
  return {"[TileDB::Query] Error", msg};
};

class DimensionLabelQuery {
 public:
  virtual ~DimensionLabelQuery() = default;

  /** TODO */
  virtual Status add_label_range(
      const void* start, const void* end, const void* stride) = 0;

  /** TODO */
  virtual Status add_label_range_var(
      const void* start,
      uint64_t start_size,
      const void* end,
      uint64_t end_size) = 0;

  /**
   * Marks a query that has not yet been started as failed. This should not be
   * called asynchronously to cancel an in-progress query; for that use the
   * parent StorageManager's cancellation mechanism.
   * @return Status
   */
  virtual Status cancel() = 0;

  /** TODO */
  virtual Status create_data_query() = 0;

  /** TODO */
  virtual Status finalize() = 0;

  /** TODO */
  virtual tuple<Status, Range> get_index_range() const = 0;

  /** TODO */
  virtual Status resolve_labels() = 0;

  /** TODO */
  virtual Status set_index_data_buffer(
      void* const buffer,
      uint64_t* const buffer_size,
      const bool check_null_buffers) = 0;

  /** TODO */
  virtual Status set_index_ranges(const std::vector<Range>& ranges) = 0;

  /**
   * Sets the data for an dimension label where the label values fixed sized.
   *
   * @param buffer The buffer that will hold the data to be read.
   * @param buffer_size This initially contains the allocated
   *     size of `buffer`, but after the termination of the function
   *     it will contain the size of the useful (read) data in `buffer`.
   * @param check_null_buffers If true, null buffers are not allowed.
   * @return Status
   */
  virtual Status set_label_data_buffer(
      void* const buffer,
      uint64_t* const buffer_size,
      const bool check_null_buffers) = 0;

  /** TODO */
  virtual QueryStatus status_data_query() const = 0;

  /** TODO */
  virtual QueryStatus status_resolve_labels() const = 0;

  /** Submits the query to the storage manager. */
  virtual Status submit_data_query() = 0;
};

class OrderedLabelsQuery : public DimensionLabelQuery {
 public:
  OrderedLabelsQuery() = delete;

  OrderedLabelsQuery(
      shared_ptr<DimensionLabel> dimension_label,
      StorageManager* storage_manager);

  /** Disable copy and move. */
  DISABLE_COPY_AND_COPY_ASSIGN(OrderedLabelsQuery);
  DISABLE_MOVE_AND_MOVE_ASSIGN(OrderedLabelsQuery);

  Status add_label_range(
      const void* start, const void* end, const void* stride) override;

  Status add_label_range_var(
      const void* start,
      uint64_t start_size,
      const void* end,
      uint64_t end_size) override;

  Status cancel() override;

  Status create_data_query() override;

  Status finalize() override;

  tuple<Status, Range> get_index_range() const override;

  Status resolve_labels() override;

  Status set_index_data_buffer(
      void* const buffer,
      uint64_t* buffer_size,
      const bool check_null_buffers) override;

  Status set_index_ranges(const std::vector<Range>& ranges) override;

  Status set_index_subarray(const void* subarray);

  Status set_label_data_buffer(
      void* const buffer,
      uint64_t* const buffer_size,
      const bool check_null_buffers) override;

  QueryStatus status_data_query() const override;

  QueryStatus status_resolve_labels() const override;

  Status submit_data_query() override;

 private:
  /** UID of the logger instance */
  inline static std::atomic<uint64_t> logger_id_ = 0;

  shared_ptr<DimensionLabel> dimension_label_;

  StorageManager* storage_manager_;

  stats::Stats* stats_;

  shared_ptr<Logger> logger_;

  tdb_unique_ptr<RangeQuery> range_query_;

  tdb_unique_ptr<Query> labelled_array_query_;

  tdb_unique_ptr<Query> indexed_array_query_;

  QueryType query_type_;
};

}  // namespace tiledb::sm

#endif
