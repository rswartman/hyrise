#pragma once

#include <map>
#include <memory>

#include "abstract_join_ordering_algorithm.hpp"
#include "join_graph_edge.hpp"

namespace opossum {

class AbstractLQPNode;
class AbstractCostEstimator;
class JoinGraph;

class GreedyOperatorOrdering : public AbstractJoinOrderingAlgorithm {
 public:
  explicit GreedyOperatorOrdering(const std::shared_ptr<AbstractCostEstimator>& cost_estimator);

  /**
   * @param join_graph      A JoinGraph for a part of an LQP with further subplans as vertices. GreedyOperatorOrdering
   *                        is only applied to this particular JoinGraph and doesn't modify the subplans in the
   *                        vertices.
   * @return                An LQP consisting of
   *                         * the operations from the JoinGraph in a greedy order, trying to minimize intermediate
   *                           cardinalities
   *                         * the subplans from the vertices below them
   */
  std::shared_ptr<AbstractLQPNode> operator()(const JoinGraph& join_graph);

 private:
  std::shared_ptr<AbstractLQPNode> _build_plan_for_edge(const std::map<JoinGraphVertexSet, std::shared_ptr<AbstractLQPNode>>& vertex_clusters,
                                                                               const JoinGraphEdge& edge) const;
};

}  // namespace opossum
