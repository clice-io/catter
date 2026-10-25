# Makes, lists and removes files once the build is done, in its working
# directory, the copy's, whose name is past ASCII, and starts a program that
# prints: its stdout is the handler's, which catter does not read.
import os
import shutil
import subprocess
import sys

import catter


class Handler(catter.Handler):
    def on_finish(self, finish):
        os.makedirs("made/中文-é/dir")
        open("made/中文-é/file.txt", "w").close()
        print(f"listed: {', '.join(sorted(os.listdir('made/中文-é')))}", flush=True)
        shutil.rmtree("made")
        print(f"removed: {not os.path.exists('made')}", flush=True)
        subprocess.run([sys.executable, "-c", "print('printed by a program the handler started')"])


catter.serve(Handler())
