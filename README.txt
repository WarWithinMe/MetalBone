What is MetalBone?
==========
MetalBone is a so-called DirectUI library for Windows. It currently use Direct2D or Skia (it's up to you) to do the drawing.


Why is there MetalBone?
==========
In Win GUI programming, every control is a Window. A button is a window, a checkbox is a window. I'm sure there're plenty people who don't like this old-style design. A windowless GUI lib is needed.
There're many GUI lib for Windows. But most of them are still Window-Oriented, and some are just a wrapper for the native Win32 API. Others are cross-platform (e.g. Qt, a window-less GUI framework), which introduces many unnecessary things.
So, I decide to make the MetalBone.


Why you want MetalBone?
==========
1.You don't have to deal with Window (i.e. controls) any more, everything is a widget.
2.It's easy to style your widgets. Comparing to some commercial DirectUI library, MetalBone uses CSS to style the widgets (as what Qt does) instead of XML.
Although it doesn't fully support CSS2 and CSS3, those relative to UI stuff are (planning) implemented.


Why you don't want MetalBone?
==========
1.You want native look and feel for your app. In this case, MetalBone is really not your choice.
2.You want something like Ribbon controls. Well, again, MetalBone is not make for this purpose.


Drawbacks:
==========
- Well, there's no default style. So, if you want to show your widget, you need to write your own css.
- The API is really not Windows-Style, i.e. public classes are prefixed with 'M' instead of 'C', function name begins with lower-case letter instead of upper-case.
- There might be many bugs! Well, I think maybe you can avoid many of them.