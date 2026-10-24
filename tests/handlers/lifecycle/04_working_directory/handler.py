# Makes, lists and removes files once the build is done, in its working
# directory, the copy's, whose name is past ASCII.
import os
import shutil

import catter


class Handler(catter.Handler):
    def on_finish(self, finish):
        os.makedirs("made/中文-é/dir")
        open("made/中文-é/file.txt", "w").close()
        print(f"listed: {', '.join(sorted(os.listdir('made/中文-é')))}")
        shutil.rmtree("made")
        print(f"removed: {not os.path.exists('made')}")


catter.serve(Handler())
