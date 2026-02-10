#pragma once

#include <uv.h>
#include <print>

void walk_cb(uv_handle_t* handle, void* arg) {
    const char* type_name = uv_handle_type_name(handle->type);

    std::println("Found active handle: addr={}, type={}, active={}, closing={}, has_ref={}",
                 (void*)handle,
                 type_name,
                 uv_is_active(handle),
                 uv_is_closing(handle),
                 uv_has_ref(handle));
}

void debug_handles(uv_loop_t* loop) {
    std::println("--- Start dumping handles ---");
    uv_walk(loop, walk_cb, NULL);
    std::println("--- End dumping handles ---");
}
