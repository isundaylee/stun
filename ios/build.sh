#!/bin/sh

SOURCE_APP="buck-out/gen/ios/Stun#dwarf-and-dsym,iphoneos-arm64,no-include-frameworks/Stun.app"
TMP_APP="/tmp/Stun.app"
TMP_EXT="$TMP_APP/PlugIns/StunPacketTunnel.appex"
TMP_IPA="/tmp/Stun.ipa"
TMP_PAYLOAD="/tmp/Stun.ipa/Payload"

TMP_PROVISION_FILE="/tmp/stun_provision.plist"
TMP_ENTITLEMENTS_FILE="/tmp/stun_entitlements.plist"

CERT='iPhone Developer: Jiahao Li'
APP_PROVISION='certs/Stun_Dev.mobileprovision'
EXT_PROVISION='certs/Stun_Packet_Tunnel_Dev.mobileprovision'

function header {
    echo "############################################################"
    echo "#" $1
    echo "############################################################"
}

function resign {
    rm -r $1/_CodeSignature
    cp "$2" "$1/embedded.mobileprovision"
    security cms -D -i "$2" > "$TMP_PROVISION_FILE"
    /usr/libexec/PlistBuddy -x -c 'Print :Entitlements' "$TMP_PROVISION_FILE" > "$TMP_ENTITLEMENTS_FILE"
    /usr/bin/codesign -f -s "$3" -vv --entitlements "$TMP_ENTITLEMENTS_FILE" "$1"
}

pushd "$(buck root)" > /dev/null

# Building the app
header 'Building iOS app'
buck build "//ios:Stun#iphoneos-arm64"

rm -rf "$TMP_APP"
cp -r "$SOURCE_APP" "$TMP_APP"

# Sign the extension
header 'Re-signing the app extension'
resign $TMP_EXT "$EXT_PROVISION" "$CERT"

# Sign the app
header 'Re-signing the app'
resign $TMP_APP "$APP_PROVISION" "$CERT"

# Install
echo "Please use XCode to deploy $TMP_APP to the device."

popd
