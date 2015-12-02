# linuxhouse
Simple house automation server and 1-wire network device firmware

### Autostart server by systemd on Raspbian Jessie

/etc/udev/rules.d/95-linuxhouse.rules:
```
KERNEL=="ttyUSB0", ENV{SYSTEMD_WANTS}="linuxhouse.service"
```

/etc/systemd/system/linuxhouse.service:
```
[Unit]
Description=linux house server
After=remote-fs.target
After=syslog.target

[Service]
User=pi
ExecStart=/home/pi/linuxhouse/lh/runloop
Type=forking
PIDFile=/home/pi/linuxhouse/lh/runloop.pid
```

lh/runloop:
```
#!/bin/bash
cd `dirname $0`
export AVROW=/dev/ttyUSB0
./setport
./eventloop <$AVROW >>eventloop.log 2>&1 & echo $! >runloop.pid
```
