#include <eventide/loop.h>
#include <eventide/stream.h>

#include "config/ipc.h"
#include "ipc-data.h"

inline auto& default_loop() noexcept {
    static eventide::event_loop loop{};
    return loop;
}

template <typename Task>
auto wait(Task&& task) {
    default_loop().schedule(task);
    default_loop().run();
    return task.result();
}
