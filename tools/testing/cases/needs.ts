/// What a test needs of the machine it runs on beside catter's own: where
/// the machine lacks it, the test runs only on CI, where it fails, and is
/// listed elsewhere as needing it.

import { spawnSync } from "node:child_process";

export type Need = "eslogger" | "go" | "sudo" | "xcode";

const PROBES: Record<Need, () => boolean> = {
    // catter runs eslogger as root: sudo must start it without asking for a
    // password, and Endpoint Security must take it as a client. Refused, it
    // exits at once; taken, it runs until the probe ends it.
    eslogger: () =>
        spawnSync("sudo", ["-n", "/usr/bin/eslogger", "exec", "fork", "exit"], {
            stdio: "ignore",
            timeout: 3000,
        }).signal === "SIGTERM",
    // A Go toolchain.
    go: () => spawnSync("go", ["version"], { stdio: "ignore" }).status === 0,
    // sudo, which runs a program as root without asking for a password, for
    // a user other than root.
    sudo: () =>
        process.getuid?.() !== 0 &&
        spawnSync("sudo", ["-n", "true"], { stdio: "ignore" }).status === 0,
    // Xcode itself, beside the command line tools: xcodebuild.
    xcode: () => spawnSync("xcodebuild", ["-version"], { stdio: "ignore" }).status === 0,
};

const found = new Map<Need, boolean>();

/// Whether the machine has `need`.
export function has(need: Need): boolean {
    let result = found.get(need);
    if (result === undefined) {
        result = PROBES[need]();
        found.set(need, result);
    }
    return result;
}

/// Whether the tests run on CI, where a test runs whatever it needs.
export function onCI(): boolean {
    return process.env["CI"] !== undefined && process.env["CI"] !== "";
}
