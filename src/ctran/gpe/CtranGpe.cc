// (c) Meta Platforms, Inc. and affiliates. Confidential and proprietary.

#include "CtranGpe.h"
#include <iostream>
#include "CtranGpeImpl.h"
#include "checks.h"
#include "comm.h"

OpElem::OpElem(enum opType type, ncclComm_t comm) : type(type), comm(comm) {}

OpElem::OpElem(enum opType type, cudaStream_t stream, ncclComm_t comm)
    : type(type), stream(stream), comm(comm) {}

OpElem::~OpElem() {}

CtranGpe::CtranGpe(int cudaDev) {
  this->pimpl = std::unique_ptr<Impl>(new Impl());
  this->pimpl->t =
      std::thread{CtranGpe::Impl::gpeThreadFn, this->pimpl.get(), cudaDev};
}

CtranGpe::~CtranGpe() {
  this->pimpl->terminate();
  this->pimpl->t.join();
}

ncclResult_t CtranGpe::submit(
    std::vector<std::unique_ptr<struct OpElem>> opGroup,
    opFunc func,
    KernelConfig& kernelConfig,
    const void* ncclKernel) {
  return this->pimpl->submit(
      CtranGpeCmd::TypeEnum::GRAPH_ENQUEUE,
      std::move(opGroup),
      func,
      kernelConfig,
      ncclKernel);
}
