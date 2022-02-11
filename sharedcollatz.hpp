#ifndef SHAREDCOLLATZ_HPP
#define SHAREDCOLLATZ_HPP

#include <assert.h>
#include <memory>
#include "sharedresults.hpp"

/*inline uint64_t calcSharedCollatz(InfInt n, const std::shared_ptr<SharedResults>& SR) {

    uint64_t count = 0;
    assert(n > 0);

    std::vector<std::pair<uint64_t, uint64_t>> to_fill;
    uint32_t new_nums = 0;

    while (n != 1)
    {
        SR->mtx.lock();
        if (n < 1000000 && SR->resultArray[n.toUnsignedInt()] != 0) {
            count = count + SR->resultArray[n.toUnsignedInt()];
            for (auto elem : to_fill) {
                SR->resultArray[elem.first] = count - elem.second;
            }
            SR->mtx.unlock();
            return count;
        }
        else if (n < 1000000 && new_nums < 1000000) {
            new_nums++;
            to_fill.emplace_back(std::make_pair(n.toUnsignedInt(), count));
        }

        SR->mtx.unlock();
        ++count;
        if (n % 2 == 1)
        {
            n *= 3;
            n += 1;
        }
        else
        {
            n /= 2;
        }
    }

    SR->mtx.lock();
    for (auto elem : to_fill) {
        SR->resultArray[elem.first] = count - elem.second;
    }
    SR->mtx.unlock();

    return count;
}*/

inline uint64_t calcSharedCollatz(InfInt n, const std::shared_ptr<SharedResults>& SR) {

    uint64_t count = 0;
    assert(n > 0);

    std::vector<std::pair<uint64_t, uint64_t>> to_fill;
    uint32_t new_nums = 0;

    while (n != 1)
    {
        if (n < 1000000) SR->mtx[n.toUnsignedInt()].lock();
        if (n < 1000000 && SR->resultArray[n.toUnsignedInt()] != 0) {
            count = count + SR->resultArray[n.toUnsignedInt()];
            SR->mtx[n.toUnsignedInt()].unlock();
            for (auto elem : to_fill) {
                SR->mtx[elem.first].lock();
                SR->resultArray[elem.first] = count - elem.second;
                SR->mtx[elem.first].unlock();
            }
            return count;
        }
        else if (n < 1000000 && new_nums < 1000000) {
            new_nums++;
            to_fill.emplace_back(std::make_pair(n.toUnsignedInt(), count));
        }

        if (n < 1000000) SR->mtx[n.toUnsignedInt()].unlock();
        ++count;
        if (n % 2 == 1)
        {
            n *= 3;
            n += 1;
        }
        else
        {
            n /= 2;
        }
    }

    //SR->mtx.lock();
    for (auto elem : to_fill) {
        SR->mtx[elem.first].lock();
        SR->resultArray[elem.first] = count - elem.second;
        SR->mtx[elem.first].unlock();
    }
    //SR->mtx.unlock();

    return count;
}

#endif
