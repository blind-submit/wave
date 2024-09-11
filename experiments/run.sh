#!/bin/bash

# print usage information and exit
usage()
{
    echo "Usage: ${0} [OPTIONS]"
    echo "Options:"
    echo "    -r <RATE>    - new rate"
    echo "    -d <DELAY>   - new delay"
    echo "    -t           - disable tc usage"
    echo "    -n <BITSIZE> - bit size"
    echo "    -J <LUTSIZE> - lut size"
    echo "    -p           - turn off compilation and preprocessing"
    echo "    -f <FILEDIR> - file directory"
    exit "${1}"
}

# set defaults to optional command-line arguments
RATE="100mbit"
DELAY="10ms"
TC="ON"
BITSIZE=28
LUTSIZE=10
PREP=true
FILEDIR="results"

# get optional command-line arguments
while getopts ":r:d:tn:J:pf:h" ARG ; do
    case "${ARG}" in
        r) RATE="${OPTARG}" ;;
        d) DELAY="${OPTARG}" ;;
        t) TC="" ;;
        n) BITSIZE="${OPTARG}" ;;
        J) LUTSIZE="${OPTARG}" ;;
        p) PREP=false ;;
        f) FILEDIR="${OPTARG}" ;;
        h) usage "0" ;;
        :) echo "'${OPTARG}' missing required argument" ; usage "1" ;;
        \?) echo "'${OPTARG}' option not found" ; usage "1" ;;
    esac
done

# change working directory to location of script
cd "$(dirname "${0}")"

# create directory for results
if [ ! -d $FILEDIR ]; then
    mkdir $FILEDIR
fi

FILE=$FILEDIR/n_${BITSIZE}_J_${LUTSIZE}
echo Running experiments with n=$BITSIZE and J=$LUTSIZE

if $PREP ; then
    rm params.hpp
    echo "#ifndef PARAMS_HPP__
    #define PARAMS_HPP__

    static constexpr std::size_t L = 64;
    static constexpr std::size_t n = $BITSIZE;
    static constexpr std::size_t J = $LUTSIZE, j=n-J;
    static constexpr auto twoj = (1ul << j);
    static constexpr auto twoJ = (1ul << J);
    int32_t bitlength = L;

    static constexpr std::size_t num_threads = 1;

    using input_type = dpf::modint<L>;
    // using share_type = grotto::fixedpoint<fracbits, integral_type>;
    using output_type = dpf::modint<L>;
    std::size_t interval_bytes = twoJ * sizeof(output_type);

    output_type * scaled_lut;
    std::size_t count = 100;

    #endif   // PARAMS_HPP_" > params.hpp

    rm -rf bin
    mkdir bin
    make

    echo Haar Preprocessing
    ./bin/dealer-Haar-file >> $FILE.log

    echo Bior Preproccesing
    ./bin/dealer-bior-file >> $FILE.log
fi

echo Running bior
# Docker documentation: https://docs.docker.com/reference/cli/docker/container/run/
# run peer0 (the peer that will wait for others to connect) and set traffic control
docker container run --name "peer0" \
                     --detach \
                     --rm \
                     --mount "type=bind,source=./${FILEDIR},destination=/${FILEDIR}" \
                     --mount 'type=bind,source=./lut.dat,destination=/lut.dat' \
                     --mount 'type=bind,source=./bin,destination=/cbin' \
                     --mount 'type=bind,source=./bior.0,destination=/bior.0' \
                     dwt:experiments \
                     /bin/bash -c "/cbin/client0-bior-online >> /${FILE}_bior_peer0.log 2>&1"

# if traffic control not disabled, pass rate and delay
if [[ -n "${TC}" ]] ; then
    ./traffic_control.sh -r "${RATE}" -d "${DELAY}" "peer0"
fi

# run peer1 attached to peer0's local network
docker container run --name "peer1" \
                     -it \
                     --rm \
                     --mount "type=bind,source=./${FILEDIR},destination=/${FILEDIR}" \
                     --mount 'type=bind,source=./lut.dat,destination=/lut.dat' \
                     --mount 'type=bind,source=./bin,destination=/cbin' \
                     --mount 'type=bind,source=./bior.1,destination=/bior.1' \
                     --network "container:peer0" \
                     dwt:experiments \
                     /bin/bash -c "/cbin/client1-bior-online 2>&1 | tee -a /${FILE}_bior_peer1.log"

docker wait peer0

echo Running Haar
# Docker documentation: https://docs.docker.com/reference/cli/docker/container/run/
# run peer0 (the peer that will wait for others to connect) and set traffic control
docker container run --name "peer0" \
                     --detach \
                     --rm \
                     --mount "type=bind,source=./${FILEDIR},destination=/${FILEDIR}" \
                     --mount 'type=bind,source=./lut.dat,destination=/lut.dat' \
                     --mount 'type=bind,source=./bin,destination=/cbin' \
                     --mount 'type=bind,source=./Haar.0,destination=/Haar.0' \
                     dwt:experiments \
                     /bin/bash -c "/cbin/client0-Haar-online >> /${FILE}_haar_peer0.log 2>&1"

# if traffic control not disabled, pass rate and delay
if [[ -n "${TC}" ]] ; then
    ./traffic_control.sh -r "${RATE}" -d "${DELAY}" "peer0"
fi

# run peer1 attached to peer0's local network
docker container run --name "peer1" \
                     -it \
                     --rm \
                     --mount "type=bind,source=./${FILEDIR},destination=/${FILEDIR}" \
                     --mount 'type=bind,source=./lut.dat,destination=/lut.dat' \
                     --mount 'type=bind,source=./bin,destination=/cbin' \
                     --mount 'type=bind,source=./Haar.1,destination=/Haar.1' \
                     --network "container:peer0" \
                     dwt:experiments \
                     /bin/bash -c "/cbin/client1-Haar-online 2>&1 | tee -a /${FILE}_haar_peer1.log"

docker wait peer0