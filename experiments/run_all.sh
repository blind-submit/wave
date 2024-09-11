#!/bin/bash

LAN_RATE="5000mbit"
LAN_DELAY="0.2ms"
LAN_FILEDIR="lan"

WAN_RATE="300mbit"
WAN_DELAY="70ms"
WAN_FILEDIR="wan"

for n in $(seq 28 32); do
	for J in $(seq 10 24); do
		if [ ! -f ${LAN_FILEDIR}/n_${n}_J_${J}.log ]; then
			./run.sh -r ${LAN_RATE} -d ${LAN_DELAY} -n $n -J $J -f ${LAN_FILEDIR}
			./run.sh -r ${WAN_RATE} -d ${WAN_DELAY} -n $n -J $J -p -f ${WAN_FILEDIR}
		fi
	done
	if [ ! -f ${LAN_FILEDIR}/n_${n}_J_${n}.log ]; then
		./run.sh -r ${LAN_RATE} -d ${LAN_DELAY} -n $n -J $n -f ${LAN_FILEDIR}
		./run.sh -r ${WAN_RATE} -d ${WAN_DELAY} -n $n -J $n -p -f ${WAN_FILEDIR}
	fi
done

python3 run_results.py --nmin 28 --nmax 32 --Jmin 10 --Jmax 24 --dir ${LAN_FILEDIR}
python3 run_results.py --nmin 28 --nmax 32 --Jmin 10 --Jmax 24 --dir ${WAN_FILEDIR}