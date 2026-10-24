# Layout of the HARP installer window, for dmgbuild (https://dmgbuild.readthedocs.io).
# The app path is passed in by package.sh with -D app=<path>.

import os.path

app = defines["app"]

format = "UDZO"
files = [app]
symlinks = {"Applications": "/Applications"}

window_rect = ((100, 100), (730, 520))
icon_size = 80
icon_locations = {
    os.path.basename(app): (250, 405),
    "Applications": (480, 405),
}
