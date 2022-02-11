#ifndef SHAREDRESULTS_HPP
#define SHAREDRESULTS_HPP

#include <mutex>
#include <atomic>

class SharedResults
{
    static const uint32_t N = 1e6;
public:
    std::mutex mtx[N];
    std::atomic<uint64_t> resultArray[N];
};

#endif