#!/bin/bash
# usage:
#   sudo nice --adjustment=-10 env PS_BRANCH=8.0 RUN_TIME_SECONDS=30 THREADS_LIST="8" WORKLOAD_NAME=point_select.txt ROOT_DIR=/mnt/optane/auto-perf-test /mnt/optane/auto-perf-test/perf-tests/auto-perf-test.sh
# or add with "crontab -e":
# 0 18 * * * sudo nice --adjustment=-10 env PS_BRANCH=8.0 WORKLOAD_NAME=mdcallag/daily.txt TEMPLATE_PATH=/mnt/fast/template_datadir /mnt/fast/przemek/perf-tests/auto-perf-test.sh
#
# to kill all deps: sudo killall -9 ps-performance-test.sh auto-perf-test.sh mysqld sysbench dstat iostat

function install_deps_debian() {
    export DEBIAN_FRONTEND=noninteractive
    local PACKAGES_TO_INSTALL="smartmontools g++ dstat mutt ca-certificates git pkg-config dpkg-dev make cmake ccache bison python-is-python3 python3-pip linux-tools-$(uname -r)"
    local PACKAGES_LIBS="libtirpc-dev libgflags-dev libxml-simple-perl libeatmydata1 libfido2-dev libicu-dev libevent-dev libudev-dev libaio-dev libmecab-dev libnuma-dev liblz4-dev libzstd-dev libedit-dev libpam-dev libssl-dev libcurl4-openssl-dev libldap2-dev libkrb5-dev libsasl2-dev libsasl2-modules-gssapi-mit"
    local PACKAGES_PROTOBUF="protobuf-compiler libprotobuf-dev libprotoc-dev"
    command -v sendmail >/dev/null 2>&1 || { PACKAGES_TO_INSTALL+=" sendmail"; }
    sudo apt update
    sudo apt -yq --no-install-suggests --no-install-recommends --allow-unauthenticated install $PACKAGES_TO_INSTALL $PACKAGES_LIBS $PACKAGES_PROTOBUF $SELECTED_CXX
    pip install requests pandas tabulate
}

function setup_git_repo() {
    if [ $# -lt 1 ]; then echo "Usage: setup_git_repo <REPO_DIR> [GIT_BRANCH] [GIT_REPO]"; return 1; fi
    local REPO_DIR=$1
    local GIT_BRANCH=$2
    local GIT_REPO=$3

    if [ ! -d "${REPO_DIR}" ]; then
        git clone "${GIT_REPO}" "${REPO_DIR}"
    fi

    if [ ! -d "${REPO_DIR}" ]; then return 1; fi

    pushd $REPO_DIR
    git remote set-url origin "${GIT_REPO}"
    git fetch --all

    git reset --hard
    git clean -xdf
    git checkout "origin/${GIT_BRANCH}" || git checkout "tags/${GIT_BRANCH}"
    git submodule update --init
    popd
}

function call_cmake() {
    if [ $# -lt 2 ]; then echo "Usage: call_cmake <REPO_DIR> <BUILD_DIR>"; return 1; fi
    local REPO_DIR=$1
    local BUILD_DIR=$2
    local BUILD_TYPE="RelWithDebInfo"
    local BOOST_DIR="../_deps"
    echo "SELECTED_CC=$SELECTED_CC (`which $SELECTED_CC`) SELECTED_CXX=$SELECTED_CXX (`which $SELECTED_CXX`) BUILD_TYPE=$BUILD_TYPE"

    rm -rf $BUILD_DIR
    mkdir -p $BUILD_DIR
    pushd $BUILD_DIR
    OPTIONS_DEBUG="-DCMAKE_C_FLAGS_DEBUG=-g1 -DCMAKE_CXX_FLAGS_DEBUG=-g1"
    OPTIONS_BUILD="-DMYSQL_MAINTAINER_MODE=OFF -DCMAKE_BUILD_TYPE=$BUILD_TYPE -DBUILD_CONFIG=mysql_release -DWITH_PACKAGE_FLAGS=OFF -DDOWNLOAD_BOOST=1 -DWITH_BOOST=$BOOST_DIR"
    OPTIONS_COMPILER="-DCMAKE_C_COMPILER=$SELECTED_CC -DCMAKE_CXX_COMPILER=$SELECTED_CXX -DCMAKE_C_COMPILER_LAUNCHER=ccache -DCMAKE_CXX_COMPILER_LAUNCHER=ccache"
    OPTIONS_COMPONENTS="-DWITH_ROCKSDB=ON -DWITH_COREDUMPER=ON -DWITH_COMPONENT_KEYRING_VAULT=ON -DWITH_PAM=ON -DWITH_NUMA=ON"
    OPTIONS_LIBS="-DWITH_MECAB=system -DWITH_SYSTEM_LIBS=ON -DWITH_EDITLINE=system -DWITH_ZLIB=bundled -DWITH_LZ4=bundled"
    OPTIONS_OPTIMIZED="-DWITH_ROUTER=OFF -DWITH_UNIT_TESTS=OFF"
    SELECTED_OPTIONS="$OPTIONS_DEBUG $OPTIONS_BUILD $OPTIONS_COMPILER $OPTIONS_COMPONENTS $OPTIONS_LIBS $OPTIONS_OPTIMIZED"
    echo "SELECTED_OPTIONS=$SELECTED_OPTIONS"
    cmake $REPO_DIR $SELECTED_OPTIONS
    cmake -L .
    popd
}

function build_sysbench() {
    if [ $# -lt 1 ]; then echo "Usage: build_sysbench <BUILD_DIR>"; return 1; fi
    local BUILD_DIR=$1

    sudo apt -y install make automake libtool pkg-config libaio-dev libmysqlclient-dev libssl-dev

    pushd $BUILD_DIR
    ./autogen.sh
    ./configure
    make -j$(nproc)
    ./src/sysbench --version
    popd
}

function build_ps() {
    if [ $# -lt 1 ]; then echo "Usage: build_ps <BUILD_DIR>"; return 1; fi
    local BUILD_DIR=$1

    echo "SELECTED_CC=$SELECTED_CC (`which $SELECTED_CC`) SELECTED_CXX=$SELECTED_CXX (`which $SELECTED_CXX`) BUILD_TYPE=$BUILD_TYPE"
    pushd $BUILD_DIR
    NPROC=`nproc --all`
    echo "Using $NPROC threads for compilation"
    rm -f bin/mysqld
    make -j${NPROC}
    if [[ $? != 0 ]]; then echo make failed; exit -1; fi
    ccache --show-stats
    df -Th
    popd
}

function run_perf_tests() {
    if [ $# -lt 4 ]; then echo "Usage: run_perf_tests <MAIN_DIR> <BUILD_PATH> <PERFTEST_PATH> <SYSBENCH_REPO_DIR>"; return 1; fi
    local MAIN_DIR=$1
    local BUILD_PATH=$2
    local PERFTEST_PATH=$3
    local SYSBENCH_REPO_DIR=$4

    # mysqld and sysbench parameters
    export INNODB_CACHE=${INNODB_CACHE:-96G}
    export NUM_TABLES=${NUM_TABLES:-16}
    export DATASIZE=${DATASIZE:-10M}
    export RUN_TIME_SECONDS=${RUN_TIME_SECONDS:-300}
    export THREADS_LIST=${THREADS_LIST:-"8 16 32 64"}

    # path to template databases
    export TEMPLATE_PATH=${TEMPLATE_PATH:-$MAIN_DIR/template_datadir}
    # path to work directory and results
    export WORKSPACE=${WORKSPACE:-$MAIN_DIR/perf-results}

    export SYSBENCH_BIN=$SYSBENCH_REPO_DIR/src/sysbench
    export SYSBENCH_LUA=$SYSBENCH_REPO_DIR/src/lua

    # path to files from https://github.com/Percona-QA/perf-tests
    CNFFILE_NAME=${CNFFILE_NAME:-stable8x-innodb.cnf}
    WORKLOAD_NAME=${WORKLOAD_NAME:-read_write.txt}
    export WORKLOAD_SCRIPT=${WORKLOAD_SCRIPT:-${PERFTEST_PATH}/workloads/${WORKLOAD_NAME}}

    REPEAT_NUM=${REPEAT_NUM:-1}
    for i in $(seq $REPEAT_NUM); do
        local NICE_DATE=$(date +"%Y-%m-%d_%H:%M")
        ${PERFTEST_PATH}/ps-performance-test.sh ${PS_BRANCH}@${PS_GIT_HASH}_${NICE_DATE} ${BUILD_PATH} "${PERFTEST_PATH}/cnf/${CNFFILE_NAME}"
    done
}

SELECTED_CC=${SELECTED_CC:-gcc-13}
SELECTED_CXX=${SELECTED_CXX:-g++-13}
ROOT_DIR=${ROOT_DIR:-/mnt/fast/auto-perf-test}
export RESULTS_EMAIL=${RESULTS_EMAIL:-przemyslaw.skibinski@percona.com}

PS_REPO_DIR=${PS_REPO_DIR:-$ROOT_DIR/sources}
PS_REPO_URL=${PS_REPO_URL:-https://github.com/percona/percona-server}
PS_BRANCH=${PS_BRANCH:-8.0}
PS_BUILD_DIR=${PS_BUILD_DIR:-$ROOT_DIR/$PS_BRANCH-rel-$SELECTED_CC}

SYSBENCH_REPO_DIR=${SYSBENCH_REPO_DIR:-$ROOT_DIR/sysbench}
SYSBENCH_REPO_URL=${SYSBENCH_REPO_URL:-https://github.com/inikep/sysbench}
SYSBENCH_BRANCH=${SYSBENCH_BRANCH:-mdcallag}

PERF_TESTS_REPO_DIR=${PERF_TESTS_REPO_DIR:-$ROOT_DIR/perf-tests}
PERF_TESTS_REPO_URL=${PERF_TESTS_REPO_URL:-https://github.com/Percona-QA/perf-tests.git}
PERF_TESTS_BRANCH=${PERF_TESTS_BRANCH:-main}

mkdir -p ${ROOT_DIR} > /dev/null 2>&1
install_deps_debian | tee $PS_BUILD_DIR-install-deps.log
setup_git_repo $PERF_TESTS_REPO_DIR $PERF_TESTS_BRANCH $PERF_TESTS_REPO_URL | tee $PS_BUILD_DIR-setup-perf-tests-repo.log
setup_git_repo $SYSBENCH_REPO_DIR $SYSBENCH_BRANCH $SYSBENCH_REPO_URL | tee $PS_BUILD_DIR-setup-sysbench-repo.log
setup_git_repo $PS_REPO_DIR $PS_BRANCH $PS_REPO_URL| tee $PS_BUILD_DIR-setup-ps-repo.log

pushd $PS_REPO_DIR; PS_GIT_HASH=$(git rev-parse --short HEAD); popd
echo "PS_GIT_HASH=$PS_GIT_HASH PS_REPO_URL=$PS_REPO_URL PS_BRANCH=$PS_BRANCH"

build_sysbench $SYSBENCH_REPO_DIR | tee $PS_BUILD_DIR-sysbench-make.log
call_cmake $PS_REPO_DIR $PS_BUILD_DIR | tee $PS_BUILD_DIR-cmake.log
build_ps $PS_BUILD_DIR | tee $PS_BUILD_DIR-make.log
run_perf_tests $ROOT_DIR $PS_BUILD_DIR $PERF_TESTS_REPO_DIR $SYSBENCH_REPO_DIR | tee $PS_BUILD_DIR-perf-test.log
