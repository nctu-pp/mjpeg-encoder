#include <iostream>
#include <filesystem>
#include <getopt.h>

#include "model/Model.h"
#include "core/encoder/AbstractMJPEGEncoder.h"

using namespace std;
using namespace model;

void showHelp(char *const exeName) {
    cout
            << "Usage: " << exeName << " options" << endl
            << "Options:" << endl
            << "\t-i --input <path>\tRAW Video Path." << endl
            << "\t-s --size <width>x<height>\tVideo resolution." << endl
            << "\t-r --fps <fps>\tVideo FPS." << endl
            << "\t-o --output <path>\tOutput path." << endl
            << "\t-q --quality <number, [0-100]>\tJPEG quality." << endl
            << "\t-t --threads <number>\tUse number threads." << endl
            << "\t-k --kind <serial|openmp|opencl>\tSelect implementation, default is serial." << endl
            << "\t-T --temp-dir <path>\tTemp dir, default is system temp dir." << endl
            << "\t-h --help\tShow usage." << endl;
}

int main(int argc, char *argv[]) {
    Arguments arguments{
        .tmpDir = filesystem::temp_directory_path().string(),
        .quality = 100,
    };

    if (argc == 1) {
        showHelp(argv[0]);
        return 0;
    }

    // parse argv options
    do {
        static struct option long_options[] = {
                {"input",    required_argument, nullptr, 'i'},
                {"size",     required_argument, nullptr, 's'},
                {"fps",      required_argument, nullptr, 'r'},
                {"output",   required_argument, nullptr, 'o'},
                {"quality",  optional_argument, nullptr, 'q'},
                {"threads",  optional_argument, nullptr, 't'},
                {"temp-dir", optional_argument, nullptr, 'T'},
                {"kind",     optional_argument, nullptr, 'k'},
                {"help",     no_argument,       nullptr, 'h'},
        };
        char opt;
        while ((opt = (char) getopt_long(argc, argv, "i:s:r:o:t:T:q:h?", long_options, nullptr)) != EOF) {
            switch (opt) {
                case 'i':
                    arguments.input = string(optarg);
                    break;
                case 's':
                    scanf(optarg, "%dx%d", &(arguments.size.width), &(arguments.size.height));
                    break;
                case 'r':
                    arguments.fps = stoi(optarg);
                    break;
                case 'o':
                    arguments.output = string(optarg);
                    break;
                case 't':
                    arguments.numThreads = stoi(optarg);
                    break;
                case 'q':
                    arguments.quality = stoi(optarg);
                    break;
                case 'T':
                    arguments.tmpDir = string(optarg);
                    break;
                case 'k':
                    arguments.kind = model::parseImplKind(optarg);
                    break;
                case '?':
                default:
                    showHelp(argv[0]);
                    return 1;
            }
        }
    } while (false);

    auto mjpegEncoder = core::encoder::AbstractMJPEGEncoder::getInstance(arguments);

    mjpegEncoder->start();
    mjpegEncoder->finalize();

    return 0;
}
