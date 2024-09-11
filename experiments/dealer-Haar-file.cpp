#include <chrono>

#define ASIO_HAS_IO_URING 1
#include "dpf.hpp"
#include "grotto.hpp"

#include "params.hpp"
#include "Haar.hpp"

using asio::ip::tcp;

int main(int argc, char * argv[])
{
    asio::io_context io_context{1};

    asio::thread_pool workers{num_threads};
    auto work_executor = workers.get_executor();

    try
    {
        asio::stream_file client0{io_context, "Haar.0", asio::stream_file::write_only |
                                        asio::stream_file::create     |
                                        asio::stream_file::truncate};
        asio::stream_file client1{io_context, "Haar.1", asio::stream_file::write_only |
                                        asio::stream_file::create     |
                                        asio::stream_file::truncate};

        auto ret = async_make_preprocess_Haar<L>(client0, client1, work_executor, count, asio::use_future);

        auto before = std::chrono::high_resolution_clock::now();
        io_context.run();
        auto after = std::chrono::high_resolution_clock::now();

        std::chrono::duration<double, std::milli> elapsed = after - before;
        auto [bytes0, bytes1] = ret.get();

        // std::cout << "Wrote " << bytes0 << " (" << bytes1 << ") bytes in " << elapsed.count() << " ms\n";
	std::cout << "haar_dealer_bytes," << bytes0 << std::endl;
	std::cout << "haar_dealer_bytes1," << bytes1 << std::endl;
	std::cout << "haar_dealer_time_ms," << elapsed.count() << std::endl;
    }
    catch (const std::exception & e)
    {
        std::cout << e.what() << "\n";
        exit(EXIT_FAILURE);
    }

    return EXIT_SUCCESS;
}