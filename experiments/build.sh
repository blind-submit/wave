#!/bin/bash

# change working directory to location of script to make sure correct Dockerfile can be found
cd "$(dirname "${0}")"

# command documentation: https://docs.docker.com/reference/cli/docker/buildx/build/
# `tag` is the name to give the container
docker image build --tag dwt:experiments --file Dockerfile .