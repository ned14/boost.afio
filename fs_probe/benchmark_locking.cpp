/* benchmark_locking.cpp
Test the performance of various file locking mechanisms
(C) 2016 Niall Douglas http://www.nedprod.com/
File Created: Mar 2016


Boost Software License - Version 1.0 - August 17th, 2003

Permission is hereby granted, free of charge, to any person or organization
obtaining a copy of the software and accompanying documentation covered by
this license (the "Software") to use, reproduce, display, distribute,
execute, and transmit the Software, and to prepare derivative works of the
Software, and to permit third-parties to whom the Software is furnished to
do so, all subject to the following:

The copyright notices in the Software and this entire statement, including
the above license grant, this restriction and the following disclaimer,
must be included in all copies of the Software, in whole or in part, and
all derivative works of the Software, unless such copies or derivative
works are solely in the form of machine-executable object code generated by
a source language processor.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT
SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE
FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
DEALINGS IN THE SOFTWARE.
*/

#define _CRT_SECURE_NO_WARNINGS 1

#include "boost/afio/v2/detail/child_process.hpp"
#include "boost/afio/v2/file_handle.hpp"

#include <iostream>
#include <vector>

namespace afio = BOOST_AFIO_V2_NAMESPACE;

namespace append_only_mutual_exclusion
{
  union alignas(16) uint128 {
    unsigned char bytes[16];
#if defined(__i386__) || defined(_M_IX86) || defined(__x86_64__) || defined(_M_X64)
#if defined(__SSE2__) || (defined(_M_IX86_FP) && _M_IX86_FP >= 2)
    // Strongly hint to the compiler what to do here
    __m128i sse;
#endif
#endif
  };
  using uint64 = unsigned long long;
  struct header
  {
    uint128 hash;
    uint64 unique_id;
    uint64 time_t_offset;
    uint64 first_valid_lock_request;
    uint64 end_last_hole_punch;
    char _padding[128 - 48];
  };
  static_assert(sizeof(header) == 128, "header is not 128 bytes long!");
  struct lock_request
  {
    uint128 hash;
    uint64 unique_id;
    uint64 us_timestamp : 56;
    uint64 want_to_lock_items : 8;
    uint128 want_to_lock[6];
  };
  static_assert(sizeof(lock_request) == 128, "lock_request is not 128 bytes long!");
}

int main(int argc, char *argv[])
{
  if(argc < 2)
  {
    std::cerr << "Usage: " << argv[0] << " <no of waiters>" << std::endl;
    return 1;
  }
  if(strcmp(argv[1], "spawned") || argc < 3)
  {
    size_t waiters = atoi(argv[1]);
    if(!waiters)
    {
      std::cerr << "Usage: " << argv[0] << " <no of waiters>" << std::endl;
      return 1;
    }
    std::vector<afio::detail::child_process> children;
    auto mypath = afio::detail::current_process_path();
#ifdef UNICODE
    std::vector<afio::stl1z::filesystem::path::string_type> args = {L"spawned", L"00"};
#else
    std::vector<afio::stl1z::filesystem::path::string_type> args = {"spawned", "00"};
#endif
    auto env = afio::detail::current_process_env();
    for(size_t n = 0; n < waiters; n++)
    {
      if(n >= 10)
      {
        args[1][0] = (char) ('0' + (n / 10));
        args[1][1] = (char) ('0' + (n % 10));
      }
      else
      {
        args[1][0] = (char) ('0' + n);
        args[1][1] = 0;
      }
      auto child = afio::detail::child_process::launch(mypath, args, env);
      if(child.has_error())
      {
        std::cerr << "FATAL: Child " << n << " could not be launched due to " << child.get_error().message() << std::endl;
        return 1;
      }
      children.push_back(std::move(child.get()));
    }
    // Wait for all children to tell me they are ready
    char buffer[1024];
    for(auto &child : children)
    {
      auto &i = child.cout();
      i.get();
      if(!child.cout().getline(buffer, sizeof(buffer)) || 0 != strncmp(buffer, "READY", 5))
      {
        std::cerr << "ERROR: Child wrote unexpected output '" << buffer << "'" << std::endl;
        return 1;
      }
    }
    // Issue go command to all children
    for(auto &child : children)
      child.cin() << "GO" << std::endl;
    // Wait for benchmark to complete
    std::this_thread::sleep_for(std::chrono::seconds(5));
    // Tell children to quit
    for(auto &child : children)
      child.cin() << "STOP" << std::endl;
    unsigned long long results = 0, result;
    for(size_t n = 0; n < children.size(); n++)
    {
      auto &child = children[n];
      if(!child.cout().getline(buffer, sizeof(buffer)) || 0 != strncmp(buffer, "RESULT(", 7))
      {
        std::cerr << "ERROR: Child wrote unexpected output '" << buffer << "'" << std::endl;
        return 1;
      }
      result = atol(&buffer[7]);
      std::cout << "Child " << n << " reports result " << result << std::endl;
      results += result;
    }
    std::cout << "Total result: " << results << std::endl;
    return 0;
  }

  // I am a spawned child. Tell parent I am ready.
  std::cout << "READY(" << argv[2] << ")" << std::endl;
  // Wait for parent to let me proceed
  std::atomic<int> done(-1);
  std::thread worker([&done] {
    while(done == -1)
      std::this_thread::yield();
    while(!done)
    {
      // todo
      std::this_thread::yield();
    }
  });
  for(;;)
  {
    char buffer[1024];
    // This blocks
    std::cin.getline(buffer, sizeof(buffer));
    if(0 == strcmp(buffer, "GO"))
    {
      // Launch worker thread
      done = 0;
    }
    else if(0 == strcmp(buffer, "STOP"))
    {
      done = 1;
      worker.join();
      std::cout << "RESULTS(0)" << std::endl;
      return 0;
    }
  }
}