#include <sys/mman.h>
#include <chrono>

#define ASIO_HAS_IO_URING 1
#include "dpf.hpp"
#include "grotto.hpp"

#include "params.hpp"
#include "Haar.hpp"

static const uint16_t peer_port = 31338;

using asio::ip::tcp;

int main(int argc, char * argv[])
{
    int fd = open("lut.dat", O_RDONLY);
    scaled_lut = reinterpret_cast<output_type *>(mmap(nullptr, interval_bytes, PROT_READ, MAP_PRIVATE|MAP_NORESERVE, fd, off_t(0)));

    asio::io_context io_context{1};

    asio::thread_pool workers{num_threads};
    auto work_executor = workers.get_executor();

    try
    {
        asio::stream_file dealer{io_context, "Haar.0", asio::stream_file::read_only};
        tcp::acceptor acceptor{io_context,
            tcp::endpoint(tcp::v4(), peer_port)};

        tcp::socket peer{io_context};
        acceptor.accept(peer);
        peer.set_option(tcp::no_delay(true));
        // std::cout << "Received connection from peer\n";

        auto ret = async_online_Haar<L>(dealer, peer, work_executor, 100, count, asio::use_future);

        auto before = std::chrono::high_resolution_clock::now();
        io_context.run();
        auto after = std::chrono::high_resolution_clock::now();

        std::chrono::duration<double, std::milli> elapsed = after - before;
        auto [dealer_read, peer_read, wrote] = ret.get();

        // std::cout << "Read " << peer_read << " bytes and wrote " << wrote << " bytes in " << elapsed.count() << " ms\n";
		// std::cout << "haar_bytes_read," << peer_read << std::endl;
		// std::cout << "haar_bytes_wrote," << wrote << std::endl;
		std::cout << "haar_time_ms," << elapsed.count() << std::endl;
    }
    catch (const std::exception & e)
    {
        std::cout << e.what() << "\n";
        exit(EXIT_FAILURE);
    }

    return EXIT_SUCCESS;
}