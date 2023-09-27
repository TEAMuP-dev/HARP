SIGN_ID="$1"
ARTIFACTS_DIR="$2"

if [[ -z "$SIGN_ID" || -z "$ARTIFACTS_DIR" ]]; then
    echo "Usage: $0 <signing-identity> <artifacts-directory>"
    exit 1
fi

# Find all .dylib and .so files under the specified directory
find "$ARTIFACTS_DIR" -type f \( -name "*.dylib" -o -name "*.so" \) | while read artifact; do
    echo "Code signing $artifact"
    codesign -s "$SIGN_ID" -vvv --timestamp --deep -i com.HARP.HARP --force --entitlements "./py/client/entitlements.plist" "$artifact"
done

echo "Code signing gradiojuce_client directory"
codesign -s "$SIGN_ID" -vvv --timestamp --deep -i com.HARP.HARP --force --entitlements "./py/client/entitlements.plist" "$ARTIFACTS_DIR/gradiojuce_client"
