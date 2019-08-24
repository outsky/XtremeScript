#!/bin/bash

for i in `ls *.ok`
do
    cp $i ${i%.*}.xss
done
