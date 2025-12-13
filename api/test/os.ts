import { debug, io, os } from "catter";

debug.assert_throw(
  os.platfrom() == "linux" ||
    os.platfrom() == "windows" ||
    os.platfrom() == "macos",
);

debug.assert_throw(
  os.arch() == "x86" ||
    os.arch() == "x64" ||
    os.arch() == "arm" ||
    os.arch() == "arm64",
);

io.println(`Operating System: ${os.platfrom()}`);
io.println(`Architecture: ${os.arch()}`);
