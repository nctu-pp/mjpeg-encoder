#include <iostream>
#include "model/Color.h"

using namespace std;
using namespace model::color;

int main(int argc, char *argv[]) {
    // TODO: parse argv
    // -i xxx.raw -s 854x480 -r 20 -o yyy.avi -t threadNumber

    RGBA red;
    red.color.r = 0xff;
    red.color.g = 1;
    red.color.b = 2;
    cout << "Hello, World!" << endl;
    return 0;
}
