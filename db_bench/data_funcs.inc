#!/bin/echo This script should be sourced in a shell, not executed directly

#**********************************************************************************************
# Percona Database Benchmark scripts
# data_funcs.inc - data processing functions
#**********************************************************************************************

source ${SCRIPT_DIR}/db_bench/workloads.inc

# Function to process workload names and return WORKLOAD_ARRAY[] and WORKLOAD_PARAMS[] arrays
function process_workload_names() {
  WORKLOAD_ARRAY=()
  WORKLOAD_PARAMS=()

  # Replace aliases in workloads with actual strings
  WORKLOAD_FULL_NAMES="$WORKLOAD_NAMES"
  for ALIAS in "${!aliases[@]}"; do
    WORKLOAD_FULL_NAMES="${WORKLOAD_FULL_NAMES//${ALIAS}/${aliases[$ALIAS]}}"
  done

  echo "- Evaluated 'WORKLOAD_NAMES=$WORKLOAD_NAMES' to '$WORKLOAD_FULL_NAMES'"

  # Split workloads into an array
  IFS=',' read -r -a WORKLOAD_FULL_ARRAY <<< "$WORKLOAD_FULL_NAMES"

  for WORKLOAD in "${WORKLOAD_FULL_ARRAY[@]}"; do
    # Iterate through configurations
    for KEY in "${!load[@]}"; do
      if [[ $KEY == ${WORKLOAD} ]]; then
        WORKLOAD_ARRAY+=("$WORKLOAD")
        WORKLOAD_PARAMS+=("${load[$WORKLOAD]}")
        break
      fi
    done
  done
}

function print_parameters() {
  local ENDLINE=$1
  variables=("BENCH_NAME" "BUILD_PATH" "CONFIG_FILES" "INNODB_CACHE" "NUM_TABLES" "DATASIZE" "THREADS_LIST" "WRITES_TIME_SECONDS" "READS_TIME_SECONDS" "WARMUP_TIME_SECONDS"
             "WORKLOAD_WARMUP_TIME" "WORKSPACE" "TEMPLATE_PATH" "CACHE_DIR" "BENCH_DIR" "DATA_DIR" "BACKUP_DIR" "CXXFLAGS" "MYEXTRA" "SYSBENCH_EXTRA" "SCALING_GOVERNOR" "RESULTS_EMAIL" "WORKLOAD_NAMES")
  for variable in "${variables[@]}"; do echo "$variable=${!variable}${ENDLINE}"; done
  echo "==========${ENDLINE}"
  for ((i=0; i<${#WORKLOAD_ARRAY[@]}; i++)); do
    WORKLOAD_PARAMETERS=$(eval echo ${WORKLOAD_PARAMS[i]})
    echo "${WORKLOAD_ARRAY[i]}=${WORKLOAD_PARAMETERS}${ENDLINE}"
  done
}

function diff_to_average() {
    local csv_file="$1"
    diff_output=$(awk -F ',' 'BEGIN {
        for (i=2; i<=NF; i++) {
            sum[i] = 0
            count[i] = 0
        }
    }
    {
        if (FNR != total_rows) { # Process all rows except the last one
            for (i=2; i<=NF; i++) {
                if ($i != "") {
                    count[i]++
                    sum[i] += $i
                }
            }
        } else { # Process the last row
            for (i=2; i<=NF; i++) {
                last_row_data[i] = $i
            }
        }
    }
    END {
        for (i=2; i<=NF; i++) {
          avg[i] = (count[i] > 0) ? sum[i] / count[i] : 0
          printf ", %.2f%%", ((last_row_data[i] - avg[i]) / avg[i]) * 100
        }
        printf "\n"

    }' total_rows=$(awk 'END{print NR}' "$csv_file") "$csv_file")
    echo $diff_output
}

function average() {
    local csv_file="$1"
    awk -F ',' 'BEGIN {
        for (i=2; i<=NF; i++) {
            sum[i] = 0
            count[i] = 0
        }
    }
    {
        for (i=2; i<=NF; i++) {
            if ($i != "") {
                count[i]++
                sum[i] += $i
            }
        }
    }
    END {
        for (i=2; i<=NF; i++) {
           avg[i] = (count[i] > 0) ? sum[i] / count[i] : 0
           printf ", %.2f", avg[i]
        }
        printf "\n"

    }' "$csv_file"
}

function standard_deviation_percent() {
    local csv_file="$1"
    awk -F ',' 'BEGIN {
        for (i=2; i<=NF; i++) {
            sum[i] = 0
            count[i] = 0
            sumsq[i] = 0
        }
    }
    {
        for (i=2; i<=NF; i++) {
            if ($i != "") {
                count[i]++
                sum[i] += $i
                sumsq[i] += $i^2
            }
        }
    }
    END {
        for (i=2; i<=NF; i++) {
           avg[i] = (count[i] > 0) ? sum[i] / count[i] : 0
           printf ", %.2f%%", (sqrt((sumsq[i]/count[i]) - (avg[i])**2) / avg[i]) * 100
        }
        printf "\n"

    }' "$csv_file"
}

function csv_to_html_table() {
    local INPUT_NAME=$1
    local USE_COLOR=$2

    echo "<table>"

    while IFS=',' read -r -a fields; do
        echo "  <tr>"
        for ((i=0; i<${#fields[@]}; i++)); do
            if [ -n "${fields[i]}" ]; then
              if [ $i -eq 0 ]; then
                  echo "    <td>${fields[i]}</td>"
              else
                  local FIELD="${fields[i]//[% ]/}"
                  if [ "$USE_COLOR" = "color" ] && [[ "${FIELD}" =~ ^-?[0-9]+(\.[0-9]+)?$ ]] && (( $(echo "${FIELD} > 1.0 || ${FIELD} < -1.0" | bc -l) )); then
                      echo "    <td style=\"text-align: right; color: red;\">${fields[i]}</td>"
                  else
                      echo "    <td style=\"text-align: right;\">${fields[i]}</td>"
                  fi
              fi
            fi
        done
        echo "  </tr>"
    done < "$INPUT_NAME"

    echo "</table>"
}

function create_html_page() {
  echo "<!DOCTYPE html>"
  echo "<html>"
  echo "<head>"
  echo "<style>"
  echo "table, th, td {"
  echo "  border: 1px solid;"
  echo "  border-collapse: collapse;"
  echo "  border-color: #DDDDDD;"
  echo "}"
  echo "</style>"
  echo "</head>"
  echo "<body>"
  cat $1
  echo "<BR>"
  cat $2
  echo "<BR>"
  cat $3
  echo "<BR>"
  cat $4
  echo "</body>"
  echo "</html>"
}
