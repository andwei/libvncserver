#ifndef _RFB_RFBCONFIG_H
#cmakedefine _RFB_RFBCONFIG_H 1
 
/* rfb/rfbconfig.h. Generated automatically by cmake. */

/* Enable 24 bit per pixel in native framebuffer */
#cmakedefine LIBVNCSERVER_ALLOW24BPP  1 

/* work around when write() returns ENOENT but does not mean it */
#cmakedefine LIBVNCSERVER_ENOENT_WORKAROUND 1

/* Define to 1 if you have the <endian.h> header file. */
#cmakedefine LIBVNCSERVER_HAVE_ENDIAN_H 1

/* Define to 1 if you have the <fcntl.h> header file. */
#cmakedefine LIBVNCSERVER_HAVE_FCNTL_H  1 

/* Define to 1 if you have the `gettimeofday' function. */
#cmakedefine LIBVNCSERVER_HAVE_GETTIMEOFDAY  1 

/* Define to 1 if you have the `jpeg' library (-ljpeg). */
#cmakedefine LIBVNCSERVER_HAVE_LIBJPEG  1 

/* Define if you have the `png' library (-lpng). */
#cmakedefine LIBVNCSERVER_HAVE_LIBPNG  1

/* Define to 1 if you have the `pthread' library (-lpthread). */
#cmakedefine LIBVNCSERVER_HAVE_LIBPTHREAD  1 

/* Define to 1 if you have the `z' library (-lz). */
#cmakedefine LIBVNCSERVER_HAVE_LIBZ  1 

/* Define to 1 if you have the <netinet/in.h> header file. */
#cmakedefine LIBVNCSERVER_HAVE_NETINET_IN_H  1 

/* Define to 1 if you have the <sys/endian.h> header file. */
#cmakedefine LIBVNCSERVER_HAVE_SYS_ENDIAN_H 1

/* Define to 1 if you have the <sys/socket.h> header file. */
#cmakedefine LIBVNCSERVER_HAVE_SYS_SOCKET_H  1 

/* Define to 1 if you have the <sys/stat.h> header file. */
#cmakedefine LIBVNCSERVER_HAVE_SYS_STAT_H  1 

/* Define to 1 if you have the <sys/time.h> header file. */
#cmakedefine LIBVNCSERVER_HAVE_SYS_TIME_H  1 

/* Define to 1 if you have the <sys/types.h> header file. */
#cmakedefine LIBVNCSERVER_HAVE_SYS_TYPES_H  1 

/* Define to 1 if you have <sys/wait.h> that is POSIX.1 compatible. */
#cmakedefine LIBVNCSERVER_HAVE_SYS_WAIT_H  1 

/* Define to 1 if you have <sys/uio.h> */
#cmakedefine LIBVNCSERVER_HAVE_SYS_UIO_H  1 

/* Define to 1 if you have the <unistd.h> header file. */
#cmakedefine LIBVNCSERVER_HAVE_UNISTD_H  1 

/* Define to 1 if you have the `vfork' function. */
#cmakedefine LIBVNCSERVER_HAVE_VFORK  1 

/* Define to 1 if you have the <vfork.h> header file. */
#cmakedefine LIBVNCSERVER_HAVE_VFORK_H  1 

/* Define to 1 if you have the `vprintf' function. */
#cmakedefine LIBVNCSERVER_HAVE_VPRINTF  1 

/* Define to 1 if `fork' works. */
#cmakedefine LIBVNCSERVER_HAVE_WORKING_FORK  1 

/* Define to 1 if `vfork' works. */
#cmakedefine LIBVNCSERVER_HAVE_WORKING_VFORK  1 

/* Define to 1 if `mmap' exists. */
#cmakedefine LIBVNCSERVER_HAVE_MMAP  1 

/* Define to 1 if `fork' exists. */
#cmakedefine LIBVNCSERVER_HAVE_FORK  1 

/* Define to 1 if you have the <ws2tcpip.h> header file. */
#cmakedefine LIBVNCSERVER_HAVE_WS2TCPIP_H  1 

/* Enable IPv6 support */
#cmakedefine LIBVNCSERVER_IPv6  1 

/* Need a typedef for in_addr_t */
#cmakedefine LIBVNCSERVER_NEED_INADDR_T 1

/* Define to the full name and version of this package. */
#define LIBVNCSERVER_PACKAGE_STRING  "@FULL_PACKAGE_NAME@ @PACKAGE_VERSION@"

/* Define to the version of this package. */
#define LIBVNCSERVER_PACKAGE_VERSION  "@PACKAGE_VERSION@"
#define LIBVNCSERVER_VERSION "@PACKAGE_VERSION@"
#define LIBVNCSERVER_VERSION_MAJOR "@VERSION_MAJOR@"
#define LIBVNCSERVER_VERSION_MINOR "@VERSION_MINOR@"
#define LIBVNCSERVER_VERSION_PATCHLEVEL "@VERSION_PATCHLEVEL@"

/* Define to 1 if libgcrypt is present */
#cmakedefine LIBVNCSERVER_WITH_CLIENT_GCRYPT 1

/* Define to 1 if GnuTLS is present */
#cmakedefine LIBVNCSERVER_WITH_CLIENT_TLS 1

/* Define to 1 if OpenSSL is present */
#cmakedefine LIBVNCSERVER_HAVE_LIBSSL 1

/* Define to 1 to build with websockets */
#cmakedefine LIBVNCSERVER_WITH_WEBSOCKETS 1

/* Define to 1 if your processor stores words with the most significant byte
   first (like Motorola and SPARC, unlike Intel and VAX). */
#cmakedefine LIBVNCSERVER_WORDS_BIGENDIAN 1

/* Define to empty if `const' does not conform to ANSI C. */
/* #cmakedefine const @CMAKE_CONST@ */

/* Define to `__inline__' or `__inline' if that's what the C compiler
   calls it, or to nothing if 'inline' is not supported under any name.  */
/* #ifndef __cplusplus */
/* #cmakedefine inline @CMAKE_INLINE@ */
/* #endif */

/* Define to `int' if <sys/types.h> does not define. */
#cmakedefine HAVE_LIBVNCSERVER_PID_T 1
#ifndef HAVE_LIBVNCSERVER_PID_T
typedef int pid_t;
#endif

/* The type for size_t */
#cmakedefine HAVE_LIBVNCSERVER_SIZE_T 1
#ifndef HAVE_LIBVNCSERVER_SIZE_T
typedef int size_t;
#endif

/* The type for socklen */
#cmakedefine HAVE_LIBVNCSERVER_SOCKLEN_T 1
#ifndef HAVE_LIBVNCSERVER_SOCKLEN_T
typedef int socklen_t;
#endif

/* once: _RFB_RFBCONFIG_H */
#endif
