# Distribution

### MacOS
Codesigning and packaging for distribution is done through the script located at `packaging/package.sh`.
You'll need to set up a developer account with Apple and create a certificate in order to sign the plugin.
For more information on codesigning and notarization for macOS, please refer to the [pamplejuce](https://github.com/sudara/pamplejuce) template.

The script requires the following variables to be passed:
```
# Retrieve values from either environment variables or command-line arguments
DEV_ID_APPLICATION # Developer ID Application certificate
ARTIFACTS_PATH # should be packaging/dmg/HARP.app
PROJECT_NAME # "HARP"
PRODUCT_NAME # "HARP"
NOTARIZATION_USERNAME # Apple ID
NOTARIZATION_PASSWORD # App-specific password for notarization
TEAM_ID # Team ID for notarization
```

Usage:
```bash
./HARP/packaging/package.sh <DEV_ID_APPLICATION> <ARTIFACTS_PATH> <PROJECT_NAME> <PRODUCT_NAME> <NOTARIZATION_USERNAME> <NOTARIZATION_PASSWORD> <TEAM_ID>
```

After running `package.sh`, you should have a signed and notarized dmg file in the `packaging/` directory.

### Windows

(Coming soon)

### Linux

(Coming soon)
