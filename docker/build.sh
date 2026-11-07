#!/usr/bin/env bash
set -euo pipefail

usage() {
    echo "usage: $0 <amd64|arm64> <aasdk|openauto|dash|all> [extra cmake args...]" >&2
    echo "  env: BUILD_TYPE (default Release), RUN_TESTS=0 to skip aasdk unit tests on amd64," >&2
    echo "       REBUILD_IMAGE=1 to rebuild the docker image first" >&2
    exit 1
}

[[ $# -ge 2 ]] || usage
arch=$1
target=$2
shift 2

docker_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
src_root=$(cd "$docker_dir/../.." && pwd)

case $arch in
    amd64) image=opendsh-build:amd64; dockerfile=Dockerfile.amd64; toolchain=() ;;
    arm64) image=opendsh-build:arm64-cross; dockerfile=Dockerfile.arm64-cross
           toolchain=(-DCMAKE_TOOLCHAIN_FILE=/opt/toolchain-aarch64.cmake) ;;
    *) usage ;;
esac

case $target in
    aasdk|openauto|dash) repos=("$target") ;;
    all) repos=(aasdk openauto dash) ;;
    *) usage ;;
esac

if [[ ${REBUILD_IMAGE:-0} == 1 ]] || ! docker image inspect "$image" >/dev/null 2>&1; then
    docker build -t "$image" -f "$docker_dir/$dockerfile" "$docker_dir"
fi

build_type=${BUILD_TYPE:-Release}
run_tests=${RUN_TESTS:-1}
[[ $arch == amd64 ]] || run_tests=0

for repo in "${repos[@]}"; do
    build_dir=/src/_build/$arch/$repo
    stage=/src/_stage/$arch
    repo_args=()
    if [[ $repo == aasdk ]]; then
        repo_args=(-DAASDK_TEST=$([[ $run_tests == 1 ]] && echo ON || echo OFF))
    fi

    echo "==> $repo ($arch, $build_type)"
    docker run --rm \
        -u "$(id -u):$(id -g)" \
        -e HOME=/tmp \
        -v "$src_root:/src" \
        -w /src \
        "$image" \
        bash -euo pipefail -c '
            build_dir=$1; stage=$2; repo=$3; run_tests=$4; shift 4
            cmake -S "/src/$repo" -B "$build_dir" -G Ninja \
                -DCMAKE_INSTALL_PREFIX="$stage" \
                -DCMAKE_PREFIX_PATH="$stage" \
                "$@"
            cmake --build "$build_dir" --parallel
            cmake --install "$build_dir"
            if [[ $repo == aasdk && $run_tests == 1 ]]; then
                "$build_dir/aasdk_ut" --log_level=test_suite --report_level=short
            fi
        ' _ "$build_dir" "$stage" "$repo" "$run_tests" \
        -DCMAKE_BUILD_TYPE="$build_type" "${toolchain[@]}" "${repo_args[@]}" "$@"
done
