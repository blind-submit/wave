.PHONY: all
all: dealers clients

.PHONY: dealers
dealers: bin/dealer-Haar bin/dealer-Haar-file bin/dealer-bior bin/dealer-bior-file

.PHONY: clients
clients: clients-Haar clients-bior

.PHONY: clients-bior client0-bior client1-bior
clients-bior: client0-bior client1-bior
client0-bior: bin/client0-bior-preprocess bin/client0-bior-online
client1-bior: bin/client1-bior-preprocess bin/client1-bior-online

.PHONY: clients-Haar client0-Haar client1-Haar
clients-Haar: client0-Haar client1-Haar
client0-Haar: bin/client0-Haar-preprocess bin/client0-Haar-online
client1-Haar: bin/client1-Haar-preprocess bin/client1-Haar-online

nillion: nillion.cpp
	g++ -g -std=c++17 -march=native -O3 -o nillion nillion.cpp ../thirdparty/EzPC/FSS/lib/libfss.a  -Iinclude -I../include -I../thirdparty -lbsd -DLIBDPF_HAS_ASIO -I../thirdparty/asio/asio/include -luring -pthread -Wno-ignored-attributes -I../thirdparty/cryptoTools

bin/dealer-Haar-file: dealer-Haar-file.cpp include/Haar.hpp
	g++ -g -std=c++17 -march=native -O3 -o bin/dealer-Haar-file dealer-Haar-file.cpp ../thirdparty/EzPC/FSS/lib/libfss.a  -Iinclude -I../include -I../thirdparty -lbsd -DLIBDPF_HAS_ASIO -I../thirdparty/asio/asio/include -luring -pthread -Wno-ignored-attributes -I../thirdparty/cryptoTools

bin/dealer-bior-file: dealer-bior-file.cpp include/bior.hpp
	g++ -g -std=c++17 -march=native -O3 -o bin/dealer-bior-file dealer-bior-file.cpp ../thirdparty/EzPC/FSS/lib/libfss.a  -Iinclude -I../include -I../thirdparty -lbsd -DLIBDPF_HAS_ASIO -I../thirdparty/asio/asio/include -luring -pthread -Wno-ignored-attributes -I../thirdparty/cryptoTools

bin/client0-Haar-online: client0-Haar-online.cpp include/Haar.hpp
	g++ -g -std=c++17 -march=native -O3 -o bin/client0-Haar-online client0-Haar-online.cpp ../thirdparty/EzPC/FSS/lib/libfss.a  -Iinclude -I../include -I../thirdparty -lbsd -DLIBDPF_HAS_ASIO -I../thirdparty/asio/asio/include -luring -pthread -Wno-ignored-attributes -I../thirdparty/cryptoTools

bin/client0-bior-online: client0-bior-online.cpp include/bior.hpp
	g++ -g -std=c++17 -march=native -O3 -o bin/client0-bior-online client0-bior-online.cpp ../thirdparty/EzPC/FSS/lib/libfss.a  -Iinclude -I../include -I../thirdparty -lbsd -DLIBDPF_HAS_ASIO -I../thirdparty/asio/asio/include -luring -pthread -Wno-ignored-attributes -I../thirdparty/cryptoTools

bin/client0-Haar-preprocess: client0-Haar-preprocess.cpp include/Haar.hpp
	g++ -g -std=c++17 -march=native -O3 -o bin/client0-Haar-preprocess client0-Haar-preprocess.cpp ../thirdparty/EzPC/FSS/lib/libfss.a  -Iinclude -I../include -I../thirdparty -lbsd -DLIBDPF_HAS_ASIO -I../thirdparty/asio/asio/include -luring -pthread -Wno-ignored-attributes -I../thirdparty/cryptoTools

bin/client0-bior-preprocess: client0-bior-preprocess.cpp include/bior.hpp
	g++ -g -std=c++17 -march=native -O3 -o bin/client0-bior-preprocess client0-bior-preprocess.cpp ../thirdparty/EzPC/FSS/lib/libfss.a  -Iinclude -I../include -I../thirdparty -lbsd -DLIBDPF_HAS_ASIO -I../thirdparty/asio/asio/include -luring -pthread -Wno-ignored-attributes -I../thirdparty/cryptoTools

bin/client1-Haar-online: client1-Haar-online.cpp include/Haar.hpp
	g++ -g -std=c++17 -march=native -O3 -o bin/client1-Haar-online client1-Haar-online.cpp ../thirdparty/EzPC/FSS/lib/libfss.a  -Iinclude -I../include -I../thirdparty -lbsd -DLIBDPF_HAS_ASIO -I../thirdparty/asio/asio/include -luring -pthread -Wno-ignored-attributes -I../thirdparty/cryptoTools

bin/client1-bior-online: client1-bior-online.cpp include/bior.hpp
	g++ -g -std=c++17 -march=native -O3 -o bin/client1-bior-online client1-bior-online.cpp ../thirdparty/EzPC/FSS/lib/libfss.a  -Iinclude -I../include -I../thirdparty -lbsd -DLIBDPF_HAS_ASIO -I../thirdparty/asio/asio/include -luring -pthread -Wno-ignored-attributes -I../thirdparty/cryptoTools

bin/client1-Haar-preprocess: client1-Haar-preprocess.cpp
	g++ -g -std=c++17 -march=native -O3 -o bin/client1-Haar-preprocess client1-Haar-preprocess.cpp ../thirdparty/EzPC/FSS/lib/libfss.a  -Iinclude -I../include -I../thirdparty -lbsd -DLIBDPF_HAS_ASIO -I../thirdparty/asio/asio/include -luring -pthread -Wno-ignored-attributes -I../thirdparty/cryptoTools

bin/client1-bior-preprocess: client1-bior-preprocess.cpp
	g++ -g -std=c++17 -march=native -O3 -o bin/client1-bior-preprocess client1-bior-preprocess.cpp ../thirdparty/EzPC/FSS/lib/libfss.a  -Iinclude -I../include -I../thirdparty -lbsd -DLIBDPF_HAS_ASIO -I../thirdparty/asio/asio/include -luring -pthread -Wno-ignored-attributes -I../thirdparty/cryptoTools

clean:
	rm -rf bin/nillion