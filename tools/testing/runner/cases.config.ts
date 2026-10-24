import * as os from "node:os";
import { defineConfig } from "vitest/config";

export default defineConfig({
    test: {
        root: import.meta.dirname,
        include: ["cases.ts"],
        globalSetup: ["summary.ts"],
        pool: "forks",
        // Every case is a suite of its own: without this, vitest runs them
        // one after another and only a case's own tests side by side.
        sequence: { concurrent: true },
        // A test runs a build: one per core.
        maxConcurrency: os.availableParallelism(),
        testTimeout: 300_000,
        hookTimeout: 60_000,
    },
});
