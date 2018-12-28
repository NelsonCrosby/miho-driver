#ifdef MIHO_SUBSYSTEM_MOUSE


#include "../internal.h"
#include "../controls.h"


static void m_mouse_move(struct m_subsys_call *call);
static void m_mouse_button(struct m_subsys_call *call);

extern struct m_subsys_function m_mouse_funcs[] = {
    {"", NULL},
    {"", NULL},
    {"", NULL},
    // Commands start at 3 for dumb historical reasons
    {"move", m_mouse_move},         // 3
    {"button", m_mouse_button},     // 4
    {NULL, NULL}
};


static void m_mouse_move(struct m_subsys_call *call)
{
    if (m_subsys_error_arg_count(call, 2)) return;

    int dx, dy;
    if (!cbor_value_is_integer(&call->arg_iter)) {
        m_subsys_error(call, MIHO_ERRCODE_VALUES,
                       "expected int for argument dx", 0);
        return;
    }
    cbor_value_get_int(&call->arg_iter, &dx);
    cbor_value_advance_fixed(&call->arg_iter);
    if (!cbor_value_is_integer(&call->arg_iter)) {
        m_subsys_error(call, MIHO_ERRCODE_VALUES,
                       "expected int for argument dy", 0);
        return;
    }
    cbor_value_get_int(&call->arg_iter, &dy);
    cbor_value_advance_fixed(&call->arg_iter);

    m_ctl_mouse_move(dx, dy);

    m_subsys_return(call);
}


static void m_mouse_button(struct m_subsys_call *call)
{
    if (m_subsys_error_arg_count(call, 2)) return;

    int button;
    bool down;
    if (!cbor_value_is_unsigned_integer(&call->arg_iter)) {
        m_subsys_error(call, MIHO_ERRCODE_VALUES,
                       "expected unsigned int for argument button", 0);
        return;
    }
    cbor_value_get_int(&call->arg_iter, &button);
    cbor_value_advance_fixed(&call->arg_iter);
    if (!cbor_value_is_boolean(&call->arg_iter)) {
        m_subsys_error(call, MIHO_ERRCODE_VALUES,
                       "expected bool for argument isDown", 0);
        return;
    }
    cbor_value_get_boolean(&call->arg_iter, &down);
    cbor_value_advance_fixed(&call->arg_iter);

    if (!(
        button == M_MOUSE_LEFT ||
        button == M_MOUSE_RIGHT ||
        button == M_MOUSE_MIDDLE
    )) {
        m_subsys_error(call, MIHO_ERRCODE_VALUES,
                       "expected argument button to be a valid button", 0);
        return;
    }

    m_ctl_mouse_button(button, down);

    m_subsys_return(call);
}


#endif
