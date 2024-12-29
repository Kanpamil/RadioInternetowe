#include <iostream>
extern "C" {
#include <libavcodec/avcodec.h>
}

int main() {
    avcodec_register_all();
    std::cout << "FFmpeg is successfully linked!" << std::endl;
    return 0;
}
