#include <sys/mman.h>
#include <cstdarg>
#include <chrono>

#include "CLI11.hpp"
    #include "LeapFormatter.hpp"

#define ASIO_HAS_IO_URING 1
#include "dpf.hpp"
#include "grotto.hpp"
using asio::ip::tcp;

static constexpr std::size_t L = 64;
static constexpr std::size_t n = 32;
static constexpr std::size_t J = 22, j=n-J;
static constexpr auto twoj = (1ul << j);
static constexpr auto twoJ = (1ul << J);
int32_t bitlength = L;

static constexpr std::size_t num_threads = 1;

using input_type = dpf::modint<L>;
// using share_type = grotto::fixedpoint<fracbits, integral_type>;
using output_type = dpf::modint<L>;
std::size_t interval_bytes = twoJ * sizeof(output_type);

output_type * scaled_lut;

#include "Haar.hpp"
#include "bior.hpp"

static constexpr auto program_friendly_name = "Wave Hello to Privacy artifact";
static constexpr auto program_version       = "1.0";
static constexpr auto program_authors       = "Copyright (C) 2024 Ryan Henry and others";
static constexpr auto license_text =
      "License GPLv3+: GNU GPL version 3 or later <https://gnu.org/licenses/gpl.html>.\n"
      "This is free software: you are free to change and redistribute it.\n"
      "There is NO WARRANTY, to the extent permitted by law.";

static constexpr const char * default_dealer_port = "31337";
static constexpr const char * default_peer_port   = "31338";

tcp::no_delay disable_nagle{false};  ///< disable Nagle's algorithm
bool do_quickack{false};  ///< enable TCP_QUICKACK
bool print_stats{true};  ///< emit costs metrics

// struct InputBitsBase
// {};

// template <std::size_t N>
// struct InputBits : public InputBitsBase
// {};

// using InputBitsCreator = std::function<std::unique_ptr<InputBitsBase>()>;

// template<std::size_t... Ns>
// void print_sequence(std::index_sequence<Ns...> int_seq)
// {
//     std::map<std::size_t, InputBitsCreator> map;
//     ((map.emplace(Ns, []() { return std::make_unique<InputBits<Ns>>(); }), ...));
//     std::cout << '\n';
// }

// template <std::size_t... Ns>
// struct InputBitsCreatorInserter
// {
//     static void insert(std::map<std::size_t, InputBitsCreator>& creatorMap,
//         std::index_sequence<Ns...>)
//     {
//         ((creatorMap.emplace(Ns, []() { return std::make_unique<InputBits<Ns>>(); }), ...));
//     }
// };

// std::map<std::size_t, InputBitsCreator> initializeInputBitsCreatorMap()
// {
//     std::map<std::size_t, InputBitsCreator> creators;
//     InputBitsCreatorInserter<>::insert(creators, std::make_index_sequence<256>{});
//     return creators;
// }

// std::unique_ptr<InputBitsBase> createModint(std::size_t const n)
// {
//     static std::map<std::size_t, InputBitsCreator> const creatorMap = initializeInputBitsCreatorMap();
//     auto const it = creatorMap.find(n);
//     if (it != creatorMap.end())
//     {
//         return it->second();
//     }
//     return nullptr;
// }

/// check for a positive-valued `uint8_t` (i.e., `val>0`)
template <uint8_t Max = std::numeric_limits<uint8_t>::max()>
const CLI::Range PositiveUint8((uint64_t)1, (uint64_t)Max, "POSITIVE");

/// check for a positive-valued `uint16_t` (i.e., `val>0`)
template <uint16_t Max = std::numeric_limits<uint16_t>::max()>
const CLI::Range PositiveUint16(uint16_t{1}, Max, "POSITIVE");

enum class Verbosity : int { silent = 0, quiet, lo, mid, hi };
static Verbosity verbosity = Verbosity::quiet;  ///< verbosity level
const std::map<std::string, Verbosity> verbosity_map{
    {"silent", Verbosity::silent},
    {"quiet",  Verbosity::quiet },
    {"lo",     Verbosity::lo    },
    {"mid",    Verbosity::mid   },
    {"hi",     Verbosity::hi    }};

constexpr uint16_t stoi_impl(const char * string_literal, uint16_t value = 0)
{
    auto digit = *string_literal;
    if (digit != '\0')
    {
        dpf::utils::constexpr_maybe_throw<std::domain_error>('0' >= digit || digit >= '9',
            string_literal );
        return stoi_impl(string_literal + 1, (digit - '0') + value * 10);
    }
    return value;
}

constexpr uint16_t stoi(const char * string_literal)
{
    return stoi_impl(string_literal);
}

void maybe_printf(Verbosity message_verbosity,
    const char * format, ...)
{
    va_list argptr;
    va_start(argptr, format);

    if (message_verbosity <= verbosity) vprintf(format, argptr);

    va_end(argptr);
}

auto print_version = [](bool)
{
    printf("%s version %s\n%s\n%s\n", program_friendly_name,
        program_version, program_authors, license_text);
    exit(EXIT_SUCCESS);
};

struct parameter_set
{
    enum transform_t { Haar, bior } transform;
    // enum method_t { Pika, Grotto} method;
    std::size_t input_bits = 64;                 ///< input bitlength (`N` in `dpf::modint<N>`)
    std::size_t fractional_bits = 16;            ///< fractional precision (`F` in `grotto::fixedpoint<F, dpf::modint<N>>`)
    std::size_t signal_bits = 32;                ///< pre-DWT quantization granularity
    std::size_t level = 1;                       ///< DWT multi-resolution analysis level
};

int main(int argc, char * argv[])
{
    fss_init();

    std::string config_file;                     ///< path to SAML config file
    std::size_t count                  = 1;      ///< number of DWT evaluations
    std::size_t num_threads            = 1;      ///< number of worker threads
        //= asio::detail::default_thread_pool_size();
    // functionality
    parameter_set params               = {};
    std::string lut_file               = "lut";  ///< binary file containing the LUT
    // preprocess_dealer
    // preprocess_dealer_file
    bool use_file                      = false;  ///< output to files (!sockets)
    bool overwrite_files               = false;  ///< clobber existing files
    std::string outfile0;                        ///< file for peer0's output
    std::string outfile1;                        ///< file for peer1's output
    // preprocess_dealer_socket
    bool use_socket                    = true;   ///< stream to sockets (!files)
    std::string dealer_local_address   = "0.0.0.0";  ///< dealer iface to listen on
    uint16_t dealer_local_port         = stoi(default_dealer_port);  ///< dealer port to listen on
    // preprocess_client
    std::string outfile;                         ///< file for client's output
    std::string dealer_remote_address;           ///< address to connect to
    std::string dealer_remote_port     = default_dealer_port;  ///< port to connect on
    // online_client
    std::string infile;                          ///< file containing dealer values
    // online_client_listener
    uint16_t peer_local_port = 31338;            ///< port to listen for peer on
    std::string peer_local_address     = "0.0.0.0";  ///< iface to listen for peer on
    // online_client_connecter
    std::string peer_remote_port       = default_peer_port;  ///< port to connect to peer on
    std::string peer_remote_address;             ///< address to connect to peer on

    CLI::App args(program_friendly_name, program_invocation_short_name);
    args.fallthrough(false);
    args.get_formatter()
        ->label("REQUIRED", "(REQ'D)");
    args.formatter(std::make_unique<CLI::LeapFormatter>());

    asio::io_context io_context(1);  // enable optimizations that make sense
                                     // only if *just one thread* invokes run
    // N.B.: the "--threads" controls how many threads will be allocated to
    //       a thread_pool; the io_context will indeed always have a single
    //       thread regardless of the number of threads

    // first we set up universal options; that is, options that transcend any
    // specific choice of subcommand

    // ./foo [UNIVERSAL_OPTIONS] subcommand .

    // help
    args.set_help_all_flag("--help-all", "Print help for all subcommands and exit");

    // verbose mode
    auto verbose = args.add_flag("--verbosity,-v", verbosity,
        "Verbosity level; each -v increments level to a maximum of hi{4}")
            ->capture_default_str()
            ->always_capture_default()
            ->transform(CLI::Transformer(verbosity_map, CLI::ignore_case))
            ->multi_option_policy(CLI::MultiOptionPolicy::Sum)
            ->check(CLI::Range(0,4))
            ->option_text("VALUE IN {silent,quiet,lo,mid,hi} OR {0,1,2,3,4}");

    // silent mode
    args.add_flag("--silent",
        [](int){ verbosity = Verbosity::silent; }, "Silent mode (equivalent to --verbose=silent)")
            ->excludes(verbose);

    auto basic_config_group = args.add_option_group("Basic (uninheritable) configuration");

    // version information
    basic_config_group->add_flag("--version", print_version,
        "Print version information and exit")
            ->group("Basic configuration");

    // use specified TOML configuration file in lieu of/in addition to args
    basic_config_group->set_config("--config", config_file,
        "Try to read configuration from the given TOML file")
            ->configurable(false)
            ->option_text("TEXT:FILENAME")
            ->group("Basic configuration");

    // print effective configuration in (TOML format) to `stdout`
    auto export_flag = basic_config_group->add_flag("--export-config",
        "Print effective configuration (in TOML format) to standard output and terminate")
            ->configurable(false)
            ->group("Basic configuration");

    // flag to indicate whether or not to output statistical data
    basic_config_group->add_flag("--stats,!--no-stats", print_stats,
        "Output data for statistics")
            ->capture_default_str()
            ->group("Basic configuration");

    // number of worker threads for doing any cryptographic heavy lifting
    auto t = basic_config_group->add_option("--threads,-t", num_threads,
        "Number of compute threads to use")
            ->capture_default_str()
            ->check(CLI::TypeValidator<uint16_t>())
            ->check(CLI::Range(1, UINT16_MAX))
            ->group("Basic configuration");

    auto functionality_group = args.add_option_group("Functionality");

    // specify the repetition count
    functionality_group->add_option("--count,-c", count,
        "Number of evaluations to perform")
            ->capture_default_str()
            ->group("Functionality");

    // input bitlength (`N` in `dpf::modint<N>`)
    functionality_group->add_option("--input-bits,--ell,-l", params.input_bits,
        "Bitlength of fixed-point inputs and outputs")
            ->capture_default_str()
            ->check(CLI::TypeValidator<uint8_t>())
            ->check(CLI::Range(8, UINT8_MAX))
            ->group("Functionality");

    // fractional precision (`F` in `grotto::fixedpoint<F, dpf::modint<N>>`)
    functionality_group->add_option("--fractional-bits,-f", params.fractional_bits,
        "Fractional bits in inputs and outputs")
            ->capture_default_str()
            ->check(CLI::TypeValidator<uint8_t>())
            ->group("Functionality");

    // DWT multi-resolution analysis level
    functionality_group->add_option("--dwt-level,-J", params.level,
        "Level of DWT approximation to use")
            ->capture_default_str()
            ->check(CLI::TypeValidator<uint8_t>())
            ->group("Functionality");

    // transform to use
    functionality_group->add_flag("--Haar,-H,!--bior,!-B",
        [&params](bool b){ params.transform = (parameter_set::transform_t)b; },
        "Use the Haar (!bior) transform")
            ->capture_default_str()
            ->group("Functionality");

    // method to use
    // functionality_group->add_flag("--pika,!--grotto",
    //     [&params](bool b){ params.method = (parameter_set::method_t)b; },
    //     "Use the Pika-like (!Grotto-like) protocol")
    //         ->capture_default_str()
    //         ->group("Functionality");

    // lookup table filename
    functionality_group->add_option("--lut-file,-L", lut_file,
        "Read LUT from specified file")
            ->capture_default_str()
            ->check(CLI::ExistingFile)
            ->group("Functionality");

    functionality_group->parse_complete_callback([params]()
    {
        if (params.fractional_bits > params.input_bits)
        {
            std::stringstream ss;
            ss << "fractional_bits exceeds input_bits: " << params.fractional_bits << ">" << params.input_bits;
            throw std::runtime_error(ss.str());
        }
        if (params.signal_bits > params.input_bits)
        {
            std::stringstream ss;
            ss << "signal_bits exceeds input_bits: " << params.fractional_bits << ">" << params.input_bits;
            throw std::runtime_error(ss.str());
        }
        if (!params.level || params.level > params.signal_bits)
        {
            std::stringstream ss;
            ss << "invalid level: " << params.level;
            throw std::runtime_error(ss.str());
        }
    });

    // Set up the subcommands (incl. associated subsubcommands and options)
    args.require_subcommand(1);

    // --------------------------------
    // Subcommand: ./foo preprocess
    // Only run dealer's data-independent offline steps
    // --------------------------------
    auto * preprocess = args.add_subcommand("preprocess",
        "Run offline (preprocessing) phase with or without clients");
    preprocess->configurable(true);  // allow in a configuration file
    preprocess->fallthrough(true);

    // preprocess requires a subsubcommand to indicate role (client or server)
    preprocess->require_subcommand(1);

    // --------------------------------------------
    // Sub-subcommand: ./foo preprocess dealer
    // --------------------------------------------

    //
    // ./foo [UNIVERSAL_OPTIONS] preprocess dealer [OPTIONS]
    //
    auto * preprocess_dealer = preprocess->add_subcommand("dealer",
        "Run preprocessing phase as the dealer");
    preprocess_dealer->configurable(true);  // allow in a configuration file
    preprocess_dealer->fallthrough(true);  // unmatched options forwarded to preprocess

    // dirty hack to require either of two option groups -- but never both
    // note that section option group must be mutually exclusie of first
    auto * preprocess_dealer_ = preprocess_dealer->add_subcommand();
    preprocess_dealer_->require_option();

    //
    // ./foo [UNIVERSAL_OPTIONS] preprocess dealer --use-files [OPTIONS] filename0 filename1
    //
    auto * preprocess_dealer_file
        = preprocess_dealer_->add_option_group("Write to a file");

    // flag to indicate usage of the file option_group
    // note: flag is mandatory *within the option_group* only
    preprocess_dealer_file->add_flag("--use-files", use_file,
        "Output preprocess values to files")
            ->required(true);

    // flag to indicate whether or not existing files should be clobbered
    preprocess_dealer_file->add_flag("--overwrite,-o,!--no-overwrite", overwrite_files,
        "Overwrite existing output files")
            ->capture_default_str();

    // [positional] first output file
    preprocess_dealer_file->add_option("outfile0", outfile0,
        "File to hold preprocessing values for peer 0")
            ->required(true)
            ->option_text("TEXT:FILENAME (REQ'D)");

    // [positional] second output file
    preprocess_dealer_file->add_option("outfile1", outfile1,
        "File to hold preprocessing values for peer 1")
            ->required(true)
            ->option_text("TEXT:FILENAME (REQ'D)");

    // post-processing sanity checks for "--use-files" options
    preprocess_dealer_file->parse_complete_callback([&overwrite_files, &outfile0, &outfile1]()
    {
        if (!overwrite_files && std::filesystem::exists(outfile0))
        {
            std::stringstream ss;
            ss << "outfile0: File already exists: " << outfile0;
            throw std::runtime_error(ss.str());
        }
        if (!overwrite_files && std::filesystem::exists(outfile1))
        {
            std::stringstream ss;
            ss << "outfile1: File already exists: " << outfile1;
            throw std::runtime_error(ss.str());
        }
    });

    //
    // ./foo [UNIVERSAL_OPTIONS] preprocess dealer --use-sockets [OPTIONS]
    //
    auto * preprocess_dealer_socket
        = preprocess_dealer_->add_option_group("Stream over TCP sockets")
            ->excludes(preprocess_dealer_file);

    // flag to indicate usage of the file option_group
    preprocess_dealer_socket->add_flag("--use-sockets", use_socket,
        "Stream preprocess values to clients over sockets")
            ->capture_default_str();

    // which IPv4 address to bind to
    preprocess_dealer_socket->add_option("--address,-a", dealer_local_address,
        "IPv4 address to bind to")
            ->capture_default_str()
            ->check(CLI::ValidIPV4);

    // which TCP port to listen on
    preprocess_dealer_socket->add_option("--port,-p", dealer_local_port,
        "TCP port number to listen for client on")
            ->capture_default_str()
            ->option_text("UINT16");

    // disable Nagle's algorithm in the TCP connection
    preprocess_dealer_socket->add_flag("--disable-nagle,-N", disable_nagle,
        "Disable Nagle's algorithm in TCP")
            ->capture_default_str();

    // enable QUICKACK in the TCP connection
    preprocess_dealer_socket->add_flag("--enable-quickack,-Q", do_quickack,
        "Enable TCP QUICKACK")
            ->capture_default_str();

    // --------------------------------------------
    // Sub-subcommand: ./foo preprocess client
    // --------------------------------------------

    //
    // ./foo [UNIVERSAL_OPTIONS] preprocess client [OPTIONS]
    //
    auto * preprocess_client = preprocess->add_subcommand("client",
        "Run preprocessing phase as a client");
    preprocess_client->configurable(true);  // allow in a configuration file
    preprocess_client->fallthrough(true);  // unmatched options forwarded to preprocess

    // which IPv4 address to connect to dealer at
    preprocess_client->add_option("address,--address,-a", dealer_remote_address,
        "Dealer's IPv4 address")
            ->required()
            ->check(CLI::ValidIPV4)
            ->group("Network options");

    // which TCP port to connect to dealer via
    preprocess_client->add_option("--port,-p", dealer_remote_port,
        "Dealer's TCP port")
            ->capture_default_str()
            ->check(CLI::TypeValidator<uint16_t>())
            ->option_text("TEXT:UINT16")
            ->group("Network options");

    // disable Nagle's algorithm in the TCP connection
    preprocess_client->add_flag("--disable-nagle,-N", disable_nagle,
        "Disable Nagle's algorithm in TCP")
            ->capture_default_str()
            ->group("Network options");

    // enable QUICKACK in the TCP connection
    preprocess_client->add_flag("--enable-quickack,-Q", do_quickack,
        "Enable TCP QUICKACK")
            ->capture_default_str()
            ->group("Network options");

    // flag to indicate whether or not existing files should be clobbered
    preprocess_client->add_flag("--overwrite,-o,!--no-overwrite", overwrite_files,
        "Overwrite existing output files")
            ->capture_default_str()
            ->group("File options");

    // file to write dealer values to
    preprocess_client->add_option("filename", outfile,
        "Write to a file")
            ->required()
            ->option_text("TEXT:FILENAME (REQ'D)")
            ->group("File options");

    // --------------------------------
    // Subcommand: ./foo online
    // Only perform the data-dependent online steps; dealer values from file
    // --------------------------------
    auto * online = args.add_subcommand("online",
        "Run online phase without dealer, using preprocessed values");
    online->configurable(true);  // allow in a configuration file
    online->fallthrough(true);
    online->require_subcommand(1);

    auto online_file = online->add_option_group("File to read from");
    online_file->require_option(1);

    // read dealer values from a file
    online_file->add_option("filename", infile,
        "Filename")
            ->capture_default_str()
            ->check(CLI::ExistingFile)
            ->option_text("TEXT:FILENAME (REQ'D)")
            ->group("File options");

    // disable Nagle's algorithm in the TCP connection
    online->add_flag("--disable-nagle,-N", disable_nagle,
        "Disable Nagle's algorithm in TCP")
            ->capture_default_str()
            ->group("Network options");

    // enable QUICKACK in the TCP connection
    online->add_flag("--enable-quickack,-Q", do_quickack,
        "Enable TCP QUICKACK")
            ->capture_default_str()
            ->group("Network options");

    //
    // ./foo online listen
    //
    auto * online_listener = online->add_subcommand("listen",
        "Listen for peer");
    online_listener->configurable(true);  // allow in a configuration file
    online_listener->fallthrough(true);  // unmatched options forwarded to online

    // which IPv4 address to bind to
    online_listener->add_option("address", peer_local_address,
        "IPv4 address to bind to")
            ->capture_default_str()
            ->check(CLI::ValidIPV4)
            ->group("Network options");

    // which TCP port to listen on
    online_listener->add_option("--port,-p", peer_local_port,
        "TCP port number to listen for peer on")
            ->option_text("UINT16")
            ->capture_default_str()
            ->check(PositiveUint16<>)
            ->group("Network options");

    //
    // ./foo online connect
    //
    auto * online_connecter = online->add_subcommand("connect",
        "Connect to peer");
    online_connecter->configurable(true);  // allow in a configuration file
    online_connecter->fallthrough(true);  // unmatched options forwarded to online

    // which IPv4 address to connect to peer at
    online_connecter->add_option("address,--address,-a", peer_remote_address,
        "Peer's IPv4 address")
            ->required()->check(CLI::ValidIPV4);

    // which TCP port to connect to peer via
    online_connecter->add_option("port,--port,-p", peer_remote_port,
        "Peer's TCP port")
            ->capture_default_str()
            ->option_text("TEXT:UINT16")
            ->group("Network options");

    // --------------------------------
    // Subcommand: ./foo full
    // Run the entire protocol with an online dealer and two peers/clients
    // --------------------------------

    auto * full = args.add_subcommand("full",
        "Run all with dealer and two peers/clients (both phases online)");
    full->configurable(true);  // allow in a configuration file
    full->fallthrough(true);
    full->require_subcommand(1);


    // disable Nagle's algorithm in the TCP connection
    full->add_flag("--disable-nagle,-N", disable_nagle,
        "Disable Nagle's algorithm in TCP")
            ->capture_default_str();

    // enable QUICKACK in the TCP connection
    full->add_flag("--enable-quickack,-Q", do_quickack,
        "Enable TCP QUICKACK")
            ->capture_default_str();


    //
    // ./foo full dealer
    //
    auto full_dealer = full->add_subcommand("dealer",
        "Run full protocol as the dealer");
    full_dealer->configurable(true);  // allow in a configuration file
    full_dealer->fallthrough(true);    // unmatched options forwarded to full

    // which IPv4 address to bind to
    full_dealer->add_option("address,--address,-a", peer_local_address,
        "IPv4 address to bind to")
            ->capture_default_str()
            ->check(CLI::ValidIPV4);

    // which TCP port to listen on
    full_dealer->add_option("--port,-p", dealer_local_port,
        "TCP port number to listen for client on")
            ->capture_default_str()
            ->option_text("UINT16");

    //
    // ./foo full peer
    //
    auto full_peer = full->add_subcommand("peer",
        "Run full protocol as a peer");
    full_peer->configurable(true);  // allow in a configuration file
    full_peer->require_subcommand(1);
    full_peer->fallthrough(true);    // unmatched options forwarded to full


    //
    // ./foo full peer listen
    //
    auto full_peer_listener = full_peer->add_subcommand("listen",
        "Listen for peer");
    full_peer_listener->configurable(true);  // allow in a configuration file
    full_peer_listener->fallthrough(true);  // unmatched options forwarded to full_peer

    // which IPv4 address to bind to
    full_peer_listener->add_option("address,--address,-a", peer_local_address,
        "IPv4 address to bind to")
            ->capture_default_str()
            ->check(CLI::ValidIPV4);

    // which TCP port to listen on
    full_peer_listener->add_option("port,--port,-p", peer_local_port,
        "TCP port number to listen for peer on")
            ->capture_default_str()
            ->option_text("UINT16");


    //
    // ./foo full peer connect
    //
    auto full_peer_connecter = full_peer->add_subcommand("connect",
        "Connect to peer");
    full_peer_connecter->configurable(true);  // allow in a configuration file
    full_peer_connecter->fallthrough(true);  // unmatched options forwarded to full_peer

    // which IPv4 address to connect to peer at
    full_peer_connecter->add_option("address,--address,-a", peer_remote_address,
        "Peer's IPv4 address")
            ->required()
            ->check(CLI::ValidIPV4);

    // which TCP port to connect to peer via
    full_peer_connecter->add_option("port,--port,-p", peer_remote_port,
        "Peer's TCP port")
            ->capture_default_str()
            ->option_text("TEXT:UINT16");

    // --------------------------------
    // Register callback functions
    // --------------------------------


    args.parse_complete_callback([&]()
    {
        if (export_flag->count() > 0)
        {
            std::stringstream config;
            config << "# ";
            for (int i = 0; i < argc; ++i)
            {
                if (strncmp(argv[i], "--export-config", 15))
                {
                    config << argv[i] << (i == argc-1 ? "\n\n" : " ");
                }
            }
            config << args.config_to_str(false, false);
            std::cout << config.str() << "\n";
            exit(0);
        }
    });

    try
    {
        args.parse(argc, argv);

        asio::thread_pool worker_pool(num_threads);
        auto work_executor = worker_pool.executor();

        if (preprocess_dealer_file->count()>0)
        {
            asio::stream_file client0{io_context};
            asio::stream_file client1{io_context};

            client0.open(outfile0, asio::stream_file::write_only |
                                    asio::stream_file::create     |
                                    asio::stream_file::truncate);
            client1.open(outfile0, asio::stream_file::write_only |
                                    asio::stream_file::create     |
                                    asio::stream_file::truncate);

            auto ret = (params.transform == parameter_set::Haar)
                     ? async_make_preprocess_Haar<L>(client0, client1, work_executor, count, asio::use_future)
                     : async_make_preprocess_bior<L,j,n>(client0, client1, work_executor, count, asio::use_future);;

            auto before = std::chrono::high_resolution_clock::now();
            io_context.run();
            auto after = std::chrono::high_resolution_clock::now();

            std::chrono::duration<double, std::milli> elapsed = after - before;
            auto [bytes0, bytes1] = ret.get();

            std::cout << "Wrote " << bytes0 << " bytes to " << outfile0 << " and " << bytes1 << " bytes to " << outfile1 << " in " << elapsed.count() << " ms (count = " << count << ")\n";

        }
        else if (preprocess_dealer_socket->count()>0)
        {
            tcp::socket client0{io_context};
            tcp::socket client1{io_context};
            tcp::acceptor acceptor{io_context, tcp::endpoint(tcp::v4(), dealer_local_port)};
            acceptor.accept(client0);
            client0.set_option(disable_nagle);
            std::cout << "Received connection from client0\n";
            acceptor.accept(client1);
            client1.set_option(disable_nagle);
            std::cout << "Received connection from client1\n";

            auto ret = (params.transform == parameter_set::Haar)
                     ? async_make_preprocess_Haar<L>(client0, client1, work_executor, count, asio::use_future)
                     : async_make_preprocess_bior<L,j,n>(client0, client1, work_executor, count, asio::use_future);;

            auto before = std::chrono::high_resolution_clock::now();
            io_context.run();
            auto after = std::chrono::high_resolution_clock::now();

            std::chrono::duration<double, std::milli> elapsed = after - before;
            auto [bytes0, bytes1] = ret.get();

            std::cout << "Wrote " << bytes0 << " bytes to client0 and " << bytes1 << " bytes to client1 in " << elapsed.count() << " ms (count = " << count << ")\n";
        }
        else if (preprocess_client->count()>0)
        {
            asio::stream_file output_file{io_context};
            tcp::resolver resolver{io_context};
            tcp::socket dealer{io_context};
            output_file.open(outfile, asio::stream_file::write_only |
                                      asio::stream_file::create     |
                                      asio::stream_file::truncate);
            auto dealer_endpoints = resolver.resolve(dealer_remote_address, dealer_remote_port);
            asio::connect(dealer, dealer_endpoints);
            std::cout << "Connected to dealer\n";

            auto ret = (params.transform == parameter_set::Haar)
                     ? async_read_preprocess_Haar<L>(dealer, output_file, work_executor, count, asio::use_future)
                     : async_read_preprocess_bior<L,j,n>(dealer, output_file, work_executor, count, asio::use_future);;

            auto before = std::chrono::high_resolution_clock::now();
            io_context.run();
            auto after = std::chrono::high_resolution_clock::now();

            std::chrono::duration<double, std::milli> elapsed = after - before;
            auto [bytes0, bytes1] = ret.get();

            std::cout << "Read " << bytes0 << " bytes from dealer and wrote " << bytes1 << " bytes to " << outfile << " in " << elapsed.count() << " ms (count = " << count << ")\n";
        }
        else if (online_connecter->count()>0)
        {
            tcp::resolver resolver{io_context};
            asio::stream_file dealer{io_context};
            tcp::socket peer{io_context};
            dealer.open(infile, asio::stream_file::read_only);
            auto peer_endpoints = resolver.resolve(peer_remote_address, peer_remote_port);

            asio::connect(peer, peer_endpoints);
            peer.set_option(disable_nagle);
            std::cout << "Connected to peer\n";

            auto ret = (params.transform == parameter_set::Haar)
                     ? async_online_Haar<L>(dealer, peer, work_executor, 100, count, asio::use_future)
                     : async_online_bior<L,j,n>(dealer, peer, work_executor, 1, 100, count, asio::use_future);

            auto before = std::chrono::high_resolution_clock::now();
            io_context.run();
            auto after = std::chrono::high_resolution_clock::now();

            std::chrono::duration<double, std::milli> elapsed = after - before;
            auto [file_read_bytes, peer_read_bytes, peer_write_bytes] = ret.get();

            std::cout << "Read " << file_read_bytes << " bytes from " << infile << " and " << peer_read_bytes << " bytes from peer, and wrote " << peer_write_bytes << " bytes from peer in " << elapsed.count() << " ms (count = " << count << ")\n";
        }
        else if (online_listener->count()>0)
        {
            asio::stream_file dealer{io_context};
            tcp::socket peer{io_context};
            dealer.open(infile, asio::stream_file::read_only);
            tcp::acceptor acceptor{io_context, tcp::endpoint(tcp::v4(), peer_local_port)};
            acceptor.accept(peer);
            peer.set_option(disable_nagle);
            std::cout << "Received connection from peer\n";

            auto ret = (params.transform == parameter_set::Haar)
                     ? async_online_Haar<L>(dealer, peer, work_executor, 100, count, asio::use_future)
                     : async_online_bior<L,j,n>(dealer, peer, work_executor, 0, 100, count, asio::use_future);

            auto before = std::chrono::high_resolution_clock::now();
            io_context.run();
            auto after = std::chrono::high_resolution_clock::now();

            std::chrono::duration<double, std::milli> elapsed = after - before;
            auto [file_read_bytes, peer_read_bytes, peer_write_bytes] = ret.get();

            std::cout << "Read " << file_read_bytes << " bytes from " << infile << " and " << peer_read_bytes << " bytes from peer, and wrote " << peer_write_bytes << " bytes from peer in " << elapsed.count() << " ms (count = " << count << ")\n";

        }
        else if (full_dealer->count()>0)
        {
            tcp::socket client0{io_context};
            tcp::socket client1{io_context};
            tcp::acceptor acceptor{io_context, tcp::endpoint(tcp::v4(), dealer_local_port)};
            acceptor.accept(client0);
            client0.set_option(disable_nagle);
            std::cout << "Received connection from client0\n";
            acceptor.accept(client1);
            client1.set_option(disable_nagle);
            std::cout << "Received connection from client1\n";

            auto ret = (params.transform == parameter_set::Haar)
                     ? async_make_preprocess_Haar<L>(client0, client1, work_executor, count, asio::use_future)
                     : async_make_preprocess_bior<L,j,n>(client0, client1, work_executor, count, asio::use_future);;

            auto before = std::chrono::high_resolution_clock::now();
            io_context.run();
            auto after = std::chrono::high_resolution_clock::now();

            std::chrono::duration<double, std::milli> elapsed = after - before;
            auto [bytes0, bytes1] = ret.get();

            std::cout << "Wrote " << bytes0 << " bytes to client0 and " << bytes1 << " bytes to client1 in " << elapsed.count() << " ms (count = " << count << ")\n";

        }
        else if (full_peer_listener->count()>0)
        {
            tcp::resolver resolver{io_context};
            tcp::socket dealer{io_context};
            tcp::socket peer{io_context};
            auto dealer_endpoints = resolver.resolve(dealer_remote_address, dealer_remote_port);
            asio::connect(dealer, dealer_endpoints);
            dealer.set_option(disable_nagle);
            std::cout << "Connected to dealer\n";
            tcp::acceptor acceptor{io_context, tcp::endpoint(tcp::v4(), peer_local_port)};
            acceptor.accept(peer);
            peer.set_option(disable_nagle);
            std::cout << "Received connection from peer\n";

            auto ret = (params.transform == parameter_set::Haar)
                     ? async_online_Haar<L>(dealer, peer, work_executor, 100, count, asio::use_future)
                     : async_online_bior<L,j,n>(dealer, peer, work_executor, 0, 100, count, asio::use_future);

            auto before = std::chrono::high_resolution_clock::now();
            io_context.run();
            auto after = std::chrono::high_resolution_clock::now();

            std::chrono::duration<double, std::milli> elapsed = after - before;
            auto [dealer_read_bytes, peer_read_bytes, peer_write_bytes] = ret.get();

            std::cout << "Read " << dealer_read_bytes << " bytes from dealer and " << peer_read_bytes << " bytes from peer, and wrote " << peer_write_bytes << " bytes from peer in " << elapsed.count() << " ms (count = " << count << ")\n";

        }
        else if (full_peer_connecter->count()>0)
        {
            tcp::resolver resolver{io_context};
            tcp::socket dealer{io_context};
            tcp::socket peer{io_context};
            auto dealer_endpoints = resolver.resolve(dealer_remote_address, dealer_remote_port);
            asio::connect(dealer, dealer_endpoints);
            dealer.set_option(disable_nagle);
            std::cout << "Connected to dealer\n";
            auto peer_endpoints = resolver.resolve(peer_remote_address, peer_remote_port);
            asio::connect(peer, peer_endpoints);
            peer.set_option(disable_nagle);
            std::cout << "Connected to peer\n";

            auto ret = (params.transform == parameter_set::Haar)
                     ? async_online_Haar<L>(dealer, peer, work_executor, 100, count, asio::use_future)
                     : async_online_bior<L,j,n>(dealer, peer, work_executor, 0, 100, count, asio::use_future);

            auto before = std::chrono::high_resolution_clock::now();
            io_context.run();
            auto after = std::chrono::high_resolution_clock::now();

            std::chrono::duration<double, std::milli> elapsed = after - before;
            auto [dealer_read_bytes, peer_read_bytes, peer_write_bytes] = ret.get();

            std::cout << "Read " << dealer_read_bytes << " bytes from dealer and " << peer_read_bytes << " bytes from peer, and wrote " << peer_write_bytes << " bytes from peer in " << elapsed.count() << " ms (count = " << count << ")\n";
        }
    }
    catch (const CLI::ParseError & e)
    {
        return args.exit(e);
    }
    catch (const std::exception & e)
    {
        maybe_printf(Verbosity::silent, "%s\n", e.what());
        exit(EXIT_FAILURE);
    }

    return EXIT_SUCCESS;
}