# Kdenlive profile file. Lines beginning with ##

# format:
# 0 id : 1 Format : 2 Name : 3 param1 : 4 param2 : 5 param3 : 6 MLT consumer : 7 MLT Normalisation: 8 extension : 9 arguments

#### HQ


 # HDV
:HQ:HDV 1080 50i:PAL:::avformat:PAL:m2t:format=mpegts vcodec=mpeg2video s=1440x1080 b=19700k ab=384000 g=12 idct_algo=1 ilme=1 profile=hdv_1080_50i
:HQ:HDV 1080 60i:NTSC:::avformat:NTSC:m2t:format=mpegts vcodec=mpeg2video s=1440x1080 b=19700k ab=384000 g=15 idct_algo=1 ilme=1 profile=hdv_1080_60i
:HQ:HDV 720 25p:PAL:::avformat:PAL:m2t:format=mpegts vcodec=mpeg2video s=1280x720 b=19700k ab=384000 g=12 profile=hdv_720_25p
:HQ:HDV 720 30p:NTSC:::avformat:NTSC:m2t:format=mpegts vcodec=mpeg2video s=1280x720 b=19700k ab=384000 g=15 profile=hdv_720_30p

# DV
:HQ:DV (Raw):PAL:::avformat:PAL:dv:format=dv idct_algo=1 pix_fmt=yuv420p s=720x576 profile=dv_pal
:HQ:DV (Raw):NTSC:::avformat:NTSC:dv:format=dv idct_algo=1 pix_fmt=yuv411p s=720x480 profile=dv_ntsc
:HQ:DV50 (Raw):PAL:::avformat:PAL:dv:format=dv idct_algo=1 pix_fmt=yuv422p s=720x576 profile=dv_pal
:HQ:DV50 (Raw):NTSC:::avformat:NTSC:dv:format=dv idct_algo=1 pix_fmt=yuv422p s=720x480 profile=dv_ntsc
:HQ:DV (AVI):PAL:::avformat:PAL:avi:format=avi vcodec=dvvideo idct_algo=1 pix_fmt=yuv420p acodec=pcm_s16le s=720x576 profile=dv_pal
:HQ:DV (AVI):NTSC:::avformat:NTSC:avi:format=avi vcodec=dvvideo idct_algo=1 pix_fmt=yuv411p acodec=pcm_s16le s=720x480 profile=dv_ntsc
:HQ:DV50 (AVI):PAL:::avformat:PAL:avi:format=avi vcodec=dvvideo idct_algo=1 pix_fmt=yuv422p s=720x576 profile=dv_pal
:HQ:DV50 (AVI):NTSC:::avformat:NTSC:avi:format=avi vcodec=dvvideo idct_algo=1 pix_fmt=yuv422p s=720x480 profile=dv_ntsc

#DV 16:9
:HQ:DV 16/9 (Raw):PAL:::avformat:PAL:dv:format=dv idct_algo=1 pix_fmt=yuv420p s=720x576 profile=dv_pal_wide
:HQ:DV 16/9 (Raw):NTSC:::avformat:NTSC:dv:format=dv idct_algo=1 pix_fmt=yuv411p s=720x480 profile=dv_ntsc_wide
:HQ:DV50 16/9 (Raw):PAL:::avformat:PAL:dv:format=dv idct_algo=1 pix_fmt=yuv422p s=720x576 profile=dv_pal_wide
:HQ:DV50 16/9 (Raw):NTSC:::avformat:NTSC:dv:format=dv idct_algo=1 pix_fmt=yuv422p s=720x480 profile=dv_ntsc_wide
:HQ:DV 16/9 (AVI):PAL:::avformat:PAL:avi:format=avi vcodec=dvvideo idct_algo=1 pix_fmt=yuv420p acodec=pcm_s16le s=720x576 profile=dv_pal_wide
:HQ:DV 16/9 (AVI):NTSC:::avformat:NTSC:avi:format=avi vcodec=dvvideo idct_algo=1 pix_fmt=yuv411p acodec=pcm_s16le s=720x480 profile=dv_ntsc_wide
:HQ:DV50 16/9 (AVI):PAL:::avformat:PAL:avi:format=avi vcodec=dvvideo idct_algo=1 pix_fmt=yuv422p s=720x576 profile=dv_pal_wide
:HQ:DV50 16/9 (AVI):NTSC:::avformat:NTSC:avi:format=avi vcodec=dvvideo idct_algo=1 pix_fmt=yuv422p s=720x480 profile=dv_ntsc_wide

#DVD
:HQ:DVD:PAL:::avformat:PAL:vob:format=dvd vcodec=mpeg2video acodec=ac3 b=5000 maxrate=8000000 minrate=0 bufsize=1835008 mux_packet_s=2048 mux_rate=10080000 ab=192000 audio_sample_rate=48000 frame_s=720x576 s=720x576 frame_rate=25 g=15 me_range=63 idct_algo=1 ilme=1  profile=dv_pal
:HQ:DVD:NTSC:::avformat:NTSC:vob:format=dvd vcodec=mpeg2video acodec=ac3 s=720x480 b=6000 maxrate=9000000 minrate=0 bufsize=1835008 mux_packet_s=2048 mux_rate=10080000 ab=192000 audio_sample_rate=48000 frame_s=720x480 frame_rate=30000/1001 g=18 me_range=63 idct_algo=1 ilme=1 profile=dv_ntsc

:HQ:DVD m2v:PAL:::avformat:PAL:m2v:format=mpeg1video b=6500 maxrate=8000000 minrate=0 bufsize=1835008 frame_s=720x576 s=720x576 frame_rate=25 g=15 me_range=63 idct_algo=1 ilme=1 profile=dv_pal
:HQ:DVD m2v:NTSC:::avformat:NTSC:m2v:format=mpeg1video s=720x480 b=6000 maxrate=9000000 minrate=0 bufsize=1835008 mux_packet_s=2048 frame_s=720x480 frame_rate=30000/1001 g=18 me_range=63 idct_algo=1 ilme=1 profile=dv_ntsc


#### MEDIUM

### MPEG

:MED:Mpeg:160x120:Low::avformat::mpeg:format=mpeg minrate=0 b=100 ab=16000 ar=22050 s=160x120 progressive=1
:MED:Mpeg:160x120:Medium::avformat::mpeg:format=mpeg minrate=0 b=200 ab=64000 ar=22050 s=160x120 progressive=1
:MED:Mpeg:160x120:High::avformat::mpeg:format=mpeg minrate=0 b=400 ab=128000 ar=32000 s=160x120 progressive=1

:MED:Mpeg:240x180:Low::avformat::mpeg:format=mpeg minrate=0 b=100 ab=32000 ar=22050 s=240x180 progressive=1
:MED:Mpeg:240x180:Medium::avformat::mpeg:format=mpeg minrate=0 b=300 ab=64000 ar=32000 s=240x180 progressive=1
:MED:Mpeg:240x180:High::avformat::mpeg:format=mpeg minrate=0 b=500 ab=128000 ar=44100 s=240x180 progressive=1

:MED:Mpeg:320x200:Low::avformat::mpeg:format=mpeg minrate=0 b=300 ab=32000 ar=22050 s=320x200 progressive=1
:MED:Mpeg:320x200:Medium::avformat::mpeg:format=mpeg minrate=0 b=500 ab=64000 ar=32000 s=320x200 progressive=1
:MED:Mpeg:320x200:High::avformat::mpeg:format=mpeg minrate=0 b=800 ab=128000 ar=44100 s=320x200 progressive=1

:MED:Mpeg:640x480:Low::avformat::mpeg:format=mpeg minrate=0 b=1000 ab=32000 ar=22050 s=640x480 progressive=1
:MED:Mpeg:640x480:Medium::avformat::mpeg:format=mpeg minrate=0 b=3000 ab=64000 ar=32000 s=640x480 progressive=1
:MED:Mpeg:640x480:High::avformat::mpeg:format=mpeg minrate=0 b=6000 ab=128000 ar=44100 s=640x480 progressive=1

:MED:Mpeg:720x576:Low::avformat::mpeg:format=mpeg minrate=0 b=6000 ab=64000 ar=32000 s=720x576 progressive=1
:MED:Mpeg:720x576:Medium::avformat::mpeg:format=mpeg minrate=0 b=9000 ab=128000 ar=44100 s=720x576 progressive=1
:MED:Mpeg:720x576:High::avformat::mpeg:format=mpeg minrate=0 b=12000 ab=384000 ar=48000 s=720x576 progressive=1

###MPEG4

:MED:Mpeg4:160x120:Low::avformat::avi:format=avi minrate=0 b=100 ab=32000 ar=22050 s=160x120 vcodec=mpeg4 mbd=2 trell=1 4mv=1 progressive=1
:MED:Mpeg4:160x120:Medium::avformat::avi:format=avi minrate=0 b=300 ab=64000 ar=22050 s=160x120 vcodec=mpeg4 mbd=2 trell=1 4mv=1 progressive=1
:MED:Mpeg4:160x120:High::avformat::avi:format=avi minrate=0 b=600 ab=128000 ar=32000 s=160x120 vcodec=mpeg4 mbd=2 trell=1 4mv=1 progressive=1

:MED:Mpeg4:240x180:Low::avformat::avi:format=avi minrate=0 b=400 ab=32000 ar=22050 s=240x180 vcodec=mpeg4 mbd=2 trell=1 4mv=1 progressive=1
:MED:Mpeg4:240x180:Medium::avformat::avi:format=avi minrate=0 b=700 ab=64000 ar=32000 s=240x180 vcodec=mpeg4 mbd=2 trell=1 4mv=1 progressive=1
:MED:Mpeg4:240x180:High::avformat::avi:format=avi minrate=0 b=900 ab=128000 ar=44100 s=240x180 vcodec=mpeg4 mbd=2 trell=1 4mv=1 progressive=1

:MED:Mpeg4:320x200:Low::avformat::avi:format=avi minrate=0 b=600 ab=32000 ar=22050 s=320x200 vcodec=mpeg4 mbd=2 trell=1 4mv=1 progressive=1
:MED:Mpeg4:320x200:Medium::avformat::avi:format=avi minrate=0 b=1000 ab=64000 ar=32000 s=320x200 vcodec=mpeg4 mbd=2 trell=1 4mv=1 progressive=1
:MED:Mpeg4:320x200:High::avformat::avi:format=avi minrate=0 b=1200 ab=128000 ar=44100 s=320x200 vcodec=mpeg4 mbd=2 trell=1 4mv=1 progressive=1

:MED:Mpeg4:640x480:Low::avformat::avi:format=avi minrate=0 b=800 ab=32000 ar=22050 s=640x480 vcodec=mpeg4 mbd=2 trell=1 4mv=1 progressive=1
:MED:Mpeg4:640x480:Medium::avformat::avi:format=avi minrate=0 b=3000 ab=64000 ar=32000 s=640x480 vcodec=mpeg4 mbd=2 trell=1 4mv=1 progressive=1
:MED:Mpeg4:640x480:High::avformat::avi:format=avi minrate=0 b=6000 ab=128000 ar=44100 s=640x480 vcodec=mpeg4 mbd=2 trell=1 4mv=1 progressive=1

:MED:Mpeg4:720x576:Low::avformat::avi:format=avi minrate=0 b=3000 ab=64000 ar=32000 s=720x576 vcodec=mpeg4 mbd=2 trell=1 4mv=1 progressive=1
:MED:Mpeg4:720x576:Medium::avformat::avi:format=avi minrate=0 b=6000 ab=128000 ar=44100 s=720x576 vcodec=mpeg4 mbd=2 trell=1 4mv=1 progressive=1
:MED:Mpeg4:720x576:High::avformat::avi:format=avi minrate=0 b=8000 ab=384000 ar=48000 s=720x576 vcodec=mpeg4 mbd=2 trell=1 4mv=1 progressive=1

### FLV

:MED:Flash:160x120:Low::avformat::flv:format=flv acodec=mp3 minrate=0 b=200 ab=8000 ar=22050 s=160x120 progressive=1
:MED:Flash:160x120:Medium::avformat::flv:format=flv acodec=mp3 minrate=0 b=600 ab=16000 ar=22050 s=160x120 progressive=1
:MED:Flash:160x120:High::avformat::flv:format=flv acodec=mp3 minrate=0 b=800 ab=32000 ar=22050 s=160x120 progressive=1

:MED:Flash:240x180:Low::avformat::flv:format=flv acodec=mp3 minrate=0 b=600 ab=8000 ar=22050 s=240x180 progressive=1
:MED:Flash:240x180:Medium::avformat::flv:format=flv acodec=mp3 minrate=0 b=800 ab=64000 ar=22050 s=240x180 progressive=1
:MED:Flash:240x180:High::avformat::flv:format=flv acodec=mp3 minrate=0 b=1000 ab=32000 ar=22050 s=240x180 progressive=1

:MED:Flash:320x240:Low::avformat::flv:format=flv acodec=mp3 minrate=0 b=800 ab=8000 ar=22050 s=320x240 progressive=1
:MED:Flash:320x240:Medium::avformat::flv:format=flv acodec=mp3 minrate=0 b=1000 ab=16000 ar=22050 s=320x240 progressive=1
:MED:Flash:320x240:High::avformat::flv:format=flv acodec=mp3 minrate=0 b=1200 ab=32000 ar=22050 s=320x240 progressive=1

:MED:Flash:640x480:Low::avformat::flv:format=flv acodec=mp3 minrate=0 b=3000 ab=16000 ar=22050 s=640x480 progressive=1
:MED:Flash:640x480:Medium::avformat::flv:format=flv acodec=mp3 minrate=0 b=5000 ab=32000 ar=22050 s=640x480 progressive=1
:MED:Flash:640x480:High::avformat::flv:format=flv acodec=mp3 minrate=0 b=7000 ab=128000 ar=22050 s=640x480 progressive=1

:MED:Flash:720x576:Low::avformat::flv:format=flv acodec=mp3 minrate=0 b=6000 ab=32000 ar=22050 s=720x576 progressive=1
:MED:Flash:720x576:Medium::avformat::flv:format=flv acodec=mp3 minrate=0 b=9000 ab=64000 ar=22050 s=720x576 progressive=1
:MED:Flash:720x576:High::avformat::flv:format=flv acodec=mp3 minrate=0 b=12000 ab=128000 ar=22050 s=720x576 progressive=1

### QUICKTIME

:MED:Quicktime:128x96:Low::avformat::mov:format=mov minrate=0 b=200 ab=8000 ar=22050 s=128x96 vcodec=svq1 acodec=ac3 progressive=1
:MED:Quicktime:128x96:Medium::avformat::mov:format=mov minrate=0 b=600 ab=16000 ar=22050 s=128x96 vcodec=svq1 acodec=ac3 progressive=1
:MED:Quicktime:128x96:High::avformat::mov:format=mov minrate=0 b=800 ab=32000 ar=22050 s=128x96 vcodec=svq1 acodec=ac3 progressive=1

:MED:Quicktime:176x144:Low::avformat::mov:format=mov minrate=0 b=600 ab=8000 ar=22050 s=176x144 vcodec=svq1 acodec=ac3 progressive=1
:MED:Quicktime:176x144:Medium::avformat::mov:format=mov minrate=0 b=800 ab=16000 ar=22050 s=176x144 vcodec=svq1 acodec=ac3 progressive=1
:MED:Quicktime:176x144:High::avformat::mov:format=mov minrate=0 b=1000 ab=32000 ar=22050 s=176x144 vcodec=svq1 acodec=ac3 progressive=1

:MED:Quicktime:352x288:Low::avformat::mov:format=mov minrate=0 b=800 ab=8000 ar=22050 s=352x288 vcodec=svq1 acodec=ac3 progressive=1
:MED:Quicktime:352x288:Medium::avformat::mov:format=mov minrate=0 b=1000 ab=16000 ar=22050 s=352x288 vcodec=svq1 acodec=ac3 progressive=1
:MED:Quicktime:352x288:High::avformat::mov:format=mov minrate=0 b=1200 ab=32000 ar=22050 s=352x288 vcodec=svq1 acodec=ac3 progressive=1

:MED:Quicktime:704x576:Low::avformat::mov:format=mov minrate=0 b=3000 ab=16000 ar=22050 s=704x576 vcodec=svq1 acodec=ac3 progressive=1
:MED:Quicktime:704x576:Medium::avformat::mov:format=mov minrate=0 b=5000 ab=32000 ar=22050 s=704x576 vcodec=svq1 acodec=ac3 progressive=1
:MED:Quicktime:704x576:High::avformat::mov:format=mov minrate=0 b=7000 ab=64000 ar=22050 s=704x576 vcodec=svq1 acodec=ac3 progressive=1

### XVID

:MED:XVid:160x120:Low::avformat::avi:minrate=0 b=2000 ab=8000 ar=22050 s=160x120 vcodec=xvid acodec=ac3 progressive=1
:MED:XVid:160x120:Medium::avformat::avi:minrate=0 b=600 ab=16000 ar=22050 s=160x120 vcodec=xvid acodec=ac3 progressive=1
:MED:XVid:160x120:High::avformat::avi:minrate=0 b=800 ab=32000 ar=22050 s=160x120 vcodec=xvid acodec=ac3 progressive=1

:MED:XVid:240x180:Low::avformat::avi:minrate=0 b=6000 ab=8000 ar=22050 s=240x180 vcodec=xvid acodec=ac3 progressive=1
:MED:XVid:240x180:Medium::avformat::avi:minrate=0 b=800 ab=16000 ar=22050 s=240x180 vcodec=xvid acodec=ac3 progressive=1
:MED:XVid:240x180:High::avformat::avi:minrate=0 b=1000 ab=32000 ar=22050 s=240x180 vcodec=xvid acodec=ac3 progressive=1

:MED:XVid:320x200:Low::avformat::avi:minrate=0 b=800 ab=8000 ar=22050 s=320x200 vcodec=xvid acodec=ac3 progressive=1
:MED:XVid:320x200:Medium::avformat::avi:minrate=0 b=1000 ab=16000 ar=22050 s=320x200 vcodec=xvid acodec=ac3 progressive=1
:MED:XVid:320x200:High::avformat::avi:minrate=0 b=1200 ab=32000 ar=22050 s=320x200 vcodec=xvid acodec=ac3 progressive=1

:MED:XVid:720x576:Low::avformat::avi:minrate=0 b=3000 ab=16000 ar=22050 s=720x576 vcodec=xvid acodec=ac3 progressive=1
:MED:XVid:720x576:Medium::avformat::avi:minrate=0 b=5000 ab=32000 ar=22050 s=720x576 vcodec=xvid acodec=ac3 progressive=1
:MED:XVid:720x576:High::avformat::avi:minrate=0 b=7000 ab=64000 ar=22050 s=720x576 vcodec=xvid acodec=ac3 progressive=1

### REALVIDEO

:MED:Real Video:160x120:Low::avformat::rm:format=rv10 minrate=0 b=100 ab=16000 ar=11025 s=160x120 g=8 progressive=1
:MED:Real Video:160x120:Medium::avformat::rm:format=rv10 minrate=0 b=300 ab=64000 ar=22050 s=160x120 g=8 progressive=1
:MED:Real Video:160x120:High::avformat::rm:format=rv10 minrate=0 b=500 ab=128000 ar=32000 s=160x120 g=8 progressive=1

:MED:Real Video:240x180:Low::avformat::rm:format=rv10 minrate=0 b=300 ab=32000 ar=22050 s=240x180 g=8 progressive=1
:MED:Real Video:240x180:Medium::avformat::rm:format=rv10 minrate=0 b=500 ab=64000 ar=32000 s=240x180 g=8 progressive=1
:MED:Real Video:240x180:High::avformat::rm:format=rv10 minrate=0 b=800 ab=128000 ar=44100 s=240x180 g=8 progressive=1

:MED:Real Video:320x200:Low::avformat::rm:format=rv10 minrate=0 b=500 ab=32000 ar=22050 s=320x200 g=8 progressive=1
:MED:Real Video:320x200:Medium::avformat::rm:format=rv10 minrate=0 b=7000 ab=64000 ar=32000 s=320x200 g=8 progressive=1
:MED:Real Video:320x200:High::avformat::rm:format=rv10 minrate=0 b=1000 ab=128000 ar=44100 s=320x200 g=8 progressive=1

:MED:Real Video:640x480:Low::avformat::rm:format=rv10 minrate=0 b=3000 ab=32000 ar=22050 s=640x480 g=8 progressive=1
:MED:Real Video:640x480:Medium::avformat::rm:format=rv10 minrate=0 b=5000 ab=64000 ar=32000 s=640x480 g=8 progressive=1
:MED:Real Video:640x480:High::avformat::rm:format=rv10 minrate=0 b=7000 ab=128000 ar=44100 s=640x480 g=8 progressive=1

:MED:Real Video:720x576:Low::avformat::rm:format=rv10 minrate=0 b=6000 ab=64000 ar=32000 s=720x576 g=8 progressive=1
:MED:Real Video:720x576:Medium::avformat::rm:format=rv10 minrate=0 b=9000 ab=128000 ar=44100 s=720x576 g=8 progressive=1
:MED:Real Video:720x576:High::avformat::rm:format=rv10 minrate=0 b=12000 ab=384000 ar=48000 s=720x576 g=8 progressive=1


### THEORA

:MED:Theora:160x112:Low::avformat::ogg:vcodec=libtheora acodec=vorbis minrate=0 b=100 ab=16000 ar=11025 s=160x112 progressive=1
:MED:Theora:160x112:Medium::avformat::ogg:vcodec=libtheora acodec=vorbis minrate=0 b=300 ab=64000 ar=22050 s=160x112 progressive=1
:MED:Theora:160x112:High::avformat::ogg:vcodec=libtheora acodec=vorbis minrate=0 b=500 ab=128000 ar=32000 s=160x112 progressive=1


:MED:Theora:320x240:Low::avformat::ogg:vcodec=libtheora acodec=vorbis minrate=0 b=600 ab=16000 ar=11025 s=320x240 progressive=1
:MED:Theora:320x240:Medium::avformat::ogg:vcodec=libtheora acodec=vorbis minrate=0 b=1000 ab=64000 ar=22050 s=320x240 progressive=1
:MED:Theora:320x240:High::avformat::ogg:vcodec=libtheora acodec=vorbis minrate=0 b=1200 ab=128000 ar=32000 s=320x240 progressive=1

:MED:Theora:640x480:Low::avformat::ogg:vcodec=libtheora acodec=vorbis minrate=0 b=3000 ab=32000 ar=22050 s=640x480 progressive=1
:MED:Theora:640x480:Medium::avformat::ogg:vcodec=libtheora acodec=vorbis minrate=0 b=5000 ab=64000 ar=32000 s=640x480 progressive=1
:MED:Theora:640x480:High::avformat::ogg:vcodec=libtheora acodec=vorbis minrate=0 b=7000 ab=128000 ar=44100 s=640x480 progressive=1


#### AUDIO

### WAV

:AUDIO:Wav:48000:::avformat::wav:format=wav ar=48000
:AUDIO:Wav:44100:::avformat::wav:format=wav ar=44100
:AUDIO:Wav:32000:::avformat::wav:format=wav ar=32000
:AUDIO:Wav:22050:::avformat::wav:format=wav ar=22050

### MP3

:AUDIO:Mp3:48000:384kb/s::avformat::mp3:format=mp3 ar=48000 ab=384000
:AUDIO:Mp3:48000:128kb/s::avformat::mp3:format=mp3 ar=48000 ab=128000
:AUDIO:Mp3:48000:64kb/s::avformat::mp3:format=mp3 ar=48000 ab=64000

:AUDIO:Mp3:44100:384kb/s::avformat::mp3:format=mp3 ar=44100 ab=384000
:AUDIO:Mp3:44100:128kb/s::avformat::mp3:format=mp3 ar=44100 ab=128000
:AUDIO:Mp3:44100:64kb/s::avformat::mp3:format=mp3 ar=44100 ab=64000

:AUDIO:Mp3:32000:128kb/s::avformat::mp3:format=mp3 ar=32000 ab=128000
:AUDIO:Mp3:32000:64kb/s::avformat::mp3:format=mp3 ar=32000 ab=64000
:AUDIO:Mp3:32000:32kb/s::avformat::mp3:format=mp3 ar=32000 ab=32000

:AUDIO:Mp3:22050:64kb/s::avformat::mp3:format=mp3 ar=22050 ab=64000
:AUDIO:Mp3:22050:32kb/s::avformat::mp3:format=mp3 ar=22050 ab=32000
:AUDIO:Mp3:22050:16kb/s::avformat::mp3:format=mp3 ar=22050 ab=16000


### AC3

:AUDIO:AC3:48000:448kb/s::avformat::ac3:format=ac3 ar=48000 ab=448000
:AUDIO:AC3:48000:224kb/s::avformat::ac3:format=ac3 ar=48000 ab=224000
:AUDIO:AC3:48000:128kb/s::avformat::ac3:format=ac3 ar=48000 ab=128000

:AUDIO:AC3:44100:384kb/s::avformat::ac3:format=ac3 ar=44100 ab=384000
:AUDIO:AC3:44100:128kb/s::avformat::ac3:format=ac3 ar=44100 ab=128000
:AUDIO:AC3:44100:64kb/s::avformat::ac3:format=ac3 ar=44100 ab=64000


### VORBIS

:AUDIO:Ogg Vorbis:48000:384kb/s::avformat::ogg:format=ogg ar=48000 ab=384000
:AUDIO:Ogg Vorbis:48000:128kb/s::avformat::ogg:format=ogg ar=48000 ab=128000
:AUDIO:Ogg Vorbis:48000:64kb/s::avformat::ogg:format=ogg ar=48000 ab=64000

:AUDIO:Ogg Vorbis:44100:384kb/s::avformat::ogg:format=ogg ar=44100 ab=384000
:AUDIO:Ogg Vorbis:44100:128kb/s::avformat::ogg:format=ogg ar=44100 ab=128000
:AUDIO:Ogg Vorbis:44100:64kb/s::avformat::ogg:format=ogg ar=44100 ab=64000

:AUDIO:Ogg Vorbis:32000:128kb/s::avformat::ogg:format=ogg ar=32000 ab=128000
:AUDIO:Ogg Vorbis:32000:64kb/s::avformat::ogg:format=ogg ar=32000 ab=64000
:AUDIO:Ogg Vorbis:32000:32kb/s::avformat::ogg:format=ogg ar=32000 ab=32000

:AUDIO:Ogg Vorbis:22050:64kb/s::avformat::ogg:format=ogg ar=22050 ab=64000
:AUDIO:Ogg Vorbis:22050:32kb/s::avformat::ogg:format=ogg ar=22050 ab=32000
:AUDIO:Ogg Vorbis:22050:16kb/s::avformat::ogg:format=ogg ar=22050 ab=16000

