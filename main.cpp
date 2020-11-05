#include <iostream>
#include <getopt.h>

#include "model/Size.h"

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
            << "\t-t --threads <number>\tUse number threads." << endl
            << "\t-h --help\tShow usage." << endl;
}

int main(int argc, char *argv[]) {

    string input;
    Size size;
    int fps = 0;
    string output;
    int numThreads = 0;

    if (argc == 1) {
        showHelp(argv[0]);
        return 0;
    }

    // parse argv options
    do {
        static struct option long_options[] = {
                {"input",   required_argument, nullptr, 'i'},
                {"size",    required_argument, nullptr, 's'},
                {"fps",     required_argument, nullptr, 'r'},
                {"output",  required_argument, nullptr, 'o'},
                {"threads", optional_argument, nullptr, 't'},
                {"help",    no_argument,       nullptr, 'h'},
        };
        char opt;
        while ((opt = (char) getopt_long(argc, argv, "i:s:r:o:t::h?", long_options, nullptr)) != EOF) {
            switch (opt) {
                case 'i':
                    input = string(optarg);
                    break;
                case 's':
                    sscanf_s(optarg, "%dx%d", &(size.width), &(size.height));
                    break;
                case 'r':
                    fps = stoi(optarg);
                    break;
                case 'o':
                    output = string(optarg);
                    break;
                case 't':
                    numThreads = stoi(optarg);
                    break;
                case '?':
                default:
                    showHelp(argv[0]);
                    return 1;
            }
        }
    } while (false);


    // -i xxx.raw -s 854x480 -r 20 -o yyy.avi -t threadNumber

    return 0;
}
