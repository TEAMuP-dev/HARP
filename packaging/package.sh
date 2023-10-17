#!/bin/bash

# Usage:
# Set environment variables or pass arguments
# ./package/package.sh <Developer ID Application> <Artifacts Path> <Project Name> <Product Name> <Notarization Username> <Notarization Password> <Team ID>

# Function to get the value from either the environment variable or the command-line argument
set -e 
trap 'echo "Error on line $LINENO: command failed with status $?" >&2' ERR

DEV_ID_APPLICATION=$1
ARTIFACTS_PATH=$2
PROJECT_NAME=$3
PRODUCT_NAME=$4
NOTARIZATION_USERNAME=$5
NOTARIZATION_PASSWORD=$6
TEAM_ID=$7

# check if the version file exists
if [ ! -f "VERSION" ]; then
    echo "Error: VERSION not found. are you in the root directory of the project?" >&2
    exit 1
fi

VERSION=$(cat ./VERSION)

# Check if the necessary variables are set
for var in DEV_ID_APPLICATION ARTIFACTS_PATH PROJECT_NAME PRODUCT_NAME NOTARIZATION_USERNAME NOTARIZATION_PASSWORD TEAM_ID; do
    if [ -z "${!var}" ]; then
        echo "Error: $var is not set" >&2
        exit 1
    fi
done

# Install appdmg
npm install -g appdmg

# Create necessary directories
rm -rf packaging/dmg
rm packaging/${PRODUCT_NAME}.dmg || true
mkdir -p packaging/dmg

# Create directories for the dmg symlinks
sudo mkdir -m 755 -p /Library/Audio/Plug-Ins/Components && sudo mkdir -m 755 -p /Library/Audio/Plug-Ins/VST3
ln -s /Library/Audio/Plug-Ins/VST3 "packaging/dmg/Your Mac's VST3 folder"
cp -r "${ARTIFACTS_PATH}/VST3/${PROJECT_NAME}.vst3" packaging/dmg

# Make sign.sh executable and run it
chmod +x ./py/client/sign.sh
./py/client/sign.sh "$DEV_ID_APPLICATION" "packaging/dmg/${PROJECT_NAME}.vst3/Contents/Resources/gradiojuce_client/"

# Sign the application
codesign -s "$DEV_ID_APPLICATION" --timestamp -i com.HARP.HARP --options runtime --force "packaging/dmg/${PROJECT_NAME}.vst3/Contents/Resources/gradiojuce_client/gradiojuce_client"
codesign -s "$DEV_ID_APPLICATION" --timestamp -i com.HARP.HARP --force "packaging/dmg/${PROJECT_NAME}.vst3/Contents/MacOS/HARP"
codesign -s "$DEV_ID_APPLICATION" --timestamp -i com.HARP.HARP --force "packaging/dmg/${PROJECT_NAME}.vst3"

# Create the .dmg
cd packaging && appdmg dmg.json "${PRODUCT_NAME}.dmg"

# Sign the .dmg
codesign -s "$DEV_ID_APPLICATION" --deep --timestamp -i com.HARP.HARP --force "${PRODUCT_NAME}.dmg" 

# Notarize the .dmg
xcrun notarytool submit "${PRODUCT_NAME}.dmg" --apple-id "$NOTARIZATION_USERNAME" --password "$NOTARIZATION_PASSWORD" --team-id "$TEAM_ID" --wait
xcrun stapler staple "${PRODUCT_NAME}.dmg"

mv "${PRODUCT_NAME}.dmg" "${PRODUCT_NAME}-MacOS-${VERSION}.dmg"