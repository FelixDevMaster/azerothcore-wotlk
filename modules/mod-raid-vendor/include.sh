#!/usr/bin/env bash

MOD_RAID_VENDOR_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/" && pwd)"

if [[ -f "$MOD_RAID_VENDOR_ROOT/conf/conf.sh.dist" ]]; then
  # shellcheck disable=SC1091
  source "$MOD_RAID_VENDOR_ROOT/conf/conf.sh.dist"
fi

if [[ -f "$MOD_RAID_VENDOR_ROOT/conf/conf.sh" ]]; then
  # shellcheck disable=SC1091
  source "$MOD_RAID_VENDOR_ROOT/conf/conf.sh"
fi
