
#include <QMap>
#include <QFile>
#include <mlt++/Mlt.h>
#include <iostream>
#include <cstdlib>

int info(char *filename)
{

    // Initialize MLT
    Mlt::Factory::init(NULL);

    // Load an arbitrary profile
    Mlt::Profile prof("hdv_1080_25p");


    std::cout << "MLT initialized, profile loaded. Loading clips ..." << std::endl;


    Mlt::Producer producer(prof, filename);
    if (!producer.is_valid()) {
        std::cout << filename << " is invalid." << std::endl;
        return 2;
    }
    std::cout << "Successfully loaded producer " << filename << std::endl;

    int streams = atoi(producer.get("meta.media.nb_streams"));
    std::cout << "Number of streams: " << streams << std::endl;

    int audioStream = -1;
    for (int i = 0; i < streams; i++) {
        std::string propertyName = QString("meta.media.%1.stream.type").arg(i).toStdString();
        std::cout << "Producer " << i << ": " << producer.get(propertyName.c_str());
        if (strcmp("audio", producer.get(propertyName.c_str())) == 0) {
            std::cout << " (Using this stream)";
            audioStream = i;
        }
        std::cout << std::endl;
    }

    if (audioStream >= 0) {
        QMap<QString, QString> map;
        map.insert("Sampling format", QString("meta.media.%1.codec.sample_fmt").arg(audioStream));
        map.insert("Sampling rate", QString("meta.media.%1.codec.sample_rate").arg(audioStream));
        map.insert("Bit rate", QString("meta.media.%1.codec.bit_rate").arg(audioStream));
        map.insert("Channels", QString("meta.media.%1.codec.channels").arg(audioStream));
        map.insert("Codec", QString("meta.media.%1.codec.name").arg(audioStream));
        map.insert("Codec, long name", QString("meta.media.%1.codec.long_name").arg(audioStream));

        std::cout << "Audio properties (stream " << audioStream << ")" << std::endl;
        foreach (QString key, map.keys()) {
            std::string value = map.value(key).toStdString();
            std::cout << "\t" << key.toStdString() << ": " << producer.get(value.c_str())
                      << "  (" << value << ")" << std::endl;
        }
    }

    return 0;
}

int main(int argc, char *argv[])
{
    char *fileMain;
    char *fileSub;
    if (argc > 2) {
        fileMain = argv[1];
        fileSub = argv[2];
    } else {
        std::cout << "Usage: " << argv[0] << " <main audio file> <second audio file>" << std::endl;
        return 0;
    }
    std::cout << "Trying to align (1)\n\t" << fileSub << "\nto fit on (2)\n\t" << fileMain
              << "\n, result will indicate by how much (1) has to be moved." << std::endl;


    info(fileMain);
    info(fileSub);

/*
    Mlt::Frame *frame = producer.get_frame();


    float *data = (float*)frame->get_audio(format, samplingRate, channels, nSamples);
    producer.set("video_index", "-1");

    if (KdenliveSettings::normaliseaudiothumbs()) {
        Mlt::Filter m_convert(prof, "volume");
        m_convert.set("gain", "normalise");
        producer.attach(m_convert);
    }

    int last_val = 0;
    int val = 0;
    double framesPerSecond = mlt_producer_get_fps(producer.get_producer());
    Mlt::Frame *mlt_frame;

    for (int z = (int) frame; z < (int)(frame + lengthInFrames) && producer.is_valid() &&  !m_abortAudioThumb; z++) {
        val = (int)((z - frame) / (frame + lengthInFrames) * 100.0);
        if (last_val != val && val > 1) {
            setThumbsProgress(i18n("Creating audio thumbnail for %1", url.fileName()), val);
            last_val = val;
        }
        producer.seek(z);
        mlt_frame = producer.get_frame();
        if (mlt_frame && mlt_frame->is_valid()) {
            int samples = mlt_sample_calculator(framesPerSecond, frequency, mlt_frame_get_position(mlt_frame->get_frame()));
            qint16* pcm = static_cast<qint16*>(mlt_frame->get_audio(audioFormat, frequency, channels, samples));
            for (int c = 0; c < channels; c++) {
                QByteArray audioArray;
                audioArray.resize(arrayWidth);
                for (int i = 0; i < audioArray.size(); i++) {
                    audioArray[i] = ((*(pcm + c + i * samples / audioArray.size())) >> 9) + 127 / 2 ;
                }
                f.write(audioArray);
                storeIn[z][c] = audioArray;
            }
        } else {
            f.write(QByteArray(arrayWidth, '\x00'));
        }
        delete mlt_frame;
    }
    f.close();
    setThumbsProgress(i18n("Creating audio thumbnail for %1", url.fileName()), -1);
    if (m_abortAudioThumb) {
        f.remove();
    } else {
        clip->updateAudioThumbnail(storeIn);
    }*/

    return 0;

}
