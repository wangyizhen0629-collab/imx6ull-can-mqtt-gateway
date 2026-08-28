#!/bin/sh

# i.MX6ULL 只读信息采集脚本：不会修改接口、进程、服务、文件系统或 init 配置。

section()
{
    printf '\n[%s]\n' "$1"
}

run()
{
    printf '$'
    for argument in "$@"; do
        printf ' %s' "$argument"
    done
    printf '\n'
    "$@" 2>&1
    status=$?
    if [ "$status" -ne 0 ]; then
        printf '[exit_status=%s]\n' "$status"
    fi
}

section metadata
run date '+%Y-%m-%dT%H:%M:%S%z'
run uname -a

section operating_system
if [ -r /etc/os-release ]; then
    run cat /etc/os-release
else
    printf 'NOT AVAILABLE: /etc/os-release\n'
fi
run busybox 2>/dev/null

section cpu_and_memory
run cat /proc/cpuinfo
run cat /proc/meminfo

section init
run readlink /proc/1/exe
run ps

section network_links
run ip -details -statistics link show can0
run ip address show eth0

section can_procfs
for path in /proc/net/can/rcvlist_all /proc/net/can/rcvlist_fil \
            /proc/net/can/stats; do
    if [ -r "$path" ]; then
        run cat "$path"
    else
        printf 'NOT AVAILABLE: %s\n' "$path"
    fi
done

section mqtt_files
run find /lib /usr/lib /usr/include -maxdepth 3 \
    \( -name 'libmosquitto.so*' -o -name 'libmosquitto.a' \
       -o -name 'mosquitto.h' -o -name 'libmosquitto.pc' \) -print

section available_commands
for command_name in ip candump cansend pkg-config gatewayd; do
    if command -v "$command_name" >/dev/null 2>&1; then
        command -v "$command_name"
    else
        printf '%s: NOT FOUND\n' "$command_name"
    fi
done

section reminder
printf '%s\n' 'READ-ONLY COLLECTION COMPLETE'
printf '%s\n' 'No CAN state, service, process, or /etc setting was changed.'
