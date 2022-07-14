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

#ifndef TILEDB_RANGE_QUERY_H
#define TILEDB_RANGE_QUERY_H

#include <string>
#include "tiledb/sm/dimension_label/dimension_label.h"
#include "tiledb/sm/enums/query_status.h"
#include "tiledb/sm/query/query.h"

namespace tiledb::sm {

class Array;
class DimensionLabel;
class Query;

/** Return a Status_RangeQueryError error class Status with a given
 * message. Note: currently set to return Query error. **/
inline Status Status_RangeQueryError(const std::string& msg) {
  return {"[TileDB::Query] Error", msg};
};

/**
 * This class should be considered deprecated at creation. This RangeQuery
 * should be replaced with a range strategy that can handle multiple ranges
 * inside a single query.
 *
 * The range query assumes that the index values for the dimension label index
 * are consecutive values that are increasing or decreasing and that there is
 * no gaps in the label.
 *
 */
class RangeQuery {
 public:
  RangeQuery() = delete;

  RangeQuery(
      DimensionLabel* dimension_label,
      StorageManager* storage_manager,
      const void* start,
      const void* end);

  /** Disable copy and move. */
  DISABLE_COPY_AND_COPY_ASSIGN(RangeQuery);
  DISABLE_MOVE_AND_MOVE_ASSIGN(RangeQuery);

  /** Cancel the query. */
  Status cancel();

  /**
   * Finalize the query and update the computed index range if it contains an
   * extra value.
   */
  Status finalize();

  /** Returns the index range computed by the range query. */
  inline const Range& index_range() const {
    return computed_index_range_;
  }

  /** Returns the status of the query. */
  inline QueryStatus status() const {
    return status_;
  }

  /** Submits the query. */
  Status submit();

 private:
  LabelOrder order_;
  uint64_t label_data_size_;
  uint64_t index_data_size_;
  Range input_label_range_;
  Range computed_label_range_;
  Range computed_index_range_;
  Query lower_bound_query_;
  Query upper_bound_query_;
  std::function<bool(const Range& range1, const Range& range2)>
      has_extra_range_element_;
  std::function<void(Range& range)> fix_index_range_;
  QueryStatus status_{QueryStatus::UNINITIALIZED};
};

}  // namespace tiledb::sm

#endif
