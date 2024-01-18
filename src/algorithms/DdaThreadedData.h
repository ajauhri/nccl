// (c) Meta Platforms, Inc. and affiliates. Confidential and proprietary.
#pragma once

#include <stdint.h>
#include <mutex>
#include <unordered_map>
#include <unordered_set>
#include <memory>

namespace nccl {
namespace algorithms {

/**
 * Threadsafe Singleton class that captures threaded-data shared
 * among multiple threads
 */
class DdaThreadedData {
 public:
  // get underneath singleton instance
  static DdaThreadedData* get();

  void clear();
  void clear(uint64_t commHash);

  bool registerRank(uint64_t commHash, int rank);
  bool unregisterRank(uint64_t commHash, int rank);
  bool hasRank(uint64_t commHash, int rank);
  size_t numRanks(uint64_t commHash);

  ~DdaThreadedData();

 private:
  DdaThreadedData();

  // delete copy constructors
  DdaThreadedData(const DdaThreadedData&) = delete;
  DdaThreadedData& operator=(const DdaThreadedData&) = delete;

  // static instance/mutex
  static std::unique_ptr<DdaThreadedData> data_;
  static std::mutex instanceMutex_;

  // internal data members
  std::unordered_map<uint64_t, std::unordered_set<int>> commToRanks_;
};

} // namespace algorithms
} // namespace nccl
