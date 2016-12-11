#!/bin/bash

CLICK=$1
OUTPUT_FILE=$2
ITERATIONS=$3
EXPERIMENT_DIR=$4

SCRIPT=$(realpath $0)
SCRIPTPATH=$(dirname $SCRIPT)

echo "Running the basic SGX test"

CSV_HEADER="Counter Count,Source Count,Sink Count,Counter Byte Count,Counter Packet Rate (packets/s),Counter Bit Rate (bit/s),Counter Byte Rate (bytes/s)"
echo "$CSV_HEADER" > $OUTPUT_FILE

source /opt/intel/sgxsdk/environment
cd $SCRIPTPATH/../NFV_Basic_SGX/sgx/enclave_enclave1
rm enclave.token

for ((i=0;i<$ITERATIONS;i++));
do
  echo "Basic SGX experiment $i"
  $CLICK $EXPERIMENT_DIR/wire_infinite_source_sgx.click | tee -a $OUTPUT_FILE
done
