#!/bin/sh

SOURCE_APP="buck-out/gen/ios/Stun#dwarf-and-dsym,iphoneos-arm64,no-include-frameworks/Stun.app"
TMP_APP="/tmp/Stun.app"
TMP_EXT="$TMP_APP/PlugIns/StunPacketTunnel.appex"
TMP_PAYLOAD="/tmp/Stun.ipa/Payload"
TMP_IPA="/tmp/Stun.ipa"
TMP_IPA_OUTPUT="/tmp/Stun.resigned.ipa"

if [ $1 = "release" ]; then
    CERT='iPhone Distribution: Jiahao Li'
    APP_PROVISION='certs/Stun_Distr.mobileprovision'
    EXT_PROVISION='certs/Stun_Packet_Tunnel_Distr.mobileprovision'
else
    CERT='iPhone Developer: Jiahao Li'
    APP_PROVISION='certs/Stun_Dev.mobileprovision'
    EXT_PROVISION='certs/Stun_Packet_Tunnel_Dev.mobileprovision'
fi

function header {
    echo "############################################################"
    echo "#" $1
    echo "############################################################"
}

function resign {
    TMP_PROV="/tmp/stun_provision.plist"
    TMP_ENT="/tmp/stun_entitlements.plist"

    rm -r $1/_CodeSignature
    cp "$2" "$1/embedded.mobileprovision"
    security cms -D -i "$2" > "$TMP_PROV"
    /usr/libexec/PlistBuddy -x -c 'Print :Entitlements' "$TMP_PROV" > "$TMP_ENT"
    /usr/bin/codesign -f -s "$3" -vv --entitlements "$TMP_ENT" "$1"
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

if [ $1 = "release" ]; then
    # Generates the .ipa
    rm -rf $TMP_PAYLOAD
    mkdir -p $TMP_PAYLOAD
    cp -r $TMP_APP $TMP_PAYLOAD

    pushd $TMP_IPA > /dev/null

    header 'Generating .ipa'
    zip -qr $TMP_IPA_OUTPUT Payload

    popd

    echo "Distribution .ipa is generated at $TMP_IPA_OUTPUT"
else
    echo "Please use XCode to deploy $TMP_APP to the device."
fi

popd
