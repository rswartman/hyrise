#pragma once

#include "storage/table.hpp"

namespace opossum {

/**
 * Indicates whether the comparison of two tables should happen order sensitive (Yes) or whether it should just be
 * checked whether both tables contain the same rows, independent of order.
 */
enum class OrderSensitivity { Yes, No };

/**
 * "Strict" enforces that both tables have precisely the same column types, "Lenient" allows float instead of double, double
 * instead of float, long instead of int, int instead of long
 */
enum class TypeCmpMode { Strict, Lenient };

/**
 * When comparing tables generated by Hyrise to those generated by, e.g. SQLite, minor differences are to be expected
 * (since sqlite uses double for arithmetics, Hyrise might use float) so for large numbers
 * FloatComparisonMode::RelativeDifference is better since it allows derivation independent of the absolute value.
 * When checking against manually generated tables, FloatComparisonMode::AbsoluteDifference is the better choice.
 */
enum class FloatComparisonMode { RelativeDifference, AbsoluteDifference };

bool check_segment_equal(const std::shared_ptr<BaseSegment>& segment_to_test,
                         const std::shared_ptr<BaseSegment>& expected_segment,
                         OrderSensitivity order_sensitivity = OrderSensitivity::Yes,
                         TypeCmpMode type_cmp_mode = TypeCmpMode::Strict,
                         FloatComparisonMode float_comparison_mode = FloatComparisonMode::AbsoluteDifference);

// Compares two tables for equality
bool check_table_equal(const std::shared_ptr<const Table>& opossum_table,
                       const std::shared_ptr<const Table>& expected_table, OrderSensitivity order_sensitivity,
                       TypeCmpMode type_cmp_mode, FloatComparisonMode float_comparison_mode);

}  // namespace opossum
