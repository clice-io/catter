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
      "api/output/**",
      "scripts/build/**",
      "scripts/output/**",
      "release-assets/**",
      "**/.turbo/**",
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
