# Kdenlive profile file. Lines beginning with ##

# format:
# 0 id : 1 Format : 2 Name : 3 param1 : 4 param2 : 5 param3 : 6 MLT consumer : 7 MLT Normalisation: 8 extension : 9 arguments

#### HQ

# DV
0:HQ:DV:Raw DV:PAL::libdv:PAL:dv:
1:HQ:DV:Raw DV:NTSC::libdv:NTSC:dv:
2:HQ:DV:Avi DV:PAL::avformat:PAL:avi:format=avi vcodec=dvvideo acodec=pcm_s16le size=720x576
3:HQ:DV:Avi DV:NTSC::avformat:NTSC:avi:format=avi vcodec=dvvideo acodec=pcm_s16le size=720x480

#DVD
4:HQ:DVD:PAL:::avformat:PAL:vob:format=dvd vcodec=mpeg2video acodec=ac3 size=720x576 video_bit_rate=6000000 video_rc_max_rate=9000000 video_rc_min_rate=0 video_rc_buffer_size=1835008 mux_packet_size=2048 mux_rate=10080000 audio_bit_rate=448000 audio_sample_rate=48000 frame_size=720x576 frame_rate=25 gop_size=15
5:HQ:DVD:NTSC:::avformat:NTSC:vob:format=dvd vcodec=mpeg2video acodec=ac3 size=720x480 video_bit_rate=6000000 video_rc_max_rate=9000000 video_rc_min_rate=0 video_rc_buffer_size=1835008 mux_packet_size=2048 mux_rate=10080000 audio_bit_rate=448000 audio_sample_rate=48000 frame_size=720x480 frame_rate=30000/1001 gop_size=18

#### MEDIUM

### MPEG

6:MED:Mpeg:160x120:Low::avformat::mpeg:format=mpeg video_rc_min_rate=0 video_bit_rate=100000 audio_bit_rate=16000 frequency=22050 size=160x120 progressive=1
7:MED:Mpeg:160x120:Medium::avformat::mpeg:format=mpeg video_rc_min_rate=0 video_bit_rate=200000 audio_bit_rate=64000 frequency=22050 size=160x120 progressive=1
8:MED:Mpeg:160x120:High::avformat::mpeg:format=mpeg video_rc_min_rate=0 video_bit_rate=400000 audio_bit_rate=128000 frequency=32000 size=160x120 progressive=1

9:MED:Mpeg:240x180:Low::avformat::mpeg:format=mpeg video_rc_min_rate=0 video_bit_rate=100000 audio_bit_rate=32000 frequency=22050 size=240x180 progressive=1
10:MED:Mpeg:240x180:Medium::avformat::mpeg:format=mpeg video_rc_min_rate=0 video_bit_rate=300000 audio_bit_rate=64000 frequency=32000 size=240x180 progressive=1
11:MED:Mpeg:240x180:High::avformat::mpeg:format=mpeg video_rc_min_rate=0 video_bit_rate=500000 audio_bit_rate=128000 frequency=44100 size=240x180 progressive=1

12:MED:Mpeg:320x200:Low::avformat::mpeg:format=mpeg video_rc_min_rate=0 video_bit_rate=300000 audio_bit_rate=32000 frequency=22050 size=320x200 progressive=1
13:MED:Mpeg:320x200:Medium::avformat::mpeg:format=mpeg video_rc_min_rate=0 video_bit_rate=500000 audio_bit_rate=64000 frequency=32000 size=320x200 progressive=1
14:MED:Mpeg:320x200:High::avformat::mpeg:format=mpeg video_rc_min_rate=0 video_bit_rate=800000 audio_bit_rate=128000 frequency=44100 size=320x200 progressive=1

15:MED:Mpeg:640x480:Low::avformat::mpeg:format=mpeg video_rc_min_rate=0 video_bit_rate=1000000 audio_bit_rate=32000 frequency=22050 size=640x480 progressive=1
16:MED:Mpeg:640x480:Medium::avformat::mpeg:format=mpeg video_rc_min_rate=0 video_bit_rate=3000000 audio_bit_rate=64000 frequency=32000 size=640x480 progressive=1
17:MED:Mpeg:640x480:High::avformat::mpeg:format=mpeg video_rc_min_rate=0 video_bit_rate=6000000 audio_bit_rate=128000 frequency=44100 size=640x480 progressive=1

18:MED:Mpeg:720x576:Low::avformat::mpeg:format=mpeg video_rc_min_rate=0 video_bit_rate=6000000 audio_bit_rate=64000 frequency=32000 size=720x576 progressive=1
19:MED:Mpeg:720x576:Medium::avformat::mpeg:format=mpeg video_rc_min_rate=0 video_bit_rate=9000000 audio_bit_rate=128000 frequency=44100 size=720x576 progressive=1
20:MED:Mpeg:720x576:High::avformat::mpeg:format=mpeg video_rc_min_rate=0 video_bit_rate=12000000 audio_bit_rate=384000 frequency=48000 size=720x576 progressive=1

###MPEG4

21:MED:Mpeg4:160x120:Low::avformat::mpeg:format=avi video_rc_min_rate=0 video_bit_rate=100000 audio_bit_rate=32000 frequency=22050 size=160x120 vcodec=mpeg4 mbd=2 trell=1 v4mv=1 progressive=1
22:MED:Mpeg4:160x120:Medium::avformat::mpeg:format=avi video_rc_min_rate=0 video_bit_rate=300000 audio_bit_rate=64000 frequency=22050 size=160x120 vcodec=mpeg4 mbd=2 trell=1 v4mv=1 progressive=1
23:MED:Mpeg4:160x120:High::avformat::mpeg:format=avi video_rc_min_rate=0 video_bit_rate=600000 audio_bit_rate=128000 frequency=32000 size=160x120 vcodec=mpeg4 mbd=2 trell=1 v4mv=1 progressive=1

24:MED:Mpeg4:240x180:Low::avformat::mpeg:format=avi video_rc_min_rate=0 video_bit_rate=400000 audio_bit_rate=32000 frequency=22050 size=240x180 vcodec=mpeg4 mbd=2 trell=1 v4mv=1 progressive=1
25:MED:Mpeg4:240x180:Medium::avformat::mpeg:format=avi video_rc_min_rate=0 video_bit_rate=700000 audio_bit_rate=64000 frequency=32000 size=240x180 vcodec=mpeg4 mbd=2 trell=1 v4mv=1 progressive=1
26:MED:Mpeg4:240x180:High::avformat::mpeg:format=avi video_rc_min_rate=0 video_bit_rate=900000 audio_bit_rate=128000 frequency=44100 size=240x180 vcodec=mpeg4 mbd=2 trell=1 v4mv=1 progressive=1

27:MED:Mpeg4:320x200:Low::avformat::mpeg:format=avi video_rc_min_rate=0 video_bit_rate=600000 audio_bit_rate=32000 frequency=22050 size=320x200 vcodec=mpeg4 mbd=2 trell=1 v4mv=1 progressive=1
28:MED:Mpeg4:320x200:Medium::avformat::mpeg:format=avi video_rc_min_rate=0 video_bit_rate=1000000 audio_bit_rate=64000 frequency=32000 size=320x200 vcodec=mpeg4 mbd=2 trell=1 v4mv=1 progressive=1
29:MED:Mpeg4:320x200:High::avformat::mpeg:format=avi video_rc_min_rate=0 video_bit_rate=1200000 audio_bit_rate=128000 frequency=44100 size=320x200 vcodec=mpeg4 mbd=2 trell=1 v4mv=1 progressive=1

30:MED:Mpeg4:640x480:Low::avformat::mpeg:format=avi video_rc_min_rate=0 video_bit_rate=800000 audio_bit_rate=32000 frequency=22050 size=640x480 vcodec=mpeg4 mbd=2 trell=1 v4mv=1 progressive=1
31:MED:Mpeg4:640x480:Medium::avformat::mpeg:format=avi video_rc_min_rate=0 video_bit_rate=3000000 audio_bit_rate=64000 frequency=32000 size=640x480 vcodec=mpeg4 mbd=2 trell=1 v4mv=1 progressive=1
32:MED:Mpeg4:640x480:High::avformat::mpeg:format=avi video_rc_min_rate=0 video_bit_rate=6000000 audio_bit_rate=128000 frequency=44100 size=640x480 vcodec=mpeg4 mbd=2 trell=1 v4mv=1 progressive=1

33:MED:Mpeg4:720x576:Low::avformat::mpeg:format=avi video_rc_min_rate=0 video_bit_rate=3000000 audio_bit_rate=64000 frequency=32000 size=720x576 vcodec=mpeg4 mbd=2 trell=1 v4mv=1 progressive=1
34:MED:Mpeg4:720x576:Medium::avformat::mpeg:format=avi video_rc_min_rate=0 video_bit_rate=6000000 audio_bit_rate=128000 frequency=44100 size=720x576 vcodec=mpeg4 mbd=2 trell=1 v4mv=1 progressive=1
35:MED:Mpeg4:720x576:High::avformat::mpeg:format=avi video_rc_min_rate=0 video_bit_rate=8000000 audio_bit_rate=384000 frequency=48000 size=720x576 vcodec=mpeg4 mbd=2 trell=1 v4mv=1 progressive=1

### FLV

36:MED:Flash:160x120:Low::avformat::flv:format=flv acodec=mp3 video_rc_min_rate=0 video_bit_rate=200000 audio_bit_rate=8000 frequency=22050 size=160x120 progressive=1
37:MED:Flash:160x120:Medium::avformat::flv:format=flv acodec=mp3 video_rc_min_rate=0 video_bit_rate=600000 audio_bit_rate=16000 frequency=22050 size=160x120 progressive=1
38:MED:Flash:160x120:High::avformat::flv:format=flv acodec=mp3 video_rc_min_rate=0 video_bit_rate=800000 audio_bit_rate=32000 frequency=22050 size=160x120 progressive=1

39:MED:Flash:240x180:Low::avformat::flv:format=flv acodec=mp3 video_rc_min_rate=0 video_bit_rate=600000 audio_bit_rate=8000 frequency=22050 size=240x180 progressive=1
40:MED:Flash:240x180:Medium::avformat::flv:format=flv acodec=mp3 video_rc_min_rate=0 video_bit_rate=800000 audio_bit_rate=64000 frequency=22050 size=240x180 progressive=1
41:MED:Flash:240x180:High::avformat::flv:format=flv acodec=mp3 video_rc_min_rate=0 video_bit_rate=1000000 audio_bit_rate=32000 frequency=22050 size=240x180 progressive=1

42:MED:Flash:320x240:Low::avformat::flv:format=flv acodec=mp3 video_rc_min_rate=0 video_bit_rate=800000 audio_bit_rate=8000 frequency=22050 size=320x240 progressive=1
43:MED:Flash:320x240:Medium::avformat::flv:format=flv acodec=mp3 video_rc_min_rate=0 video_bit_rate=1000000 audio_bit_rate=16000 frequency=22050 size=320x240 progressive=1
44:MED:Flash:320x240:High::avformat::flv:format=flv acodec=mp3 video_rc_min_rate=0 video_bit_rate=1200000 audio_bit_rate=32000 frequency=22050 size=320x240 progressive=1

45:MED:Flash:640x480:Low::avformat::flv:format=flv acodec=mp3 video_rc_min_rate=0 video_bit_rate=3000000 audio_bit_rate=16000 frequency=22050 size=640x480 progressive=1
46:MED:Flash:640x480:Medium::avformat::flv:format=flv acodec=mp3 video_rc_min_rate=0 video_bit_rate=5000000 audio_bit_rate=32000 frequency=22050 size=640x480 progressive=1
47:MED:Flash:640x480:High::avformat::flv:format=flv acodec=mp3 video_rc_min_rate=0 video_bit_rate=7000000 audio_bit_rate=128000 frequency=22050 size=640x480 progressive=1

48:MED:Flash:720x576:Low::avformat::flv:format=flv acodec=mp3 video_rc_min_rate=0 video_bit_rate=6000000 audio_bit_rate=32000 frequency=22050 size=720x576 progressive=1
49:MED:Flash:720x576:Medium::avformat::flv:format=flv acodec=mp3 video_rc_min_rate=0 video_bit_rate=9000000 audio_bit_rate=64000 frequency=22050 size=720x576 progressive=1
50:MED:Flash:720x576:High::avformat::flv:format=flv acodec=mp3 video_rc_min_rate=0 video_bit_rate=12000000 audio_bit_rate=128000 frequency=22050 size=720x576 progressive=1

### QUICKTIME

51:MED:Quicktime:128x96:Low::avformat::mov:format=mov video_rc_min_rate=0 video_bit_rate=200000 audio_bit_rate=8000 frequency=22050 size=128x96 vcodec=svq1 acodec=ac3 progressive=1
52:MED:Quicktime:128x96:Medium::avformat::mov:format=mov video_rc_min_rate=0 video_bit_rate=600000 audio_bit_rate=16000 frequency=22050 size=128x96 vcodec=svq1 acodec=ac3 progressive=1
53:MED:Quicktime:128x96:High::avformat::mov:format=mov video_rc_min_rate=0 video_bit_rate=800000 audio_bit_rate=32000 frequency=22050 size=128x96 vcodec=svq1 acodec=ac3 progressive=1

54:MED:Quicktime:176x144:Low::avformat::mov:format=mov video_rc_min_rate=0 video_bit_rate=600000 audio_bit_rate=8000 frequency=22050 size=176x144 vcodec=svq1 acodec=ac3 progressive=1
55:MED:Quicktime:176x144:Medium::avformat::mov:format=mov video_rc_min_rate=0 video_bit_rate=800000 audio_bit_rate=16000 frequency=22050 size=176x144 vcodec=svq1 acodec=ac3 progressive=1
56:MED:Quicktime:176x144:High::avformat::mov:format=mov video_rc_min_rate=0 video_bit_rate=1000000 audio_bit_rate=32000 frequency=22050 size=176x144 vcodec=svq1 acodec=ac3 progressive=1

57:MED:Quicktime:352x288:Low::avformat::mov:format=mov video_rc_min_rate=0 video_bit_rate=800000 audio_bit_rate=8000 frequency=22050 size=352x288 vcodec=svq1 acodec=ac3 progressive=1
58:MED:Quicktime:352x288:Medium::avformat::mov:format=mov video_rc_min_rate=0 video_bit_rate=1000000 audio_bit_rate=16000 frequency=22050 size=352x288 vcodec=svq1 acodec=ac3 progressive=1
59:MED:Quicktime:352x288:High::avformat::mov:format=mov video_rc_min_rate=0 video_bit_rate=1200000 audio_bit_rate=32000 frequency=22050 size=352x288 vcodec=svq1 acodec=ac3 progressive=1

60:MED:Quicktime:704x576:Low::avformat::mov:format=mov video_rc_min_rate=0 video_bit_rate=3000000 audio_bit_rate=16000 frequency=22050 size=704x576 vcodec=svq1 acodec=ac3 progressive=1
61:MED:Quicktime:704x576:Medium::avformat::mov:format=mov video_rc_min_rate=0 video_bit_rate=5000000 audio_bit_rate=32000 frequency=22050 size=704x576 vcodec=svq1 acodec=ac3 progressive=1
62:MED:Quicktime:704x576:High::avformat::mov:format=mov video_rc_min_rate=0 video_bit_rate=7000000 audio_bit_rate=64000 frequency=22050 size=704x576 vcodec=svq1 acodec=ac3 progressive=1

### REALVIDEO

63:MED:Real Video:160x120:Low::avformat::rm:format=rv10 video_rc_min_rate=0 video_bit_rate=100000 audio_bit_rate=16000 frequency=11025 size=160x120 gop_size=8 progressive=1
64:MED:Real Video:160x120:Medium::avformat::rm:format=rv10 video_rc_min_rate=0 video_bit_rate=300000 audio_bit_rate=64000 frequency=22050 size=160x120 gop_size=8 progressive=1
65:MED:Real Video:160x120:High::avformat::rm:format=rv10 video_rc_min_rate=0 video_bit_rate=500000 audio_bit_rate=128000 frequency=32000 size=160x120 gop_size=8 progressive=1

66:MED:Real Video:240x180:Low::avformat::rm:format=rv10 video_rc_min_rate=0 video_bit_rate=300000 audio_bit_rate=32000 frequency=22050 size=240x180 gop_size=8 progressive=1
67:MED:Real Video:240x180:Medium::avformat::rm:format=rv10 video_rc_min_rate=0 video_bit_rate=500000 audio_bit_rate=64000 frequency=32000 size=240x180 gop_size=8 progressive=1
68:MED:Real Video:240x180:High::avformat::rm:format=rv10 video_rc_min_rate=0 video_bit_rate=800000 audio_bit_rate=128000 frequency=44100 size=240x180 gop_size=8 progressive=1

69:MED:Real Video:320x200:Low::avformat::rm:format=rv10 video_rc_min_rate=0 video_bit_rate=500000 audio_bit_rate=32000 frequency=22050 size=320x200 gop_size=8 progressive=1
70:MED:Real Video:320x200:Medium::avformat::rm:format=rv10 video_rc_min_rate=0 video_bit_rate=700000 audio_bit_rate=64000 frequency=32000 size=320x200 gop_size=8 progressive=1
71:MED:Real Video:320x200:High::avformat::rm:format=rv10 video_rc_min_rate=0 video_bit_rate=1000000 audio_bit_rate=128000 frequency=44100 size=320x200 gop_size=8 progressive=1

72:MED:Real Video:640x480:Low::avformat::rm:format=rv10 video_rc_min_rate=0 video_bit_rate=3000000 audio_bit_rate=32000 frequency=22050 size=640x480 gop_size=8 progressive=1
73:MED:Real Video:640x480:Medium::avformat::rm:format=rv10 video_rc_min_rate=0 video_bit_rate=5000000 audio_bit_rate=64000 frequency=32000 size=640x480 gop_size=8 progressive=1
74:MED:Real Video:640x480:High::avformat::rm:format=rv10 video_rc_min_rate=0 video_bit_rate=7000000 audio_bit_rate=128000 frequency=44100 size=640x480 gop_size=8 progressive=1

75:MED:Real Video:720x576:Low::avformat::rm:format=rv10 video_rc_min_rate=0 video_bit_rate=6000000 audio_bit_rate=64000 frequency=32000 size=720x576 gop_size=8 progressive=1
76:MED:Real Video:720x576:Medium::avformat::rm:format=rv10 video_rc_min_rate=0 video_bit_rate=9000000 audio_bit_rate=128000 frequency=44100 size=720x576 gop_size=8 progressive=1
77:MED:Real Video:720x576:High::avformat::rm:format=rv10 video_rc_min_rate=0 video_bit_rate=12000000 audio_bit_rate=384000 frequency=48000 size=720x576 gop_size=8 progressive=1


#### AUDIO

### WAV

78:AUDIO:Wav:48000:::avformat::wav:format=wav frequency=48000
79:AUDIO:Wav:44100:::avformat::wav:format=wav frequency=44100
80:AUDIO:Wav:32000:::avformat::wav:format=wav frequency=32000
81:AUDIO:Wav:22050:::avformat::wav:format=wav frequency=22050

### MP3

82:AUDIO:Mp3:48000:384kb/s::avformat::mp3:format=mp3 frequency=48000 audio_bit_rate=384000
83:AUDIO:Mp3:48000:128kb/s::avformat::mp3:format=mp3 frequency=48000 audio_bit_rate=128000
84:AUDIO:Mp3:48000:64kb/s::avformat::mp3:format=mp3 frequency=48000 audio_bit_rate=64000

85:AUDIO:Mp3:44100:384kb/s::avformat::mp3:format=mp3 frequency=44100 audio_bit_rate=384000
86:AUDIO:Mp3:44100:128kb/s::avformat::mp3:format=mp3 frequency=44100 audio_bit_rate=128000
87:AUDIO:Mp3:44100:64kb/s::avformat::mp3:format=mp3 frequency=44100 audio_bit_rate=64000

88:AUDIO:Mp3:32000:128kb/s::avformat::mp3:format=mp3 frequency=32000 audio_bit_rate=128000
89:AUDIO:Mp3:32000:64kb/s::avformat::mp3:format=mp3 frequency=32000 audio_bit_rate=64000
90:AUDIO:Mp3:32000:32kb/s::avformat::mp3:format=mp3 frequency=32000 audio_bit_rate=32000

91:AUDIO:Mp3:22050:64kb/s::avformat::mp3:format=mp3 frequency=22050 audio_bit_rate=64000
92:AUDIO:Mp3:22050:32kb/s::avformat::mp3:format=mp3 frequency=22050 audio_bit_rate=32000
93:AUDIO:Mp3:22050:16kb/s::avformat::mp3:format=mp3 frequency=22050 audio_bit_rate=16000


### VORBIS

94:AUDIO:Ogg Vorbis:48000:384kb/s::avformat::ogg:format=ogg frequency=48000 audio_bit_rate=384000
95:AUDIO:Ogg Vorbis:48000:128kb/s::avformat::ogg:format=ogg frequency=48000 audio_bit_rate=128000
96:AUDIO:Ogg Vorbis:48000:64kb/s::avformat::ogg:format=ogg frequency=48000 audio_bit_rate=64000

97:AUDIO:Ogg Vorbis:44100:384kb/s::avformat::ogg:format=ogg frequency=44100 audio_bit_rate=384000
98:AUDIO:Ogg Vorbis:44100:128kb/s::avformat::ogg:format=ogg frequency=44100 audio_bit_rate=128000
99:AUDIO:Ogg Vorbis:44100:64kb/s::avformat::ogg:format=ogg frequency=44100 audio_bit_rate=64000

100:AUDIO:Ogg Vorbis:32000:128kb/s::avformat::ogg:format=ogg frequency=32000 audio_bit_rate=128000
101:AUDIO:Ogg Vorbis:32000:64kb/s::avformat::ogg:format=ogg frequency=32000 audio_bit_rate=64000
102:AUDIO:Ogg Vorbis:32000:32kb/s::avformat::ogg:format=ogg frequency=32000 audio_bit_rate=32000

103:AUDIO:Ogg Vorbis:22050:64kb/s::avformat::ogg:format=ogg frequency=22050 audio_bit_rate=64000
104:AUDIO:Ogg Vorbis:22050:32kb/s::avformat::ogg:format=ogg frequency=22050 audio_bit_rate=32000
105:AUDIO:Ogg Vorbis:22050:16kb/s::avformat::ogg:format=ogg frequency=22050 audio_bit_rate=16000

