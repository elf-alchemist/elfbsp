//------------------------------------------------------------------------------
//
//  ELFBSP
//
//------------------------------------------------------------------------------
//
//  Copyright 2026 Michael Brown
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 2
//  of the License, or (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//------------------------------------------------------------------------------

#pragma once

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <mutex>
#include <thread>
#include <vector>

// How many threads to use; hardware_concurrency can return 0 when it does not know.
size_t ChooseThreadCount();

// The threads of a team all run the same function once and stay alive until it ends,
// the same way an OpenMP parallel region works. Each thread keeps its own scratch space on
// its own stack and the team meets at a barrier between steps instead of being started again.
struct TeamSync
{
  explicit TeamSync(size_t threadCount) : threadCount_(threadCount)
  {
  }

  // Every thread of the team must call this. It returns once all of them have finished.
  void Wait();

  // Every thread of the team must call this with the same count. Each index from 0 to count - 1
  // is handed to one thread, to whichever thread is free (like OpenMP's dynamic scheduler).
  // It returns once every thread has finished.
  template <typename Body>
  void For(size_t threadIndex, size_t count, Body &&body)
  {
    if (threadIndex == 0)
    {
      next_.value.store(0, std::memory_order_relaxed);
    }
    Wait(); // The counter is back at zero for everybody

    for (;;)
    {
      const size_t index = next_.value.fetch_add(1, std::memory_order_relaxed);
      if (index >= count) break;

      body(index);
    }

    Wait(); // Everybody is finished
  }

private:
  // The counter gets a cache line of its own, because every thread changes it.
  struct alignas(64) PaddedCounter
  {
    std::atomic<size_t> value{0};
  };

  size_t threadCount_;
  std::mutex mutex_;
  std::condition_variable wakeUp_;
  size_t arrived_ = 0;
  size_t generation_ = 0;
  PaddedCounter next_;
};

// Runs body(threadIndex, team) once on every thread of the team.
// This thread is one of them. :p
template <typename Body>
void RunTeam(size_t threadCount, Body &&body)
{
  TeamSync team(threadCount);

  std::vector<std::jthread> threads;
  threads.reserve(threadCount - 1);

  for (size_t threadIndex = 1; threadIndex < threadCount; ++threadIndex)
  {
    threads.emplace_back([&body, &team, threadIndex]() { body(threadIndex, team); });
  }

  body(static_cast<size_t>(0), team);
}
