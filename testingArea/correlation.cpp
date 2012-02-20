
#include "../src/lib/audio/audioEnvelope.h"
#include "../src/lib/audio/fftCorrelation.h"

#include <QCoreApplication>
#include <QStringList>
#include <QImage>
#include <QDebug>
#include <iostream>

void printUsage(const char *path)
{
    std::cout << "This executable takes two audio/video files A and B and determines " << std::endl
              << "how much B needs to be shifted in order to be synchronized with A." << std::endl << std::endl
              << path << " <main audio file> <second audio file>" << std::endl
              << "\t-h, --help\n\t\tDisplay this help" << std::endl
              << "\t--profile=<profile>\n\t\tUse the given profile for calculation (run: melt -query profiles)" << std::endl
              << "\t--no-images\n\t\tDo not save envelope and correlation images" << std::endl
                 ;
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QStringList args = app.arguments();
    args.removeAt(0);

    std::string profile = "atsc_1080p_24";
    bool saveImages = true;

    // Load arguments
    foreach (QString str, args) {

        if (str.startsWith("--profile=")) {
            QString s = str;
            s.remove(0, QString("--profile=").length());
            profile = s.toStdString();
            args.removeOne(str);

        } else if (str == "-h" || str == "--help") {
            printUsage(argv[0]);
            return 0;

        } else if (str == "--no-images") {
            saveImages = false;
            args.removeOne(str);
        }

    }

    if (args.length() < 2) {
        printUsage(argv[0]);
        return 1;
    }



    std::string fileMain(args.at(0).toStdString());
    args.removeFirst();
    std::string fileSub = args.at(0).toStdString();
    args.removeFirst();


    qDebug() << "Unused arguments: " << args;


    if (argc > 2) {
        fileMain = argv[1];
        fileSub = argv[2];
    } else {
        std::cout << "Usage: " << argv[0] << " <main audio file> <second audio file>" << std::endl;
        return 0;
    }
    std::cout << "Trying to align (2)\n\t" << fileSub << "\nto fit on (1)\n\t" << fileMain
              << "\n, result will indicate by how much (2) has to be moved." << std::endl
              << "Profile used: " << profile << std::endl
                 ;


    // Initialize MLT
    Mlt::Factory::init(NULL);

    // Load an arbitrary profile
    Mlt::Profile prof(profile.c_str());

    // Load the MLT producers
    Mlt::Producer prodMain(prof, fileMain.c_str());
    if (!prodMain.is_valid()) {
        std::cout << fileMain << " is invalid." << std::endl;
        return 2;
    }
    Mlt::Producer prodSub(prof, fileSub.c_str());
    if (!prodSub.is_valid()) {
        std::cout << fileSub << " is invalid." << std::endl;
        return 2;
    }


    // Build the audio envelopes for the correlation
    AudioEnvelope *envelopeMain = new AudioEnvelope(&prodMain);
    envelopeMain->loadEnvelope();
    envelopeMain->loadStdDev();
    envelopeMain->dumpInfo();

    AudioEnvelope *envelopeSub = new AudioEnvelope(&prodSub);
    envelopeSub->loadEnvelope();
    envelopeSub->loadStdDev();
    envelopeSub->dumpInfo();

    int leftSize = envelopeMain->envelopeSize();
    int rightSize = envelopeSub->envelopeSize();
    float left[leftSize];
    float right[rightSize];
    const int64_t *leftEnv = envelopeMain->envelope();
    const int64_t *rightEnv = envelopeSub->envelope();

    for (int i = 0; i < leftSize; i++) {
        left[i] = double(leftEnv[i])/envelopeMain->maxValue();
        if (i < 20) std::cout << left[i] << " ";
    }
    std::cout << " (max: " << envelopeMain->maxValue() << ")" << std::endl;
    for (int i = 0; i < rightSize; i++) {
        right[i] = double(rightEnv[i])/envelopeSub->maxValue();
    }

    float *correlated;
    int corrSize = 0;
    FFTCorrelation::correlate(left, leftSize, right, rightSize, &correlated, corrSize);

    qDebug() << "Correlated: Size " << corrSize;

    float max = 0;
    for (int i = 0; i < corrSize; i++) {
        if (correlated[i] > max) {
            max = correlated[i];
        }
    }
    qDebug() << "Max correlation value is " << max;

    QImage img(corrSize, 400, QImage::Format_ARGB32);
    img.fill(qRgb(255,255,255));
    for (int x = 0; x < corrSize; x++) {
        float val = correlated[x]/max;
        for (int y = 0; y < 400*val; y++) {
            img.setPixel(x, 400-1-y, qRgb(50,50,50));
        }
    }
    img.save("correlated-fft.png");


    delete correlated;

}
