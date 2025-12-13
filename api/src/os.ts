import { os_arch, os_name } from "catter-c";

export {};

export function platfrom(): "linux" | "windows" | "macos" {
  return os_name();
}

export function arch(): "x86" | "x64" | "arm" | "arm64" {
  return os_arch();
}

export function dir_sep(): "/" | "\\" {
  return os_name() === "windows" ? "\\" : "/";
}
