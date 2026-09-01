#!/bin/sh

printf 'start pid=%s args=%s\n' "$$" "$*" >> "$FAKE_GATEWAY_LOG"

mode=$(sed -n '1p' "$FAKE_GATEWAY_MODE")
case $mode in
    crash)
        exit 42
        ;;
    crash_once)
        if [ ! -f "$FAKE_GATEWAY_ONCE" ]; then
            : > "$FAKE_GATEWAY_ONCE"
            exit 42
        fi
        ;;
    hold) ;;
    *) exit 64 ;;
esac

terminate()
{
    printf 'term pid=%s\n' "$$" >> "$FAKE_GATEWAY_LOG"
    exit 0
}

trap 'terminate' TERM INT
while :; do
    sleep 1
done
