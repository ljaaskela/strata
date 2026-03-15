#ifndef VELK_THREAD_H
#define VELK_THREAD_H

#include <cstdint>

#ifdef _WIN32
extern "C" __declspec(dllimport) unsigned long __stdcall GetCurrentThreadId();
#else
#include <pthread.h>
#endif

namespace velk {

/** @brief Returns the current OS thread ID. */
inline uint32_t current_thread_id()
{
#ifdef _WIN32
    return static_cast<uint32_t>(GetCurrentThreadId());
#elif defined(__APPLE__)
    uint64_t tid = 0;
    pthread_threadid_np(nullptr, &tid);
    return static_cast<uint32_t>(tid);
#else
    return static_cast<uint32_t>(pthread_self());
#endif
}

} // namespace velk

#endif // VELK_THREAD_H

