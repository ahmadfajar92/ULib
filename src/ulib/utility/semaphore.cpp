// ============================================================================
//
// = LIBRARY
//    ULib - c++ library
//
// = FILENAME
//    semaphore.cpp
//
// = AUTHOR
//    Stefano Casazza
//
// ============================================================================

#include <ulib/file.h>
#include <ulib/timeval.h>
#include <ulib/utility/semaphore.h>

#ifndef __clang__
U_DUMP_KERNEL_VERSION(LINUX_VERSION_CODE)
#endif

UFile*      USemaphore::flock;
USemaphore* USemaphore::first;

/**
 * The initial value of the semaphore can be specified. An initial value is often used when used to lock a finite
 * resource or to specify the maximum number of thread instances that can access a specified resource.
 *
 * @param resource specify initial resource count or 1 default
 */

void USemaphore::init(sem_t* ptr, int resource)
{
   U_TRACE(1, "USemaphore::init(%p,%d)", ptr, resource)

#if defined(HAVE_SEM_INIT) && LINUX_VERSION_CODE > KERNEL_VERSION(2,6,7)
   if (ptr)
      {
      psem = ptr;

      // initialize semaphore object sem to value, share it with other processes

      U_INTERNAL_ASSERT_POINTER(psem)

      bool broken = (U_SYSCALL(sem_init, "%p,%d,%u", psem, 1, resource) == -1); // 1 -> semaphore is shared between processes

      if (broken) U_ERROR("USemaphore::init(%p,%u) failed", ptr, resource);

      next  = first;
      first = this;

      U_INTERNAL_DUMP("first = %p next = %p", first, next)

      U_INTERNAL_ASSERT_DIFFERS(first, next)
      }

#  ifdef DEBUG
   int _value = getValue();

   if (_value != resource) U_ERROR("USemaphore::init(%p,%u) failed - value = %d", ptr, resource, _value);
#  endif
#else
#  ifdef _MSWINDOWS_
   psem = (sem_t*) ::CreateSemaphore((LPSECURITY_ATTRIBUTES)NULL, (LONG)resource, 1000000, (LPCTSTR)NULL);
#  else
   if (flock == 0)
      {
      flock = U_NEW(UFile);

      if (flock->mkTemp(0) == false) U_ERROR("USemaphore::init(%p,%u) failed", ptr, resource);
      }
#  endif
#endif
}

/**
 * Destroying a semaphore also removes any system resources associated with it. If a semaphore has threads currently
 * waiting on it, those threads will all continue when a semaphore is destroyed
 */

USemaphore::~USemaphore()
{
   U_TRACE_UNREGISTER_OBJECT(0, USemaphore)

#if defined(HAVE_SEM_INIT) && LINUX_VERSION_CODE > KERNEL_VERSION(2,6,7)
   U_INTERNAL_ASSERT_POINTER(psem)

   (void) sem_destroy(psem); // Free resources associated with semaphore object sem
#else
#  ifdef _MSWINDOWS_
   ::CloseHandle((HANDLE)psem);
#  else
   (void) flock->close();
#  endif
#endif
}

/**
 * Posting to a semaphore increments its current value and releases the first thread waiting for the semaphore
 * if it is currently at 0. Interestingly, there is no support to increment a semaphore by any value greater than 1
 * to release multiple waiting threads in either pthread or the win32 API. Hence, if one wants to release
 * a semaphore to enable multiple threads to execute, one must perform multiple post operations
 */

void USemaphore::post()
{
   U_TRACE(1, "USemaphore::post()")

#if defined(HAVE_SEM_INIT) && LINUX_VERSION_CODE > KERNEL_VERSION(2,6,7)
   U_INTERNAL_ASSERT_POINTER(psem)

   U_INTERNAL_DUMP("value = %d", getValue())

   (void) U_SYSCALL(sem_post, "%p", psem); // unlock a semaphore

   U_INTERNAL_DUMP("value = %d", getValue())
#else
#  ifdef _MSWINDOWS_
   ::ReleaseSemaphore((HANDLE)psem, 1, (LPLONG)NULL);
#  else
   (void) flock->unlock();
#  endif
#endif
}

// NB: check if process has restarted and it had a lock armed (DEADLOCK)...

bool USemaphore::checkForDeadLock(UTimeVal& time)
{
   U_TRACE(1, "USemaphore::checkForDeadLock(%p)", &time)

#if defined(HAVE_SEM_INIT) && LINUX_VERSION_CODE > KERNEL_VERSION(2,6,7)
   bool sleeped = false;

   for (USemaphore* item = first; item; item = item->next)
      {
      if (item->getValue() <= 0)
         {
         sleeped = true;

         time.nanosleep();

         if (item->getValue() <= 0)
            {
            time.nanosleep();

            if (item->getValue() <= 0) item->post(); // unlock the semaphore
            }
         }
      }

   U_RETURN(sleeped);
#else
   U_RETURN(false);
#endif
}

/**
 * Wait is used to keep a thread held until the semaphore counter is greater than 0. If the current thread is held, then
 * another thread must increment the semaphore. Once the thread is accepted, the semaphore is automatically decremented,
 * and the thread continues execution.
 *
 * @return false if timed out
 * @param timeout period in milliseconds to wait
 */

bool USemaphore::wait(time_t timeoutMS)
{
   U_TRACE(1, "USemaphore::wait(%ld)", timeoutMS) // problem with sanitize address

#if defined(HAVE_SEM_INIT) && LINUX_VERSION_CODE > KERNEL_VERSION(2,6,7)
   int rc;

   U_INTERNAL_ASSERT_POINTER(psem)

   U_INTERNAL_DUMP("value = %d", getValue())

   if (timeoutMS == 0) rc = U_SYSCALL(sem_wait, "%p", psem);
   else
      {
      // Wait for sem being posted

      U_INTERNAL_ASSERT(u_now->tv_sec > 1260183779) // 07/12/2009

      struct timespec abs_timeout = { u_now->tv_sec + timeoutMS / 1000L, 0 };

      U_INTERNAL_DUMP("abs_timeout = { %d, %d }", abs_timeout.tv_sec, abs_timeout.tv_nsec)

      rc = U_SYSCALL(sem_timedwait, "%p,%p", psem, &abs_timeout);
      }

   U_INTERNAL_DUMP("value = %d", getValue())

   if (rc == 0) U_RETURN(true);
#else
#  ifdef _MSWINDOWS_
   if (::WaitForSingleObject((HANDLE)psem, (timeoutMS ? timeoutMS : INFINITE)) == WAIT_OBJECT_0) U_RETURN(true);
#  else
   if (flock->unlock()) U_RETURN(true);
#  endif
#endif

   U_RETURN(false);
}

// DEBUG

#if defined(U_STDCPP_ENABLE) && defined(DEBUG)
const char* USemaphore::dump(bool reset) const
{
   *UObjectIO::os << "next         " << (void*)next  << '\n'
                  << "psem         " << (void*)psem  << '\n'
                  << "flock (UFile " << (void*)flock << ')';

   if (reset)
      {
      UObjectIO::output();

      return UObjectIO::buffer_output;
      }

   return 0;
}
#endif
