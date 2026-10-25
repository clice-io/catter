import { test } from "@catter/tools/testing";

// The executable of a real build with the objects and sources it is made of.
test(async (t) => {
    await t.setup("cmake -S . -B build -G Ninja");
    await t.cli("targets -- cmake --build build -j 1");
});
