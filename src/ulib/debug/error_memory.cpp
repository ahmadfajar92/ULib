// ============================================================================
//
// = LIBRARY
//    ULib - c++ library
//
// = FILENAME
//    error_memory.cpp
//
// = AUTHOR
//    Stefano Casazza
//
// ============================================================================

#include <ulib/base/utility.h>
#include <ulib/internal/common.h>

char* UMemoryError::pbuffer;

const char* UMemoryError::getErrorType(const void* pobj) const
{
   if (invariant()) return "";

   // (ABW) Array Beyond Write | (FMR) Free Memory Read

   (void) sprintf(pbuffer, "[pobj = %p _this = %p - %s]", pobj, _this, (_this ? "ABW" : "FMR"));

   if (UObjectDB::fd > 0)
      {
      uint32_t n = u__strlen(pbuffer, __PRETTY_FUNCTION__),
               l = UObjectDB::dumpObject(pbuffer+n+1, U_MAX_SIZE_PREALLOCATE-n-1, pobj);

      if (l)
         {
         pbuffer[n  +1] = '\n';
         pbuffer[n+l+1] = '\n';
         pbuffer[n+l+2] = '\0';
         }
      }

   return pbuffer;
}
