import { debug, io, os } from "catter";

debug.assertThrow(
  os.platform() == "linux" ||
    os.platform() == "windows" ||
    os.platform() == "macos",
);

debug.assertThrow(
  os.arch() == "x86" ||
    os.arch() == "x64" ||
    os.arch() == "arm" ||
    os.arch() == "arm64",
);

io.println(`Operating System: ${os.platform()}`);
io.println(`Architecture: ${os.arch()}`);
