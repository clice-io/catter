import { test } from "@catter/tools/testing";

// Each program catter-proxy starts reports itself once, though the system
// runs it otherwise than its file says: a script through its interpreter, a
// file of no format through the shell, a program through the dynamic loader
// named as the program.
const loader =
    process.arch === "arm64" ? "/lib/ld-linux-aarch64.so.1" : "/lib64/ld-linux-x86-64.so.2";

test(
    "linux",
    { except: { darwin: "Linux's dynamic loader", win32: "the POSIX programs" } },
    async (t) => {
        // The loader of the machine's architecture, by one name on every one.
        await t.setup(["ln", "-s", loader, "ld.so"]);
        await t.cli("tree -w 1000 -- ./script.sh one");
        await t.cli("tree -w 1000 -- ./plain two");
        await t.cli([
            "tree",
            "-w",
            "1000",
            "--",
            "./ld.so",
            process.execPath,
            "child.mjs",
            "three",
        ]);
    },
);
