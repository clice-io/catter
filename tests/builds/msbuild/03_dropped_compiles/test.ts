import { test } from "@catter/tools/testing";

// A handler drops every compile. MSBuild's tracker starts cl.exe suspended and
// prepares it before it runs: a dropped compile still lets it, and ends with
// 0, so the build goes on to the link, which finds no object files.
test(
    {
        except: {
            linux: "MSBuild builds for Windows",
            darwin: "MSBuild builds for Windows",
        },
    },
    async (t) => {
        await t.setup("cmake -S . -B build");
        await t.cli([
            "run",
            "--handler",
            "python handler.py",
            "--",
            "cmake",
            "--build",
            "build",
            "--config",
            "Debug",
            "--",
            "-m:1",
            "-nologo",
        ]);
    },
);
