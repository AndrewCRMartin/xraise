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

#define ERROR 1
#define OK    0

#define FIND_PARENTS (0)
#define FIND_CHILDREN (1)

typedef unsigned long ULONG;
typedef unsigned char UCHAR;
typedef unsigned int  UINT;

int GetMouseLocation(Display *display, int *x_ret, int *y_ret,
                     int *screen_num_ret, Window *window_ret);
int FindClientWindow(Display *display, Window window, 
                     Window *window_ret, int direction);
void Debug(const char *format, ...) ;
int GetWindowProperty(Display *display, Window window, 
                            const char *property, UCHAR **value,
                            long *nitems, Atom *type, int *size);
UCHAR *GetWindowPropertyByAtom(Display *display, Window window,
                                       Atom atom, long *nitems, 
                                       Atom *type, int *size);

/***********************************************************************/
int main(int argc, char **argv)
{
   int x, y, screen_num;
   Window window;
   Display *display;
   char *displayName;

   if((displayName = getenv("DISPLAY"))==NULL)
   {
      display = XOpenDisplay(":0");
   }
   else
   {
      display = XOpenDisplay(displayName);
   }

   if(!GetMouseLocation(display, &x, &y, &screen_num, &window))
   {
#if (DEBUG > 3)
      printf("x:%d y:%d screen:%d window:%ld\n", 
             x, y, screen_num, window);
#endif
      if(strstr(argv[0], "lower"))
      {
         if(!XLowerWindow(display, window))
         {
            fprintf(stderr, "XRaiseWindow error?\n");
            exit(1);
         }
      }
      else
      {
         if(!XRaiseWindow(display, window))
         {
            fprintf(stderr, "XRaiseWindow error?\n");
            exit(1);
         }
      }
   }
   XCloseDisplay(display);

   return(0);
}


/***********************************************************************/
int FindClientWindow(Display *display, Window window, 
                     Window *window_ret, int direction) 
{
   Window dummy, parent, *children = NULL;
   UINT nchildren;
   Atom atom_wmstate = XInternAtom(display, "WM_STATE", False);
   
   int done = False;
   long items;
   
   while (!done) 
   {
      if (window == 0) 
      {
         return ERROR;
      }
      
      Debug("get_window_property on %lu", window);
      GetWindowPropertyByAtom(display, window, atom_wmstate, 
                              &items, NULL, NULL);
      
      if (items == 0) 
      {
         /* This window doesn't have WM_STATE property, 
            keep searching. 
         */
         Debug("window %lu has no WM_STATE property, digging more.", 
               window);
         XQueryTree(display, window, &dummy, &parent, 
                    &children, &nchildren);
         
         if (direction == FIND_PARENTS) 
         {
            Debug("searching parents");
            /* Don't care about the children, 
               but we still need to free them 
            */
            if (children != NULL)
               XFree(children);
            window = parent;
         } 
         else if (direction == FIND_CHILDREN) 
         {
            UINT i = 0;
            int ret;
            Debug("searching %d children", nchildren);
            
            done = True; /* recursion should end us */
            for (i = 0; i < nchildren; i++) 
            {
               ret = FindClientWindow(display, children[i], 
                                      &window, direction);
#if (DEBUG > 3)
               fprintf(stderr, "findclient: %ld\n", window);
#endif
               if (ret == OK) 
               {
                  *window_ret = window;
                  break;
               }
            }
            if (nchildren == 0) 
            {
               return ERROR;
            }
            if (children != NULL)
               XFree(children);
         } 
         else 
         {
            fprintf(stderr, "Invalid find_client direction (%d)\n", 
                    direction);
            *window_ret = 0;
            if (children != NULL)
               XFree(children);
            return ERROR;
         }
      } 
      else 
      {
         *window_ret = window;
         done = True;
      }
   }
   return OK;
}

/***********************************************************************/
void Debug(const char *format, ...) 
{
   va_list args;
   va_start(args, format);

#if DEBUG > 2
   vfprintf(stderr, format, args);
   fprintf(stderr, "\n");
#endif
}

/***********************************************************************/
int GetWindowProperty(Display *display, Window window, 
                      const char *property,
                      UCHAR **value, long *nitems, 
                      Atom *type, int *size) 
{
   *value = GetWindowPropertyByAtom(display, window, 
                                    XInternAtom(display,property,False),
                                    nitems, type, size);
   if (*value == NULL) 
   {
      return ERROR;
   }
   return OK;
}

/***********************************************************************/
/* Arbitrary window property retrieval
 * slightly modified version from xprop.c from Xorg */
UCHAR *GetWindowPropertyByAtom(Display *display, Window window, 
                               Atom atom, long *nitems, 
                               Atom *type, int *size) 
{
   Atom actual_type;
   int actual_format;
   ULONG _nitems;
   /*ULONG nbytes;*/
   ULONG bytes_after; /* unused */
   UCHAR *prop;
   int status;
   
   status = XGetWindowProperty(display, window, atom, 0, (~0L),
                               False, AnyPropertyType, &actual_type,
                               &actual_format, &_nitems, &bytes_after,
                               &prop);
   if (status == BadWindow) 
   {
      fprintf(stderr, "window id # 0x%lx does not exists!", window);
      return NULL;
   }
   if (status != Success) 
   {
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
   
   if (nitems != NULL) 
   {
      *nitems = _nitems;
   }
   
   if (type != NULL) 
   {
      *type = actual_type;
   }
   
   if (size != NULL) 
   {
      *size = actual_format;
   }
   return prop;
}

/***********************************************************************/
int GetMouseLocation(Display *display, int *x_ret, int *y_ret,
                     int *screen_num_ret, Window *window_ret) 
{
   int ret        = False,
       rootX      = 0, 
       rootY      = 0, 
       windowX    = 0,
       windowY    = 0,
       screen_num = 0,
       i          = 0;
   Window window = 0,
          rootWindow = 0;
   UINT mask = 0;
   int screencount = ScreenCount(display);
   
   /* Step through the screens associated with this display */
   for (i = 0; i < screencount; i++) 
   {
      Screen *screen = ScreenOfDisplay(display, i);

      /* If the pointer is on this screen we get the info for the 
         pointer, store the screen number and break out of the loop
       */
      if((ret=XQueryPointer(display, RootWindowOfScreen(screen),
                            &rootWindow, &window,
                            &rootX, &rootY, 
                            &windowX, &windowY, &mask))==True)
      {
         screen_num = i;
         break;
      }
   }
   
   /* If the caller is requesting the window number... */
   if (window_ret != NULL) 
   {
      /* If it's not the root window, find the client window */
      if ((window != rootWindow) && (window != 0))
      {
         int findret;
         Window clientWindow = 0;
         
         /* Search up the stack for a client window for this window */
         findret = FindClientWindow(display, window, 
                                    &clientWindow, FIND_PARENTS);
         if (findret == ERROR) 
         {
            /* If no client found, search down the stack */
            findret = FindClientWindow(display, window, 
                                       &clientWindow, FIND_CHILDREN);
         }
#ifdef DEBUG
         fprintf(stderr, "Window: %ld, Root window: %ld, \
Client Window: %ld, Return: %d\n",
                 window, rootWindow, clientWindow, findret);
#endif
         if (findret == OK) 
         {
            window = clientWindow;
         }
      } 
      else 
      {
         window = rootWindow;
      }
   }
#if (DEBUG > 3)
   printf("mouseloc root: %ld\n", rootWindow);
   printf("mouseloc window: %ld\n", window);
#endif
   
   if (ret == True) 
   {
      if (x_ret != NULL) *x_ret = rootX;
      if (y_ret != NULL) *y_ret = rootY;
      if (screen_num_ret != NULL) *screen_num_ret = screen_num;
      if (window_ret != NULL) *window_ret = window;
   }
   else
   {
      fprintf(stderr, "XQueryPointer() failed\n");
      return(ERROR);
   }

   return(OK);
}

