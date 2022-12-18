#!/bin/bash

# webcamStream <domain> <port> <device:1|2>
# inicia el stream desde la webcam a el servidor

if test $# -ne 3
then
    name=$1
    echo "Wrong n° arguments $#: webcamStream <domain> <port> <device:1|2> $1"
    exit -2;
fi

domain=$1;
port=$2;
ip=$(dig "$domain" +short);
device=${3:-1};

if test $device -eq 0
then 
    echo "option 1: webcam";
    device="/dev/video0";
    ffmpeg -f v4l2 -v 32 -xerror -framerate 25 -video_size 640x480 -i $device \
    -c:v mpeg2video -pix_fmt yuv420p -s 720x480 -aspect 16:9 -streamid 0:180 \
    -b:v 2298k -maxrate 2298k -minrate 2298k -bufsize 2298k \
    -f mpegts -muxrate 2700000.0 udp://$ip:$port;

elif test $device -eq 1
then
    echo "option 1: webcam";
    device="/dev/video1";
    ffmpeg -f v4l2 -v 32 -xerror -framerate 25 -video_size 640x480 -i $device \
    -c:v mpeg2video -pix_fmt yuv420p -s 720x480 -aspect 16:9 -streamid 0:180 \
    -b:v 2298k -maxrate 2298k -minrate 2298k -bufsize 2298k \
    -f mpegts -muxrate 2700000.0 udp://$ip:$port;
elif test $device -eq 2
then
    echo "option 2: local video";
    device="big_buck_bunny_480p_30mb.mp4";
    ffmpeg -re -i $device -v 32 -xerror -f mpegts udp://$ip:$port;
else
    echo "Wrong device option <device>: 1: $1 ; 2:$2 ; $device"
    exit -3;
fi

#never here 
exit -4;

#udp://127.0.0.1:23000
