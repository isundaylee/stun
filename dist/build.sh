#!/bin/bash

pushd "$(dirname "$0")"
cd ..
buck build @.buckconfig.release :main
cp buck-out/gen/main dist/stun
cd dist
zip stun.zip *.example stun
popd
