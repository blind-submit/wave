#include <chrono>

#define ASIO_HAS_IO_URING 1
#include "dpf.hpp"
#include "grotto.hpp"

#include "params.hpp"
#include "Haar.hpp"

static const std::string dealer_address = "127.0.0.1";
static const std::string dealer_port = "31337";

using asio::ip::tcp;

int main(int argc, char * argv[])
{
    asio::io_context io_context{1};

    asio::thread_pool workers{num_threads};
    auto work_executor = workers.get_executor();

    try
    {
        asio::stream_file outfile{io_context, "Haar.0",
            asio::stream_file::write_only |
            asio::stream_file::create     |
            asio::stream_file::truncate};

        asio::ip::tcp::resolver resolver{io_context};
        auto dealer_endpoints = resolver.resolve(dealer_address, dealer_port);

        tcp::socket dealer{io_context};
        asio::connect(dealer, dealer_endpoints);
        dealer.set_option(tcp::no_delay(true));
        std::cout << "Connected to peer\n";

        auto ret = async_read_preprocess_Haar<L>(dealer, outfile, work_executor, count, asio::use_future);

        auto before = std::chrono::high_resolution_clock::now();
        io_context.run();
        auto after = std::chrono::high_resolution_clock::now();

        std::chrono::duration<double, std::milli> elapsed = after - before;
        auto [read, wrote] = ret.get();

        std::cout << "Read " << read << " and wrote " << wrote << " bytes in " << elapsed.count() << " ms\n";
    }
    catch (const std::exception & e)
    {
        std::cout << e.what() << "\n";
        exit(EXIT_FAILURE);
    }

    return EXIT_SUCCESS;
}