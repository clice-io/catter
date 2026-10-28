import { test } from "@catter/tools/testing";

// A library and the executable linked with it: Ninja runs one command at a time (-j 1), each
// within it, and the critical path goes from the library's source through its archive to the
// executable.
test(async (t) => {
    await t.setup("cmake -S . -B build -G Ninja");
    await t.cli("profile -q -- cmake --build build -j 1");
    t.show("trace.json", t.disk.trace());
});
