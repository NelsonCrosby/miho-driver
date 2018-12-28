#ifndef MIHO_H_CONTROLS
#define MIHO_H_CONTROLS


#ifdef MIHO_SUBSYSTEM_MOUSE
enum m_mouse_button {
    M_MOUSE_LEFT = 0, M_MOUSE_RIGHT = 1, M_MOUSE_MIDDLE = 2,
};
void m_ctl_mouse_move(int dx, int dy);
void m_ctl_mouse_button(enum m_mouse_button button, int down);
#endif


#endif // include guard
