#include <memory>

#include "base_test.hpp"
#include "gtest/gtest.h"

#include "concurrency/transaction_context.hpp"
#include "operators/delete.hpp"
#include "operators/get_table.hpp"
#include "operators/validate.hpp"
#include "storage/chunk.hpp"
#include "storage/storage_manager.hpp"
#include "storage/table.hpp"

namespace opossum {

class OperatorsGetTableTest : public BaseTest {
 protected:
  void SetUp() override {
    StorageManager::get().add_table("tableWithValues", load_table("resources/test_data/tbl/int_int_float.tbl", 1u));
  }
};

TEST_F(OperatorsGetTableTest, GetOutput) {
  auto get_table = std::make_shared<GetTable>("tableWithValues");
  get_table->execute();

  EXPECT_TABLE_EQ_UNORDERED(get_table->get_output(), load_table("resources/test_data/tbl/int_int_float.tbl", 1u));
}

TEST_F(OperatorsGetTableTest, ThrowsUnknownTableName) {
  auto get_table = std::make_shared<GetTable>("anUglyTestTable");

  EXPECT_THROW(get_table->execute(), std::exception) << "Should throw unknown table name exception";
}

TEST_F(OperatorsGetTableTest, OperatorName) {
  auto get_table = std::make_shared<opossum::GetTable>("tableWithValues");

  EXPECT_EQ(get_table->name(), "GetTable");
}

TEST_F(OperatorsGetTableTest, ExcludedChunks) {
  auto get_table = std::make_shared<opossum::GetTable>("tableWithValues");

  get_table->set_excluded_chunk_ids({ChunkID(0), ChunkID(2)});
  get_table->execute();

  auto original_table = StorageManager::get().get_table("tableWithValues");
  auto table = get_table->get_output();
  EXPECT_EQ(table->chunk_count(), ChunkID(2));
  EXPECT_EQ(table->get_value<int>(ColumnID(0), 0u), original_table->get_value<int>(ColumnID(0), 1u));
  EXPECT_EQ(table->get_value<int>(ColumnID(0), 1u), original_table->get_value<int>(ColumnID(0), 3u));
}

TEST_F(OperatorsGetTableTest, ExcludedColumns) {
  auto get_table = std::make_shared<opossum::GetTable>("tableWithValues");

  get_table->set_excluded_column_ids({ColumnID(1)});
  get_table->execute();

  auto table = get_table->get_output();
  EXPECT_EQ(table->column_count(), 2u);
  EXPECT_EQ(table->get_value<int>(ColumnID{0}, 0u), 9);
  EXPECT_EQ(table->get_value<float>(ColumnID{1}, 1u), 10.5f);
}

TEST_F(OperatorsGetTableTest, ExcludedColumnsAndChunks) {
  auto get_table = std::make_shared<opossum::GetTable>("tableWithValues");

  get_table->set_excluded_column_ids({ColumnID{0}});
  get_table->set_excluded_chunk_ids({ChunkID{0}, ChunkID{2}});
  get_table->execute();

  auto table = get_table->get_output();
  EXPECT_EQ(table->column_count(), 2u);
  EXPECT_EQ(table->get_value<int>(ColumnID{0}, 0u), 10);
  EXPECT_EQ(table->get_value<float>(ColumnID{1}, 0u), 10.5f);
  EXPECT_EQ(table->get_value<float>(ColumnID{1}, 1u), 9.5f);
}

TEST_F(OperatorsGetTableTest, ExcludeCleanedUpChunk) {
  auto get_table = std::make_shared<opossum::GetTable>("tableWithValues");
  auto context = std::make_shared<TransactionContext>(1u, 3u);

  auto original_table = StorageManager::get().get_table("tableWithValues");
  auto chunk = original_table->get_chunk(ChunkID{0});

  chunk->set_cleanup_commit_id(CommitID{2u});

  get_table->set_transaction_context(context);
  get_table->execute();

  auto table = get_table->get_output();
  EXPECT_EQ(original_table->chunk_count(), 4);
  EXPECT_EQ(table->chunk_count(), 3);
}

TEST_F(OperatorsGetTableTest, ExcludePhysicallyDeletedChunks) {
  auto original_table = StorageManager::get().get_table("tableWithValues");
  EXPECT_EQ(original_table->chunk_count(), 4);

  // Invalidate all records to be able to call remove_chunk()
  auto context = std::make_shared<TransactionContext>(1u, 1u);
  auto get_table = std::make_shared<opossum::GetTable>("tableWithValues");
  get_table->set_transaction_context(context);
  get_table->execute();
  EXPECT_EQ(get_table->get_output()->chunk_count(), 4);
  auto vt = std::make_shared<opossum::Validate>(get_table);
  vt->set_transaction_context(context);
  vt->execute();

  // Delete all rows from table so calling original_table->remove_chunk() below is legal
  auto delete_all = std::make_shared<opossum::Delete>(vt);
  delete_all->set_transaction_context(context);
  delete_all->execute();
  EXPECT_FALSE(delete_all->execute_failed());
  context->commit();

  /* 
   * Not setting cleanup commit ids is intentional,
   * because we delete the chunks manually for this test.
  */

  // Delete chunks physically
  original_table->remove_chunk(ChunkID{0});
  EXPECT_FALSE(original_table->get_chunk(ChunkID{0}));
  original_table->remove_chunk(ChunkID{2});
  EXPECT_FALSE(original_table->get_chunk(ChunkID{2}));

  // Check GetTable filtering
  auto context2 = std::make_shared<TransactionContext>(2u, 1u);
  auto get_table_2 = std::make_shared<opossum::GetTable>("tableWithValues");
  get_table_2->set_transaction_context(context2);

  get_table_2->execute();
  EXPECT_EQ(get_table_2->get_output()->chunk_count(), 2);
}

TEST_F(OperatorsGetTableTest, ExcludedChunksCombined) {
  // 1. --- Physical deletion of a chunk
  auto original_table = StorageManager::get().get_table("tableWithValues");
  EXPECT_EQ(original_table->chunk_count(), 4);

  // Invalidate all records to be able to call remove_chunk()
  auto context = std::make_shared<TransactionContext>(1u, 1u);
  auto get_table = std::make_shared<opossum::GetTable>("tableWithValues");
  get_table->set_transaction_context(context);
  get_table->execute();
  EXPECT_EQ(get_table->get_output()->chunk_count(), 4);
  auto vt = std::make_shared<opossum::Validate>(get_table);
  vt->set_transaction_context(context);
  vt->execute();

  // Delete all rows from table so calling original_table->remove_chunk() below is legal
  auto delete_all = std::make_shared<opossum::Delete>(vt);
  delete_all->set_transaction_context(context);
  delete_all->execute();
  EXPECT_FALSE(delete_all->execute_failed());
  context->commit();

  /* 
   * Not setting cleanup commit ids is intentional,
   * because we delete the chunks manually for this test.
  */

  // Delete chunks physically
  original_table->remove_chunk(ChunkID{2});
  EXPECT_FALSE(original_table->get_chunk(ChunkID{2}));

  // 2. --- Logical deletion of a chunk
  auto get_table_2 = std::make_shared<opossum::GetTable>("tableWithValues");

  auto context2 = std::make_shared<TransactionContext>(1u, 3u);

  auto modified_table = StorageManager::get().get_table("tableWithValues");
  auto chunk = modified_table->get_chunk(ChunkID{1});

  chunk->set_cleanup_commit_id(CommitID{2u});

  // 3. --- Set excluded chunk ids
  get_table_2->set_excluded_chunk_ids({ChunkID(0u)});
  get_table_2->set_transaction_context(context2);

  get_table_2->execute();
  EXPECT_EQ(get_table_2->get_output()->chunk_count(), 1);
}
}  // namespace opossum
