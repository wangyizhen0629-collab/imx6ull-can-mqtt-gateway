@echo off
setlocal
>"E:\imx6ull-can-mqtt-gateway\imx6ull-can-mqtt-gateway\artifacts\20260831T220718p0800-m6-lan-1000\subscriber_lifecycle.txt" echo subscriber_wrapper_started_at=%date% %time%
"E:\mosquitto\mosquitto_sub.exe" -d -h 192.168.50.1 -p 18884 -q 1 -t "vehicle/imx6ull-gateway-m6/telemetry/20260831T220718p0800-m6-lan-1000" -C 1000 -F "%%p" 1>"E:\imx6ull-can-mqtt-gateway\imx6ull-can-mqtt-gateway\artifacts\20260831T220718p0800-m6-lan-1000\subscriber.jsonl" 2>"E:\imx6ull-can-mqtt-gateway\imx6ull-can-mqtt-gateway\artifacts\20260831T220718p0800-m6-lan-1000\subscriber.stderr.log"
set "subscriber_exit=%ERRORLEVEL%"
>"E:\imx6ull-can-mqtt-gateway\imx6ull-can-mqtt-gateway\artifacts\20260831T220718p0800-m6-lan-1000\subscriber_exit.txt" echo subscriber_exit=%subscriber_exit%
>>"E:\imx6ull-can-mqtt-gateway\imx6ull-can-mqtt-gateway\artifacts\20260831T220718p0800-m6-lan-1000\subscriber_lifecycle.txt" echo subscriber_wrapper_finished_at=%date% %time%
exit /b %subscriber_exit%
