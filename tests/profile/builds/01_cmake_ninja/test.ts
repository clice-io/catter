import { test } from "@catter/tools/testing";

// A library of two sources: Ninja runs one command at a time (-j 1), each within it, and each
// compile's target is the library its object goes into.
test(async (t) => {
    await t.setup("cmake -S . -B build -G Ninja");
    await t.cli("profile -q -- cmake --build build -j 1");
    t.show("trace.json", t.disk.trace());
});
