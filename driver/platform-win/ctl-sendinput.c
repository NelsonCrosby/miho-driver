#include <windows.h>
#include <WinUser.h>

#include "../controls.h"


#ifdef MIHO_SUBSYSTEM_MOUSE


void m_ctl_mouse_move(int dx, int dy)
{
    INPUT input;
    input.type = INPUT_MOUSE;
    input.mi.dx = dx;
    input.mi.dy = dy;
    input.mi.mouseData = 0;
    input.mi.dwFlags = MOUSEEVENTF_MOVE;
    input.mi.time = 0;
    input.mi.dwExtraInfo = NULL;
    SendInput(1, &input, sizeof(input));
}


void m_ctl_mouse_button(enum m_mouse_button button, int down)
{
    INPUT input;
    input.type = INPUT_MOUSE;
    input.mi.dx = 0;
    input.mi.dy = 0;
    input.mi.mouseData = 0;
    input.mi.time = 0;
    input.mi.dwExtraInfo = NULL;

    switch (button) {
        case M_MOUSE_LEFT:
            input.mi.dwFlags = down
                ? MOUSEEVENTF_LEFTDOWN
                : MOUSEEVENTF_LEFTUP;
            break;
        case M_MOUSE_RIGHT:
            input.mi.dwFlags = down
                ? MOUSEEVENTF_RIGHTDOWN
                : MOUSEEVENTF_RIGHTUP;
            break;
        case M_MOUSE_MIDDLE:
            input.mi.dwFlags = down
                ? MOUSEEVENTF_MIDDLEDOWN
                : MOUSEEVENTF_MIDDLEUP;
            break;
    }

    SendInput(1, &input, sizeof(input));
}


#endif
