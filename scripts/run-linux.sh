#!/usr/bin/env bash
set -eu
root_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
export MEDICAL_DB_HOST="${MEDICAL_DB_HOST:-localhost}"
export MEDICAL_DB_NAME="${MEDICAL_DB_NAME:-medical_system}"
export MEDICAL_DB_USER="${MEDICAL_DB_USER:-root}"
export MEDICAL_DB_PASSWORD="${MEDICAL_DB_PASSWORD-}"
exec "$root_dir/build/medical_system" "$@"
