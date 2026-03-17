import { os_arch, os_name } from "catter-c";

export {};

/**
 * Returns the current operating system name.
 *
 * @returns The platform identifier used by catter.
 *
 * @example
 * ```typescript
 * if (platform() === "windows") {
 *   println("running on Windows");
 * }
 * ```
 */
export function platform(): "linux" | "windows" | "macos" {
  return os_name();
}

/**
 * Returns the current CPU architecture.
 *
 * @returns The architecture identifier reported by catter.
 *
 * @example
 * ```typescript
 * println("current arch: " + arch());
 * ```
 */
export function arch(): "x86" | "x64" | "arm" | "arm64" {
  return os_arch();
}
