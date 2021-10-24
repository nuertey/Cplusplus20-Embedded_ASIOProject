/***********************************************************************
* @file
*
* Threading utilities to be frequently taken advantage of :).
*
* @note
*
* @warning
*
*  Created: February 2, 2019
*   Author: Nuertey Odzeyem
***********************************************************************/
#pragma once

#include <string.h>
#include <sched.h>
#include <limits.h>
#include <pthread.h>
#include <assert.h>
#include <unistd.h>
#include <sys/prctl.h>
#include <sys/syscall.h>

#include <iostream>
#include <thread>
#include <string>

#define gettid() syscall(SYS_gettid)

namespace Utility
{
    // Note that the OS mandates that priority limits be tied to the chosen scheduling policy.
    const size_t    THREAD_SCHEDULING_POLICY    = SCHED_RR;    // Round-Robin scheduling policy.
    const size_t    THREAD_PRIORITY_HIGHEST     = sched_get_priority_max(THREAD_SCHEDULING_POLICY);

    // ----------------- User-facing threads that absolutely must NOT be delayed -----------------
    const size_t    PLATFORM_THREAD_PRIORITY    = THREAD_PRIORITY_HIGHEST;
    const size_t    APPLICATION_THREAD_PRIORITY = THREAD_PRIORITY_HIGHEST;
    const size_t    GUI_THREAD_PRIORITY         = THREAD_PRIORITY_HIGHEST;

    const size_t    THREAD_PRIORITY_HIGH        = THREAD_PRIORITY_HIGHEST - 10;
    const size_t    THREAD_PRIORITY_MEDIUM      = THREAD_PRIORITY_HIGHEST - 40;
    const size_t    THREAD_PRIORITY_LOW         = THREAD_PRIORITY_HIGHEST - 80;

    const size_t    THREAD_PRIORITY_LOWEST      = sched_get_priority_min(THREAD_SCHEDULING_POLICY);

    const size_t    THREAD_NAME_LENGTH          = 16;  // Length is restricted by the OS. Includes null-termination.
    const size_t    RECOMMENDED_BUFFER_SIZE     = 20;  // For retrievals; must be at least 16; OS will null-terminate.

    // Set the given thread's affinity to be exclusively on the given 
    // logical CPU number.
    inline void PinThreadToCPU(std::thread& t, int cpuNumber)
    {
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        CPU_SET(cpuNumber, &cpuset);
        int rc = pthread_setaffinity_np(t.native_handle(), 
                                        sizeof(cpu_set_t), 
                                        &cpuset);
        if (rc != 0)
        {
            std::cerr << __PRETTY_FUNCTION__ 
                      << ": Error calling pthread_setaffinity_np: " 
                      << rc << "\n";
        }
    }

    /*****************************************************************************
    * Convert the current thread into a realtime thread at the specified priority
    * level.
    *
    * @param[in] priority - Requested priority of the realtime thread.
    *
    * @return   None. However logs with errno are created on error(s).
    *
    * @note     Put some thought into choosing your priority level so as not to
    *           overwhelm other critical threads.
    *
    * @warning
    *******************************************************************************/
    inline void ConvertToRealtimeThread(size_t priority = THREAD_PRIORITY_LOWEST)
    {
        if ((priority >= THREAD_PRIORITY_LOWEST) && (priority <= THREAD_PRIORITY_HIGHEST))
        {
            int status = -1;
            struct sched_param schedParam;
            memset(&schedParam, 0, sizeof(schedParam));
            std::cout << __PRETTY_FUNCTION__ << ": Real-time scheduling policy priorities :-> "
                      << THREAD_PRIORITY_LOWEST << " <= priority <= " << THREAD_PRIORITY_HIGHEST << '\n';
            schedParam.sched_priority = priority;
            status = sched_setscheduler(0, THREAD_SCHEDULING_POLICY, &schedParam);
            if (0 == status)
            {
                std::cout << __PRETTY_FUNCTION__ << ": Real-time scheduling policy set. My thread priority :-> " << priority << '\n';
            }
            else
            {
                std::cout << __PRETTY_FUNCTION__ << ": Warning! Real-time scheduling policy set failed. Errno = "
                          << strerror(errno) << "; priority :-> " << priority << '\n';
            }
        }
        else
        {
            std::cout << __PRETTY_FUNCTION__ << ": Error! Invalid requested thread priority level :-> " << priority << '\n';
        }
    }

    /*****************************************************************************
    * Set the thread name within the kernel so GDB, "ps", "top" and other system
    * commands and tools can see and display those user-specified names.
    *
    * @param[in] threadId - Opaque thread ID object specifying the thread whose
    *                       name is to be changed.
    * @param[in] name     - the new name to be given to the thread. Will be truncated
    *                       if greater than 15 characters.
    *
    * @return   None. However logs with errno are created on error(s).
    *
    * @note     By default, all threads created using pthread_create() inherit
    *           the program name.  The SetThreadName() function can be used to
    *           set a unique name for a thread, which can be useful for debugging
    *           multi-threaded applications.  The thread name is a meaningful C
    *           language string, whose length is restricted to 16 characters,
    *           including the terminating null byte ('\0').
    *
    * @warning
    *******************************************************************************/
    inline void SetThreadName(pthread_t threadId, const char * name)
    {
        if ('\0' != name[0])
        {
            std::string name_str(name, (size_t)((strlen(name) < (THREAD_NAME_LENGTH - 1)) ? strlen(name) : (THREAD_NAME_LENGTH - 1)));

            int retval = -1;

            // Can return (34) Invalid Directory or Filename when /proc/[pid]/task/[tid]/comm doesn't exist.
            // Note that this error is harmless as this is only a debugging api.
            retval = pthread_setname_np(threadId, name);
            if (0 != retval)
            {
                std::cout << __PRETTY_FUNCTION__
                          << ": Warning! Thread name \"" << name_str.c_str()
                          << "\" set failed. Errno(" << retval
                          << ") = " << strerror(retval) << '\n';
            }
        }
        else
        {
            std::cout << __PRETTY_FUNCTION__ << ": Error! Invalid requested thread name." << '\n';
        }
    }

    /*****************************************************************************
    * Retrieve the thread name within the kernel.
    *
    * @param[in] threadId - Opaque thread ID object specifying the thread whose
    *                       name is to be retrieved.
    * @param[in] name     - Buffer used to return the thread name.
    * @param[in] length   - Specifies the number of bytes available in the buffer.
    *
    * @return   None. However logs with errno are created on error(s).
    *
    * @note     The buffer specified by name should be at least 16 characters
    *           in length. The returned thread name in the output buffer will
    *           be null terminated.
    *
    * @warning
    *******************************************************************************/
    inline void GetThreadName(pthread_t threadId, char * name, size_t length)
    {
        if (THREAD_NAME_LENGTH <= length)
        {
            int retval = -1;

            retval = pthread_getname_np(threadId, name, length);
            if (0 != retval)
            {
                std::cout << __PRETTY_FUNCTION__
                          << ": Warning! Thread name get failed. Errno = " << strerror(errno) << '\n';
            }
        }
        else
        {
            std::cout << __PRETTY_FUNCTION__ << ": Error! Insufficient buffer length." << '\n';
        }
    }

    inline void SetThreadName(std::thread * thread, const char * name)
    {
        auto handle = thread->native_handle();
        SetThreadName(handle, name);
    }

    inline void GetThreadName(std::thread * thread, char * name, size_t length)
    {
        auto handle = thread->native_handle();
        GetThreadName(handle, name, length);
    }

    inline void SetThreadName(const char * name)
    {
        if (gettid() == getpid())
        {
            std::cout << __PRETTY_FUNCTION__ << ": Warning! Main thread should NOT be renamed \
            as it will cause tools like killall to stop working." << '\n';
            return;
        }

        if ('\0' != name[0])
        {
            std::string name_str(name, (size_t)((strlen(name) < (THREAD_NAME_LENGTH - 1)) ? strlen(name) : (THREAD_NAME_LENGTH - 1)));

            int retval = 0;

            retval = prctl(PR_SET_NAME, name_str.c_str(), NULL, NULL, NULL);
            if (-1 == retval)
            {
                std::cout << __PRETTY_FUNCTION__
                     << ": Warning! Thread name \"" << name_str.c_str() << "\" set failed. Errno = " << strerror(errno) << '\n';
            }
        }
        else
        {
            std::cout << __PRETTY_FUNCTION__ << ": Error! Invalid requested thread name." << '\n';
        }
    }

    inline void GetThreadName(char * name, size_t length)
    {
        if (THREAD_NAME_LENGTH <= length)
        {
            int retval = 0;

            retval = prctl(PR_GET_NAME, name);
            if (-1 == retval)
            {
                std::cout << __PRETTY_FUNCTION__ << ": Warning! Thread name get failed. Errno = " << strerror(errno) << '\n';
            }
        }
        else
        {
            std::cout << __PRETTY_FUNCTION__ << ": Error! Insufficient buffer length." << '\n';
        }
    }
} 
