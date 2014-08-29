/* xdo library
 * - getwindowfocus contributed by Lee Pumphret
 * - keysequence_{up,down} contributed by Magnus Boman
 *
 * See the following url for an explanation of how keymaps work in X11
 * http://www.in-ulm.de/~mascheck/X11/xmodmap.html
 */

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#include <X11/Xlib.h>

#define XDO_ERROR 1
#define XDO_SUCCESS 0

#define XDO_FIND_PARENTS (0)
#define XDO_FIND_CHILDREN (1)

int xdo_get_mouse_location2(Display *dpy, int *x_ret, int *y_ret,
                            int *screen_num_ret, Window *window_ret);
int xdo_find_window_client(Display *dpy, Window window, Window *window_ret,
                           int direction);
int _is_success(const char *funcname, int code);
void _xdo_debug(const char *format, ...) ;
int xdo_get_window_property(Display *dpy, Window window, const char *property,
                            unsigned char **value, long *nitems, Atom *type, int *size);
unsigned char *xdo_get_window_property_by_atom(Display *dpy, Window window, Atom atom,
                                               long *nitems, Atom *type, int *size);




int main(int argc, char **argv)
{
   int x, y, screen_num;
   Window window;
   Display *dpy;
   char *display;

   if((display = getenv("DISPLAY"))==NULL)
   {
      dpy = XOpenDisplay(":0");
   }
   else
   {
      dpy = XOpenDisplay(getenv("DISPLAY"));
   }

   if(!xdo_get_mouse_location2(dpy, &x, &y, &screen_num, &window))
   {
      /*   printf("x:%d y:%d screen:%d window:%ld\n", ret, x, y, screen_num, window); */
      if(strstr(argv[0], "lower"))
      {
         if(!XLowerWindow(dpy, window))
         {
            fprintf(stderr, "XRaiseWindow error?\n");
            exit(1);
         }
      }
      else
      {
         if(!XRaiseWindow(dpy, window))
         {
            fprintf(stderr, "XRaiseWindow error?\n");
            exit(1);
         }
      }
   }
   XCloseDisplay(dpy);

   return(0);
}


int xdo_get_mouse_location2(Display *dpy, int *x_ret, int *y_ret,
                            int *screen_num_ret, Window *window_ret) 
{
  int ret = False;
  int x = 0, y = 0, screen_num = 0;
  int i = 0;
  Window window = 0;
  Window root = 0;
  int dummy_int = 0;
  unsigned int dummy_uint = 0;
  int screencount = ScreenCount(dpy);

  for (i = 0; i < screencount; i++) {
    Screen *screen = ScreenOfDisplay(dpy, i);
    ret = XQueryPointer(dpy, RootWindowOfScreen(screen),
                        &root, &window,
                        &x, &y, &dummy_int, &dummy_int, &dummy_uint);
    if (ret == True) {
      screen_num = i;
      break;
    }
  }

  if (window_ret != NULL) {
    /* Find the client window if we are not root. */
    if (window != root && window != 0) {
      int findret;
      Window client = 0;

      /* Search up the stack for a client window for this window */
      findret = xdo_find_window_client(dpy, window, &client, XDO_FIND_PARENTS);
      if (findret == XDO_ERROR) {
        /* If no client found, search down the stack */
        findret = xdo_find_window_client(dpy, window, &client, XDO_FIND_CHILDREN);
      }
      /*fprintf(stderr, "%ld, %ld, %ld, %d\n", window, root, client, findret);*/
      if (findret == XDO_SUCCESS) {
        window = client;
      }
    } else {
      window = root;
    }
  }
  /*printf("mouseloc root: %ld\n", root);*/
  /*printf("mouseloc window: %ld\n", window);*/

  if (ret == True) {
    if (x_ret != NULL) *x_ret = x;
    if (y_ret != NULL) *y_ret = y;
    if (screen_num_ret != NULL) *screen_num_ret = screen_num;
    if (window_ret != NULL) *window_ret = window;
  }

  return _is_success("XQueryPointer", ret == False);
}

int xdo_find_window_client(Display *dpy, Window window, Window *window_ret,
                           int direction) 
{
  /* for XQueryTree */
  Window dummy, parent, *children = NULL;
  unsigned int nchildren;
  Atom atom_wmstate = XInternAtom(dpy, "WM_STATE", False);

  int done = False;
  long items;


  while (!done) {
    if (window == 0) {
      return XDO_ERROR;
    }

    _xdo_debug("get_window_property on %lu", window);
    xdo_get_window_property_by_atom(dpy, window, atom_wmstate, &items, NULL, NULL);

    if (items == 0) {
      /* This window doesn't have WM_STATE property, keep searching. */
      _xdo_debug("window %lu has no WM_STATE property, digging more.", window);
      XQueryTree(dpy, window, &dummy, &parent, &children, &nchildren);

      if (direction == XDO_FIND_PARENTS) {
        _xdo_debug("searching parents");
        /* Don't care about the children, but we still need to free them */
        if (children != NULL)
          XFree(children);
        window = parent;
      } else if (direction == XDO_FIND_CHILDREN) {
        unsigned int i = 0;
        int ret;
        _xdo_debug("searching %d children", nchildren);

        done = True; /* recursion should end us */
        for (i = 0; i < nchildren; i++) {
          ret = xdo_find_window_client(dpy, children[i], &window, direction);
          /*fprintf(stderr, "findclient: %ld\n", window);*/
          if (ret == XDO_SUCCESS) {
            *window_ret = window;
            break;
          }
        }
        if (nchildren == 0) {
          return XDO_ERROR;
        }
        if (children != NULL)
          XFree(children);
      } else {
        fprintf(stderr, "Invalid find_client direction (%d)\n", direction);
        *window_ret = 0;
        if (children != NULL)
          XFree(children);
        return XDO_ERROR;
      }
    } else {
      *window_ret = window;
      done = True;
    }
  }
  return XDO_SUCCESS;
}

int _is_success(const char *funcname, int code)
{
  /* Nonzero is failure. */
   if (code != 0)
    fprintf(stderr, "%s failed (code=%d)\n", funcname, code);
  return code;
}

void _xdo_debug(const char *format, ...) 
{
  va_list args;

  va_start(args, format);
#ifdef DEBUG
  vfprintf(stderr, format, args);
  fprintf(stderr, "\n");
#endif
} /* _xdo_debug */

int xdo_get_window_property(Display *dpy, Window window, const char *property,
                            unsigned char **value, long *nitems, Atom *type, int *size) {
    *value = xdo_get_window_property_by_atom(dpy, window, XInternAtom(dpy, property, False), nitems, type, size);
    if (*value == NULL) {
        return XDO_ERROR;
    }
    return XDO_SUCCESS;
}

/* Arbitrary window property retrieval
 * slightly modified version from xprop.c from Xorg */
unsigned char *xdo_get_window_property_by_atom(Display *dpy, Window window, Atom atom,
                                            long *nitems, Atom *type, int *size) {
  Atom actual_type;
  int actual_format;
  unsigned long _nitems;
  /*unsigned long nbytes;*/
  unsigned long bytes_after; /* unused */
  unsigned char *prop;
  int status;

  status = XGetWindowProperty(dpy, window, atom, 0, (~0L),
                              False, AnyPropertyType, &actual_type,
                              &actual_format, &_nitems, &bytes_after,
                              &prop);
  if (status == BadWindow) {
    fprintf(stderr, "window id # 0x%lx does not exists!", window);
    return NULL;
  } if (status != Success) {
    fprintf(stderr, "XGetWindowProperty failed!");
    return NULL;
  }

  /*
   *if (actual_format == 32)
   *  nbytes = sizeof(long);
   *else if (actual_format == 16)
   *  nbytes = sizeof(short);
   *else if (actual_format == 8)
   *  nbytes = 1;
   *else if (actual_format == 0)
   *  nbytes = 0;
   */

  if (nitems != NULL) {
    *nitems = _nitems;
  }

  if (type != NULL) {
    *type = actual_type;
  }

  if (size != NULL) {
    *size = actual_format;
  }
  return prop;
}

