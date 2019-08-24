#!/bin/bash

for i in `ls *.c`
do
    cp $i ${i%.*}.xss
done
