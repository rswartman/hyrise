#pragma once

#include <memory>

#include "cost_estimation_cache.hpp"
#include "statistics/cardinality_estimation_cache.hpp"
#include "types.hpp"

namespace opossum {

class AbstractLQPNode;
class TableStatistics;
class AbstractCardinalityEstimator;

/**
 * Base class of an algorithm that predicts Cost for operators and plans.
 */
class AbstractCostEstimator {
 public:
  explicit AbstractCostEstimator(const std::shared_ptr<AbstractCardinalityEstimator>& cardinality_estimator);
  virtual ~AbstractCostEstimator() = default;

  /**
   * Estimate the Cost of a (sub-)plan.
   * If `cost_estimation_cache.cost_by_lqp` is enabled:
   *     Tries to obtain subplan costs from `cost_estimation_cache`. Stores the cost for @param lqp in the
   *     `cost_estimation_cache` cache
   * @return The estimated cost of an @param lqp. Calls estimate_node_cost() on each individual node of the plan.
   */
  Cost estimate_plan_cost(const std::shared_ptr<AbstractLQPNode>& lqp) const;

  /**
   * @return the estimated cost of a single node. The `cost_estimation_cache` will not be used
   */
  virtual Cost estimate_node_cost(const std::shared_ptr<AbstractLQPNode>& node) const = 0;

  /**
   * @return a new instance of this estimator with a new instance of the wrapped cardinality estimator, both with mint
   *         caches. Used so that caching guarantees can be enabled on the returned estimator.
   */
  virtual std::shared_ptr<AbstractCostEstimator> new_instance() const = 0;

  /**
   * Promises to the CostEstimator (and underlying CardinalityEstimator) that it will only be used to estimate bottom-up
   * constructed plans. That is, the Cost/Cardinality of a node, once constructed, never changes.
   * This enables the usage of a <lqp-ptr> -> <cost> cache.
   */
  void guarantee_bottom_up_construction();

  std::shared_ptr<AbstractCardinalityEstimator> cardinality_estimator;

  mutable CostEstimationCache cost_estimation_cache;
};

}  // namespace opossum
