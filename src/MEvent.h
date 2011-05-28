#pragma once
#include "MBGlobal.h"
#include "MUtils.h"

namespace MetalBone
{
    class MRegion;
    class METALBONE_EXPORT MEvent
    {
        public:
            inline MEvent(bool a = true);
            inline ~MEvent();
            inline void accept();
            inline void ignore();
            inline bool isAccepted() const;

        private:
            bool accepted;
            MB_DISABLE_COPY(MEvent);
    };

    class METALBONE_EXPORT MPaintEvent : public MEvent
    {
        public:
            inline explicit MPaintEvent(const MRegion&);
            inline const MRegion& getUpdateRegion() const;

        private:
            const MRegion& region;
    };

    class METALBONE_EXPORT MResizeEvent : public MEvent
    {
        public:
            inline MResizeEvent(long oldWidth, long oldHeight, long newWidth, long newHeight);
            inline MSize getOldSize() const;
            inline MSize getNewSize() const;

        private:
            MSize oldSize;
            MSize newSize;
    };


    enum MouseButton
    {
        NoButton     = 0,
        LeftButton   = MK_LBUTTON,
        RightButton  = MK_RBUTTON,
        MiddleButton = MK_MBUTTON,
        ControlKey   = MK_CONTROL,
        ShiftKey     = MK_SHIFT,
        XButton1     = MK_XBUTTON1,
        XButton2     = MK_XBUTTON2
    };
    class METALBONE_EXPORT MMouseEvent : public MEvent
    {
        public:
            inline MMouseEvent(int, int, int, int, unsigned int mouseButton);

            inline unsigned int button()           const;
            inline int          getX()             const;
            inline int          getY()             const;
            inline int          getGlobalX()       const;
            inline int          getGlobalY()       const;
            inline void         offsetPos(int,int);

        private:
            int ix,iy,igx,igy;
            unsigned int btn;
    };
    
    enum KeyboardModifier
    {
        NoModifier     = 0,
        AltModifier    = MOD_ALT,
        ShiftModifier  = MOD_SHIFT,
        CtrlModifier   = MOD_CONTROL,
        WinModifier    = MOD_WIN,
        KeypadModifier = 0x80
    };
    class METALBONE_EXPORT MKeyEvent : public MEvent
    {
        public:
            inline MKeyEvent(unsigned int vk, unsigned int modifiers = NoModifier);
            inline unsigned int getModifier()   const;
            inline unsigned int getVirtualKey() const;

        private:
            unsigned int virtualKey;
            unsigned int modifiers;
    };

    class METALBONE_EXPORT MCharEvent : public MEvent
    {
        public:
            inline MCharEvent(unsigned int ch);
            inline unsigned int getChar() const;

        private:
            unsigned int ch;
    };





    inline MEvent::MEvent(bool a):accepted(a){}
    inline MEvent::~MEvent(){}
    inline void MEvent::accept() { accepted = true; }
    inline void MEvent::ignore() { accepted = false;}
    inline bool MEvent::isAccepted() const { return accepted; }

    inline MPaintEvent::MPaintEvent(const MRegion& r):region(r){}
    inline const MRegion& MPaintEvent::getUpdateRegion() const
        { return region; }

    inline MResizeEvent::MResizeEvent(long ow, long oh, long nw, long nh)
    {
        oldSize.cx = ow;
        oldSize.cy = oh;
        newSize.cx = nw;
        newSize.cy = nh;
    }
    inline MSize MResizeEvent::getOldSize() const
        { return oldSize; }
    inline MSize MResizeEvent::getNewSize() const
        { return newSize; }

    inline MMouseEvent::MMouseEvent(int x, int y, int gx, int gy,unsigned int b):
        ix(x),iy(y),igx(gx),igy(gy),btn(b){}
    inline int MMouseEvent::getX()            const { return ix;  }
    inline int MMouseEvent::getY()            const { return iy;  }
    inline int MMouseEvent::getGlobalX()      const { return igx; }
    inline int MMouseEvent::getGlobalY()      const { return igy; }
    inline unsigned int MMouseEvent::button() const { return btn; }
    inline void MMouseEvent::offsetPos(int x,int y)
        { ix += x; iy += y; }

    inline MKeyEvent::MKeyEvent(unsigned int vk, unsigned int m):virtualKey(vk),modifiers(m){}
    inline unsigned int MKeyEvent::getVirtualKey() const { return virtualKey; }
    inline unsigned int MKeyEvent::getModifier()   const { return modifiers; }

    inline MCharEvent::MCharEvent(unsigned int c):ch(c){}
    inline unsigned int MCharEvent::getChar() const { return ch; }
}
