#include "tiledb/sm/dimension_label/range_query.h"
#include "tiledb/sm/array/array.h"
#include "tiledb/sm/array_schema/attribute.h"
#include "tiledb/sm/array_schema/dimension.h"
#include "tiledb/sm/dimension_label/dimension_label.h"
#include "tiledb/sm/enums/datatype.h"
#include "tiledb/sm/query/query.h"
#include "tiledb/sm/storage_manager/storage_manager.h"

namespace tiledb::sm {

std::function<bool(const Range&, const Range&)> label_upper_bound_greater_than(
    Datatype type) {
  switch (type) {
    case Datatype::INT8:
      return upper_bound_greater_than<int8_t>;
    case Datatype::UINT8:
      return upper_bound_greater_than<uint8_t>;
    case Datatype::INT16:
      return upper_bound_greater_than<int16_t>;
    case Datatype::UINT16:
      return upper_bound_greater_than<uint16_t>;
    case Datatype::INT32:
      return upper_bound_greater_than<int32_t>;
    case Datatype::UINT32:
      return upper_bound_greater_than<uint32_t>;
    case Datatype::INT64:
      return upper_bound_greater_than<int64_t>;
    case Datatype::UINT64:
      return upper_bound_greater_than<uint64_t>;
    case Datatype::FLOAT32:
      return upper_bound_greater_than<float>;
    case Datatype::FLOAT64:
      return upper_bound_greater_than<double>;
    case Datatype::DATETIME_YEAR:
    case Datatype::DATETIME_MONTH:
    case Datatype::DATETIME_WEEK:
    case Datatype::DATETIME_DAY:
    case Datatype::DATETIME_HR:
    case Datatype::DATETIME_MIN:
    case Datatype::DATETIME_SEC:
    case Datatype::DATETIME_MS:
    case Datatype::DATETIME_US:
    case Datatype::DATETIME_NS:
    case Datatype::DATETIME_PS:
    case Datatype::DATETIME_FS:
    case Datatype::DATETIME_AS:
    case Datatype::TIME_HR:
    case Datatype::TIME_MIN:
    case Datatype::TIME_SEC:
    case Datatype::TIME_MS:
    case Datatype::TIME_US:
    case Datatype::TIME_NS:
    case Datatype::TIME_PS:
    case Datatype::TIME_FS:
    case Datatype::TIME_AS:
      return upper_bound_greater_than<int64_t>;
    default:
      throw std::invalid_argument(
          "Index datatype '" + datatype_str(type) +
          "' not supported for range queries.");
  }
}

std::function<void(Range& range)> index_range_fixer(
    LabelOrder order, Datatype type) {
  if (order != LabelOrder::INCREASING_LABELS &&
      order != LabelOrder::DECREASING_LABELS)
    throw std::invalid_argument(
        "Support for reading ranges is only implemented for increasing and "
        "decreasing labels.");
  switch (type) {
    case Datatype::INT8:
      return order == LabelOrder::INCREASING_LABELS ?
                 decrease_upper_bound<int8_t> :
                 increase_upper_bound<int8_t>;
    case Datatype::UINT8:
      return order == LabelOrder::INCREASING_LABELS ?
                 decrease_upper_bound<uint8_t> :
                 increase_upper_bound<uint8_t>;
    case Datatype::INT16:
      return order == LabelOrder::INCREASING_LABELS ?
                 decrease_upper_bound<int16_t> :
                 increase_upper_bound<int16_t>;
    case Datatype::UINT16:
      return order == LabelOrder::INCREASING_LABELS ?
                 decrease_upper_bound<uint16_t> :
                 increase_upper_bound<uint16_t>;
    case Datatype::INT32:
      return order == LabelOrder::INCREASING_LABELS ?
                 decrease_upper_bound<int32_t> :
                 increase_upper_bound<int32_t>;
    case Datatype::UINT32:
      return order == LabelOrder::INCREASING_LABELS ?
                 decrease_upper_bound<uint32_t> :
                 increase_upper_bound<uint32_t>;
    case Datatype::INT64:
      return order == LabelOrder::INCREASING_LABELS ?
                 decrease_upper_bound<int64_t> :
                 increase_upper_bound<int64_t>;
    case Datatype::UINT64:
      return order == LabelOrder::INCREASING_LABELS ?
                 decrease_upper_bound<uint64_t> :
                 increase_upper_bound<uint64_t>;
    case Datatype::DATETIME_YEAR:
    case Datatype::DATETIME_MONTH:
    case Datatype::DATETIME_WEEK:
    case Datatype::DATETIME_DAY:
    case Datatype::DATETIME_HR:
    case Datatype::DATETIME_MIN:
    case Datatype::DATETIME_SEC:
    case Datatype::DATETIME_MS:
    case Datatype::DATETIME_US:
    case Datatype::DATETIME_NS:
    case Datatype::DATETIME_PS:
    case Datatype::DATETIME_FS:
    case Datatype::DATETIME_AS:
    case Datatype::TIME_HR:
    case Datatype::TIME_MIN:
    case Datatype::TIME_SEC:
    case Datatype::TIME_MS:
    case Datatype::TIME_US:
    case Datatype::TIME_NS:
    case Datatype::TIME_PS:
    case Datatype::TIME_FS:
    case Datatype::TIME_AS:
      return order == LabelOrder::INCREASING_LABELS ?
                 decrease_upper_bound<int64_t> :
                 increase_upper_bound<int64_t>;
    default:
      throw std::invalid_argument(
          "Index datatype '" + datatype_str(type) +
          "' not supported for range queries.");
  }
}

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
    , upper_bound_query_{storage_manager, dimension_label->labelled_array()}
    , has_extra_range_element_{label_upper_bound_greater_than(
          dimension_label->label_dimension()->type())}
    , fix_index_range_{index_range_fixer(
          order_, dimension_label->index_attribute()->type())} {
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
  switch (order_) {
    case (LabelOrder::INCREASING_LABELS):
      throw_if_not_ok(lower_bound_query_.set_data_buffer(
          index_name,
          const_cast<void*>(computed_index_range_.start_fixed()),
          &index_data_size_));
      throw_if_not_ok(upper_bound_query_.set_data_buffer(
          index_name,
          const_cast<void*>(computed_index_range_.end_fixed()),
          &index_data_size_));
      break;
    case (LabelOrder::DECREASING_LABELS):
      throw_if_not_ok(upper_bound_query_.set_data_buffer(
          index_name,
          const_cast<void*>(computed_index_range_.start_fixed()),
          &index_data_size_));
      throw_if_not_ok(lower_bound_query_.set_data_buffer(
          index_name,
          const_cast<void*>(computed_index_range_.end_fixed()),
          &index_data_size_));
      break;
    default:
      throw std::invalid_argument(
          "Support for reading ranges is only implemented for increasing and "
          "decreasing labels.");
  }
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
  // This will compare the upper bound of the label query and fix the computed
  // index range the values do no match.
  //
  // For increasing labels, if the computed label upper bound is greater than
  // the input range, we need to decrease the range to the previous value.
  //
  // For decreasing labels, if the computed label upper bound is greater than
  // the input range, we need to increase the range to the next value.
  if (has_extra_range_element_(computed_label_range_, input_label_range_))
    fix_index_range_(computed_index_range_);
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
