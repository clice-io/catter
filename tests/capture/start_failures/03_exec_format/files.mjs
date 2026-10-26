// Writes the files the build starts, executable: a script without #!, which
// the system does not start but execvp gives to /bin/sh, and a script whose
// interpreter is not there.
import { chmodSync, writeFileSync } from "node:fs";

const files = {
  "no-format": "echo run by /bin/sh\n",
  "lost-interpreter": "#!/definitely/missing/sh\necho never\n",
};
for (const [name, content] of Object.entries(files)) {
  writeFileSync(name, content);
  chmodSync(name, 0o755);
}
