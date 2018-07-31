#include "jit_compute.hpp"

#include "../jit_operations.hpp"
#include "constant_mappings.hpp"
#include "jit_read_tuples.hpp"

namespace opossum {

JitExpression::JitExpression(const JitTupleValue& tuple_value)
    : _expression_type{JitExpressionType::Column},
      _result_value{tuple_value},
      _load_column{false},
      _input_column_index{} {}

JitExpression::JitExpression(const std::shared_ptr<const JitExpression>& child, const JitExpressionType expression_type,
                             const size_t result_tuple_index)
    : _left_child{child},
      _expression_type{expression_type},
      _result_value{JitTupleValue(_compute_result_type(), result_tuple_index)},
      _load_column{false},
      _input_column_index{} {}

JitExpression::JitExpression(const std::shared_ptr<const JitExpression>& left_child,
                             const JitExpressionType expression_type,
                             const std::shared_ptr<const JitExpression>& right_child, const size_t result_tuple_index)
    : _left_child{left_child},
      _right_child{right_child},
      _expression_type{expression_type},
      _result_value{JitTupleValue(_compute_result_type(), result_tuple_index)},
      _load_column{false},
      _input_column_index{} {}

std::string JitExpression::to_string() const {
  if (_expression_type == JitExpressionType::Column) {
    std::string load_column = _load_column ? " (Using input reader #" + std::to_string(_input_column_index) + ")" : "";
    return "x" + std::to_string(_result_value.tuple_index()) + load_column;
  }

  const auto left = _left_child->to_string() + " ";
  const auto right = _right_child ? _right_child->to_string() + " " : "";
  return "(" + left + jit_expression_type_to_string.left.at(_expression_type) + " " + right + ")";
}

void JitExpression::compute(JitRuntimeContext& context) const {
  // We are dealing with an already computed value here, so there is nothing to do.
  if (_expression_type == JitExpressionType::Column) {
    if (_load_column) context.inputs[_input_column_index]->read_value(context);
    return;
  }

  _left_child->compute(context);

  if (!jit_expression_is_binary(_expression_type)) {
    switch (_expression_type) {
      case JitExpressionType::Not:
        jit_not(_left_child->result(), _result_value, context);
        break;
      case JitExpressionType::IsNull:
        jit_is_null(_left_child->result(), _result_value, context);
        break;
      case JitExpressionType::IsNotNull:
        jit_is_not_null(_left_child->result(), _result_value, context);
        break;
      default:
        Fail("Expression type is not supported.");
    }
    return;
  }

  // Check, whether right side can be pruned
  // AND: false and true/false/null = false
  // OR:  true  or  true/false/null = true
  if (_expression_type == JitExpressionType::And && !_left_child->result().is_null(context) &&
      !_left_child->result().get<bool>(context)) {
    return jit_and(_left_child->result(), _right_child->result(), _result_value, context, true);
  } else if (_expression_type == JitExpressionType::Or && !_left_child->result().is_null(context) &&
             _left_child->result().get<bool>(context)) {
    return jit_or(_left_child->result(), _right_child->result(), _result_value, context, true);
  }

  _right_child->compute(context);

  // Hack as strings cannot be currently specialised
  if (_result_value.data_type() == DataType::Bool && _left_child->result().data_type() == DataType::String &&
      _right_child->result().data_type() == DataType::String) {
    no_inline::compute_binary(_left_child->result(), _expression_type, _right_child->result(), _result_value, context);
  } else {
    compute_binary(_left_child->result(), _expression_type, _right_child->result(), _result_value, context);
  }
}

void compute_binary(const JitTupleValue& lhs, const JitExpressionType expression_type, const JitTupleValue& rhs,
                    const JitTupleValue& result, JitRuntimeContext& context) {
  switch (expression_type) {
    case JitExpressionType::Addition:
      jit_compute(jit_addition, lhs, rhs, result, context);
      break;
    case JitExpressionType::Subtraction:
      jit_compute(jit_subtraction, lhs, rhs, result, context);
      break;
    case JitExpressionType::Multiplication:
      jit_compute(jit_multiplication, lhs, rhs, result, context);
      break;
    case JitExpressionType::Division:
      jit_compute(jit_division, lhs, rhs, result, context);
      break;
    case JitExpressionType::Modulo:
      jit_compute(jit_modulo, lhs, rhs, result, context);
      break;
    case JitExpressionType::Power:
      jit_compute(jit_power, lhs, rhs, result, context);
      break;

    case JitExpressionType::Equals:
      jit_compute(jit_equals, lhs, rhs, result, context);
      break;
    case JitExpressionType::NotEquals:
      jit_compute(jit_not_equals, lhs, rhs, result, context);
      break;
    case JitExpressionType::GreaterThan:
      jit_compute(jit_greater_than, lhs, rhs, result, context);
      break;
    case JitExpressionType::GreaterThanEquals:
      jit_compute(jit_greater_than_equals, lhs, rhs, result, context);
      break;
    case JitExpressionType::LessThan:
      jit_compute(jit_less_than, lhs, rhs, result, context);
      break;
    case JitExpressionType::LessThanEquals:
      jit_compute(jit_less_than_equals, lhs, rhs, result, context);
      break;
    case JitExpressionType::Like:
      jit_compute(jit_like, lhs, rhs, result, context);
      break;
    case JitExpressionType::NotLike:
      jit_compute(jit_not_like, lhs, rhs, result, context);
      break;

    case JitExpressionType::And:
      jit_and(lhs, rhs, result, context, false);
      break;
    case JitExpressionType::Or:
      jit_or(lhs, rhs, result, context, false);
      break;
    default:
      Fail("Expression type is not supported.");
  }
}

std::pair<const DataType, const bool> JitExpression::_compute_result_type() {
  if (!jit_expression_is_binary(_expression_type)) {
    switch (_expression_type) {
      case JitExpressionType::Not:
        return std::make_pair(DataType::Bool, _left_child->result().is_nullable());
      case JitExpressionType::IsNull:
      case JitExpressionType::IsNotNull:
        return std::make_pair(DataType::Bool, false);
      default:
        Fail("Expression type not supported.");
    }
  }

  DataType result_data_type;
  switch (_expression_type) {
    case JitExpressionType::Addition:
      result_data_type =
          jit_compute_type(jit_addition, _left_child->result().data_type(), _right_child->result().data_type());
      break;
    case JitExpressionType::Subtraction:
      result_data_type =
          jit_compute_type(jit_subtraction, _left_child->result().data_type(), _right_child->result().data_type());
      break;
    case JitExpressionType::Multiplication:
      result_data_type =
          jit_compute_type(jit_multiplication, _left_child->result().data_type(), _right_child->result().data_type());
      break;
    case JitExpressionType::Division:
      result_data_type =
          jit_compute_type(jit_division, _left_child->result().data_type(), _right_child->result().data_type());
      break;
    case JitExpressionType::Modulo:
      result_data_type =
          jit_compute_type(jit_modulo, _left_child->result().data_type(), _right_child->result().data_type());
      break;
    case JitExpressionType::Power:
      result_data_type =
          jit_compute_type(jit_power, _left_child->result().data_type(), _right_child->result().data_type());
      break;
    case JitExpressionType::Equals:
    case JitExpressionType::NotEquals:
    case JitExpressionType::GreaterThan:
    case JitExpressionType::GreaterThanEquals:
    case JitExpressionType::LessThan:
    case JitExpressionType::LessThanEquals:
    case JitExpressionType::Like:
    case JitExpressionType::NotLike:
    case JitExpressionType::And:
    case JitExpressionType::Or:
      result_data_type = DataType::Bool;
      break;
    default:
      Fail("Expression type not supported.");
  }

  return std::make_pair(result_data_type, _left_child->result().is_nullable() || _right_child->result().is_nullable());
}

}  // namespace opossum
