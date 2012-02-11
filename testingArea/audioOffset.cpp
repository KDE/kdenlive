
#include <QFile>
#include <mlt++/Mlt.h>
#include <iostream>
#include <cstdlib>

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


    // Initialize MLT
    Mlt::Factory::init(NULL);

    // Load an arbitrary profile
    Mlt::Profile prof("hdv_1080_25p");


    std::cout << "MLT initialized, profile loaded. Loading clips ..." << std::endl;


    Mlt::Producer producer(prof, fileMain);
    if (!producer.is_valid()) {
        std::cout << fileMain << " is invalid." << std::endl;
        return 2;
    }
    std::cout << "Successfully loaded producer " << fileMain << std::endl;
    std::cout << "Some data: " << std::endl
              << "Length: " << producer.get_length() << std::endl
              << "fps: " << producer.get_fps() << std::endl;

    int nSamples = 50;
    Mlt::Frame *frame = producer.get_frame();
    float *data = frame->get_audio(mlt_audio_float, 24000, 1, nSamples);

    for (int i = 0; i < nSamples; i++) {
        std::cout << " " << data[i];
    }
    std::cout << std::endl;

/*
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
