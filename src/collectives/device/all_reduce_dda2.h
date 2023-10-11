// (c) Meta Platforms, Inc. and affiliates. Confidential and proprietary.

#pragma once

#define DECL_DDA2_FUNC_NRANKS(T, NRANKS)                                \
  template __global__ void ncclKernel_AllReduce_DDA2_Flat<T, NRANKS>(   \
    uintptr_t barrierFlag,                                              \
    DdaDeviceState* devStates,                                          \
    int rank,                                                           \
    const T* sendbuff,                                                  \
    T* recvbuff,                                                        \
    size_t count);

#define DECL_DDA2_FUNC(T)      \
  DECL_DDA2_FUNC_NRANKS(T, 2); \
  DECL_DDA2_FUNC_NRANKS(T, 4); \
  DECL_DDA2_FUNC_NRANKS(T, 8); \
  DECL_DDA2_FUNC_NRANKS(T, 16)
