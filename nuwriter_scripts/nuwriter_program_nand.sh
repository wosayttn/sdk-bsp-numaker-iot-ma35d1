#!/bin/bash

set -x -e

python3 nuwriter.py -c header-nand.json

python3 nuwriter.py -p pack-nand.json

python3 nuwriter.py -a ddrimg/enc_ddr3_winbond_512mb.bin

python3 nuwriter.py -w nand pack/pack.bin