FROM ubuntu:24.10

LABEL description="Artifact for 'Wave Hello to Privacy: Efficient Mixed-Mode MPC from Wavelet Transforms'"

RUN apt update && apt install -y g++ g++-13 bsdutils cmake git iproute2 && apt clean
WORKDIR /
RUN git clone https://github.com/axboe/liburing.git
WORKDIR /liburing
RUN make
RUN make install

WORKDIR /
RUN git clone https://github.com/mpc-msri/EzPC.git
WORKDIR /EzPC/FSS
RUN git checkout f24bf3e
RUN cmake .
RUN make
RUN make install

WORKDIR /
RUN git clone --recurse-submodules -j8 https://github.com/DigitalLibertiesLab/libdpf.git
WORKDIR /libdpf/benchmarks

WORKDIR /