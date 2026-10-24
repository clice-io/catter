// @ts-check

import { defineConfig } from "eslint/config";
import tseslint from "typescript-eslint";

export default defineConfig(
    {
        ignores: [
            "**/node_modules/**",
            ".git/**",
            ".pixi/**",
            ".xmake/**",
            ".cache/**",
            "build/**",
            "api/build/**",
            "api/dist/**",
            "api/types/**",
            "scripts/build/**",
            "scripts/dist/**",
            "scripts/output/**",
            "release-assets/**",
            "**/.turbo/**",
            // Linted by their own configs, with the type-checked rules.
            "tools/**",
            "tests/**",
        ],
    },
    tseslint.configs.recommended,
    {
        rules: {
            "@typescript-eslint/no-namespace": ["off"],
            "@typescript-eslint/no-unused-vars": ["off"],
        },
    },
);
