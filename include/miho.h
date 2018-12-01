#ifndef MIHO_H
#define MIHO_H

#include <miho/_export.h>

enum miho_status {
    MIHO_STATUS_NULL    = 0,
    MIHO_STATUS_NEW     = 1,
    MIHO_STATUS_STARTED = 2,
    MIHO_STATUS_STOPPED = 3,
    MIHO_STATUS_RUNNING = 4,
};

enum miho_update {
    MIHO_UPDATE_HELLO       = 0,
    MIHO_UPDATE_STATUS      = 1,
    MIHO_UPDATE_CONNECT     = 2,
    MIHO_UPDATE_DISCONNECT  = 3,
};

enum miho_result {
    MIHO_RESULT_OK          = 0,
    MIHO_RESULT_PROBLEM     = 1,
    MIHO_RESULT_WRONG_STATE = 2,
};

typedef struct miho *miho_t;
typedef void (*miho_update_callback_t)(
    void *userdata, miho_t miho, enum miho_update what,
    enum miho_status status, int port, int clients
);

miho_t MIHO_EXPORT miho_create(
    int port,
    miho_update_callback_t update_callback,
    void *update_userdata
);
void MIHO_EXPORT miho_close(miho_t miho);

#define MIHO_TIMEOUT_NEVER  -1

enum miho_result MIHO_EXPORT miho_start(miho_t miho);
enum miho_result MIHO_EXPORT miho_stop(miho_t miho);
enum miho_result MIHO_EXPORT miho_step(miho_t miho, int timeout);
enum miho_result MIHO_EXPORT miho_run(miho_t miho);

#endif // include guard
