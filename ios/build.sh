#!/bin/sh

SOURCE_APP="buck-out/gen/ios/Stun#dwarf-and-dsym,iphoneos-arm64,no-include-frameworks/Stun.app"
TMP_APP="/tmp/Stun.app"
TMP_EXT="/tmp/Stun.app/PlugIns/StunPacketTunnel.appex"
TMP_IPA="/tmp/Stun.ipa"
TMP_PAYLOAD="/tmp/Stun.ipa/Payload"

PROVISION_FILE="/tmp/stun_provision.plist"
ENTITLEMENTS_FILE="/tmp/stun_entitlements.plist"

pushd "$(buck root)"

buck build "//ios:Stun#iphoneos-arm64"

rm -rf "$TMP_APP"
cp -r "$SOURCE_APP" "$TMP_APP"

# Sign the extension
rm -r $TMP_EXT/_CodeSignature
cp certs/Stun_Packet_Tunnel_Dev.mobileprovision "$TMP_EXT/embedded.mobileprovision"
security cms -D -i certs/Stun_Packet_Tunnel_Dev.mobileprovision > "$PROVISION_FILE"
/usr/libexec/PlistBuddy -x -c 'Print :Entitlements' "$PROVISION_FILE" > "$ENTITLEMENTS_FILE"
/usr/bin/codesign -f -s 'iPhone Developer: Jiahao Li' -vv --entitlements "$ENTITLEMENTS_FILE" "$TMP_EXT"

# Sign the app
rm -r $TMP_APP/_CodeSignature
cp certs/Stun_Dev.mobileprovision "$TMP_APP/embedded.mobileprovision"
security cms -D -i certs/Stun_Dev.mobileprovision > "$PROVISION_FILE"
/usr/libexec/PlistBuddy -x -c 'Print :Entitlements' "$PROVISION_FILE" > "$ENTITLEMENTS_FILE"
/usr/bin/codesign -f -s 'iPhone Developer: Jiahao Li' -vv --entitlements "$ENTITLEMENTS_FILE" "$TMP_APP"

# Install
echo "Please use XCode to deploy $TMP_APP to the device."

popd
