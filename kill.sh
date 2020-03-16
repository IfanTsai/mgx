#!/bin/bash

kill -9 -`ps -ajx | grep mgx | grep master | awk '{print $2}'`
