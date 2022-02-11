#include <utility>
#include <deque>
#include <future>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>

#include "teams.hpp"
#include "contest.hpp"
#include "collatz.hpp"
#include "sharedcollatz.hpp"

// Function calculates Collatz for indexes given.
// It fills result with collatz results at those indexes.
void calcCollatzConstThreads(ContestInput Input, ContestResult& result, const std::vector<uint64_t>& indexes)
{
    for (auto idx : indexes) {
        result[idx] = calcCollatz(Input[idx]);
    }
}

// Same as above but uses sharedResults.
void calcSharedCollatzConstThreads(ContestInput Input, ContestResult& result,
                                   const std::vector<uint64_t>& indexes,
                                   const std::shared_ptr<SharedResults>& SR) {
    for (auto idx : indexes) {
        result[idx] = calcSharedCollatz(Input[idx], SR);
    }
}

// Function calculates Collatz for single InfInt, and fills result at given index.
void calcCollatzNewThreads(ContestResult& result, uint64_t idx, const InfInt& singleInput) {
    result[idx] = calcCollatz(singleInput);
}

// Same as above but uses sharedResults.
void calcSharedCollatzNewThreads(ContestResult& result, uint64_t idx,
                                 const InfInt& singleInput, const std::shared_ptr<SharedResults>& SR) {
    result[idx] = calcSharedCollatz(singleInput, SR);
}

// The only difference between this function and calcCollatzConstThreads is result array
// that has to be a pointer because it is mapped memory.
void calcCollatzConstProcesses(ContestInput Input, uint64_t *result, const std::vector<uint64_t>& indexes)
{
    for (auto idx : indexes) {
        result[idx] = calcCollatz(Input[idx]);
    }
}

ContestResult TeamNewThreads::runContestImpl(ContestInput const & contestInput)
{
    ContestResult result;
    result.resize(contestInput.size());

    uint64_t idx = 0; // To iterate over input.
    uint64_t thread_idx = 0; // To iterate over threads array.
    uint64_t to_wait = 0; // To know which thread to wait for.
    uint32_t threadCount = getSize();
    bool unlock_all = false;

    std::thread threads[threadCount];

    for(InfInt const & singleInput : contestInput)
    {
        // This means we have to wait for 1 thread to finish his work.
        if (idx >= threadCount) {
            threads[to_wait].join();
            thread_idx = to_wait;
            to_wait = (to_wait + 1) % getSize();
        }
        if (this->getSharedResults()) {
            threads[thread_idx] = createThread(calcSharedCollatzNewThreads,
                                               std::ref(result), idx, singleInput,
                                               this->getSharedResults());
        }
        else {
            threads[thread_idx] = createThread(calcCollatzNewThreads, std::ref(result), idx, singleInput);
        }
        idx++;
        // If there were at least getSize() single inputs
        if (thread_idx == threadCount - 1) unlock_all = true;
        thread_idx = (thread_idx + 1) % threadCount;
    }

    if (unlock_all) for (uint32_t i = 0; i < threadCount; i++) threads[i].join();
    else for (uint32_t i = 0; i < thread_idx; i++) threads[i].join();
    return result;
}

ContestResult TeamConstThreads::runContestImpl(ContestInput const & contestInput)
{
    ContestResult result;
    result.resize(contestInput.size());

    uint64_t idx = 0;

    uint32_t threadCount = getSize();
    std::thread threads[threadCount];
    std::vector<uint64_t> indexes[threadCount];

    // This loop splits the work for each thread.
    for (uint32_t i = 0; i < contestInput.size(); i++) {
        indexes[idx].push_back(i);
        idx = (idx + 1) % threadCount;
    }

    for (uint32_t i = 0; i < threadCount; i++) {
        if (this->getSharedResults()) {
            threads[i] = createThread(calcSharedCollatzConstThreads, contestInput,
                                      std::ref(result), indexes[i],
                                      this->getSharedResults());
        }
        else {
            threads[i] = createThread(calcCollatzConstThreads, contestInput, std::ref(result), indexes[i]);
        }
    }

    for (uint32_t i = 0; i < threadCount; i++) threads[i].join();

    return result;
}

ContestResult TeamPool::runContest(ContestInput const & contestInput)
{
    ContestResult result;
    result.resize(contestInput.size());

    std::future<uint64_t> futures[result.size()];

    uint32_t idx = 0;
    for (InfInt const & singleInput : contestInput) {
        if (this->getSharedResults()) {
            futures[idx] = pool.push(calcSharedCollatz, singleInput, this->getSharedResults());
        }
        else {
            futures[idx] = pool.push(calcCollatz, singleInput);
        }
        idx++;
    }
    for (uint32_t i = 0; i < result.size(); i++) {
        result[i] = futures[i].get();
    }

    return result;
}

ContestResult TeamNewProcesses::runContest(ContestInput const & contestInput) {
    ContestResult result;
    result.resize(contestInput.size());

    int idx = 0;
    pid_t pid;
    bool wait_all = false;
    auto input_size = contestInput.size();
    auto size = getSize();
    uint64_t *mapped_mem;

    int prot = PROT_READ | PROT_WRITE;
    int flags = MAP_SHARED | MAP_ANONYMOUS;
    int fd_memory = -1;

    // Result will be stored in the mapped memory.
    mapped_mem = (uint64_t *)mmap(NULL, input_size * sizeof(uint64_t),
                                   prot, flags, fd_memory, 0);

    for (int i = 0; i < input_size; i++) {
        switch (pid = fork()) {
            case -1:
                std::cerr << "error in fork.";
                exit(1);
            case 0:
                mapped_mem[i] = calcCollatz(contestInput[i]);
                exit(0);
            default:
                idx++;
                if (idx >= size) wait(0);
                if (idx == size) wait_all = true;
                break;
        }
    }

    // Wait for getSize() processes to finish.
    if (wait_all) for (int i = 0; i < size; i++) wait(0);
    // Wait only for input_size processes.
    else for (int i = 0; i < idx; i++) wait(0);

    for (int i = 0; i < input_size; i++) {
        result[i] = mapped_mem[i];
    }
    return result;
}

ContestResult TeamConstProcesses::runContest(ContestInput const &contestInput) {
    ContestResult result;
    result.resize(contestInput.size());

    uint64_t idx = 0;
    pid_t pid;
    auto input_size = contestInput.size();
    auto processesCount = getSize();
    uint64_t *mapped_mem;

    int prot = PROT_READ | PROT_WRITE;
    int flags = MAP_SHARED | MAP_ANONYMOUS;
    int fd_memory = -1;

    mapped_mem = (uint64_t *)mmap(NULL, input_size * sizeof(uint64_t),
                                  prot, flags, fd_memory, 0);

    std::vector<uint64_t> indexes[processesCount];
    // Split the work for each process.
    for (int i = 0; i < input_size; i++) {
        indexes[idx].push_back(i);
        idx = (idx + 1) % processesCount;
    }

    for (int i = 0; i < processesCount; i++) {
        switch (pid = fork()) {
            case -1:
                std::cerr << "error in fork.";
                exit(1);
            case 0:
                calcCollatzConstProcesses(contestInput, mapped_mem, indexes[i]);
                exit(0);
            default:
                break;
        }
    }

    for (int i = 0; i < processesCount; i++) wait(0);

    for (int i = 0; i < input_size; i++) {
        result[i] = mapped_mem[i];
    }

    return result;
}

ContestResult TeamAsync::runContest(ContestInput const &contestInput) {
    ContestResult result;
    result.resize(contestInput.size());

    std::future<uint64_t> futures[result.size()];

    int idx = 0;
    for (InfInt const &singleInput: contestInput) {
        if (this->getSharedResults()) {
            futures[idx] = std::async(calcSharedCollatz, singleInput, this->getSharedResults());
        }
        else {
            futures[idx] = std::async(calcCollatz, singleInput);
        }
        idx++;
    }
    for (int i = 0; i < result.size(); i++) {
        result[i] = futures[i].get();
    }

    return result;
}
