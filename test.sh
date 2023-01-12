
#!/bin/bash
option=$1;



if test $option -eq 0 #webcam pc_hp
then 
    ffmpeg -f v4l2 -v 32 -xerror -framerate 25 -video_size 640x480 -i /dev/video0 \
-c:v mpeg2video -pix_fmt yuv420p -s 720x480 -aspect 16:9 -streamid 0:180 \
-b:v 2298k -maxrate 2298k -minrate 2298k -bufsize 2298k \
-f mpegts -muxrate 2700000.0 udp://10.2.51.11:12345;

elif test $option -eq 1 #live youtube
then
    streamlink https://www.youtube.com/watch?v=Lcdi9O2XB4E best -O | ffmpeg -re -i pipe:0 \
    -c:v mpeg2video -pix_fmt yuv420p -s 1280x720 -aspect 16:9 -streamid 0:180 \
    -b:v 2298k -maxrate 2298k -minrate 2298k -bufsize 2298k \
    -f mpegts -muxrate 2700000.0 udp://10.2.51.11:12345;
elif test $option -eq 2 #escritorio
then
    ffmpeg -f x11grab -r 24 -s 1680x1050 -i :0.0  \
    -c:v mpeg2video -pix_fmt yuv420p -s 720x480 -aspect 16:9 -streamid 0:180 \
    -b:v 2298k -maxrate 2298k -minrate 2298k -bufsize 2298k \
    -f mpegts -muxrate 2700000.0 udp://10.2.51.11:12345;
elif test $option -eq 3 #live twitch
then
    streamlink https://www.twitch.tv/ilovedoki best -O | ffmpeg -re -i pipe:0 \
    -c:v mpeg2video -pix_fmt yuv420p -s 720x480 -aspect 16:9 -streamid 0:180 \
    -b:v 2298k -maxrate 2298k -minrate 2298k -bufsize 2298k \
    -f mpegts -muxrate 2700000.0 udp://10.2.51.11:12345;
elif test $option -eq 4 #pip
then
    ffmpeg -i bruno1.mp4 -i final.mp4 -filter_complex "[0]scale=iw/3:ih/3 [pip]; [1][pip] overlay=main_w-overlay_w-10:main_h-overlay_h-10"\
    -c:v mpeg2video -pix_fmt yuv420p -s 720x480 -aspect 16:9 -streamid 0:180 \
    -b:v 2298k -maxrate 2298k -minrate 2298k -bufsize 2298k \
    -f mpegts -muxrate 2700000.0 udp://10.2.51.11:12345;
elif test $option -eq 5 #test pip 2 lives
then
    streamlink https://www.youtube.com/watch?v=Lcdi9O2XB4E best -O | streamlink https://www.youtube.com/watch?v=V00D4K9OshI best -0 | ffmpeg -i pipe:0 -i pipe:1 -filter_complex "[0]scale=iw/3:ih/3 [pip]; [1][pip] overlay=main_w-overlay_w-10:main_h-overlay_h-10"\
    -c:v mpeg2video -pix_fmt yuv420p -s 720x480 -aspect 16:9 -streamid 0:180 \
    -b:v 2298k -maxrate 2298k -minrate 2298k -bufsize 2298k \
    -f mpegts -muxrate 2700000.0 udp://10.2.51.11:12345;
elif test $option -eq 6 #pip live con cam
then
    streamlink https://www.youtube.com/watch?v=Lcdi9O2XB4E best -O | ffmpeg -i /dev/video0 -i pipe:0  -filter_complex "[0]scale=iw/3:ih/3 [pip]; [1][pip] overlay=main_w-overlay_w-10:main_h-overlay_h-10" \
    -c:v mpeg2video -pix_fmt yuv420p -s 720x480 -aspect 16:9 -streamid 0:180 \
    -b:v 2298k -maxrate 2298k -minrate 2298k -bufsize 2298k \
    -f mpegts -muxrate 2700000.0 udp://10.2.51.11:12345;
elif test $option -eq 7 #pip live con video
then
    streamlink https://www.youtube.com/watch?v=Lcdi9O2XB4E best -O | ffmpeg -i pipe:0 -i final.mp4 -filter_complex "[0]scale=iw/3:ih/3 [pip]; [1][pip] overlay=main_w-overlay_w-10:main_h-overlay_h-10" \
    -c:v mpeg2video -pix_fmt yuv420p -s 720x480 -aspect 16:9 -streamid 0:180 \
    -b:v 2298k -maxrate 2298k -minrate 2298k -bufsize 2298k \
    -f mpegts -muxrate 2700000.0 udp://10.2.51.11:12345;
elif test $option -eq 8 #test audio pip con live de fondo
then
    streamlink https://www.youtube.com/watch?v=Lcdi9O2XB4E best -O | ffmpeg -i pipe:0 -i final.mp4 -filter_complex "[0]scale=iw/3:ih/3 [pip]; [1][pip] overlay=main_w-overlay_w-10:main_h-overlay_h-10" \
    -c:v mpeg2video -c:a aac -pix_fmt yuv420p -s 720x480 -aspect 16:9 -streamid 0:180 -streamid 1:181 \
    -b:a 128k -b:v 2298k -maxrate 2298k -minrate 2298k -bufsize 2298k \
    -f mpegts -muxrate 2700000.0 udp://10.2.51.11:12345;
elif test $option -eq 9 #test audio live youtube
then
    streamlink https://www.youtube.com/watch?v=Lcdi9O2XB4E best -O | ffmpeg -re -i pipe:0 \
    -c:v mpeg2video -c:a aac -pix_fmt yuv420p -s 720x480 -aspect 16:9 -streamid 0:180 -streamid 1:181 \
    -b:a 128k -b:v 2298k -maxrate 2298k -minrate 2298k -bufsize 2298k \
    -f mpegts -muxrate 2700000.0 udp://10.2.51.11:12345;
elif test $option -eq 10 #test audio webcam
then
    ffmpeg -i final.mp4 -i bruno1.mp4 -i bruno2.mp4 -filter_complex "[0:v][1:v][2:v]hstack=3"\
    -c:v mpeg2video -c:a aac -pix_fmt yuv420p -s 720x480 -aspect 16:9 -streamid 0:180 -streamid 1:181 \
    -b:a 128k -b:v 2298k -maxrate 2298k -minrate 2298k -bufsize 2298k \
    -f mpegts -muxrate 2700000.0 udp://10.2.51.11:12345;            
elif test $option -eq 11 #live youtube
then
    ffmpeg -f v4l2 -v 32 -xerror -framerate 25 -video_size 640x480 -i /dev/video0 \
-c:v libx264 -profile:v baseline -preset ultrafast -tune zerolatency -pix_fmt yuv420p -s 720x480 -aspect 16:9 -streamid 0:180 \
-b:v 2298k -maxrate 2298k -minrate 2298k -bufsize 2298k \
-f mpegts -muxrate 2700000.0 udp://10.2.51.11:12345;
elif test $option -eq 12 #test audio live youtube
then
    streamlink https://drive.google.com/file/d/1erOdZ7A32xRcSZ7cTBTGwL0Bi1IObuOr/view?usp=share_link best -O | ffmpeg -re -i pipe:0 \
    -c:v mpeg2video -c:a aac -pix_fmt yuv420p -s 720x480 -aspect 16:9 -streamid 0:180 -streamid 1:181 \
    -b:a 128k -b:v 2298k -maxrate 2298k -minrate 2298k -bufsize 2298k \
    -f mpegts -muxrate 2700000.0 udp://10.2.51.11:12345;
elif test $option -eq 13 #test audio live youtube
then
    streamlink https://www.twitch.tv/sebastian_usm best -O | ffmpeg -re -i pipe:0 \
    -c:v mpeg2video -c:a aac -pix_fmt yuv420p -s 720x480 -aspect 16:9 -streamid 0:180 -streamid 1:181 \
    -b:a 128k -b:v 2298k -maxrate 2298k -minrate 2298k -bufsize 2298k \
    -f mpegts -muxrate 2700000.0 udp://10.2.51.11:12345;
else
    echo "Wrong device option <device>: 1: $1 ; 2:$2 ; $device"
    exit -3;
fi



