#!/bin/bash

i=1
while [ $i -lt 123450 ]; do
    nc 127.0.0.1 8081 &
    i=$((i+1))
    echo $i
done

read
