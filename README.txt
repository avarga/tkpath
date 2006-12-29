
                           README for tkpath
                           _________________

This package implements path drawing modelled after its SVG counterpart,
see http://www.w3.org/TR/SVG11/. See the doc directory for more info.

There are five backends used for drawing. They are all platform specific
except for the Tk drawing which uses only the API found in Tk. This
backend is very limited and has some problems with multiple subpaths.
It is only to be used as a fallback when the cairo backend is missing.

The backends:

    1) CoreGraphics for MacOSX, built using ProjectBuilder
 
    2) GDI for Win32, built by VC++7 (.NET), very basic

    3) GDI+ for WinXP, built by VC++7 (.NET), runs also on older system
       using the gdiplus.dll

    4) cairo (http://cairographics.org), built using the automake system;
       the configure.in and Makefile.in files are a hack, so please help
       yourself (and me). It requires a cairo 1.0 installation since
       incompatible API changes appeared before 1.0 (libcairo.so.2 ?).

    5) Tk drawing, fallback for cairo mainly, very basic

I could think of another backend based on X11 that has more features than
the compatibility layer of Tk, since the fallback is only necessary on unix 
systems anyway. Perhaps an OpenGL backend would also be useful, mainly on
unix systems without cairo support.

Open Issues:

There are a number of design choices that I'd like to discuss.

 o How to provide coordinates for prect? As the standard Tk way (x1,y1,x2,y2), 
   using sizes (x,y,width,height), or using options (x,y,-width,-height)?

 o How to deal with gradient options? Using separate options 
   (-fill & -fillgradient) or a single one (-fill) and decide what to do
   while parsing the option.

 o What shall the precedence of the -style option compared to the individual
   options be?

Copyright (c) 2005-2006  Mats Bengtsson

BSD style license.
