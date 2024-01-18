// (c) Meta Platforms, Inc. and affiliates. Confidential and proprietary.
#pragma once

#include "nccl.h"

namespace nccl {
namespace algorithms {

/**
 * This class defines common interface for all AllReduce Algorithms
 * subclasses are expected to provide actual implementation
 */
class AlgoAllReduce {
 public:
  virtual ncclResult_t allReduce() = 0;

  virtual ~AlgoAllReduce() {}
};

} // namespace algorithms
} // namespace nccl
