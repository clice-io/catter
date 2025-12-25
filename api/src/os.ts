import { os_arch, os_name } from "catter-c";

export {};

export function platform(): "linux" | "windows" | "macos" {
  return os_name();
}

export function arch(): "x86" | "x64" | "arm" | "arm64" {
  return os_arch();
}
