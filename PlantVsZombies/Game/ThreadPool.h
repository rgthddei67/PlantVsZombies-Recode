#pragma once
#ifndef _THREAD_POOL_H
#define _THREAD_POOL_H

#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <vector>
#include <functional>
#include <algorithm>

// 鎸佷箙绾跨▼姹狅紝閬垮厤姣忓抚鍒涘缓/閿€姣佺嚎绋?
class ThreadPool {
public:
    explicit ThreadPool(int numThreads)
        : mShutdown(false), mWorkGen(0), mDoneCount(0), mNumActive(0), mTotalItems(0)
    {
        mWorkers.reserve(numThreads);
        for (int i = 0; i < numThreads; i++)
            mWorkers.emplace_back(&ThreadPool::WorkerLoop, this, i);
    }

    ~ThreadPool() {
        {
            std::unique_lock<std::mutex> lock(mMutex);
            mShutdown = true;
        }
        mStartCV.notify_all();
        for (auto& t : mWorkers) t.join();
    }

    // 灏?[0, totalItems) 鍒嗘鍒嗗彂缁欏悇绾跨▼锛岄樆濉炵洿鍒板叏閮ㄥ畬鎴?
    void Dispatch(int totalItems, std::function<void(int, int)> func) {
        if (totalItems <= 0) return;
        int n = static_cast<int>(mWorkers.size());
        if (n > totalItems) n = totalItems;

        mWorkFunc = std::move(func);
        mTotalItems = totalItems;
        mNumActive = n;
        mDoneCount.store(0);

        {
            std::unique_lock<std::mutex> lock(mMutex);
            mWorkGen++;
        }
        mStartCV.notify_all();

        std::unique_lock<std::mutex> lock(mMutex);
        mDoneCV.wait(lock, [this, n] { return mDoneCount.load() == n; });
    }

private:
    void WorkerLoop(int idx) {
        int lastGen = 0;
        while (true) {
            {
                std::unique_lock<std::mutex> lock(mMutex);
                mStartCV.wait(lock, [this, lastGen] { return mShutdown || mWorkGen != lastGen; });
                if (mShutdown) return;
                lastGen = mWorkGen;
            }
            int chunkSize = (mTotalItems + mNumActive - 1) / mNumActive;
            int start = idx * chunkSize;
            int end = std::min(start + chunkSize, mTotalItems);
            if (start < mTotalItems)
                mWorkFunc(start, end);

            if (mDoneCount.fetch_add(1) + 1 == mNumActive) {
                std::unique_lock<std::mutex> lock(mMutex);
                mDoneCV.notify_one();
            }
        }
    }

    std::vector<std::thread> mWorkers;
    std::function<void(int, int)> mWorkFunc;
    int mTotalItems;
    int mNumActive;
    std::mutex mMutex;
    std::condition_variable mStartCV;
    std::condition_variable mDoneCV;
    std::atomic<int> mDoneCount;
    bool mShutdown;
    int mWorkGen;
};

#endif
