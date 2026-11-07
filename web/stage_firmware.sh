#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Puts a built package on the site for the installer (site/firmware/): the same files the pages workflow fetches from a release.
#   web/stage_firmware.sh [build/drumlab.fwsc] [version]
set -e
cd "$(dirname "$0")/.."
f="${1:-build/drumlab.fwsc}"; v="${2:-dev}"
mkdir -p site/firmware
cp "$f" site/firmware/
sha=$(shasum -a 256 "$f" | cut -d' ' -f1)
printf '{"version":"%s","file":"%s","sha256":"%s"}\n' "$v" "$(basename "$f")" "$sha" > site/firmware/latest.json
cat site/firmware/latest.json
