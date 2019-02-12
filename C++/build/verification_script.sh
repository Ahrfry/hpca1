#!/bin/bash
# Version 1.1

echo "****************************"
echo "***Default Configuration ***"
echo "****************************"

for i in `ls ../../traces/`; do
    echo ""
    echo "--- $i ---"
	./cachesim -i ../../traces/$i
done;

echo ""
echo "Fully Assoc L1"
echo "****************************"
echo "**  c = 10, b = 5, s = 5  **"
echo "****************************"

for i in `ls ../../traces/`; do
    echo ""
    echo "--- $i ---"
	./cachesim -c 10 -b 5 -s 5 -i ../../traces/$i
done;

echo ""
echo "Fully Assoc L2"
echo "****************************"
echo "**  C = 18, b = 6, S = 12  **"
echo "****************************"

for i in `ls ../../traces/`; do
    echo ""
    echo "--- $i ---"
	./cachesim -C 18 -b 6 -S 12 -i ../../traces/$i
done;

echo ""
echo "No VC; No prefetch"
echo "****************************"
echo "***    V = 0, K = 0      ***"
echo "****************************"

for i in `ls ../../traces/`; do
    echo ""
    echo "--- $i ---"
	./cachesim -v 0 -k 0 -i ../../traces/$i
done;

echo ""
echo "No VC"
echo "****************************"
echo "***        V = 0         ***"
echo "****************************"

for i in `ls ../../traces/`; do
    echo ""
    echo "--- $i ---"
	./cachesim -v 0 -i ../../traces/$i
done;

echo ""
echo "No prefetch"
echo "****************************"
echo "***        K = 0         ***"
echo "****************************"

for i in `ls ../../traces/`; do
    echo ""
    echo "--- $i ---"
	./cachesim -k 0 -i ../../traces/$i
done;
