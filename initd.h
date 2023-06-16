#if !defined(_WIN32)
#  if defined(__APPLE__)
#    include <TargetConditionals.h>
#    if TARGET_OS_OSX
#      define _HAVE_INITD 1
#    endif
#  elif defined(__linux__) && !defined(__ANDROID__)
#    define _HAVE_INITD 1
#  endif
#endif
