#!/bin/bash

ttylog -b 9600 -f -d $1 2>&1 | tee -a log