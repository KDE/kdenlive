# Kdenlive profile file. Lines beginning with ##

# format:
# 0 id : 1 Format : 2 Name : 3 param1 : 4 param2 : 5 param3 : 6 MLT consumer : 7 MLT Normalisation: 8 extension : 9 arguments

#### HQ


 # HDV
:HQ:HDV 1080 50i:PAL:::avformat:PAL:m2t:f=mpegts vcodec=mpeg2video s=1440x1080 b=19700k ab=384000 g=12 ildct=1 ilme=1 profile=hdv_1080_50i
:HQ:HDV 1080 60i:NTSC:::avformat:NTSC:m2t:f=mpegts vcodec=mpeg2video s=1440x1080 b=19700k ab=384000 g=15 ildct=1 ilme=1 profile=hdv_1080_60i
:HQ:HDV 720 25p:PAL:::avformat:PAL:m2t:f=mpegts vcodec=mpeg2video s=1280x720 b=19700k ab=384000 g=12 profile=hdv_720_25p
:HQ:HDV 720 30p:NTSC:::avformat:NTSC:m2t:f=mpegts vcodec=mpeg2video s=1280x720 b=19700k ab=384000 g=15 profile=hdv_720_30p

# DV
:HQ:DV (Raw):PAL:::avformat:PAL:dv:f=dv ildct=1 pix_fmt=yuv420p s=720x576 profile=dv_pal
:HQ:DV (Raw):NTSC:::avformat:NTSC:dv:f=dv ildct=1 pix_fmt=yuv411p s=720x480 profile=dv_ntsc
:HQ:DV50 (Raw):PAL:::avformat:PAL:dv:f=dv ildct=1 pix_fmt=yuv422p s=720x576 profile=dv_pal
:HQ:DV50 (Raw):NTSC:::avformat:NTSC:dv:f=dv ildct=1 pix_fmt=yuv422p s=720x480 profile=dv_ntsc
:HQ:DV (AVI):PAL:::avformat:PAL:avi:f=avi vcodec=dvvideo ildct=1 pix_fmt=yuv420p acodec=pcm_s16le s=720x576 profile=dv_pal
:HQ:DV (AVI):NTSC:::avformat:NTSC:avi:f=avi vcodec=dvvideo ildct=1 pix_fmt=yuv411p acodec=pcm_s16le s=720x480 profile=dv_ntsc
:HQ:DV50 (AVI):PAL:::avformat:PAL:avi:f=avi vcodec=dvvideo ildct=1 pix_fmt=yuv422p s=720x576 profile=dv_pal
:HQ:DV50 (AVI):NTSC:::avformat:NTSC:avi:f=avi vcodec=dvvideo ildct=1 pix_fmt=yuv422p s=720x480 profile=dv_ntsc

#DV 16:9
:HQ:DV 16/9 (Raw):PAL:::avformat:PAL:dv:f=dv ildct=1 pix_fmt=yuv420p s=720x576 profile=dv_pal_wide
:HQ:DV 16/9 (Raw):NTSC:::avformat:NTSC:dv:f=dv ildct=1 pix_fmt=yuv411p s=720x480 profile=dv_ntsc_wide
:HQ:DV50 16/9 (Raw):PAL:::avformat:PAL:dv:f=dv ildct=1 pix_fmt=yuv422p s=720x576 profile=dv_pal_wide
:HQ:DV50 16/9 (Raw):NTSC:::avformat:NTSC:dv:f=dv ildct=1 pix_fmt=yuv422p s=720x480 profile=dv_ntsc_wide
:HQ:DV 16/9 (AVI):PAL:::avformat:PAL:avi:f=avi vcodec=dvvideo ildct=1 pix_fmt=yuv420p acodec=pcm_s16le s=720x576 profile=dv_pal_wide
:HQ:DV 16/9 (AVI):NTSC:::avformat:NTSC:avi:f=avi vcodec=dvvideo ildct=1 pix_fmt=yuv411p acodec=pcm_s16le s=720x480 profile=dv_ntsc_wide
:HQ:DV50 16/9 (AVI):PAL:::avformat:PAL:avi:f=avi vcodec=dvvideo ildct=1 pix_fmt=yuv422p s=720x576 profile=dv_pal_wide
:HQ:DV50 16/9 (AVI):NTSC:::avformat:NTSC:avi:f=avi vcodec=dvvideo ildct=1 pix_fmt=yuv422p s=720x480 profile=dv_ntsc_wide

#DVD
:HQ:DVD:PAL:::avformat:PAL:vob:f=dvd vcodec=mpeg2video acodec=ac3 b=5000k maxrate=8000000 minrate=0 bufsize=1835008 mux_packet_s=2048 mux_rate=10080000 ab=192000 ar=48000 s=720x576  g=15 me_range=63 ildct=1 ilme=1  profile=dv_pal
:HQ:DVD:NTSC:::avformat:NTSC:vob:f=dvd vcodec=mpeg2video acodec=ac3 s=720x480 b=6000k maxrate=9000000 minrate=0 bufsize=1835008 mux_packet_s=2048 mux_rate=10080000 ab=192000 ar=48000 g=18 me_range=63 ildct=1 ilme=1 profile=dv_ntsc

:HQ:DVD m2v:PAL:::avformat:PAL:m2v:f=mpeg1video b=6500k maxrate=8000000 minrate=0 bufsize=1835008 s=720x576 g=15 me_range=63 ildct=1 ilme=1 profile=dv_pal
:HQ:DVD m2v:NTSC:::avformat:NTSC:m2v:f=mpeg1video s=720x480 b=6000k maxrate=9000000 minrate=0 bufsize=1835008 mux_packet_s=2048 g=18 me_range=63 ildct=1 ilme=1 profile=dv_ntsc


#### MEDIUM

### MPEG

:MED:Mpeg:160x120:Low::avformat::mpeg:f=mpeg minrate=0 b=100k ab=16000 ar=22050 s=160x120 progressive=1
:MED:Mpeg:160x120:Medium::avformat::mpeg:f=mpeg minrate=0 b=200k ab=64000 ar=22050 s=160x120 progressive=1
:MED:Mpeg:160x120:High::avformat::mpeg:f=mpeg minrate=0 b=400k ab=128000 ar=32000 s=160x120 progressive=1

:MED:Mpeg:240x180:Low::avformat::mpeg:f=mpeg minrate=0 b=100k ab=32000 ar=22050 s=240x180 progressive=1
:MED:Mpeg:240x180:Medium::avformat::mpeg:f=mpeg minrate=0 b=300k ab=64000 ar=32000 s=240x180 progressive=1
:MED:Mpeg:240x180:High::avformat::mpeg:f=mpeg minrate=0 b=500k ab=128000 ar=44100 s=240x180 progressive=1

:MED:Mpeg:320x200:Low::avformat::mpeg:f=mpeg minrate=0 b=300k ab=32000 ar=22050 s=320x200 progressive=1
:MED:Mpeg:320x200:Medium::avformat::mpeg:f=mpeg minrate=0 b=500k ab=64000 ar=32000 s=320x200 progressive=1
:MED:Mpeg:320x200:High::avformat::mpeg:f=mpeg minrate=0 b=800k ab=128000 ar=44100 s=320x200 progressive=1

:MED:Mpeg:640x480:Low::avformat::mpeg:f=mpeg minrate=0 b=1000k ab=32000 ar=22050 s=640x480 progressive=1
:MED:Mpeg:640x480:Medium::avformat::mpeg:f=mpeg minrate=0 b=3000k ab=64000 ar=32000 s=640x480 progressive=1
:MED:Mpeg:640x480:High::avformat::mpeg:f=mpeg minrate=0 b=6000k ab=128000 ar=44100 s=640x480 progressive=1

:MED:Mpeg:720x576:Low::avformat::mpeg:f=mpeg minrate=0 b=6000k ab=64000 ar=32000 s=720x576 progressive=1
:MED:Mpeg:720x576:Medium::avformat::mpeg:f=mpeg minrate=0 b=9000k ab=128000 ar=44100 s=720x576 progressive=1
:MED:Mpeg:720x576:High::avformat::mpeg:f=mpeg minrate=0 b=12000k ab=384000 ar=48000 s=720x576 progressive=1

###MPEG4

:MED:Mpeg4:160x120:Low::avformat::avi:f=avi minrate=0 b=100k ab=32000 ar=22050 s=160x120 vcodec=mpeg4 mbd=2 trell=1 mv4=1 progressive=1
:MED:Mpeg4:160x120:Medium::avformat::avi:f=avi minrate=0 b=300k ab=64000 ar=22050 s=160x120 vcodec=mpeg4 mbd=2 trell=1 mv4=1 progressive=1
:MED:Mpeg4:160x120:High::avformat::avi:f=avi minrate=0 b=600k ab=128000 ar=32000 s=160x120 vcodec=mpeg4 mbd=2 trell=1 mv4=1 progressive=1

:MED:Mpeg4:240x180:Low::avformat::avi:f=avi minrate=0 b=400k ab=32000 ar=22050 s=240x180 vcodec=mpeg4 mbd=2 trell=1 mv4=1 progressive=1
:MED:Mpeg4:240x180:Medium::avformat::avi:f=avi minrate=0 b=700k ab=64000 ar=32000 s=240x180 vcodec=mpeg4 mbd=2 trell=1 mv4=1 progressive=1
:MED:Mpeg4:240x180:High::avformat::avi:f=avi minrate=0 b=900k ab=128000 ar=44100 s=240x180 vcodec=mpeg4 mbd=2 trell=1 mv4=1 progressive=1

:MED:Mpeg4:320x200:Low::avformat::avi:f=avi minrate=0 b=600k ab=32000 ar=22050 s=320x200 vcodec=mpeg4 mbd=2 trell=1 mv4=1 progressive=1
:MED:Mpeg4:320x200:Medium::avformat::avi:f=avi minrate=0 b=1000k ab=64000 ar=32000 s=320x200 vcodec=mpeg4 mbd=2 trell=1 mv4=1 progressive=1
:MED:Mpeg4:320x200:High::avformat::avi:f=avi minrate=0 b=1200k ab=128000 ar=44100 s=320x200 vcodec=mpeg4 mbd=2 trell=1 mv4=1 progressive=1

:MED:Mpeg4:640x480:Low::avformat::avi:f=avi minrate=0 b=800k ab=32000 ar=22050 s=640x480 vcodec=mpeg4 mbd=2 trell=1 mv4=1 progressive=1
:MED:Mpeg4:640x480:Medium::avformat::avi:f=avi minrate=0 b=3000k ab=64000 ar=32000 s=640x480 vcodec=mpeg4 mbd=2 trell=1 mv4=1 progressive=1
:MED:Mpeg4:640x480:High::avformat::avi:f=avi minrate=0 b=6000k ab=128000 ar=44100 s=640x480 vcodec=mpeg4 mbd=2 trell=1 mv4=1 progressive=1

:MED:Mpeg4:720x576:Low::avformat::avi:f=avi minrate=0 b=3000k ab=64000 ar=32000 s=720x576 vcodec=mpeg4 mbd=2 trell=1 mv4=1 progressive=1
:MED:Mpeg4:720x576:Medium::avformat::avi:f=avi minrate=0 b=6000k ab=128000 ar=44100 s=720x576 vcodec=mpeg4 mbd=2 trell=1 mv4=1 progressive=1
:MED:Mpeg4:720x576:High::avformat::avi:f=avi minrate=0 b=8000k ab=384000 ar=48000 s=720x576 vcodec=mpeg4 mbd=2 trell=1 mv4=1 progressive=1

### FLV

:MED:Flash:160x120:Low::avformat::flv:f=flv acodec=libmp3lame minrate=0 b=200k ab=8000 ar=22050 s=160x120 progressive=1
:MED:Flash:160x120:Medium::avformat::flv:f=flv acodec=libmp3lame minrate=0 b=600k ab=16000 ar=22050 s=160x120 progressive=1
:MED:Flash:160x120:High::avformat::flv:f=flv acodec=libmp3lame minrate=0 b=800k ab=32000 ar=22050 s=160x120 progressive=1

:MED:Flash:240x180:Low::avformat::flv:f=flv acodec=libmp3lame minrate=0 b=600k ab=8000 ar=22050 s=240x180 progressive=1
:MED:Flash:240x180:Medium::avformat::flv:f=flv acodec=libmp3lame minrate=0 b=800k ab=64000 ar=22050 s=240x180 progressive=1
:MED:Flash:240x180:High::avformat::flv:f=flv acodec=libmp3lame minrate=0 b=1000k ab=32000 ar=22050 s=240x180 progressive=1

:MED:Flash:320x240:Low::avformat::flv:f=flv acodec=libmp3lame minrate=0 b=800k ab=8000 ar=22050 s=320x240 progressive=1
:MED:Flash:320x240:Medium::avformat::flv:f=flv acodec=libmp3lame minrate=0 b=1000k ab=16000 ar=22050 s=320x240 progressive=1
:MED:Flash:320x240:High::avformat::flv:f=flv acodec=libmp3lame minrate=0 b=1200k ab=32000 ar=22050 s=320x240 progressive=1

:MED:Flash:640x480:Low::avformat::flv:f=flv acodec=libmp3lame minrate=0 b=3000k ab=16000 ar=22050 s=640x480 progressive=1
:MED:Flash:640x480:Medium::avformat::flv:f=flv acodec=libmp3lame minrate=0 b=5000k ab=32000 ar=22050 s=640x480 progressive=1
:MED:Flash:640x480:High::avformat::flv:f=flv acodec=libmp3lame minrate=0 b=7000k ab=128000 ar=22050 s=640x480 progressive=1

:MED:Flash:720x576:Low::avformat::flv:f=flv acodec=libmp3lame minrate=0 b=6000k ab=32000 ar=22050 s=720x576 progressive=1
:MED:Flash:720x576:Medium::avformat::flv:f=flv acodec=libmp3lame minrate=0 b=9000k ab=64000 ar=22050 s=720x576 progressive=1
:MED:Flash:720x576:High::avformat::flv:f=flv acodec=libmp3lame minrate=0 b=12000k ab=128000 ar=22050 s=720x576 progressive=1

### QUICKTIME

:MED:Quicktime:128x96:Low::avformat::mov:f=mov minrate=0 b=200k ab=8000 ar=22050 s=128x96 vcodec=svq1 acodec=ac3 progressive=1
:MED:Quicktime:128x96:Medium::avformat::mov:f=mov minrate=0 b=600k ab=16000 ar=22050 s=128x96 vcodec=svq1 acodec=ac3 progressive=1
:MED:Quicktime:128x96:High::avformat::mov:f=mov minrate=0 b=800k ab=32000 ar=22050 s=128x96 vcodec=svq1 acodec=ac3 progressive=1

:MED:Quicktime:176x144:Low::avformat::mov:f=mov minrate=0 b=600k ab=8000 ar=22050 s=176x144 vcodec=svq1 acodec=ac3 progressive=1
:MED:Quicktime:176x144:Medium::avformat::mov:f=mov minrate=0 b=800k ab=16000 ar=22050 s=176x144 vcodec=svq1 acodec=ac3 progressive=1
:MED:Quicktime:176x144:High::avformat::mov:f=mov minrate=0 b=1000k ab=32000 ar=22050 s=176x144 vcodec=svq1 acodec=ac3 progressive=1

:MED:Quicktime:352x288:Low::avformat::mov:f=mov minrate=0 b=800k ab=8000 ar=22050 s=352x288 vcodec=svq1 acodec=ac3 progressive=1
:MED:Quicktime:352x288:Medium::avformat::mov:f=mov minrate=0 b=1000k ab=16000 ar=22050 s=352x288 vcodec=svq1 acodec=ac3 progressive=1
:MED:Quicktime:352x288:High::avformat::mov:f=mov minrate=0 b=1200k ab=32000 ar=22050 s=352x288 vcodec=svq1 acodec=ac3 progressive=1

:MED:Quicktime:704x576:Low::avformat::mov:f=mov minrate=0 b=3000k ab=16000 ar=22050 s=704x576 vcodec=svq1 acodec=ac3 progressive=1
:MED:Quicktime:704x576:Medium::avformat::mov:f=mov minrate=0 b=5000k ab=32000 ar=22050 s=704x576 vcodec=svq1 acodec=ac3 progressive=1
:MED:Quicktime:704x576:High::avformat::mov:f=mov minrate=0 b=7000k ab=64000 ar=22050 s=704x576 vcodec=svq1 acodec=ac3 progressive=1

### XVID

:MED:XVid:160x120:Low::avformat::avi:minrate=0 b=2000k ab=8000 ar=22050 s=160x120 vcodec=mpeg vtag=XVID acodec=ac3 progressive=1
:MED:XVid:160x120:Medium::avformat::avi:minrate=0 b=600k ab=16000 ar=22050 s=160x120 vcodec=mpeg vtag=XVID acodec=ac3 progressive=1
:MED:XVid:160x120:High::avformat::avi:minrate=0 b=800k ab=32000 ar=22050 s=160x120 vcodec=mpeg vtag=XVID acodec=ac3 progressive=1

:MED:XVid:240x180:Low::avformat::avi:minrate=0 b=6000k ab=8000 ar=22050 s=240x180 vcodec=mpeg vtag=XVID acodec=ac3 progressive=1
:MED:XVid:240x180:Medium::avformat::avi:minrate=0 b=800k ab=16000 ar=22050 s=240x180 vcodec=mpeg vtag=XVID acodec=ac3 progressive=1
:MED:XVid:240x180:High::avformat::avi:minrate=0 b=1000k ab=32000 ar=22050 s=240x180 vcodec=mpeg vtag=XVID acodec=ac3 progressive=1

:MED:XVid:320x200:Low::avformat::avi:minrate=0 b=800k ab=8000 ar=22050 s=320x200 vcodec=mpeg vtag=XVID acodec=ac3 progressive=1
:MED:XVid:320x200:Medium::avformat::avi:minrate=0 b=1000k ab=16000 ar=22050 s=320x200 vcodec=mpeg vtag=XVID acodec=ac3 progressive=1
:MED:XVid:320x200:High::avformat::avi:minrate=0 b=1200k ab=32000 ar=22050 s=320x200 vcodec=mpeg vtag=XVID acodec=ac3 progressive=1

:MED:XVid:720x576:Low::avformat::avi:minrate=0 b=3000k ab=16000 ar=22050 s=720x576 vcodec=mpeg vtag=XVID acodec=ac3 progressive=1
:MED:XVid:720x576:Medium::avformat::avi:minrate=0 b=5000k ab=32000 ar=22050 s=720x576 vcodec=mpeg vtag=XVID acodec=ac3 progressive=1
:MED:XVid:720x576:High::avformat::avi:minrate=0 b=7000k ab=64000 ar=22050 s=720x576 vcodec=mpeg vtag=XVID acodec=ac3 progressive=1

### REALVIDEO

:MED:Real Video:160x120:Low::avformat::rm:f=rv10 minrate=0 b=100k ab=16000 ar=11025 s=160x120 g=8 progressive=1
:MED:Real Video:160x120:Medium::avformat::rm:f=rv10 minrate=0 b=300k ab=64000 ar=22050 s=160x120 g=8 progressive=1
:MED:Real Video:160x120:High::avformat::rm:f=rv10 minrate=0 b=500k ab=128000 ar=32000 s=160x120 g=8 progressive=1

:MED:Real Video:240x180:Low::avformat::rm:f=rv10 minrate=0 b=300k ab=32000 ar=22050 s=240x180 g=8 progressive=1
:MED:Real Video:240x180:Medium::avformat::rm:f=rv10 minrate=0 b=500k ab=64000 ar=32000 s=240x180 g=8 progressive=1
:MED:Real Video:240x180:High::avformat::rm:f=rv10 minrate=0 b=800k ab=128000 ar=44100 s=240x180 g=8 progressive=1

:MED:Real Video:320x200:Low::avformat::rm:f=rv10 minrate=0 b=500k ab=32000 ar=22050 s=320x200 g=8 progressive=1
:MED:Real Video:320x200:Medium::avformat::rm:f=rv10 minrate=0 b=7000k ab=64000 ar=32000 s=320x200 g=8 progressive=1
:MED:Real Video:320x200:High::avformat::rm:f=rv10 minrate=0 b=1000k ab=128000 ar=44100 s=320x200 g=8 progressive=1

:MED:Real Video:640x480:Low::avformat::rm:f=rv10 minrate=0 b=3000k ab=32000 ar=22050 s=640x480 g=8 progressive=1
:MED:Real Video:640x480:Medium::avformat::rm:f=rv10 minrate=0 b=5000k ab=64000 ar=32000 s=640x480 g=8 progressive=1
:MED:Real Video:640x480:High::avformat::rm:f=rv10 minrate=0 b=7000k ab=128000 ar=44100 s=640x480 g=8 progressive=1

:MED:Real Video:720x576:Low::avformat::rm:f=rv10 minrate=0 b=6000k ab=64000 ar=32000 s=720x576 g=8 progressive=1
:MED:Real Video:720x576:Medium::avformat::rm:f=rv10 minrate=0 b=9000k ab=128000 ar=44100 s=720x576 g=8 progressive=1
:MED:Real Video:720x576:High::avformat::rm:f=rv10 minrate=0 b=12000k ab=384000 ar=48000 s=720x576 g=8 progressive=1


### THEORA

:MED:Theora:160x112:Low::avformat::ogg:vcodec=libtheora acodec=vorbis minrate=0 b=100k ab=16000 ar=11025 s=160x112 progressive=1
:MED:Theora:160x112:Medium::avformat::ogg:vcodec=libtheora acodec=vorbis minrate=0 b=300k ab=64000 ar=22050 s=160x112 progressive=1
:MED:Theora:160x112:High::avformat::ogg:vcodec=libtheora acodec=vorbis minrate=0 b=500k ab=128000 ar=32000 s=160x112 progressive=1


:MED:Theora:320x240:Low::avformat::ogg:vcodec=libtheora acodec=vorbis minrate=0 b=600k ab=16000 ar=11025 s=320x240 progressive=1
:MED:Theora:320x240:Medium::avformat::ogg:vcodec=libtheora acodec=vorbis minrate=0 b=1000k ab=64000 ar=22050 s=320x240 progressive=1
:MED:Theora:320x240:High::avformat::ogg:vcodec=libtheora acodec=vorbis minrate=0 b=1200k ab=128000 ar=32000 s=320x240 progressive=1

:MED:Theora:640x480:Low::avformat::ogg:vcodec=libtheora acodec=vorbis minrate=0 b=3000k ab=32000 ar=22050 s=640x480 progressive=1
:MED:Theora:640x480:Medium::avformat::ogg:vcodec=libtheora acodec=vorbis minrate=0 b=5000k ab=64000 ar=32000 s=640x480 progressive=1
:MED:Theora:640x480:High::avformat::ogg:vcodec=libtheora acodec=vorbis minrate=0 b=7000k ab=128000 ar=44100 s=640x480 progressive=1


#### AUDIO

### WAV

:AUDIO:Wav:48000:::avformat::wav:f=wav ar=48000
:AUDIO:Wav:44100:::avformat::wav:f=wav ar=44100
:AUDIO:Wav:32000:::avformat::wav:f=wav ar=32000
:AUDIO:Wav:22050:::avformat::wav:f=wav ar=22050

### MP3

:AUDIO:Mp3:48000:384kb/s::avformat::libmp3lame:f=libmp3lame ar=48000 ab=384000
:AUDIO:Mp3:48000:128kb/s::avformat::libmp3lame:f=libmp3lame ar=48000 ab=128000
:AUDIO:Mp3:48000:64kb/s::avformat::libmp3lame:f=libmp3lame ar=48000 ab=64000

:AUDIO:Mp3:44100:384kb/s::avformat::libmp3lame:f=libmp3lame ar=44100 ab=384000
:AUDIO:Mp3:44100:128kb/s::avformat::libmp3lame:f=libmp3lame ar=44100 ab=128000
:AUDIO:Mp3:44100:64kb/s::avformat::libmp3lame:f=libmp3lame ar=44100 ab=64000

:AUDIO:Mp3:32000:128kb/s::avformat::libmp3lame:f=libmp3lame ar=32000 ab=128000
:AUDIO:Mp3:32000:64kb/s::avformat::libmp3lame:f=libmp3lame ar=32000 ab=64000
:AUDIO:Mp3:32000:32kb/s::avformat::libmp3lame:f=libmp3lame ar=32000 ab=32000

:AUDIO:Mp3:22050:64kb/s::avformat::libmp3lame:f=libmp3lame ar=22050 ab=64000
:AUDIO:Mp3:22050:32kb/s::avformat::libmp3lame:f=libmp3lame ar=22050 ab=32000
:AUDIO:Mp3:22050:16kb/s::avformat::libmp3lame:f=libmp3lame ar=22050 ab=16000


### AC3

:AUDIO:AC3:48000:448kb/s::avformat::ac3:f=ac3 ar=48000 ab=448000
:AUDIO:AC3:48000:224kb/s::avformat::ac3:f=ac3 ar=48000 ab=224000
:AUDIO:AC3:48000:128kb/s::avformat::ac3:f=ac3 ar=48000 ab=128000

:AUDIO:AC3:44100:384kb/s::avformat::ac3:f=ac3 ar=44100 ab=384000
:AUDIO:AC3:44100:128kb/s::avformat::ac3:f=ac3 ar=44100 ab=128000
:AUDIO:AC3:44100:64kb/s::avformat::ac3:f=ac3 ar=44100 ab=64000


### VORBIS

:AUDIO:Ogg Vorbis:48000:384kb/s::avformat::ogg:f=ogg ar=48000 ab=384000
:AUDIO:Ogg Vorbis:48000:128kb/s::avformat::ogg:f=ogg ar=48000 ab=128000
:AUDIO:Ogg Vorbis:48000:64kb/s::avformat::ogg:f=ogg ar=48000 ab=64000

:AUDIO:Ogg Vorbis:44100:384kb/s::avformat::ogg:f=ogg ar=44100 ab=384000
:AUDIO:Ogg Vorbis:44100:128kb/s::avformat::ogg:f=ogg ar=44100 ab=128000
:AUDIO:Ogg Vorbis:44100:64kb/s::avformat::ogg:f=ogg ar=44100 ab=64000

:AUDIO:Ogg Vorbis:32000:128kb/s::avformat::ogg:f=ogg ar=32000 ab=128000
:AUDIO:Ogg Vorbis:32000:64kb/s::avformat::ogg:f=ogg ar=32000 ab=64000
:AUDIO:Ogg Vorbis:32000:32kb/s::avformat::ogg:f=ogg ar=32000 ab=32000

:AUDIO:Ogg Vorbis:22050:64kb/s::avformat::ogg:f=ogg ar=22050 ab=64000
:AUDIO:Ogg Vorbis:22050:32kb/s::avformat::ogg:f=ogg ar=22050 ab=32000
:AUDIO:Ogg Vorbis:22050:16kb/s::avformat::ogg:f=ogg ar=22050 ab=16000

