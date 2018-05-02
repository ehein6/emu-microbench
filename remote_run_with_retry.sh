#!/bin/bash

# Executes a list of bash scripts on the Emu chick
# If a job times out or crashes, automatically reboots and continues

# Prevent ssh from gobbling up stdin


# Reboot the chick
function reboot_chick()
{
    echo "Rebooting Emu Chick..."
    ssh -n root@karrawingi-login "emu_all_start_chicknode CHICK" &> /dev/null
    return $?
}

# Run a script on the remote node
function run_script()
{
    scp "$script" "$remote_node:$script"
    timeout "$time_limit" ssh -n "$remote_node" "$script"
    scp "$remote_node:$output" "$output"
}

# Check for a system error in the script output
function check_output()
{

    error=$(grep -F "File /tmp/num_nodes does not exist.  Have nodes been started?" "$output" | wc -l)
    if [ $error -ne 0 ]; then
        echo "ERROR: Chick not initialized"
        return 1
    fi

    error=$(grep -F "[FATALERROR]" "$output" | wc -l)
    if [ $error -ne 0 ]; then
        echo "ERROR: Other fatal error detected"
        return 1
    fi

    return 0
}


# ssh alias for Emu node to connect to
remote_node="n0"
# Working directory, relative on host, within $HOME on remote node
workdir="$1"
# Timeout for each job
time_limit="15m"

# Check for valid script directory
if [ ! -f "$1/jobnames" ]; then
    echo "Missing list of job names in $1"
    exit 1
fi

# Make working directory on remote node
echo "Setting up remote working directory $workdir on $remote_node..."
ssh -n "$remote_node" "mkdir -p $workdir/scripts $workdir/results"

# Run each job
while read name; do
    date
    script="$workdir/scripts/$name.sh"
    output="$workdir/results/$name.log"

    if [ -f "$output" ]; then
        echo "Output exists for $name, skipping"
        continue
    else
        echo "Running $script"
        echo "Output file is $output"
    fi

    run_script

    check_output

    until [ $? -eq 0 ]; do
        reboot_chick
    done

    sleep 0.4

done <"$1/jobnames"


