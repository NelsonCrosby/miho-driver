#include "internal.h"
#include "m-io.h"


extern int m_subsys_error_arg_count(
    struct m_subsys_call *call, unsigned expect
);


int m_subsys_register(
    miho_t miho, const char *name,
    struct m_subsys_function *funcs, void *func_userdata
)
{
    if (miho->subsys_count >= miho->subsys_max) {
        if (miho->subsys_max == 0) {
            miho->subsys_max = 8;
        } else {
            miho->subsys_max *= 2;
        }
        miho->subsystems = realloc(
            miho->subsystems, sizeof(*miho->subsystems) * miho->subsys_max
        );

        if (miho->subsystems == NULL) {
            return 0;
        }

        for (size_t i = miho->subsys_count; i < miho->subsys_max; i += 1) {
            miho->subsystems[i].subsys_name = NULL;
            miho->subsystems[i].func_count = 0;
            miho->subsystems[i].funcs = NULL;
            miho->subsystems[i].func_userdata = NULL;
        }
    }

    struct m_subsys *subsys = &miho->subsystems[miho->subsys_count];
    subsys->subsys_name = name;
    subsys->func_count = 0;
    subsys->func_name_len_total = 0;
    subsys->funcs = funcs;
    subsys->func_userdata = func_userdata;

    unsigned i;
    for (i = 0; funcs[i].func_name != NULL; i += 1) {
        subsys->func_name_len_total += strlen(funcs[i].func_name);
    }
    subsys->func_count = i;

    miho->subsys_count += 1;
    miho->subsys_name_len_total += strlen(name);

    return 1;
}


static int m_subsys_on_write(
    struct miho_client *client, struct m_client *mclient, int ok,
    size_t ok_length, size_t buffer_length, void *buffer
)
{
    free(buffer);
    client->pending -= 1;
    if (client->closing && client->pending == 0) {
        m_client_close(mclient);
    }

    return 1;
}


void m_subsys_return(struct m_subsys_call *call)
{
    if (call->result_return >= M_RETURN_OK) {
        size_t result_length = cbor_encoder_get_buffer_size(
            &call->results, call->results_buffer
        );
        uint8_t *enc_buf = malloc(32 + result_length);
        CborEncoder enc_root, enc_arr, enc_resp;

        // TODO: Check function results
        cbor_encoder_init(&enc_root, enc_buf, 32 + result_length, 0);
        cbor_encoder_create_array(&enc_root, &enc_arr, 3);
        cbor_encode_uint(&enc_arr, call->subsystem);
        cbor_encode_uint(&enc_arr, call->result_return);
        cbor_encoder_create_array(&enc_arr, &enc_resp, call->result_count);

        size_t enc_head_len = cbor_encoder_get_buffer_size(&enc_resp, enc_buf);
        memcpy(enc_buf + enc_head_len, call->results_buffer, result_length);

        if (m_client_write(
            call->client->mclient, enc_head_len + result_length, enc_buf,
            m_subsys_on_write, call->client
        )) {
            call->client->pending += 1;
        }
    }

    free(call->arg_buffer);
    free(call);
}


static void m_core_list_subsystems(struct m_subsys_call *call);
static void m_core_list_functions(struct m_subsys_call *call);

extern struct m_subsys_function m_core_funcs[] = {
    {"list_subsystems", m_core_list_subsystems},
    {"list_functions", m_core_list_functions},
    {NULL, NULL}
};


static void m_core_list_subsystems(struct m_subsys_call *call)
{
    if (m_subsys_error_arg_count(call, 0)) return;

    miho_t miho = call->miho;
    size_t subsys_count = miho->subsys_count;
 
    size_t buflen = subsys_count * 2 + miho->subsys_name_len_total;
    call->result_return = M_RETURN_OK;
    call->result_count = (unsigned) subsys_count - 1;
    uint8_t *results_buffer = call->results_buffer = malloc(buflen);
    cbor_encoder_init(&call->results, call->results_buffer, buflen, 0);
    for (size_t i = 1; i < subsys_count; i += 1) {
        const char *subsys_name = miho->subsystems[i].subsys_name;
        cbor_encode_text_stringz(&call->results, subsys_name);
    }

    m_subsys_return(call);
    free(results_buffer);
}


static void m_core_list_functions(struct m_subsys_call *call)
{
    if (m_subsys_error_arg_count(call, 1)) return;

    uint64_t q_subsystem;
    if (!cbor_value_is_unsigned_integer(&call->arg_iter)) {
        m_subsys_error(call, MIHO_ERRCODE_VALUES,
                       "expected unsigned int for argument subsystem", 0);
        return;
    }
    cbor_value_get_uint64(&call->arg_iter, &q_subsystem);

    if (q_subsystem >= call->miho->subsys_count) {
        m_subsys_error(call, MIHO_ERRCODE_VALUES,
                       "not a valid subsystem", 0);
    }

    struct m_subsys *subsys = &call->miho->subsystems[q_subsystem];
    size_t buflen = subsys->func_count * 2 + subsys->func_name_len_total;
    call->result_return = M_RETURN_OK;
    call->result_count = (unsigned) subsys->func_count;
    uint8_t *results_buffer = call->results_buffer = malloc(buflen);
    cbor_encoder_init(&call->results, call->results_buffer, buflen, 0);
    for (size_t i = 0; i < subsys->func_count; i += 1) {
        if (subsys->funcs[i].func_call == NULL) {
            cbor_encode_null(&call->results);
        } else {
            const char *func_name = subsys->funcs[i].func_name;
            cbor_encode_text_stringz(&call->results, func_name);
        }
    }

    m_subsys_return(call);
    free(results_buffer);
}
