#!/bin/sh

SOURCE_APP="buck-out/gen/ios/Stun#dwarf-and-dsym,iphoneos-arm64,no-include-frameworks/Stun.app"
TMP_APP="/tmp/Stun.app"

PROVISION_FILE="/tmp/stun_provision.plist"
ENTITLEMENTS_FILE="/tmp/stun_entitlements.plist"

pushd "$(buck root)"

buck build "//ios:Stun#iphoneos-arm64"

rm -rf "$TMP_APP"
cp -r "$SOURCE_APP" "$TMP_APP"

security cms -D -i certs/Stun_Dev.mobileprovision > "$PROVISION_FILE"

pushd "/tmp"
/usr/libexec/PlistBuddy -x -c 'Print :Entitlements' "$PROVISION_FILE" > "$ENTITLEMENTS_FILE"
/usr/bin/codesign -f -s 'iPhone Developer: Jiahao Li' --entitlements "$ENTITLEMENTS_FILE" "$TMP_APP"

ios-deploy --debug --bundle "$TMP_APP"
popd

popd
