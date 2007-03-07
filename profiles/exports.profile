# Kdenlive profile file. Lines beginning with ##

# format:
# 0 id : 1 Format : 2 Name : 3 param1 : 4 param2 : 5 param3 : 6 MLT consumer : 7 MLT Normalisation: 8 extension : 9 arguments

#### HQ

# DV
:HQ:DV:Raw DV:PAL::libdv:PAL:dv:
:HQ:DV:Raw DV:NTSC::libdv:NTSC:dv:
:HQ:DV:Avi DV:PAL::avformat:PAL:avi:format=avi vcodec=dvvideo acodec=pcm_s16le size=720x576
:HQ:DV:Avi DV:NTSC::avformat:NTSC:avi:format=avi vcodec=dvvideo acodec=pcm_s16le size=720x480

#DVD
:HQ:DVD:PAL:::avformat:PAL:vob:format=dvd aspect=4:3 vcodec=mpeg2video acodec=ac3 video_bit_rate=6500000 video_rc_max_rate=8000000 video_rc_min_rate=0 video_rc_buffer_size=1835008 mux_packet_size=2048 mux_rate=10080000 audio_bit_rate=192000 audio_sample_rate=48000 frame_size=720x576 frame_rate=25 gop_size=15 me_range=63
:HQ:DVD:NTSC:::avformat:NTSC:vob:format=dvd vcodec=mpeg2video acodec=ac3 size=720x480 video_bit_rate=6000000 video_rc_max_rate=9000000 video_rc_min_rate=0 video_rc_buffer_size=1835008 mux_packet_size=2048 mux_rate=10080000 audio_bit_rate=192000 audio_sample_rate=48000 frame_size=720x480 frame_rate=30000/1001 gop_size=18 me_range=63

:HQ:DVD m2v:PAL:::avformat:PAL:m2v:format=mpeg1video aspect=4:3 video_bit_rate=6500000 video_rc_max_rate=8000000 video_rc_min_rate=0 video_rc_buffer_size=1835008 frame_size=720x576 frame_rate=25 gop_size=15 me_range=63
:HQ:DVD m2v:NTSC:::avformat:NTSC:m2v:format=mpeg1video size=720x480 video_bit_rate=6000000 video_rc_max_rate=9000000 video_rc_min_rate=0 video_rc_buffer_size=1835008 mux_packet_size=2048 frame_size=720x480 frame_rate=30000/1001 gop_size=18 me_range=63


#### MEDIUM

### MPEG

:MED:Mpeg:160x120:Low::avformat::mpeg:format=mpeg video_rc_min_rate=0 video_bit_rate=100000 audio_bit_rate=16000 frequency=22050 size=160x120 progressive=1
:MED:Mpeg:160x120:Medium::avformat::mpeg:format=mpeg video_rc_min_rate=0 video_bit_rate=200000 audio_bit_rate=64000 frequency=22050 size=160x120 progressive=1
:MED:Mpeg:160x120:High::avformat::mpeg:format=mpeg video_rc_min_rate=0 video_bit_rate=400000 audio_bit_rate=128000 frequency=32000 size=160x120 progressive=1

:MED:Mpeg:240x180:Low::avformat::mpeg:format=mpeg video_rc_min_rate=0 video_bit_rate=100000 audio_bit_rate=32000 frequency=22050 size=240x180 progressive=1
:MED:Mpeg:240x180:Medium::avformat::mpeg:format=mpeg video_rc_min_rate=0 video_bit_rate=300000 audio_bit_rate=64000 frequency=32000 size=240x180 progressive=1
:MED:Mpeg:240x180:High::avformat::mpeg:format=mpeg video_rc_min_rate=0 video_bit_rate=500000 audio_bit_rate=128000 frequency=44100 size=240x180 progressive=1

:MED:Mpeg:320x200:Low::avformat::mpeg:format=mpeg video_rc_min_rate=0 video_bit_rate=300000 audio_bit_rate=32000 frequency=22050 size=320x200 progressive=1
:MED:Mpeg:320x200:Medium::avformat::mpeg:format=mpeg video_rc_min_rate=0 video_bit_rate=500000 audio_bit_rate=64000 frequency=32000 size=320x200 progressive=1
:MED:Mpeg:320x200:High::avformat::mpeg:format=mpeg video_rc_min_rate=0 video_bit_rate=800000 audio_bit_rate=128000 frequency=44100 size=320x200 progressive=1

:MED:Mpeg:640x480:Low::avformat::mpeg:format=mpeg video_rc_min_rate=0 video_bit_rate=1000000 audio_bit_rate=32000 frequency=22050 size=640x480 progressive=1
:MED:Mpeg:640x480:Medium::avformat::mpeg:format=mpeg video_rc_min_rate=0 video_bit_rate=3000000 audio_bit_rate=64000 frequency=32000 size=640x480 progressive=1
:MED:Mpeg:640x480:High::avformat::mpeg:format=mpeg video_rc_min_rate=0 video_bit_rate=6000000 audio_bit_rate=128000 frequency=44100 size=640x480 progressive=1

:MED:Mpeg:720x576:Low::avformat::mpeg:format=mpeg video_rc_min_rate=0 video_bit_rate=6000000 audio_bit_rate=64000 frequency=32000 size=720x576 progressive=1
:MED:Mpeg:720x576:Medium::avformat::mpeg:format=mpeg video_rc_min_rate=0 video_bit_rate=9000000 audio_bit_rate=128000 frequency=44100 size=720x576 progressive=1
:MED:Mpeg:720x576:High::avformat::mpeg:format=mpeg video_rc_min_rate=0 video_bit_rate=12000000 audio_bit_rate=384000 frequency=48000 size=720x576 progressive=1

###MPEG4

:MED:Mpeg4:160x120:Low::avformat::avi:format=avi video_rc_min_rate=0 video_bit_rate=100000 audio_bit_rate=32000 frequency=22050 size=160x120 vcodec=mpeg4 mbd=2 trell=1 v4mv=1 progressive=1
:MED:Mpeg4:160x120:Medium::avformat::avi:format=avi video_rc_min_rate=0 video_bit_rate=300000 audio_bit_rate=64000 frequency=22050 size=160x120 vcodec=mpeg4 mbd=2 trell=1 v4mv=1 progressive=1
:MED:Mpeg4:160x120:High::avformat::avi:format=avi video_rc_min_rate=0 video_bit_rate=600000 audio_bit_rate=128000 frequency=32000 size=160x120 vcodec=mpeg4 mbd=2 trell=1 v4mv=1 progressive=1

:MED:Mpeg4:240x180:Low::avformat::avi:format=avi video_rc_min_rate=0 video_bit_rate=400000 audio_bit_rate=32000 frequency=22050 size=240x180 vcodec=mpeg4 mbd=2 trell=1 v4mv=1 progressive=1
:MED:Mpeg4:240x180:Medium::avformat::avi:format=avi video_rc_min_rate=0 video_bit_rate=700000 audio_bit_rate=64000 frequency=32000 size=240x180 vcodec=mpeg4 mbd=2 trell=1 v4mv=1 progressive=1
:MED:Mpeg4:240x180:High::avformat::avi:format=avi video_rc_min_rate=0 video_bit_rate=900000 audio_bit_rate=128000 frequency=44100 size=240x180 vcodec=mpeg4 mbd=2 trell=1 v4mv=1 progressive=1

:MED:Mpeg4:320x200:Low::avformat::avi:format=avi video_rc_min_rate=0 video_bit_rate=600000 audio_bit_rate=32000 frequency=22050 size=320x200 vcodec=mpeg4 mbd=2 trell=1 v4mv=1 progressive=1
:MED:Mpeg4:320x200:Medium::avformat::avi:format=avi video_rc_min_rate=0 video_bit_rate=1000000 audio_bit_rate=64000 frequency=32000 size=320x200 vcodec=mpeg4 mbd=2 trell=1 v4mv=1 progressive=1
:MED:Mpeg4:320x200:High::avformat::avi:format=avi video_rc_min_rate=0 video_bit_rate=1200000 audio_bit_rate=128000 frequency=44100 size=320x200 vcodec=mpeg4 mbd=2 trell=1 v4mv=1 progressive=1

:MED:Mpeg4:640x480:Low::avformat::avi:format=avi video_rc_min_rate=0 video_bit_rate=800000 audio_bit_rate=32000 frequency=22050 size=640x480 vcodec=mpeg4 mbd=2 trell=1 v4mv=1 progressive=1
:MED:Mpeg4:640x480:Medium::avformat::avi:format=avi video_rc_min_rate=0 video_bit_rate=3000000 audio_bit_rate=64000 frequency=32000 size=640x480 vcodec=mpeg4 mbd=2 trell=1 v4mv=1 progressive=1
:MED:Mpeg4:640x480:High::avformat::avi:format=avi video_rc_min_rate=0 video_bit_rate=6000000 audio_bit_rate=128000 frequency=44100 size=640x480 vcodec=mpeg4 mbd=2 trell=1 v4mv=1 progressive=1

:MED:Mpeg4:720x576:Low::avformat::avi:format=avi video_rc_min_rate=0 video_bit_rate=3000000 audio_bit_rate=64000 frequency=32000 size=720x576 vcodec=mpeg4 mbd=2 trell=1 v4mv=1 progressive=1
:MED:Mpeg4:720x576:Medium::avformat::avi:format=avi video_rc_min_rate=0 video_bit_rate=6000000 audio_bit_rate=128000 frequency=44100 size=720x576 vcodec=mpeg4 mbd=2 trell=1 v4mv=1 progressive=1
:MED:Mpeg4:720x576:High::avformat::avi:format=avi video_rc_min_rate=0 video_bit_rate=8000000 audio_bit_rate=384000 frequency=48000 size=720x576 vcodec=mpeg4 mbd=2 trell=1 v4mv=1 progressive=1

### FLV

:MED:Flash:160x120:Low::avformat::flv:format=flv acodec=mp3 video_rc_min_rate=0 video_bit_rate=200000 audio_bit_rate=8000 frequency=22050 size=160x120 progressive=1
:MED:Flash:160x120:Medium::avformat::flv:format=flv acodec=mp3 video_rc_min_rate=0 video_bit_rate=600000 audio_bit_rate=16000 frequency=22050 size=160x120 progressive=1
:MED:Flash:160x120:High::avformat::flv:format=flv acodec=mp3 video_rc_min_rate=0 video_bit_rate=800000 audio_bit_rate=32000 frequency=22050 size=160x120 progressive=1

:MED:Flash:240x180:Low::avformat::flv:format=flv acodec=mp3 video_rc_min_rate=0 video_bit_rate=600000 audio_bit_rate=8000 frequency=22050 size=240x180 progressive=1
:MED:Flash:240x180:Medium::avformat::flv:format=flv acodec=mp3 video_rc_min_rate=0 video_bit_rate=800000 audio_bit_rate=64000 frequency=22050 size=240x180 progressive=1
:MED:Flash:240x180:High::avformat::flv:format=flv acodec=mp3 video_rc_min_rate=0 video_bit_rate=1000000 audio_bit_rate=32000 frequency=22050 size=240x180 progressive=1

:MED:Flash:320x240:Low::avformat::flv:format=flv acodec=mp3 video_rc_min_rate=0 video_bit_rate=800000 audio_bit_rate=8000 frequency=22050 size=320x240 progressive=1
:MED:Flash:320x240:Medium::avformat::flv:format=flv acodec=mp3 video_rc_min_rate=0 video_bit_rate=1000000 audio_bit_rate=16000 frequency=22050 size=320x240 progressive=1
:MED:Flash:320x240:High::avformat::flv:format=flv acodec=mp3 video_rc_min_rate=0 video_bit_rate=1200000 audio_bit_rate=32000 frequency=22050 size=320x240 progressive=1

:MED:Flash:640x480:Low::avformat::flv:format=flv acodec=mp3 video_rc_min_rate=0 video_bit_rate=3000000 audio_bit_rate=16000 frequency=22050 size=640x480 progressive=1
:MED:Flash:640x480:Medium::avformat::flv:format=flv acodec=mp3 video_rc_min_rate=0 video_bit_rate=5000000 audio_bit_rate=32000 frequency=22050 size=640x480 progressive=1
:MED:Flash:640x480:High::avformat::flv:format=flv acodec=mp3 video_rc_min_rate=0 video_bit_rate=7000000 audio_bit_rate=128000 frequency=22050 size=640x480 progressive=1

:MED:Flash:720x576:Low::avformat::flv:format=flv acodec=mp3 video_rc_min_rate=0 video_bit_rate=6000000 audio_bit_rate=32000 frequency=22050 size=720x576 progressive=1
:MED:Flash:720x576:Medium::avformat::flv:format=flv acodec=mp3 video_rc_min_rate=0 video_bit_rate=9000000 audio_bit_rate=64000 frequency=22050 size=720x576 progressive=1
:MED:Flash:720x576:High::avformat::flv:format=flv acodec=mp3 video_rc_min_rate=0 video_bit_rate=12000000 audio_bit_rate=128000 frequency=22050 size=720x576 progressive=1

### QUICKTIME

:MED:Quicktime:128x96:Low::avformat::mov:format=mov video_rc_min_rate=0 video_bit_rate=200000 audio_bit_rate=8000 frequency=22050 size=128x96 vcodec=svq1 acodec=ac3 progressive=1
:MED:Quicktime:128x96:Medium::avformat::mov:format=mov video_rc_min_rate=0 video_bit_rate=600000 audio_bit_rate=16000 frequency=22050 size=128x96 vcodec=svq1 acodec=ac3 progressive=1
:MED:Quicktime:128x96:High::avformat::mov:format=mov video_rc_min_rate=0 video_bit_rate=800000 audio_bit_rate=32000 frequency=22050 size=128x96 vcodec=svq1 acodec=ac3 progressive=1

:MED:Quicktime:176x144:Low::avformat::mov:format=mov video_rc_min_rate=0 video_bit_rate=600000 audio_bit_rate=8000 frequency=22050 size=176x144 vcodec=svq1 acodec=ac3 progressive=1
:MED:Quicktime:176x144:Medium::avformat::mov:format=mov video_rc_min_rate=0 video_bit_rate=800000 audio_bit_rate=16000 frequency=22050 size=176x144 vcodec=svq1 acodec=ac3 progressive=1
:MED:Quicktime:176x144:High::avformat::mov:format=mov video_rc_min_rate=0 video_bit_rate=1000000 audio_bit_rate=32000 frequency=22050 size=176x144 vcodec=svq1 acodec=ac3 progressive=1

:MED:Quicktime:352x288:Low::avformat::mov:format=mov video_rc_min_rate=0 video_bit_rate=800000 audio_bit_rate=8000 frequency=22050 size=352x288 vcodec=svq1 acodec=ac3 progressive=1
:MED:Quicktime:352x288:Medium::avformat::mov:format=mov video_rc_min_rate=0 video_bit_rate=1000000 audio_bit_rate=16000 frequency=22050 size=352x288 vcodec=svq1 acodec=ac3 progressive=1
:MED:Quicktime:352x288:High::avformat::mov:format=mov video_rc_min_rate=0 video_bit_rate=1200000 audio_bit_rate=32000 frequency=22050 size=352x288 vcodec=svq1 acodec=ac3 progressive=1

:MED:Quicktime:704x576:Low::avformat::mov:format=mov video_rc_min_rate=0 video_bit_rate=3000000 audio_bit_rate=16000 frequency=22050 size=704x576 vcodec=svq1 acodec=ac3 progressive=1
:MED:Quicktime:704x576:Medium::avformat::mov:format=mov video_rc_min_rate=0 video_bit_rate=5000000 audio_bit_rate=32000 frequency=22050 size=704x576 vcodec=svq1 acodec=ac3 progressive=1
:MED:Quicktime:704x576:High::avformat::mov:format=mov video_rc_min_rate=0 video_bit_rate=7000000 audio_bit_rate=64000 frequency=22050 size=704x576 vcodec=svq1 acodec=ac3 progressive=1

### XVID

:MED:XVid:160x120:Low::avformat::avi:video_rc_min_rate=0 video_bit_rate=200000 audio_bit_rate=8000 frequency=22050 size=160x120 vcodec=xvid acodec=ac3 progressive=1
:MED:XVid:160x120:Medium::avformat::avi:video_rc_min_rate=0 video_bit_rate=600000 audio_bit_rate=16000 frequency=22050 size=160x120 vcodec=xvid acodec=ac3 progressive=1
:MED:XVid:160x120:High::avformat::avi:video_rc_min_rate=0 video_bit_rate=800000 audio_bit_rate=32000 frequency=22050 size=160x120 vcodec=xvid acodec=ac3 progressive=1

:MED:XVid:240x180:Low::avformat::avi:video_rc_min_rate=0 video_bit_rate=600000 audio_bit_rate=8000 frequency=22050 size=240x180 vcodec=xvid acodec=ac3 progressive=1
:MED:XVid:240x180:Medium::avformat::avi:video_rc_min_rate=0 video_bit_rate=800000 audio_bit_rate=16000 frequency=22050 size=240x180 vcodec=xvid acodec=ac3 progressive=1
:MED:XVid:240x180:High::avformat::avi:video_rc_min_rate=0 video_bit_rate=1000000 audio_bit_rate=32000 frequency=22050 size=240x180 vcodec=xvid acodec=ac3 progressive=1

:MED:XVid:320x200:Low::avformat::avi:video_rc_min_rate=0 video_bit_rate=800000 audio_bit_rate=8000 frequency=22050 size=320x200 vcodec=xvid acodec=ac3 progressive=1
:MED:XVid:320x200:Medium::avformat::avi:video_rc_min_rate=0 video_bit_rate=1000000 audio_bit_rate=16000 frequency=22050 size=320x200 vcodec=xvid acodec=ac3 progressive=1
:MED:XVid:320x200:High::avformat::avi:video_rc_min_rate=0 video_bit_rate=1200000 audio_bit_rate=32000 frequency=22050 size=320x200 vcodec=xvid acodec=ac3 progressive=1

:MED:XVid:720x576:Low::avformat::avi:video_rc_min_rate=0 video_bit_rate=3000000 audio_bit_rate=16000 frequency=22050 size=720x576 vcodec=xvid acodec=ac3 progressive=1
:MED:XVid:720x576:Medium::avformat::avi:video_rc_min_rate=0 video_bit_rate=5000000 audio_bit_rate=32000 frequency=22050 size=720x576 vcodec=xvid acodec=ac3 progressive=1
:MED:XVid:720x576:High::avformat::avi:video_rc_min_rate=0 video_bit_rate=7000000 audio_bit_rate=64000 frequency=22050 size=720x576 vcodec=xvid acodec=ac3 progressive=1

### REALVIDEO

:MED:Real Video:160x120:Low::avformat::rm:format=rv10 video_rc_min_rate=0 video_bit_rate=100000 audio_bit_rate=16000 frequency=11025 size=160x120 gop_size=8 progressive=1
:MED:Real Video:160x120:Medium::avformat::rm:format=rv10 video_rc_min_rate=0 video_bit_rate=300000 audio_bit_rate=64000 frequency=22050 size=160x120 gop_size=8 progressive=1
:MED:Real Video:160x120:High::avformat::rm:format=rv10 video_rc_min_rate=0 video_bit_rate=500000 audio_bit_rate=128000 frequency=32000 size=160x120 gop_size=8 progressive=1

:MED:Real Video:240x180:Low::avformat::rm:format=rv10 video_rc_min_rate=0 video_bit_rate=300000 audio_bit_rate=32000 frequency=22050 size=240x180 gop_size=8 progressive=1
:MED:Real Video:240x180:Medium::avformat::rm:format=rv10 video_rc_min_rate=0 video_bit_rate=500000 audio_bit_rate=64000 frequency=32000 size=240x180 gop_size=8 progressive=1
:MED:Real Video:240x180:High::avformat::rm:format=rv10 video_rc_min_rate=0 video_bit_rate=800000 audio_bit_rate=128000 frequency=44100 size=240x180 gop_size=8 progressive=1

:MED:Real Video:320x200:Low::avformat::rm:format=rv10 video_rc_min_rate=0 video_bit_rate=500000 audio_bit_rate=32000 frequency=22050 size=320x200 gop_size=8 progressive=1
:MED:Real Video:320x200:Medium::avformat::rm:format=rv10 video_rc_min_rate=0 video_bit_rate=700000 audio_bit_rate=64000 frequency=32000 size=320x200 gop_size=8 progressive=1
:MED:Real Video:320x200:High::avformat::rm:format=rv10 video_rc_min_rate=0 video_bit_rate=1000000 audio_bit_rate=128000 frequency=44100 size=320x200 gop_size=8 progressive=1

:MED:Real Video:640x480:Low::avformat::rm:format=rv10 video_rc_min_rate=0 video_bit_rate=3000000 audio_bit_rate=32000 frequency=22050 size=640x480 gop_size=8 progressive=1
:MED:Real Video:640x480:Medium::avformat::rm:format=rv10 video_rc_min_rate=0 video_bit_rate=5000000 audio_bit_rate=64000 frequency=32000 size=640x480 gop_size=8 progressive=1
:MED:Real Video:640x480:High::avformat::rm:format=rv10 video_rc_min_rate=0 video_bit_rate=7000000 audio_bit_rate=128000 frequency=44100 size=640x480 gop_size=8 progressive=1

:MED:Real Video:720x576:Low::avformat::rm:format=rv10 video_rc_min_rate=0 video_bit_rate=6000000 audio_bit_rate=64000 frequency=32000 size=720x576 gop_size=8 progressive=1
:MED:Real Video:720x576:Medium::avformat::rm:format=rv10 video_rc_min_rate=0 video_bit_rate=9000000 audio_bit_rate=128000 frequency=44100 size=720x576 gop_size=8 progressive=1
:MED:Real Video:720x576:High::avformat::rm:format=rv10 video_rc_min_rate=0 video_bit_rate=12000000 audio_bit_rate=384000 frequency=48000 size=720x576 gop_size=8 progressive=1


### THEORA

:MED:Theora:160x112:Low::avformat::avi:vcodec=libtheora acodec=vorbis video_rc_min_rate=0 video_bit_rate=100000 audio_bit_rate=16000 frequency=11025 size=160x112 progressive=1
:MED:Theora:160x112:Medium::avformat::avi:vcodec=libtheora acodec=vorbis video_rc_min_rate=0 video_bit_rate=300000 audio_bit_rate=64000 frequency=22050 size=160x112 progressive=1
:MED:Theora:160x112:High::avformat::avi:vcodec=libtheora acodec=vorbis video_rc_min_rate=0 video_bit_rate=500000 audio_bit_rate=128000 frequency=32000 size=160x112 progressive=1


:MED:Theora:320x240:Low::avformat::avi:vcodec=libtheora acodec=vorbis video_rc_min_rate=0 video_bit_rate=600000 audio_bit_rate=16000 frequency=11025 size=320x240 progressive=1
:MED:Theora:320x240:Medium::avformat::avi:vcodec=libtheora acodec=vorbis video_rc_min_rate=0 video_bit_rate=1000000 audio_bit_rate=64000 frequency=22050 size=320x240 progressive=1
:MED:Theora:320x240:High::avformat::avi:vcodec=libtheora acodec=vorbis video_rc_min_rate=0 video_bit_rate=1200000 audio_bit_rate=128000 frequency=32000 size=320x240 progressive=1

:MED:Theora:640x480:Low::avformat::avi:vcodec=libtheora acodec=vorbis video_rc_min_rate=0 video_bit_rate=3000000 audio_bit_rate=32000 frequency=22050 size=640x480 progressive=1
:MED:Theora:640x480:Medium::avformat::avi:vcodec=libtheora acodec=vorbis video_rc_min_rate=0 video_bit_rate=5000000 audio_bit_rate=64000 frequency=32000 size=640x480 progressive=1
:MED:Theora:640x480:High::avformat::avi:vcodec=libtheora acodec=vorbis video_rc_min_rate=0 video_bit_rate=7000000 audio_bit_rate=128000 frequency=44100 size=640x480 progressive=1


#### AUDIO

### WAV

:AUDIO:Wav:48000:::avformat::wav:format=wav frequency=48000
:AUDIO:Wav:44100:::avformat::wav:format=wav frequency=44100
:AUDIO:Wav:32000:::avformat::wav:format=wav frequency=32000
:AUDIO:Wav:22050:::avformat::wav:format=wav frequency=22050

### MP3

:AUDIO:Mp3:48000:384kb/s::avformat::mp3:format=mp3 frequency=48000 audio_bit_rate=384000
:AUDIO:Mp3:48000:128kb/s::avformat::mp3:format=mp3 frequency=48000 audio_bit_rate=128000
:AUDIO:Mp3:48000:64kb/s::avformat::mp3:format=mp3 frequency=48000 audio_bit_rate=64000

:AUDIO:Mp3:44100:384kb/s::avformat::mp3:format=mp3 frequency=44100 audio_bit_rate=384000
:AUDIO:Mp3:44100:128kb/s::avformat::mp3:format=mp3 frequency=44100 audio_bit_rate=128000
:AUDIO:Mp3:44100:64kb/s::avformat::mp3:format=mp3 frequency=44100 audio_bit_rate=64000

:AUDIO:Mp3:32000:128kb/s::avformat::mp3:format=mp3 frequency=32000 audio_bit_rate=128000
:AUDIO:Mp3:32000:64kb/s::avformat::mp3:format=mp3 frequency=32000 audio_bit_rate=64000
:AUDIO:Mp3:32000:32kb/s::avformat::mp3:format=mp3 frequency=32000 audio_bit_rate=32000

:AUDIO:Mp3:22050:64kb/s::avformat::mp3:format=mp3 frequency=22050 audio_bit_rate=64000
:AUDIO:Mp3:22050:32kb/s::avformat::mp3:format=mp3 frequency=22050 audio_bit_rate=32000
:AUDIO:Mp3:22050:16kb/s::avformat::mp3:format=mp3 frequency=22050 audio_bit_rate=16000


### AC3

:AUDIO:AC3:48000:448kb/s::avformat::ac3:format=ac3 frequency=48000 audio_bit_rate=448000
:AUDIO:AC3:48000:224kb/s::avformat::ac3:format=ac3 frequency=48000 audio_bit_rate=224000
:AUDIO:AC3:48000:128kb/s::avformat::ac3:format=ac3 frequency=48000 audio_bit_rate=128000

:AUDIO:AC3:44100:384kb/s::avformat::ac3:format=ac3 frequency=44100 audio_bit_rate=384000
:AUDIO:AC3:44100:128kb/s::avformat::ac3:format=ac3 frequency=44100 audio_bit_rate=128000
:AUDIO:AC3:44100:64kb/s::avformat::ac3:format=ac3 frequency=44100 audio_bit_rate=64000


### VORBIS

:AUDIO:Ogg Vorbis:48000:384kb/s::avformat::ogg:format=ogg frequency=48000 audio_bit_rate=384000
:AUDIO:Ogg Vorbis:48000:128kb/s::avformat::ogg:format=ogg frequency=48000 audio_bit_rate=128000
:AUDIO:Ogg Vorbis:48000:64kb/s::avformat::ogg:format=ogg frequency=48000 audio_bit_rate=64000

:AUDIO:Ogg Vorbis:44100:384kb/s::avformat::ogg:format=ogg frequency=44100 audio_bit_rate=384000
:AUDIO:Ogg Vorbis:44100:128kb/s::avformat::ogg:format=ogg frequency=44100 audio_bit_rate=128000
:AUDIO:Ogg Vorbis:44100:64kb/s::avformat::ogg:format=ogg frequency=44100 audio_bit_rate=64000

:AUDIO:Ogg Vorbis:32000:128kb/s::avformat::ogg:format=ogg frequency=32000 audio_bit_rate=128000
:AUDIO:Ogg Vorbis:32000:64kb/s::avformat::ogg:format=ogg frequency=32000 audio_bit_rate=64000
:AUDIO:Ogg Vorbis:32000:32kb/s::avformat::ogg:format=ogg frequency=32000 audio_bit_rate=32000

:AUDIO:Ogg Vorbis:22050:64kb/s::avformat::ogg:format=ogg frequency=22050 audio_bit_rate=64000
:AUDIO:Ogg Vorbis:22050:32kb/s::avformat::ogg:format=ogg frequency=22050 audio_bit_rate=32000
:AUDIO:Ogg Vorbis:22050:16kb/s::avformat::ogg:format=ogg frequency=22050 audio_bit_rate=16000

