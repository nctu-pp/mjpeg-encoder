#include <iostream>
#include <filesystem>
#include <getopt.h>
#include <sys/stat.h>

#include "model/Model.h"
#include "core/encoder/AbstractMJPEGEncoder.h"
#include "core/Utils.h"

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
            << "\t-d --device <cpu|gpu>, default is cpu." << endl
            << "\t-h --help\tShow usage." << endl;
}

void checkArguments(const Arguments &arguments) {
    bool hasError = false;
    struct stat statBuffer;
    if (arguments.input.empty()) {
        cerr << "Missing argument [-i]." << endl;
        hasError = true;
    } else {
        if (stat(arguments.input.c_str(), &statBuffer) != 0) {
            cerr << "Cannot found file [" << arguments.input << "]." << endl;
            hasError = true;
        } else {
            size_t perFrameSize = arguments.size.width * arguments.size.height * sizeof(model::color::RGBA);
            if (statBuffer.st_size % perFrameSize != 0) {
                cerr << "Invalid input file or resolution, file size " << statBuffer.st_size
                     << " not divisible by " << perFrameSize
                     << " (" << arguments.size.width << "x" << arguments.size.height << ")." << endl;
                hasError = true;
            }
        }
    }

    if (arguments.output.empty()) {
        cerr << "Missing argument [-o]." << endl;
        hasError = true;
    }
    if (arguments.size.height <= 0 || arguments.size.width <= 0) {
        cerr << "Missing or wrong argument [-s]." << endl;
        hasError = true;
    }
    if (arguments.fps <= 0) {
        cerr << "Missing or wrong argument [-r]." << endl;
        hasError = true;
    }

    if (hasError) {
        exit(-1);
    }
}

int main(int argc, char *argv[]) {
    TEST_TIME_START(main);
    Arguments arguments{
            .tmpDir = filesystem::temp_directory_path().string(),
            .quality = 100,
            .kind = Serial,
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
                {"quality",  required_argument, nullptr, 'q'},
                {"threads",  required_argument, nullptr, 't'},
                {"temp-dir", required_argument, nullptr, 'T'},
                {"kind",     required_argument, nullptr, 'k'},
                {"device",   required_argument, nullptr, 'd'},
                {"help",     no_argument,       nullptr, 'h'},
        };
        char opt;
        while ((opt = (char) getopt_long(argc, argv, ":i:s:r:o:t:T:q:k:d:h?", long_options, nullptr)) != EOF) {
            switch (opt) {
                case 'i':
                    arguments.input = string(optarg);
                    break;
                case 's':
                    sscanf(optarg, // NOLINT(cert-err34-c)
                           "%dx%d", &(arguments.size.width), &(arguments.size.height));
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
                case 'd':
                    arguments.device = model::parseCLDevice(optarg);
                    break;
                case ':':
                    /* missing option argument 参数缺失*/
                    cerr << "option -'" << optopt << "' requires an argument" << endl;
                    exit(-1);
                    break;
                case '?':
                default:
                    showHelp(argv[0]);
                    return 1;
            }
        }
    } while (false);

    checkArguments(arguments);

    auto mjpegEncoder = core::encoder::AbstractMJPEGEncoder::getInstance(arguments);

    mjpegEncoder->start();
    mjpegEncoder->finalize();
    cout << endl << "RUN TIME: " << TEST_TIME_END(main) << endl;
    return 0;
}
