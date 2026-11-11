# Layout of the HARP installer window, for dmgbuild (https://dmgbuild.readthedocs.io).
# The app path is passed in by package.sh with -D app=<path>.

import os.path


app = defines["app"]

format = "UDZO"
files = [app]
symlinks = {"Applications": "/Applications"}

window_rect = ((200, 120), (480, 280))
icon_size = 80
icon_locations = {
    os.path.basename(app): (125, 120),
    "Applications": (355, 120),
}
