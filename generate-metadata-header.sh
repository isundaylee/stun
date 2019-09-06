#!/bin/bash

version="0.10.1"
git_commit_id="$(git rev-parse HEAD)"

if git diff-index --quiet HEAD --; then
    git_dirty_status="false";
else
    git_dirty_status="true";
fi

cat > $OUT <<EOL
#pragma once

#include <string>

const std::string kVersion = "$version";
const std::string kGitCommitId = "$git_commit_id";
const bool kGitDirtyStatus = $git_dirty_status;
EOL