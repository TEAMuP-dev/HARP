#!/bin/bash

# Packages HARP.app into a DMG, then code signs and notarizes it if credentials are provided.
#
# Usage: packaging/package.sh <path/to/HARP.app> <output.dmg>
#
# Optional environment variables (see docs/SIGNING.md):
#   MACOS_SIGNING_IDENTITY  Keychain identity used for signing, e.g. "Developer ID Application: Name (TEAMID)".
#                           If unset, the DMG is left unsigned.
#   APPLE_ID                Apple Account used for notarization. If unset, notarization is skipped.
#   APPLE_APP_PASSWORD      App-specific password for APPLE_ID.
#
# Requires dmgbuild (pipx install dmgbuild).

set -euo pipefail

if [ $# -ne 2 ]; then
    echo "Usage: $0 <path/to/HARP.app> <output.dmg>" >&2
    exit 1
fi

APP=$1
DMG=$2

if [ -n "${MACOS_SIGNING_IDENTITY:-}" ]; then
    # The hardened runtime and a secure timestamp are required for notarization
    codesign --force --timestamp --options runtime --sign "$MACOS_SIGNING_IDENTITY" "$APP"
fi

mkdir -p "$(dirname "$DMG")"
dmgbuild -s "$(dirname "$0")/dmg_settings.py" -D app="$APP" HARP "$DMG"

if [ -n "${MACOS_SIGNING_IDENTITY:-}" ]; then
    codesign --force --timestamp --sign "$MACOS_SIGNING_IDENTITY" "$DMG"

    if [ -n "${APPLE_ID:-}" ]; then
        # Submit under the team that signed the app
        TEAM_ID=$(codesign -dv "$APP" 2>&1 | sed -n 's/^TeamIdentifier=//p')

        # Notarizing the DMG covers the app inside it, and stapling lets Gatekeeper verify it offline
        xcrun notarytool submit "$DMG" --apple-id "$APPLE_ID" --password "$APPLE_APP_PASSWORD" \
            --team-id "$TEAM_ID" --wait
        xcrun stapler staple "$DMG"
    fi
fi
