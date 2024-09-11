#!/bin/bash

# print usage information and exit
usage()
{
    echo "Usage: ${0} [OPTIONS] CONTAINER"
    echo "    -r <RATE>  - new rate to set for CONTAINER"
    echo "    -d <DELAY> - new delay to set for CONTAINER"
    exit "${1}"
}

# get optional command-line arguments
while getopts ":r:d:h" ARG ; do
    case "${ARG}" in
        r) NEW_RULE="${NEW_RULE}rate ${OPTARG} " ;;
        d) NEW_RULE="${NEW_RULE}delay ${OPTARG} " ;;
        h) usage "0" ;;
        :) echo "'${OPTARG}' missing required argument" ; usage "1" ;;
        \?) echo "'${OPTARG}' option not found" ; usage "1" ;;
    esac
done

# get first non-option command-line argument as container
CONTAINER="${!OPTIND}"
if [[ -z "${CONTAINER}" ]] ; then
    usage "1"
fi

# make sure supplied continer exists and is running
if [[ -z "$(docker container list --filter "name=${CONTAINER}" --filter "status=running" | awk '{ print $NF }' | grep -w "${CONTAINER}")" ]] ; then
    echo "${CONTAINER} does not appear to be a running Docker container"
    usage "1"
fi

# check if a tc-netem rule already exists
OLD_TC="$(docker exec "${CONTAINER}" tc qdisc show dev lo)"
OLD_RULE="$(echo "${OLD_TC}" | grep -oP '(?<=netem )[^ ]+')"
OLD_RATE="$(echo "${OLD_TC}" | grep -oP '(?<=rate )[^ ]+')"
OLD_DELAY="$(echo "${OLD_TC}" | grep -oP '(?<=delay )[^ ]+')"
OLD_RATE="${OLD_RATE:="∞"}"
OLD_DELAY="${OLD_DELAY:="0"}"

# delete old rule if one already exists
if [[ -n "${OLD_RULE}" ]] ; then
    docker exec --privileged "${CONTAINER}" tc qdisc del dev lo root
fi

# add new rule if requested
if [[ -n "${NEW_RULE}" ]] ; then
    docker exec --privileged "${CONTAINER}" tc qdisc add dev lo root netem ${NEW_RULE}
fi

# get new rule information
NEW_TC="$(docker exec "${CONTAINER}" tc qdisc show dev lo)"
NEW_RATE="$(echo "${NEW_TC}" | grep -oP '(?<=rate )[^ ]+')"
NEW_DELAY="$(echo "${NEW_TC}" | grep -oP '(?<=delay )[^ ]+')"
NEW_RATE="${NEW_RATE:="∞"}"
NEW_DELAY="${NEW_DELAY:="0"}"

# print out information about old and new rules
IP_ADDRESS="$(docker inspect -f '{{range.NetworkSettings.Networks}}{{.IPAddress}}{{end}}' "${CONTAINER}")"
echo "${CONTAINER} (${IP_ADDRESS}): rate ${OLD_RATE} delay ${OLD_DELAY} -> rate ${NEW_RATE} delay ${NEW_DELAY}"
