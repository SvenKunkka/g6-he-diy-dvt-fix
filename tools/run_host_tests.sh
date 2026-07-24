#!/bin/sh
set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
project_dir=$(CDPATH= cd -- "$script_dir/.." && pwd)
build_dir="$project_dir/build-host"

mkdir -p "$build_dir"
cc -std=c11 -Wall -Wextra -Werror \
	-I"$project_dir/include" \
	"$project_dir/src/g6_config.c" \
	"$project_dir/src/g6_input_guard.c" \
	"$project_dir/tests/test_config.c" \
	-o "$build_dir/test_config"
"$build_dir/test_config"
