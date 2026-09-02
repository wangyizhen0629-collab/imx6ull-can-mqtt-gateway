#!/bin/sh

# 只采集原始/proc计数；CPU百分比在Ubuntu侧用相邻样本差值计算，避免板端浮点差异。

set -eu

usage()
{
    echo "usage: $0 --output PATH (--pid PID | --pid-file PATH) --samples COUNT [--interval-sec SEC] [--expected-exe PATH]" >&2
}

output=
fixed_pid=
pid_file=
samples=
interval_sec=1
expected_exe=

while [ "$#" -gt 0 ]; do
    case "$1" in
        --output)
            [ "$#" -ge 2 ] || { usage; exit 2; }
            output=$2
            shift 2
            ;;
        --pid)
            [ "$#" -ge 2 ] || { usage; exit 2; }
            fixed_pid=$2
            shift 2
            ;;
        --pid-file)
            [ "$#" -ge 2 ] || { usage; exit 2; }
            pid_file=$2
            shift 2
            ;;
        --samples)
            [ "$#" -ge 2 ] || { usage; exit 2; }
            samples=$2
            shift 2
            ;;
        --interval-sec)
            [ "$#" -ge 2 ] || { usage; exit 2; }
            interval_sec=$2
            shift 2
            ;;
        --expected-exe)
            [ "$#" -ge 2 ] || { usage; exit 2; }
            expected_exe=$2
            shift 2
            ;;
        *)
            usage
            exit 2
            ;;
    esac
done

[ -n "$output" ] && [ -n "$samples" ] || { usage; exit 2; }
if { [ -n "$fixed_pid" ] && [ -n "$pid_file" ]; } || \
   { [ -z "$fixed_pid" ] && [ -z "$pid_file" ]; }; then
    usage
    exit 2
fi

case "$samples" in
    ''|*[!0-9]*) echo "samples must be a positive integer" >&2; exit 2 ;;
esac
case "$interval_sec" in
    ''|*[!0-9]*) echo "interval-sec must be a positive integer" >&2; exit 2 ;;
esac
[ "$samples" -gt 0 ] || { echo "samples must be greater than zero" >&2; exit 2; }
[ "$interval_sec" -gt 0 ] || { echo "interval-sec must be greater than zero" >&2; exit 2; }
[ ! -e "$output" ] || { echo "refusing to overwrite existing output: $output" >&2; exit 2; }
[ -d "$(dirname "$output")" ] || { echo "output directory does not exist" >&2; exit 2; }

printf '%s\n' 'sample_index,epoch_s,uptime_s,pid,starttime_ticks,proc_ticks,total_ticks,vmrss_kb,vmhwm_kb,linux_state,sample_status' > "$output"

index=1
while [ "$index" -le "$samples" ]; do
    epoch_s=$(date +%s)
    uptime_s=$(awk '{print $1; exit}' /proc/uptime)
    pid=$fixed_pid
    if [ -n "$pid_file" ]; then
        pid=
        if [ -r "$pid_file" ]; then
            IFS= read -r pid < "$pid_file" || true
        fi
    fi

    case "$pid" in
        ''|*[!0-9]*)
            printf '%s,%s,%s,%s,,,,,,,MISSING\n' "$index" "$epoch_s" "$uptime_s" "$pid" >> "$output"
            ;;
        *)
            if [ ! -r "/proc/$pid/stat" ] || [ ! -r "/proc/$pid/status" ]; then
                printf '%s,%s,%s,%s,,,,,,,MISSING\n' "$index" "$epoch_s" "$uptime_s" "$pid" >> "$output"
            else
                sample_status=OK
                if [ -n "$expected_exe" ]; then
                    actual_exe=$(readlink "/proc/$pid/exe" 2>/dev/null || true)
                    if [ "$actual_exe" != "$expected_exe" ]; then
                        sample_status=IDENTITY_MISMATCH
                    fi
                fi

                proc_stat=$(cat "/proc/$pid/stat" 2>/dev/null || true)
                stat_tail=${proc_stat##*) }
                proc_values=$(printf '%s\n' "$stat_tail" | awk '
                    NF >= 20 { print $1, $12 + $13, $20 }
                ')
                total_ticks=$(awk '
                    /^cpu / {
                        total = 0
                        for (field = 2; field <= NF; field++) total += $field
                        printf "%.0f", total
                        exit
                    }
                ' /proc/stat 2>/dev/null || true)
                memory_values=$(awk '
                    /^VmRSS:/ { rss = $2; have_rss = 1 }
                    /^VmHWM:/ { hwm = $2; have_hwm = 1 }
                    END {
                        if (!have_rss || !have_hwm) exit 1
                        printf "%s %s", rss + 0, hwm + 0
                    }
                ' "/proc/$pid/status" 2>/dev/null || true)

                if [ -z "$proc_values" ] || [ -z "$total_ticks" ] || [ -z "$memory_values" ]; then
                    printf '%s,%s,%s,%s,,,,,,,READ_ERROR\n' "$index" "$epoch_s" "$uptime_s" "$pid" >> "$output"
                else
                    set -- $proc_values
                    linux_state=$1
                    proc_ticks=$2
                    starttime_ticks=$3
                    set -- $memory_values
                    vmrss_kb=$1
                    vmhwm_kb=$2
                    printf '%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s\n' \
                        "$index" "$epoch_s" "$uptime_s" "$pid" \
                        "$starttime_ticks" "$proc_ticks" "$total_ticks" \
                        "$vmrss_kb" "$vmhwm_kb" "$linux_state" "$sample_status" >> "$output"
                fi
            fi
            ;;
    esac

    if [ "$index" -lt "$samples" ]; then
        sleep "$interval_sec"
    fi
    index=$((index + 1))
done
