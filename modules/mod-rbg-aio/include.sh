#!/usr/bin/env bash

MOD_RBG_AIO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/" && pwd)"

if [[ -f "$MOD_RBG_AIO_ROOT/conf/conf.sh.dist" ]]; then
  # shellcheck disable=SC1091
  source "$MOD_RBG_AIO_ROOT/conf/conf.sh.dist"
fi

if [[ -f "$MOD_RBG_AIO_ROOT/conf/conf.sh" ]]; then
  # shellcheck disable=SC1091
  source "$MOD_RBG_AIO_ROOT/conf/conf.sh"
fi
