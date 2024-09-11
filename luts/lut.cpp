// // C program to read particular bytes
// // from the existing file

#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <iostream>

int main(int argc, char * argv[])
{
    /// ..
    int fd = open("lut", O_RDONLY);
    int bytes = 2048;
    auto lut = reinterpret_cast<long long *>(mmap(nullptr, bytes, PROT_READ, MAP_PRIVATE|MAP_NORESERVE, fd, off_t(0)));
    /// use lut as an array

    for (int i = 0; i <= 16; i++) {
        std::cout << lut[i] << '\n';
    }

    munmap(lut, bytes);
    close(fd);
    return 0;
}
