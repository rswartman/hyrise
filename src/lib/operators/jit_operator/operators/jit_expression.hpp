#pragma once

#include <boost/preprocessor/seq/for_each.hpp>
#include <boost/preprocessor/seq.hpp>
#include <boost/preprocessor/seq/enum.hpp>

#include <variant>

#include "all_type_variant.hpp"

#include "operators/jit_operator/jit_types.hpp"

namespace opossum {

class BaseJitSegmentReaderWrapper;

/* JitExpression represents a SQL expression - this includes arithmetic and logical expressions as well as comparisons.
 * Each JitExpression works on JitTupleValues and is structured as a binary tree. All leaves of that tree reference a tuple
 * value in the JitRuntimeContext and are of type JitExpressionType::Column - independent of whether these values actually
 * came from a column, are literal values or placeholders.
 * Each JitExpression can compute its value and stores it in its assigned result JitTupleValue. JitExpressions are also
 * able to compute the data type of the expression they represent.
 *
 * Using AbstractExpression as a base class for JitExpressions seems like a logical choice. However, AbstractExpression
 * adds a lot of bloat during code specialization. We thus decided against deriving from it here.
 */
using JitAllTypeVariant = std::variant<BOOST_PP_SEQ_ENUM(BOOST_PP_SEQ_TRANSFORM(GET_ELEM, 0, JIT_DATA_TYPE_INFO))>;
class JitExpression {
 public:
  explicit JitExpression(const JitTupleValue& tuple_value, const AllTypeVariant& variant = AllTypeVariant(), const bool use_value_id = false);
  JitExpression(const std::shared_ptr<const JitExpression>& child, const JitExpressionType expression_type,
                const size_t result_tuple_index);
  JitExpression(const std::shared_ptr<const JitExpression>& left_child, const JitExpressionType expression_type,
                const std::shared_ptr<const JitExpression>& right_child, const size_t result_tuple_index);

  std::string to_string() const;

  JitExpressionType expression_type() const { return _expression_type; }
  std::shared_ptr<const JitExpression> left_child() const { return _left_child; }
  std::shared_ptr<const JitExpression> right_child() const { return _right_child; }
  const JitTupleValue& result() const { return _result_value; }

  /* Triggers the (recursive) computation of the value represented by this expression.
   * The result is not returned, but stored in the _result_value tuple value.
   * The compute() function MUST be called before the result value in the runtime tuple can safely be accessed through
   * the _result_value.
   * The _result_value itself, however, can safely be passed around before (e.g. by calling the result() function),
   * since it only abstractly represents the result slot in the runtime tuple.
   */
  void compute(JitRuntimeContext& context) const;

  template <typename T>
  Value<T> compute_and_get(JitRuntimeContext& context) const;

  void set_load_column(const std::shared_ptr<BaseJitSegmentReaderWrapper> input_segment_wrapper) const {
#if JIT_LAZY_LOAD
    const_cast<bool&>(_load_column) = true;
    const_cast<std::shared_ptr<BaseJitSegmentReaderWrapper>&>(_input_segment_wrapper) = input_segment_wrapper;
#endif
  }

  void set_expression_type(const JitExpressionType expression_type) const {
    const_cast<JitExpressionType&>(_expression_type) = expression_type;
  }

 private:
  std::pair<const DataType, const bool> _compute_result_type();

  const std::shared_ptr<const JitExpression> _left_child;
  const std::shared_ptr<const JitExpression> _right_child;
  const JitExpressionType _expression_type;
  const JitTupleValue _result_value;
  const JitAllTypeVariant _variant;
  const bool _is_null = false;
  const bool _use_value_id = false;
#if JIT_LAZY_LOAD
  const bool _load_column = false;
  const std::shared_ptr<BaseJitSegmentReaderWrapper> _input_segment_wrapper;
#endif
};

}  // namespace opossum
