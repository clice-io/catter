import { test } from "@catter/tools/testing";

// Commands that feed others: the dependencies of a.c (-MM) and its compile,
// which alone gets an entry; b.c preprocessed and then compiled, whose entry
// is the preprocess made the compile of b.c into b.o; c.c compiled to assembly from
// sub/ and assembled from the top, whose assembly gets none of its own. A
// probe compiles the null device, and nvcc preprocesses with gcc: neither is a
// source of the build.
test(async (t) => {
    await t.cli("compile-commands -o compile_commands.json --from events.json");
    t.show("compile_commands.json", t.disk.cdb());
});
