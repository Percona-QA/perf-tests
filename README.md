# db-bench.sh

A sysbench-based database benchmarking script. All options are configured through environment variables with sensible defaults.

## Environment Variables

### Directories

| Variable | Default | Description |
|---|---|---|
| `WORKSPACE` | `$PWD` | Root workspace directory. Used as the base for other directory defaults. |
| `TEMPLATE_PATH` | `$WORKSPACE/template_datadir` | Path to the template data directory used for preparing benchmark runs. |
| `CACHE_DIR` | `$WORKSPACE/results_cache` | Directory for caching benchmark results. |
| `BENCH_DIR` | `$WORKSPACE` | Base directory for benchmark output. A subdirectory named `$BENCH_NAME` is created within it. |
| `DATA_DIR` | `$WORKSPACE` | Base directory for database data. A subdirectory named `${BENCH_NAME}-datadir` is created within it. |
| `BENCH_NAME` | *(required)* | Name of the benchmark run. Used to derive `BENCH_DIR` and `DATA_DIR` subdirectory names. |

### General

| Variable | Default | Description |
|---|---|---|
| `ENGINE` | `innodb` | Database engine type. Supported values: `innodb`, `rocksdb`, `postgres`. |
| `BUILD_PATH` | *(required)* | Path to the database server binaries (e.g. MySQL/Percona Server or PostgreSQL bin directory). |
| `CONFIG_FILES` | *(required)* | Space-separated list of server configuration files to iterate over. See [`./cnf/`](cnf/) for available presets. |
| `EXTRA_CONFIG_FILES` | *(unset)* | Space-separated list of extra config files. All files are appended to the main config file. File names prefixed with `extra-` are shown without the prefix in email subjects (e.g. `cnf/extra-no-ACID.cnf cnf/extra-nproc64.cnf` appears as `+no-ACID+nproc64`). See [`./cnf/extra-*.cnf`](cnf/) for available extras. |
| `RUN_NAME` | *(unset)* | Human-readable name for the benchmark run, used in email subjects and Slack messages. |
| `BENCHMARK_LOGGING` | `Y` | Enable or disable verbose benchmark logging including CPU, memory, iostat, dstat (`Y`/`N`). |
| `RESTART_SERVER` | `Y` | `Y` restarts the server before every concurrency level of `THREADS_LIST`, so each measurement starts with a cold cache and a fully flushed engine. `N` starts one server per workload and keeps it running across all concurrency levels, which measures steady state instead and makes each run inherit the state of the previous one. |
| `SMART_DEVICE` | `/dev/nvme0n1` | Block device used for S.M.A.R.T. disk statistics collection. |
| `TRIM_AFTER_BENCH` | `Y` | Run `fstrim` after the data directory is removed, so the SSD learns those blocks are free. Without it its spare area shrinks and the write latency of the next benchmark depends on how much the previous one wrote. |
| `FSTRIM_PATHS` | *(unset)* | Space-separated paths to trim; each trims the whole filesystem holding it. Defaults to the filesystem holding `DATA_DIR`. |
| `IDLE_AFTER_BENCH` | `0` | Seconds to idle at the very end of the run (after the results are published), giving the storage time to finish its internal housekeeping. Use `1200` or more when benchmarks run back to back on the same machine. |
| `WORKLOAD_NAMES` | `reads,writes` | Comma-separated list of workload names or aliases to run. See [`db-bench/workloads.inc`](db-bench/workloads.inc) for all available workloads and aliases (e.g. `reads`, `writes`, `pg_writes`, `mixed`, `mixed_trx`, `heavy_trx`, `prod_trx`). |
| `SCALING_GOVERNOR` | *(unset)* | CPU scaling governor to set during the benchmark (e.g. `performance`). When set, also disables address randomization and turbo boost. |
| `DISABLE_IDLE_STATES` | *(unset)* | Set to `yes` to disable CPU idle states during the benchmark (requires `SCALING_GOVERNOR`). |
| `BACKUP_DIR` | *(unset)* | Directory to copy result archives and CSV files to after the benchmark completes. |
| `RESULTS_EMAIL` | *(unset)* | Email address to send benchmark results to (requires `mutt`). |
| `SLACK_WEBHOOK_URL` | *(unset)* | Slack webhook URL for posting benchmark results. |

### Database / Sysbench

| Variable | Default | Description |
|---|---|---|
| `ENGINE_CACHE` | `32G` | Size of the database engine cache. Maps to `innodb_buffer_pool_size` (InnoDB), `rocksdb_block_cache_size` (RocksDB), or `shared_buffers` (PostgreSQL). |
| `NUM_TABLES` | `16` | Number of sysbench tables to create and benchmark against. Also used as the thread count during the data-preparation phase. |
| `DATASIZE` | `10M` | Number of rows per table in SI notation (e.g. `1M` = 1,000,000 rows, `10M` = 10,000,000 rows). Converted via `numfmt --from=si`. |
| `DB_USER` | `root` | Database user for sysbench connections. |
| `RAND_TYPE` | `uniform` | Random number distribution type (`uniform`, `gaussian`, `special`, `pareto`). |
| `RAND_SEED` | `1111` | Seed for the random number generator, ensuring reproducible runs. |
| `THREADS_LIST` | `1 4 16 64 128 256 512 1024` | Space-separated list of thread counts to benchmark. |
| `SYSBENCH_REPORT_INTERVAL` | `10` | Interval in seconds between periodic sysbench statistics reports. |
| `SYSBENCH_BIN` | `sysbench` | Path or name of the sysbench binary. |
| `SYSBENCH_LUA` | `/usr/local/share/sysbench` | Directory containing sysbench Lua scripts. |
| `SYSBENCH_WRITE` | `oltp_write_only.lua` | Lua script used for write workloads during database preparation. |
| `SYSBENCH_EXTRA` | *(unset)* | Extra options appended to every sysbench invocation. |
| `SYSBENCH_HOST` | *(unset)* | Hostname for remote sysbench connections. When set, connects via TCP instead of a local socket. |
| `REBUILD_TEMPLATE` | *(unset)* | Set to `ON` to force rebuilding the template data directory even if one already exists. |

### Timing

| Variable | Default | Description |
|---|---|---|
| `PS_START_TIMEOUT` | `300` | Maximum time in seconds to wait for the database server to start. |
| `WARMUP_TIME_SECONDS` | `0` | Warmup period in seconds, passed to sysbench as `--warmup-time`. Excluded from the reported statistics. |
| `RUN_TIME_SECONDS` | `600` | Overall run time in seconds. Used as the default for `WRITES_TIME_SECONDS`. |
| `WRITES_TIME_SECONDS` | `$RUN_TIME_SECONDS` (600) | Duration in seconds for write workloads. |
| `READS_TIME_SECONDS` | `$WRITES_TIME_SECONDS / 2` (300) | Duration in seconds for read workloads. Defaults to half of the write duration. |

### Server Options

| Variable | Default | Description |
|---|---|---|
| `MYEXTRA` | *(unset)* | Extra command-line options passed to the database server (e.g. `--disable-log-bin`). |
| `PERF_EXTRA` | *(unset)* | Extra performance-schema instrumentation options (e.g. `--performance-schema-instrument='wait/synch/mutex/innodb/%=ON'`). |
| `SSL_CERTS_PATH` | *(unset)* | Path to SSL certificate files. When set, enables SSL for both the server and sysbench connections. |
| `INSTALL_SQL_PATH` | *(unset)* | Path to an SQL file to execute on server start (MySQL only). |
| `SETUP_SQL_PATH` | *(unset)* | Path to an SQL file to execute after `INSTALL_SQL_PATH` (MySQL only). |
| `RUN_SQL_COMMANDS` | *(unset)* | SQL commands to execute inline on each server start (MySQL only). |
| `PG_TDE` | `OFF` | PostgreSQL Transparent Data Encryption mode. Supported values: `OFF`, `ON` (enables WAL encryption), `TABLE` (table-level encryption only). |

## Usage

```bash
# Minimal example (required: BENCH_NAME, BUILD_PATH, CONFIG_FILES)
export BENCH_NAME=my-test
export BUILD_PATH=/path/to/mysql/install
export CONFIG_FILES=/path/to/my.cnf
./db-bench.sh

# Custom engine, cache size, and data dimensions
ENGINE=rocksdb ENGINE_CACHE=64G NUM_TABLES=32 DATASIZE=1M \
  BENCH_NAME=my-test BUILD_PATH=/path/to/mysql CONFIG_FILES=/path/to/my.cnf \
  ./db-bench.sh

# PostgreSQL with reduced run time
ENGINE=postgres BENCH_NAME=pg-test BUILD_PATH=/usr/local/pgsql/bin \
  CONFIG_FILES=/path/to/postgresql.conf RUN_TIME_SECONDS=120 \
  ./db-bench.sh
```

---

# build-and-bench.sh

A wrapper script that automates the full workflow: clone and build the database server and sysbench from source, then run `db-bench.sh`. All `db-bench.sh` environment variables listed above are supported and passed through.

## Environment Variables

### Directories

| Variable | Default | Description |
|---|---|---|
| `ROOT_DIR` | `/mnt/fast/build-and-bench` | Root directory for all source trees, build artifacts, and results. |
| `SERVER_REPO_DIR` | `$ROOT_DIR/postgres` or `$ROOT_DIR/src_mysql` | Local clone of the database server repository (auto-selected by `ENGINE`). |
| `SERVER_BUILD_DIR` | `$ROOT_DIR/$SERVER_BRANCH-rel-$SELECTED_CC` | Directory where the server is compiled. |
| `SYSBENCH_REPO_DIR` | `$ROOT_DIR/sysbench` | Local clone of the sysbench repository. |
| `DBBENCH_REPO_DIR` | `$ROOT_DIR/db-bench` | Local clone of the db-bench (perf-tests) repository. |

### Server Source

| Variable | Default (MySQL) | Default (PostgreSQL) | Description |
|---|---|---|---|
| `ENGINE` | `innodb` | `postgres` | Determines which server is built and which defaults apply. |
| `SERVER_REPO_URL` | `https://github.com/percona/percona-server` | `https://github.com/percona/postgres` | Git URL of the database server repository. |
| `SERVER_BRANCH` | `8.0` | `TDE_REL_17_STABLE` | Git branch or tag to check out and build. |

### Sysbench Source

| Variable | Default | Description |
|---|---|---|
| `SYSBENCH_REPO_URL` | `https://github.com/inikep/sysbench` | Git URL of the sysbench repository. |
| `SYSBENCH_BRANCH` | `mdcallag` | Git branch or tag of sysbench to build. |

### db-bench Source

| Variable | Default | Description |
|---|---|---|
| `DBBENCH_REPO_URL` | `https://github.com/Percona-QA/perf-tests.git` | Git URL of the db-bench repository. |
| `DBBENCH_BRANCH` | `3.1` | Git branch or tag of db-bench to use. |

### Build

| Variable | Default | Description |
|---|---|---|
| `SELECTED_CC` | `gcc-13` | C compiler to use for building the server. |
| `SELECTED_CXX` | `g++-13` | C++ compiler to use for building the server. |
| `WITH_PGO` | `off` | Set to `on` or `1` to build MySQL/Percona Server with PGO (profile guided optimization). Ignored for `ENGINE=postgres`. |
| `FPROFILE_DIR` | `$SERVER_BUILD_DIR/bin-profile-data` | Directory with the PGO profile data. Wiped before the instrumented build. |
| `PGO_TRAIN_TARGET` | `run-profile-suite` | `make` target used to train the profile. |

### PGO (profile guided optimization)

With `WITH_PGO=on` the server is built in three passes:

1. instrumented build (`-DFPROFILE_GENERATE=1`),
2. training run (`make run-profile-suite`, i.e. mysql-test-run over the suites MySQL selected for profiling), which writes the profile to `$FPROFILE_DIR`,
3. final build (`-DFPROFILE_USE=1`, which additionally enables LTO) in the very same build directory - gcc matches the profile data by the object file paths, so the path must not change between passes.

`ccache` is not used for PGO builds, and the whole build takes roughly three times longer than a regular one. The extra logs are `cmake-pgo-generate.log`, `make-pgo-generate.log` and `pgo-train.log` in `$SERVER_BUILD_DIR`.

### Benchmark Overrides

These override `db-bench.sh` defaults with values tuned for the build-and-bench workflow:

| Variable | Default | db-bench.sh default | Description |
|---|---|---|---|
| `ENGINE_CACHE` | `96G` | `32G` | Size of the database engine cache. |
| `WRITES_TIME_SECONDS` | `300` | `600` | Duration in seconds for write workloads. |
| `THREADS_LIST` | `8 16 32 64` | `1 4 16 64 128 256 512 1024` | Thread counts to benchmark. |
| `CNFFILE_NAME` | `cnf/stable-innodb.cnf` | — | Config file name relative to the db-bench repo. Used to compute `CONFIG_FILES` when it is not set explicitly. See [`./cnf/`](cnf/) for available presets. |
| `REPEAT_NUM` | `1` | — | Number of times to repeat the full benchmark run. |
| `DBBENCH_SSL` | *(unset)* | — | Set to `on` or `1` to enable SSL connections (sets `SSL_CERTS_PATH` to `$DBBENCH_REPO_DIR/cert`). |

## Usage

```bash
# Build Percona Server 8.0 and run the default benchmark
sudo -E nice --adjustment=-10 env SERVER_BRANCH=8.0 \
  ROOT_DIR=/mnt/fast/build-and-bench \
  ./build-and-bench.sh

# Build PostgreSQL and run with custom workloads
sudo -E nice --adjustment=-10 env ENGINE=postgres \
  SERVER_BRANCH=TDE_REL_17_STABLE \
  WORKLOAD_NAMES=reads,pg_writes \
  WRITES_TIME_SECONDS=120 \
  ./build-and-bench.sh

# Quick test with minimal data
sudo -E nice --adjustment=-10 env SERVER_BRANCH=8.0 \
  WRITES_TIME_SECONDS=30 THREADS_LIST="8" \
  WORKLOAD_NAMES=POINT_SELECT \
  ROOT_DIR=/mnt/optane/auto-perf-test \
  ./build-and-bench.sh

# Use bash -xe for command tracing (-x) and exit-on-error (-e)
sudo -E nice --adjustment=-10 env SERVER_BRANCH=8.0 \
  ROOT_DIR=/mnt/fast/build-and-bench \
  bash -xe ./build-and-bench.sh
```
