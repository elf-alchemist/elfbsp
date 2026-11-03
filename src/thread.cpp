//------------------------------------------------------------------------------
//
//  ELFBSP
//
//------------------------------------------------------------------------------
//
//  Copyright 2026 Michael Brown
//  Copyright 2025-2026 Guilherme Miranda
//  Copyright 2000-2018 Andrew Apted, et al
//  Copyright 1994-1998 Colin Reed
//  Copyright 1997-1998 Lee Killough
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

#include "thread.hpp"

#include <algorithm>

// How many threads to use; hardware_concurrency can return 0 when it does not know.
size_t ChooseThreadCount()
{
  return std::max<size_t>(1, std::thread::hardware_concurrency());
}

// Every thread of the team must call this. It returns once all of them have finished.
void TeamSync::Wait()
{
  std::unique_lock<std::mutex> lock(mutex_);
  const size_t myGeneration = generation_;

  if (++arrived_ == threadCount_)
  {
    arrived_ = 0;
    ++generation_;
    wakeUp_.notify_all();
  }
  else
  {
    wakeUp_.wait(lock, [&]()
    {
      return generation_ != myGeneration;
    });
  }
}
