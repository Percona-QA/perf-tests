#!/bin/bash
# usage:
#   sudo nice --adjustment=-10 env PS_BRANCH=8.0 WRITES_TIME_SECONDS=30 THREADS_LIST="8" WORKLOAD_NAMES=POINT_SELECT ROOT_DIR=/mnt/optane/auto-perf-test /mnt/optane/auto-perf-test/perf-tests/auto-db-bench.sh
# or add with "crontab -e":
# 0 18 * * * sudo nice --adjustment=-10 env PS_BRANCH=8.0 WORKLOAD_NAMES=reads,writes TEMPLATE_PATH=/mnt/fast/template_datadir /mnt/fast/przemek/perf-tests/auto-db-bench.sh
#            sudo nice --adjustment=-10 bash -c "./run-ACID-shm.sh >run-ACID-shm.txt 2>&1"
#
# to kill all deps: sudo killall -9 db-bench.sh auto-db-bench.sh mysqld postgres sysbench dstat iostat

function install_deps_debian() {
    export DEBIAN_FRONTEND=noninteractive
    local PACKAGES_TO_INSTALL="smartmontools g++ dstat mutt ca-certificates git pkg-config dpkg-dev make cmake ccache bison python-is-python3 python3-pip linux-tools-$(uname -r)"
    command -v sendmail >/dev/null 2>&1 || { PACKAGES_TO_INSTALL+=" sendmail"; }
    sudo apt update
    sudo apt -yq --no-install-suggests --no-install-recommends --allow-unauthenticated install $PACKAGES_TO_INSTALL $SELECTED_CXX
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
    git checkout "origin/${GIT_BRANCH}" || git checkout "tags/${GIT_BRANCH}" || git checkout "${GIT_BRANCH}"
    if [[ $? != 0 ]]; then echo "git checkout ${GIT_BRANCH} failed"; exit -1; fi

    git submodule update --init
    popd
}

function build_sysbench() {
    if [ $# -lt 1 ]; then echo "Usage: build_sysbench <BUILD_DIR>"; return 1; fi
    local BUILD_DIR=$1

    sudo apt -y install make automake libtool pkg-config libaio-dev libmysqlclient-dev libpq-dev libssl-dev

    pushd $BUILD_DIR
    ./autogen.sh
    ./configure --with-pgsql
    make -j$(nproc)
    ./src/sysbench --version
    popd
}

function run_perf_tests() {
    if [ $# -lt 4 ]; then echo "Usage: run_perf_tests <MAIN_DIR> <BUILD_PATH> <PERFTEST_PATH> <SYSBENCH_REPO_DIR>"; return 1; fi
    local MAIN_DIR=$1
    export BUILD_PATH=$2
    local PERFTEST_PATH=$3
    local SYSBENCH_REPO_DIR=$4

    # mysqld/postgres and sysbench parameters
    export ENGINE_CACHE=${ENGINE_CACHE:-96G}
    export NUM_TABLES=${NUM_TABLES:-16}
    export DATASIZE=${DATASIZE:-10M}
    export WRITES_TIME_SECONDS=${WRITES_TIME_SECONDS:-300}
    export THREADS_LIST=${THREADS_LIST:-"8 16 32 64"}

    # path to template databases
    export TEMPLATE_PATH=${TEMPLATE_PATH:-$MAIN_DIR/template_datadir}
    # path to work directory and results
    export WORKSPACE=${WORKSPACE:-$MAIN_DIR/bench-results}

    export SYSBENCH_BIN=$SYSBENCH_REPO_DIR/src/sysbench
    export SYSBENCH_LUA=$SYSBENCH_REPO_DIR/src/lua

    # path to files from https://github.com/Percona-QA/perf-tests/3.0
    CNFFILE_NAME=${CNFFILE_NAME:-cnf/stable-innodb.cnf}
    export CONFIG_FILES=${CONFIG_FILES:-"${PERFTEST_PATH}/${CNFFILE_NAME}"}

    REPEAT_NUM=${REPEAT_NUM:-1}
    for i in $(seq $REPEAT_NUM); do
        local NICE_DATE=$(date +"%Y-%m-%d_%H:%M")
        export BENCH_NAME=${PS_BRANCH:0:30}@${PS_GIT_HASH}_${NICE_DATE}
        ${PERFTEST_PATH}/db-bench.sh
    done
}

SELECTED_CC=${SELECTED_CC:-gcc-13}
SELECTED_CXX=${SELECTED_CXX:-g++-13}
ROOT_DIR=${ROOT_DIR:-/mnt/fast/auto-db-bench}
export RESULTS_EMAIL=${RESULTS_EMAIL:-przemyslaw.skibinski@percona.com}

if [[ ${ENGINE} == "postgres" ]]; then
    PS_REPO_DIR=${PS_REPO_DIR:-$ROOT_DIR/postgres}
    PS_REPO_URL=${PS_REPO_URL:-https://github.com/percona/postgres}
    PS_BRANCH=${PS_BRANCH:-TDE_REL_17_STABLE}
else
    PS_REPO_DIR=${PS_REPO_DIR:-$ROOT_DIR/src_mysql}
    PS_REPO_URL=${PS_REPO_URL:-https://github.com/percona/percona-server}
    PS_BRANCH=${PS_BRANCH:-8.0}
fi
PS_BUILD_DIR=${PS_BUILD_DIR:-$ROOT_DIR/$PS_BRANCH-rel-$SELECTED_CC}
PS_BIN_DIR=${PS_BUILD_DIR}/bin

SYSBENCH_REPO_DIR=${SYSBENCH_REPO_DIR:-$ROOT_DIR/sysbench}
SYSBENCH_REPO_URL=${SYSBENCH_REPO_URL:-https://github.com/inikep/sysbench}
SYSBENCH_BRANCH=${SYSBENCH_BRANCH:-mdcallag}

DBBENCH_REPO_DIR=${DBBENCH_REPO_DIR:-$ROOT_DIR/db-bench}
DBBENCH_REPO_URL=${DBBENCH_REPO_URL:-https://github.com/Percona-QA/perf-tests.git}
DBBENCH_BRANCH=${DBBENCH_BRANCH:-3.0}

if [[ "${DBBENCH_SSL,,}" == "on" || "${DBBENCH_SSL}" == "1" ]]; then
    export SSL_CERTS_PATH=${DBBENCH_REPO_DIR}/cert
fi

mkdir -p ${ROOT_DIR} > /dev/null 2>&1
mkdir -p ${PS_BUILD_DIR} > /dev/null 2>&1
install_deps_debian | tee $PS_BUILD_DIR/install-deps.log
setup_git_repo $DBBENCH_REPO_DIR $DBBENCH_BRANCH $DBBENCH_REPO_URL | tee $PS_BUILD_DIR/setup-perf-tests-repo.log
setup_git_repo $SYSBENCH_REPO_DIR $SYSBENCH_BRANCH $SYSBENCH_REPO_URL | tee $PS_BUILD_DIR/setup-sysbench-repo.log
setup_git_repo $PS_REPO_DIR $PS_BRANCH $PS_REPO_URL| tee $PS_BUILD_DIR/setup-ps-repo.log

pushd $PS_REPO_DIR; PS_GIT_HASH=$(git rev-parse --short HEAD); popd
echo "PS_GIT_HASH=$PS_GIT_HASH PS_REPO_URL=$PS_REPO_URL PS_BRANCH=$PS_BRANCH"

build_sysbench $SYSBENCH_REPO_DIR | tee $PS_BUILD_DIR/sysbench-make.log
if [[ ${ENGINE} == "postgres" ]]; then
    build_postgres $PS_REPO_DIR $PS_BUILD_DIR | tee $PS_BUILD_DIR/make.log
else
    mysql_call_cmake $PS_REPO_DIR $PS_BIN_DIR | tee $PS_BUILD_DIR/cmake.log
    build_mysql $PS_BIN_DIR | tee $PS_BUILD_DIR/make.log
fi
run_perf_tests $ROOT_DIR $PS_BIN_DIR $DBBENCH_REPO_DIR $SYSBENCH_REPO_DIR | tee $PS_BUILD_DIR/perf-test.log
