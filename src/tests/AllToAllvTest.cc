// (c) Meta Platforms, Inc. and affiliates. Confidential and proprietary.

#include <comm.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <nccl.h>
#include <stdlib.h>
#include <cstddef>
#include "Ctran.h"
#include "checks.h"
#include "nccl_cvars.h"
#include "tests_common.cuh"

class MPIEnvironment : public ::testing::Environment {
 public:
  void SetUp() override {
    initializeMpi(0, NULL);
    // Turn off NCCL debug logging, allow user to turn on via command line
    setenv("NCCL_DEBUG", "WARN", 0);
  }
  void TearDown() override {
    finalizeMpi();
  }
  ~MPIEnvironment() override {}
};

class AllToAllvTest : public ::testing::Test {
 public:
  AllToAllvTest() = default;
  void SetUp() override {
    std::tie(this->localRank, this->globalRank, this->numRanks) = getMpiInfo();

    this->comm =
        createNcclComm(this->globalRank, this->numRanks, this->localRank);

    CUDACHECK_TEST(cudaSetDevice(this->localRank));
    CUDACHECK_TEST(cudaStreamCreate(&this->stream));
  }

  void TearDown() override {
    NCCLCHECK_TEST(ncclCommDestroy(this->comm));
    CUDACHECK_TEST(cudaStreamDestroy(this->stream));
  }

  template <typename T>
  void assignChunkValue(T* buf, size_t count, T val) {
    std::vector<T> expectedVals(count, val);
    CUDACHECKIGNORE(cudaMemcpy(
        buf, expectedVals.data(), count * sizeof(T), cudaMemcpyDefault));
  }

  template <typename T>
  int checkChunkValue(T* buf, size_t count, T val) {
    std::vector<T> observedVals(count, -1);
    CUDACHECK_TEST(cudaMemcpy(
        observedVals.data(), buf, count * sizeof(T), cudaMemcpyDefault));
    int errs = 0;
    // Use manual print rather than EXPECT_THAT to print failing location
    for (auto i = 0; i < count; ++i) {
      if (observedVals[i] != val) {
        if (errs < 10) {
          printf(
              "[%d] observedVals[%d] = %d, expectedVal = %d\n",
              this->globalRank,
              i,
              observedVals[i],
              val);
        }
        errs++;
      }
    }
    return errs;
  }

  void run(bool registFlag = false) {
#ifdef NCCL_ALLTOALLV_SUPPORTED

    // create and register buffers
    constexpr int count = 1048576;
    int *sendBuf = nullptr, *recvBuf = nullptr;
    void *sendHandle = nullptr, *recvHandle = nullptr;

    CUDACHECK_TEST(cudaMalloc(&sendBuf, count * this->numRanks * sizeof(int)));
    CUDACHECK_TEST(cudaMalloc(&recvBuf, count * this->numRanks * sizeof(int)));

    for (int r = 0; r < this->numRanks; r++) {
      int expectedVal = this->globalRank * 100 + r + 1;
      assignChunkValue(sendBuf + r * count, count, expectedVal);
      assignChunkValue(recvBuf + r * count, count, -1);
    }

    if (registFlag) {
      NCCLCHECK_TEST(ncclCommRegister(
          comm, sendBuf, count * this->numRanks * sizeof(int), &sendHandle));
      NCCLCHECK_TEST(ncclCommRegister(
          comm, recvBuf, count * this->numRanks * sizeof(int), &recvHandle));
    }

    // prepare alltoallv arguments
    std::vector<size_t> sendCounts(this->numRanks);
    std::vector<size_t> sendDispls(this->numRanks);
    std::vector<size_t> recvCounts(this->numRanks);
    std::vector<size_t> recvDispls(this->numRanks);
    for (int r = 0; r < this->numRanks; r++) {
      sendCounts[r] = r % 2 ? count : count / 2;
      sendDispls[r] = r * count;
      recvCounts[r] = this->globalRank % 2 ? count : count / 2;
      recvDispls[r] = r * count;
    }

    // run alltoallv
    auto res = ncclAllToAllv(
        sendBuf,
        sendCounts.data(),
        sendDispls.data(),
        recvBuf,
        recvCounts.data(),
        recvDispls.data(),
        ncclInt,
        comm,
        stream);
    ASSERT_EQ(res, ncclSuccess);
    CUDACHECK_TEST(cudaStreamSynchronize(stream));

    for (int r = 0; r < this->numRanks; r++) {
      int expectedVal = r * 100 + this->globalRank + 1;
      int errs =
          checkChunkValue(recvBuf + recvDispls[r], recvCounts[r], expectedVal);
      EXPECT_EQ(errs, 0) << "rank " << this->globalRank << " checked chunk "
                         << r << " at " << recvBuf + recvDispls[r] << " with "
                         << errs << " errors";
    }

    if (registFlag) {
      NCCLCHECK_TEST(ncclCommDeregister(comm, sendHandle));
      NCCLCHECK_TEST(ncclCommDeregister(comm, recvHandle));
    }

    CUDACHECK_TEST(cudaFree(sendBuf));
    CUDACHECK_TEST(cudaFree(recvBuf));
#endif
  }

 protected:
  int localRank{0};
  int globalRank{0};
  int numRanks{0};
  ncclComm_t comm;
  cudaStream_t stream;
};

TEST_F(AllToAllvTest, OutOfPlace) {
  run();
}

TEST_F(AllToAllvTest, Ctran) {
  setenv("NCCL_ALLTOALLV_ALGO", "ctran", 1);
  ncclCvarInit();
  run();
  unsetenv("NCCL_ALLTOALLV_ALGO");
}


TEST_F(AllToAllvTest, InvalidSendbuf) {
#ifdef NCCL_ALLTOALLV_SUPPORTED

  constexpr int count = 1048576;
  int* buf = nullptr;
  CUDACHECK_TEST(cudaMalloc(&buf, count * this->numRanks * sizeof(int)));

  // prepare alltoallv arguments
  std::vector<size_t> sendCounts(this->numRanks, count);
  std::vector<size_t> sendDispls(this->numRanks, 0);
  std::vector<size_t> recvCounts(this->numRanks, count);
  std::vector<size_t> recvDispls(this->numRanks, 0);

  // run alltoallv
  auto res = ncclAllToAllv(
      nullptr,
      sendCounts.data(),
      sendDispls.data(),
      buf,
      recvCounts.data(),
      recvDispls.data(),
      ncclInt,
      comm,
      stream);
  ASSERT_EQ(res, ncclInvalidArgument);
  CUDACHECK_TEST(cudaFree(buf));
#endif
}

TEST_F(AllToAllvTest, InvalidRecvbuf) {
#ifdef NCCL_ALLTOALLV_SUPPORTED
  constexpr int count = 1048576;
  int* buf = nullptr;
  CUDACHECK_TEST(cudaMalloc(&buf, count * this->numRanks * sizeof(int)));

  // prepare alltoallv arguments
  std::vector<size_t> sendCounts(this->numRanks, count);
  std::vector<size_t> sendDispls(this->numRanks, 0);
  std::vector<size_t> recvCounts(this->numRanks, count);
  std::vector<size_t> recvDispls(this->numRanks, 0);

  // run alltoallv
  auto res = ncclAllToAllv(
      buf,
      sendCounts.data(),
      sendDispls.data(),
      nullptr,
      recvCounts.data(),
      recvDispls.data(),
      ncclInt,
      comm,
      stream);
  ASSERT_EQ(res, ncclInvalidArgument);
  CUDACHECK_TEST(cudaFree(buf));
#endif
}

TEST_F(AllToAllvTest, InvalidInPlace) {
#ifdef NCCL_ALLTOALLV_SUPPORTED
  constexpr int count = 1048576;
  int* buf = nullptr;
  CUDACHECK_TEST(cudaMalloc(&buf, count * this->numRanks * sizeof(int)));

  // prepare alltoallv arguments
  std::vector<size_t> sendCounts(this->numRanks, count);
  std::vector<size_t> sendDispls(this->numRanks, 0);
  std::vector<size_t> recvCounts(this->numRanks, count);
  std::vector<size_t> recvDispls(this->numRanks, 0);

  // run alltoallv
  auto res = ncclAllToAllv(
      buf,
      sendCounts.data(),
      sendDispls.data(),
      buf,
      recvCounts.data(),
      recvDispls.data(),
      ncclInt,
      comm,
      stream);
  ASSERT_EQ(res, ncclInvalidArgument);
  CUDACHECK_TEST(cudaFree(buf));
#endif
}

int main(int argc, char* argv[]) {
  ::testing::InitGoogleTest(&argc, argv);
  ::testing::AddGlobalTestEnvironment(new MPIEnvironment);
  return RUN_ALL_TESTS();
}
