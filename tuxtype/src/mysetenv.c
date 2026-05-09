/* This file was taken from example code in GNU Gettext FAQ

   http://www.gnu.org/software/gettext/FAQ.html

   by Bruno Haible. No copyright or license is mentioned in
   the FAQ, but the text clearly supplies this code
   to help with difficulties using Gettext under Windows, and
   appears to imply that it is released into the public domain.

     - David Bruce
*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#ifdef WIN32
# include <windows.h>
#endif

int my_setenv(const char* name, const char* value)
{
    size_t namelen  = strlen(name);
    size_t valuelen = (value == NULL ? 0 : strlen(value));

#ifdef WIN32
    /* On Woe32, each process has two copies of the environment variables,
      one managed by the OS and one managed by the C library. We set
      the value in both locations, so that other software that looks in
      one place or the other is guaranteed to see the value. Even if it's
      a bit slow. See also
      <http://article.gmane.org/gmane.comp.gnu.mingw.user/8272>
      <http://article.gmane.org/gmane.comp.gnu.mingw.user/8273>
      <http://www.cygwin.com/ml/cygwin/1999-04/msg00478.html> */

    if (!SetEnvironmentVariableA(name, value))
    {
        fprintf(stderr, "Warning - SetEnvironmentVariableA(%s, %s) failed.\n",
                name, value);
    }
#endif

    /* setenv(3) is POSIX (macOS, glibc) but absent from mingw — when on
     * WIN32 the SetEnvironmentVariableA above already updated the OS-side
     * env; the libc env doesn't matter on a static-libgcc build. */
    (void)namelen;
    (void)valuelen;
#ifdef WIN32
    return 0;
#else
    return setenv(name, value, 1);
#endif
}
