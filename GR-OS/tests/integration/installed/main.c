#include <gr/c_api.h>
#include <stdio.h>
#include <string.h>
int main(void) {
    const char *request =
        "{\"abi\":1,\"op\":\"runtime.evaluate\",\"graph\":{\"nodes\":[{\"id\":\"c\",\"operation\":\"gr.bytes."
        "identity@1\",\"inputs\":[{\"literal\":{\"type\":\"gr.bytes@1\",\"representation\":\"bytes\",\"data_"
        "base64\":\"Yw==\"}}]}],\"outputs\":[{\"node\":\"c\",\"output\":0}]}}";
    char *response = NULL;
    size_t size = 0;
    if (gr_abi_version() != 1)
        return 1;
    int status = gr_call_json(request, strlen(request), &response, &size);
    int failed = status != 0 || !response || size == 0 || !strstr(response, "\"status\":\"succeeded\"") ||
                 !strstr(response, "Yw==");
    gr_free(response);
    if (failed)
        return failed;
    const uint8_t bytes[] = {1, 2, 3};
    response = NULL;
    if (gr_buffer_import(bytes, 3, "{}", 2, &response, &size) != 0)
        return 2;
    const char *key = strstr(response, "\"handle\":\"");
    char handle[128] = {0};
    if (!key) {
        gr_free(response);
        return 3;
    }
    key += strlen("\"handle\":\"");
    const char *end = strchr(key, '"');
    if (!end || end - key > 127) {
        gr_free(response);
        return 4;
    }
    memcpy(handle, key, (size_t)(end - key));
    gr_free(response);
    gr_buffer_lease *lease = NULL;
    gr_buffer_info info;
    response = NULL;
    if (gr_buffer_pin(handle, strlen(handle), &lease, &info, &response, &size) != 0)
        return 5;
    gr_free(response);
    char close_request[256];
    snprintf(close_request, sizeof(close_request), "{\"abi\":1,\"op\":\"buffer.close\",\"handle\":\"%s\"}",
             handle);
    response = NULL;
    status = gr_call_json(close_request, strlen(close_request), &response, &size);
    gr_free(response);
    failed = status || info.logical_bytes != 3 || info.data[0] != 1 || info.data[2] != 3;
    gr_buffer_unpin(lease);
    return failed;
}
