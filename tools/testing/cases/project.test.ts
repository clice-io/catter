import * as fs from "node:fs";
import * as os from "node:os";
import * as path from "node:path";
import { afterEach, describe, expect, test } from "vitest";
import { REPO_ROOT } from "../../repo.ts";
import { linkSdk } from "./project.ts";

describe("linkSdk", () => {
    const dirs: string[] = [];
    afterEach(() => {
        for (const dir of dirs.splice(0)) {
            fs.rmSync(dir, { recursive: true, force: true });
        }
    });

    function copy(files: Record<string, string>): string {
        const root = fs.mkdtempSync(path.join(os.tmpdir(), "catter-link-"));
        dirs.push(root);
        for (const [name, text] of Object.entries(files)) {
            fs.writeFileSync(path.join(root, name), text);
        }
        return root;
    }

    test("links the SDK for a package.json that depends on it", () => {
        const root = copy({
            "package.json": JSON.stringify({ dependencies: { "@catter/sdk": "*" } }),
        });
        linkSdk(root);
        expect(fs.realpathSync(path.join(root, "node_modules", "@catter", "sdk"))).toBe(
            fs.realpathSync(path.join(REPO_ROOT, "sdk", "typescript")),
        );
    });

    test("leaves any other copy alone", () => {
        const bare = copy({});
        const other = copy({ "package.json": JSON.stringify({ dependencies: { diff: "*" } }) });
        linkSdk(bare);
        linkSdk(other);
        expect(fs.existsSync(path.join(bare, "node_modules"))).toBe(false);
        expect(fs.existsSync(path.join(other, "node_modules"))).toBe(false);
    });
});
