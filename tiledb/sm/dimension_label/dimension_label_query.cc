#include "tiledb/sm/dimension_label/dimension_label_query.h"
#include "tiledb/common/logger.h"
#include "tiledb/common/logger_public.h"
#include "tiledb/sm/array/array.h"
#include "tiledb/sm/array_schema/attribute.h"
#include "tiledb/sm/array_schema/dimension.h"
#include "tiledb/sm/dimension_label/dimension_label.h"
#include "tiledb/sm/enums/datatype.h"
#include "tiledb/sm/query/query.h"
#include "tiledb/sm/storage_manager/storage_manager.h"

namespace tiledb::sm {

OrderedLabelsQuery::OrderedLabelsQuery(
    shared_ptr<DimensionLabel> dimension_label, StorageManager* storage_manager)
    : dimension_label_{dimension_label}
    , storage_manager_{storage_manager}
    , stats_{storage_manager_->stats()->create_child("DimensionLabelQuery")}
    , logger_(
          storage_manager->logger()->clone("DimensionLabelQuery", ++logger_id_))
    , range_query_{nullptr}
    , labelled_array_query_{nullptr}
    , indexed_array_query_{nullptr} {
  auto&& [status, query_type] = dimension_label_->query_type();
  throw_if_not_ok(status);
  if (!query_type.has_value())
    throw std::logic_error("Failed to ready dimension label query type.");
  query_type_ = query_type.value();
}

Status OrderedLabelsQuery::add_label_range(
    const void* start, const void* end, const void* stride) {
  if (stride != nullptr)
    return Status_DimensionLabelQueryError(
        "Cannot add range; Setting label range stride is currently "
        "unsupported.");
  if (range_query_ != nullptr)
    return Status_DimensionLabelQueryError(
        "Cannot add range; Setting more than one label range is currently "
        "unsupported.");
  if (query_type_ == QueryType::WRITE)
    return Status_DimensionLabelQueryError(
        "Cannot add range; DimensionLabel writes cannot be set by label.");
  range_query_ = tdb_unique_ptr<RangeQuery>(tdb_new(
      RangeQuery, dimension_label_.get(), storage_manager_, start, end));
  return Status::Ok();
}

Status OrderedLabelsQuery::add_label_range_var(
    const void*, uint64_t, const void*, uint64_t) {
  return Status_DimensionLabelQueryError(
      "Adding variable length ranges in not yet supported for ordered "
      "labels.");
}

Status OrderedLabelsQuery::cancel() {
  if (range_query_)
    RETURN_NOT_OK(range_query_->cancel());
  if (indexed_array_query_)
    RETURN_NOT_OK(indexed_array_query_->cancel());
  if (labelled_array_query_)
    RETURN_NOT_OK(labelled_array_query_->cancel());
  return Status::Ok();
}

Status OrderedLabelsQuery::initialize_data_query() {
  /**
   * For reading data, only the indexed array is needed. For writing to the
   * dimension label, we need to open both the indexed and labelled arrays.
   */
  if (query_type_ == QueryType::WRITE) {
    if (!dimension_label_->labelled_array()->is_empty() ||
        !dimension_label_->indexed_array()->is_empty())
      return Status_DimensionLabelQueryError(
          "Cannot write to dimension label. Currently dimension labels can "
          "only be written to once.");
    if (labelled_array_query_)
      return Status_DimensionLabelQueryError(
          "Cannot create data query. Query already exists.");
    labelled_array_query_ = tdb_unique_ptr<Query>(
        tdb_new(Query, storage_manager_, dimension_label_->labelled_array()));
    RETURN_NOT_OK(labelled_array_query_->set_layout(Layout::ROW_MAJOR));
  }
  if (indexed_array_query_)
    return Status_DimensionLabelQueryError(
        "Cannot create data query. Query already exists.");
  indexed_array_query_ = tdb_unique_ptr<Query>(
      tdb_new(Query, storage_manager_, dimension_label_->indexed_array()));
  RETURN_NOT_OK(indexed_array_query_->set_layout(Layout::ROW_MAJOR));
  return Status::Ok();
}

Status OrderedLabelsQuery::finalize() {
  if (range_query_)
    RETURN_NOT_OK(range_query_->finalize());
  if (indexed_array_query_)
    RETURN_NOT_OK(indexed_array_query_->finalize());
  if (labelled_array_query_)
    RETURN_NOT_OK(labelled_array_query_->finalize());
  return Status::Ok();
}

tuple<Status, Range> OrderedLabelsQuery::get_index_range() const {
  if (!range_query_)
    return {Status_DimensionLabelQueryError("No label range set."), Range()};
  Range range{range_query_->index_range()};
  return {(range_query_->status() == QueryStatus::COMPLETED) ?
              Status::Ok() :
              Status_DimensionLabelQueryError("Label query incomplete"),
          range};
}

Status OrderedLabelsQuery::resolve_labels() {
  if (range_query_) {
    RETURN_NOT_OK(range_query_->submit());
    RETURN_NOT_OK(range_query_->finalize());
  }
  return Status::Ok();
}

Status OrderedLabelsQuery::set_index_data_buffer(
    void* const buffer,
    uint64_t* const buffer_size,
    const bool check_null_buffers) {
  if (query_type_ != QueryType::WRITE)
    return Status_DimensionLabelQueryError(
        "Cannot set index data buffer; Index buffer only accessed on writes.");
  if (!labelled_array_query_)
    return Status_DimensionLabelQueryError(
        "Cannot set index data buffer; Data query not inialized.");
  return labelled_array_query_->set_data_buffer(
      dimension_label_->label_attribute()->name(),
      buffer,
      buffer_size,
      check_null_buffers);
}

Status OrderedLabelsQuery::set_index_ranges(const std::vector<Range>& ranges) {
  if (!indexed_array_query_)
    return Status_DimensionLabelQueryError(
        "Cannot set subarray. Data query not initialized.");
  if (query_type_ == QueryType::WRITE)
    return Status_DimensionLabelQueryError(
        "Cannot set subarray. Currently dimension labels only support writing "
        "the full array.");
  Subarray subarray{dimension_label_->indexed_array().get(),
                    Layout::ROW_MAJOR,
                    stats_,
                    logger_};
  RETURN_NOT_OK(subarray.set_ranges_for_dim(0, ranges));
  return indexed_array_query_->set_subarray(subarray);
}

Status OrderedLabelsQuery::set_label_data_buffer(
    void* const buffer,
    uint64_t* const buffer_size,
    const bool check_null_buffers) {
  if (!indexed_array_query_)
    return Status_DimensionLabelQueryError(
        "Cannot set label data buffer; Data query not initialized.");
  /**
  if (query_type_ == TILEDB::WRITE)
    check the data is sorted
  **/
  return indexed_array_query_->set_data_buffer(
      dimension_label_->label_attribute()->name(),
      buffer,
      buffer_size,
      check_null_buffers);
}

QueryStatus OrderedLabelsQuery::status_data_query() const {
  // TODO: Fix this to have a consistent status.
  if (!labelled_array_query_ && !indexed_array_query_)
    return QueryStatus::COMPLETED;
  if (!labelled_array_query_ && indexed_array_query_)
    return indexed_array_query_->status();
  if (!indexed_array_query_ && labelled_array_query_)
    return labelled_array_query_->status();
  auto labelled_status = labelled_array_query_->status();
  auto indexed_status = indexed_array_query_->status();
  if (labelled_status == QueryStatus::FAILED ||
      indexed_status == labelled_status)
    return labelled_status;
  return indexed_status;
}

QueryStatus OrderedLabelsQuery::status_resolve_labels() const {
  if (!range_query_)  // TODO: Fix to be for range  reader
    return QueryStatus::COMPLETED;
  return range_query_->status();
}

Status OrderedLabelsQuery::submit_data_query() {
  if (range_query_ && range_query_->status() != QueryStatus::COMPLETED)
    return Status_DimensionLabelQueryError(
        "Cannot set data queries until label query completes");
  if (labelled_array_query_)
    RETURN_NOT_OK(labelled_array_query_->submit());
  if (indexed_array_query_) {
    RETURN_NOT_OK(indexed_array_query_->submit());
  }
  return Status::Ok();
}

}  // namespace tiledb::sm
