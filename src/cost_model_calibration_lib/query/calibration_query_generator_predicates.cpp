#include "calibration_query_generator_predicates.hpp"

#include <boost/algorithm/string/join.hpp>
#include <boost/format.hpp>
#include <experimental/iterator>

#include <algorithm>
#include <iostream>
#include <iterator>
#include <random>
#include <vector>

#include "../configuration/calibration_column_specification.hpp"
#include "utils/assert.hpp"

namespace opossum {

const std::optional<std::string> CalibrationQueryGeneratorPredicates::generate_predicates(
    const PredicateGeneratorFunctor& predicate_generator,
    const std::map<std::string, CalibrationColumnSpecification>& column_definitions,
    const std::string& column_name_prefix) {
  std::random_device random_device;
  std::mt19937 engine{random_device()};

  size_t number_of_predicates = 2;
  std::vector<std::string> predicates {};

  auto column_definitions_copy = column_definitions;

  for (size_t i = 0; i < number_of_predicates; i++) {
    // select scan column
    std::uniform_int_distribution<u_int64_t> filter_column_dist(0, column_definitions_copy.size() - 1);
    auto filter_column = std::next(column_definitions_copy.begin(), filter_column_dist(engine));
    auto predicate = predicate_generator(*filter_column, column_definitions_copy, column_name_prefix);

    if (!predicate) continue;
    predicates.push_back(*predicate);

    // Avoid filtering on the same column twice
    column_definitions_copy.erase(filter_column->first);
  }

  if (predicates.empty()) return {};
  return boost::algorithm::join(predicates, " AND ");
}

const std::optional<std::string> CalibrationQueryGeneratorPredicates::_generate_between(
    const BetweenPredicateGeneratorFunctor& between_predicate_generator,
    const std::pair<std::string, CalibrationColumnSpecification>& filter_column,
    const std::map<std::string, CalibrationColumnSpecification>& column_definitions,
    const std::string& column_name_prefix) {
  auto between_predicate_template = "%1% BETWEEN %2% AND %3%";
  auto filter_column_name = column_name_prefix + filter_column.first;
  const auto& between_values = between_predicate_generator(filter_column, column_definitions, column_name_prefix);

  if (!between_values) return {};

  return boost::str(boost::format(between_predicate_template) % filter_column_name % between_values->first %
                    between_values->second);
}

const std::optional<std::string> CalibrationQueryGeneratorPredicates::generate_between_predicate_value(
    const std::pair<std::string, CalibrationColumnSpecification>& filter_column,
    const std::map<std::string, CalibrationColumnSpecification>& column_definitions,
    const std::string& column_name_prefix) {
  const auto& between_predicate_value =
      [](const std::pair<std::string, CalibrationColumnSpecification>& filter_column,
         const std::map<std::string, CalibrationColumnSpecification>& column_definitions,
         const std::string& column_name_prefix) {
        auto first_filter_column_value =
            CalibrationQueryGeneratorPredicates::_generate_table_scan_predicate_value(filter_column.second);
        auto second_filter_column_value =
            CalibrationQueryGeneratorPredicates::_generate_table_scan_predicate_value(filter_column.second);

        if (first_filter_column_value < second_filter_column_value) {
          return std::make_pair(first_filter_column_value, second_filter_column_value);
        }
        return std::make_pair(second_filter_column_value, first_filter_column_value);
      };

  return _generate_between(between_predicate_value, filter_column, column_definitions, column_name_prefix);
}

const std::optional<std::string> CalibrationQueryGeneratorPredicates::generate_between_predicate_column(
    const std::pair<std::string, CalibrationColumnSpecification>& filter_column,
    const std::map<std::string, CalibrationColumnSpecification>& column_definitions,
    const std::string& column_name_prefix) {
  const auto& between_predicate_column =
      [](const std::pair<std::string, CalibrationColumnSpecification>& filter_column,
         const std::map<std::string, CalibrationColumnSpecification>& column_definitions,
         const std::string& column_name_prefix) -> std::optional<std::pair<std::string, std::string>> {
        std::random_device random_device;
        std::mt19937 engine{random_device()};

        auto first_filter_column_value = _generate_table_scan_predicate_value(filter_column.second);
        auto second_filter_column_value = _generate_table_scan_predicate_value(filter_column.second);

        std::vector<std::pair<std::string, CalibrationColumnSpecification>> v(column_definitions.begin(),
                                                                              column_definitions.end());
        std::shuffle(v.begin(), v.end(), engine);

        std::optional<std::pair<std::string, CalibrationColumnSpecification>> second_column;
        for (const auto& potential_second_column : v) {
          if (potential_second_column.first == filter_column.first) continue;
          if (potential_second_column.second.type != filter_column.second.type) continue;
          second_column = potential_second_column;
        }

        std::optional<std::pair<std::string, CalibrationColumnSpecification>> third_column;
        if (second_column) {
          for (const auto& potential_third_column : v) {
            const auto& potential_third_column_type = potential_third_column.second.type;
            if (potential_third_column.first == filter_column.first ||
                potential_third_column.first == second_column->first) {
              continue;
            }
            if (potential_third_column_type != filter_column.second.type ||
                potential_third_column_type != second_column->second.type) {
              continue;
            }

            third_column = potential_third_column;
          }
        }

        if (!second_column || !third_column) return {};

        auto second_column_name = column_name_prefix + second_column->first;
        auto third_column_name = column_name_prefix + third_column->first;

        return std::make_pair(second_column_name, third_column_name);
      };

  return _generate_between(between_predicate_column, filter_column, column_definitions, column_name_prefix);
}

const std::optional<std::string> CalibrationQueryGeneratorPredicates::_generate_column_predicate(
    const ColumnPredicateGeneratorFunctor& predicate_generator,
    const std::pair<std::string, CalibrationColumnSpecification>& filter_column,
    const std::map<std::string, CalibrationColumnSpecification>& column_definitions,
    const std::string& column_name_prefix) {
  const auto predicate_template = "%1% %2% %3%";
  const auto filter_column_name = column_name_prefix + filter_column.first;
  // We only want to measure various selectivities.
  // It shouldn't be that important whether we have Point or Range Lookups.
  // Isn't it?

  // At the same time this makes sure that the probability of having empty intermediate results is reduced.
  const auto predicate_sign = "<=";

  const auto filter_column_value = predicate_generator(filter_column, column_definitions, column_name_prefix);

  if (!filter_column_value) {
    return {};
  }

  return boost::str(boost::format(predicate_template) % filter_column_name % predicate_sign % *filter_column_value);
}

const std::optional<std::string> CalibrationQueryGeneratorPredicates::generate_predicate_column_value(
    const std::pair<std::string, CalibrationColumnSpecification>& filter_column,
    const std::map<std::string, CalibrationColumnSpecification>& column_definitions,
    const std::string& column_name_prefix) {
  const auto& filter_column_value = [](const std::pair<std::string, CalibrationColumnSpecification>& filter_column,
                                       const std::map<std::string, CalibrationColumnSpecification>& column_definitions,
                                       const std::string& column_name_prefix) {
    return _generate_table_scan_predicate_value(filter_column.second);
  };

  return _generate_column_predicate(filter_column_value, filter_column, column_definitions, column_name_prefix);
}

const std::optional<std::string> CalibrationQueryGeneratorPredicates::generate_predicate_column_column(
    const std::pair<std::string, CalibrationColumnSpecification>& filter_column,
    const std::map<std::string, CalibrationColumnSpecification>& column_definitions,
    const std::string& column_name_prefix) {
  const auto& filter_column_column = [](const std::pair<std::string, CalibrationColumnSpecification>& filter_column,
                                        const std::map<std::string, CalibrationColumnSpecification>& column_definitions,
                                        const std::string& column_name_prefix) -> std::optional<std::string> {
    std::random_device random_device;
    std::mt19937 engine{random_device()};

    std::vector<std::pair<std::string, CalibrationColumnSpecification>> v(column_definitions.begin(),
                                                                          column_definitions.end());
    std::shuffle(v.begin(), v.end(), engine);

    std::optional<std::pair<std::string, CalibrationColumnSpecification>> second_column;
    for (const auto& column : v) {
      if (column.first == filter_column.first) continue;
      if (column.second.type != filter_column.second.type) continue;

      second_column = column;
    }

    if (!second_column) {
      return std::nullopt;
    }

    return column_name_prefix + second_column->first;
  };

  return _generate_column_predicate(filter_column_column, filter_column, column_definitions, column_name_prefix);
}

const std::string CalibrationQueryGeneratorPredicates::_generate_table_scan_predicate_value(
    const CalibrationColumnSpecification& column_definition) {
  auto column_type = column_definition.type;
  std::random_device random_device;
  std::mt19937 engine{random_device()};
  std::uniform_int_distribution<u_int16_t> int_dist(0, column_definition.distinct_values - 1);
  std::uniform_real_distribution<> float_dist(0, column_definition.distinct_values - 1);
  //      std::uniform_int_distribution<> char_dist(0, UCHAR_MAX);

  // Initialize with some random seed
  u_int32_t seed = int_dist(engine);

  if (column_type == "int") return std::to_string(int_dist(engine));
  if (column_type == "string") return "'" + std::string(1, 'a' + rand_r(&seed) % 26) + "'";
  if (column_type == "float") return std::to_string(float_dist(engine));

  Fail("Unsupported data type in CalibrationQueryGenerator, found " + column_type);
}

}  // namespace opossum
