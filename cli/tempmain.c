#include <Windows.h>
#include <strsafe.h>
#include <stdio.h>

#include <miho.h>


static void on_update(
    void *userdata, miho_t miho, enum miho_update what,
    enum miho_status status, int port, int clients
)
{
    printf("got %d; { status = %d, port = %d, clients = %d }\n",
           what, status, port, clients);
}


int main()
{
    miho_t miho = miho_create(6446, on_update, NULL);
    if (miho == NULL) {
        printf("ERROR in miho_create\n");
        return 1;
    }

    enum miho_result result = miho_start(miho);
    switch (result) {
        case MIHO_RESULT_OK:
            break;
        default:
            printf("ERROR in miho_start: %d\n", result);
            return 1;
    }

    result = miho_stop(miho);
    switch (result) {
        case MIHO_RESULT_OK:
            break;
        default:
            printf("ERROR in miho_stop: %d\n", result);
            return 1;
    }

    miho_close(miho);
    return 0;
}
