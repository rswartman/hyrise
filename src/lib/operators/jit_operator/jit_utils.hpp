#pragma once

#include "all_type_variant.hpp"
#include "operators/jit_operator/jit_types.hpp"
#include "storage/mvcc_columns.hpp"
#include "types.hpp"

namespace opossum {

struct Mvcc {
  TransactionID row_tid;
  CommitID begin_cid;
  CommitID end_cid;
};

class JitAggregate;

namespace no_inline {

__attribute__((noinline)) Mvcc unpack_mvcc(const MvccColumns& columns, ChunkOffset chunk_offset);

__attribute__((noinline)) void compute_binary(const JitTupleValue& lhs, const JitExpressionType expression_type,
                                              const JitTupleValue& rhs, const JitTupleValue& result,
                                              JitRuntimeContext& context);

__attribute__((noinline)) void jit_aggregate_consume(const JitAggregate& jit_aggregate, JitRuntimeContext& context);

}  // namespace no_inline

}  // namespace opossum
