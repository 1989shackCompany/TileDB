#include "tiledb/sm/dimension_label/range_query.h"
#include "tiledb/sm/array/array.h"
#include "tiledb/sm/array_schema/attribute.h"
#include "tiledb/sm/array_schema/dimension.h"
#include "tiledb/sm/dimension_label/dimension_label.h"
#include "tiledb/sm/enums/datatype.h"
#include "tiledb/sm/query/query.h"
#include "tiledb/sm/storage_manager/storage_manager.h"

namespace tiledb::sm {

RangeQuery::RangeQuery(
    DimensionLabel* dimension_label,
    StorageManager* storage_manager,
    const void* start,
    const void* end)
    : order_{dimension_label->label_order()}
    , label_data_size_{datatype_size(
          dimension_label->label_dimension()->type())}
    , index_data_size_{datatype_size(
          dimension_label->index_attribute()->type())}
    , input_label_range_{start, end, label_data_size_}
    , computed_label_range_{input_label_range_}
    , computed_index_range_{dimension_label->index_dimension()->domain()}
    , lower_bound_query_{storage_manager, dimension_label->labelled_array()}
    , upper_bound_query_{storage_manager, dimension_label->labelled_array()} {
  if (order_ != LabelOrder::INCREASING_LABELS)
    throw std::invalid_argument(
        "Support for reading ranges is only implemented for forward-ordered "
        "axes.");
  const auto label_dim = dimension_label->label_dimension();
  const auto label_domain = label_dim->domain();
  const auto label_name = label_dim->name();
  const auto index_name = dimension_label->index_attribute()->name();
  // Set ranges for lower and upper bounds.
  throw_if_not_ok(lower_bound_query_.add_range(
      0, start, label_domain.end_fixed(), nullptr));
  throw_if_not_ok(
      upper_bound_query_.add_range(0, end, label_domain.end_fixed(), nullptr));
  // Set data buffer for computed label range.
  throw_if_not_ok(lower_bound_query_.set_data_buffer(
      label_name,
      const_cast<void*>(computed_label_range_.start_fixed()),
      &label_data_size_));
  throw_if_not_ok(upper_bound_query_.set_data_buffer(
      label_name,
      const_cast<void*>(computed_label_range_.end_fixed()),
      &label_data_size_));
  // Set data buffer for computed index range.
  throw_if_not_ok(lower_bound_query_.set_data_buffer(
      index_name,
      const_cast<void*>(computed_index_range_.start_fixed()),
      &index_data_size_));
  throw_if_not_ok(upper_bound_query_.set_data_buffer(
      index_name,
      const_cast<void*>(computed_index_range_.end_fixed()),
      &index_data_size_));
}

Status RangeQuery::cancel() {
  // TODO: Change to cancel both before returning status error
  RETURN_NOT_OK(lower_bound_query_.cancel());
  RETURN_NOT_OK(upper_bound_query_.cancel());
  return Status::Ok();
}

Status RangeQuery::finalize() {
  if (!lower_bound_query_.has_results() || !upper_bound_query_.has_results()) {
    status_ = QueryStatus::FAILED;
    lower_bound_query_.finalize();
    upper_bound_query_.finalize();
    return Status_RangeQueryError("Failed to read index range from label.");
  }
  // TODO Will need to do comparisons of the types.
  status_ = QueryStatus::COMPLETED;
  RETURN_NOT_OK(lower_bound_query_.finalize());
  RETURN_NOT_OK(upper_bound_query_.finalize());
  return Status::Ok();
}

Status RangeQuery::submit() {
  // TODO: Change to cancel both before returning status error
  auto status = lower_bound_query_.submit();
  if (!status.ok()) {
    upper_bound_query_.cancel();
    return status;
  }
  status = upper_bound_query_.submit();
  if (status.ok()) {
    lower_bound_query_.cancel();
    return status;
  }
  return Status::Ok();
}

}  // namespace tiledb::sm
