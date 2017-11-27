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
cp certs/Stun_Packet_Tunnel_Distr.mobileprovision "$TMP_EXT/embedded.mobileprovision"
security cms -D -i certs/Stun_Packet_Tunnel_Distr.mobileprovision > "$PROVISION_FILE"
/usr/libexec/PlistBuddy -x -c 'Print :Entitlements' "$PROVISION_FILE" > "$ENTITLEMENTS_FILE"
/usr/bin/codesign -f -s 'iPhone Distribution: Jiahao Li' -vv --entitlements "$ENTITLEMENTS_FILE" "$TMP_EXT"

# Sign the app
rm -r $TMP_APP/_CodeSignature
cp certs/Stun_Distr.mobileprovision "$TMP_APP/embedded.mobileprovision"
security cms -D -i certs/Stun_Distr.mobileprovision > "$PROVISION_FILE"
/usr/libexec/PlistBuddy -x -c 'Print :Entitlements' "$PROVISION_FILE" > "$ENTITLEMENTS_FILE"
/usr/bin/codesign -f -s 'iPhone Distribution: Jiahao Li' -vv --entitlements "$ENTITLEMENTS_FILE" "$TMP_APP"

rm -r $TMP_IPA
mkdir -p $TMP_PAYLOAD
cp -r $TMP_APP $TMP_PAYLOAD
cd $TMP_IPA
zip -r $TMP_IPA/Stun.resigned.ipa Payload

# Install
echo "Please use XCode to deploy $TMP_APP to the device."

popd
