#!/bin/bash
parallel --workdir . -a $1/joblist --joblog $1/joblog --progress
