import { assertThrow } from "catter/debug";
import { println } from "catter/io";
import { arch, platform } from "catter/os";

assertThrow(
  platform() == "linux" || platform() == "windows" || platform() == "macos",
);

assertThrow(
  arch() == "x86" || arch() == "x64" || arch() == "arm" || arch() == "arm64",
);

println(`Operating System: ${platform()}`);
println(`Architecture: ${arch()}`);
