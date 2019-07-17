#!/usr/bin/env bash
# Copyright (c) 2019 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

export LC_ALL=C

#
# Issue blocks using a local node at a given interval. These blocks
# are eventually reorged out of existence.
#

HELPSTRING="syntax: $0 <idle-time> <chain-length> <wait-length> [--cmd=<bitcoin-cli path>] [--] [<bitcoin-cli args>]\n
  <idle-time> refers to the number of seconds to wait between generating each block\n
  <chain-length> refers to the number of blocks to generate in the to-be-orphaned chain\n
  <wait-length> refers to the number of blocks that the main network should generate before this node begins generating an alternate chain again\n
good starting values are: 60 1 1 (generate 1 block longer chain than main network with 60 seconds between each new block grinding starts; wait until main chain is 1 block longer before doing it again)"

if [ $# -lt 3 ]; then
    echo -e $HELPSTRING
    exit 1
fi

function log()
{
    echo "- $(date +%H:%M:%S): $*"
}

idletime=$1
shift

chainlen=$1
shift

waitlen=$1
shift

source $(dirname $0)/args.sh "$@"

echo "- checking node status"

conns=$($bcli $args getconnectioncount) || { echo >&2 "node error"; exit 1; }

if [ $conns -lt 1 ]; then
    echo "warning: node is not connected to any other node"
fi

MKBLOCK=$(dirname $0)/mkblock.sh

if [ ! -e "$MKBLOCK" ]; then
    >&2 echo "error: cannot locate mkblock.sh (expected to find in $MKBLOCK"
    exit 1
fi

log "node OK with $conns connection(s)"

currheight=$($bcli $args getblockcount)
(( nextheight=currheight+waitlen ))
log "current chain height = $currheight, waiting for height = $nextheight before mining alternative chain"
log "once alternative chain hit, will mine at maximum capacity with $idletime second delay between each block until $chainlen blocks have been mined"
log "hit ^C to stop"

while true; do
    # wait for next height
    while [ $nextheight -gt $currheight ]; do
        sleep 60
        currheight=$($bcli $args getblockcount)
    done
    log "height -> $currheight >= $nextheight"
    # mine
    (( stopheight=currheight+chainlen ))
    while [ $stopheight -gt $currheight ]; do
        sleep $idletime
        blockhash=$("$MKBLOCK" "$bcli" $args) || { echo "node error; aborting"; exit 1; }
        currheight=$($bcli $args getblockcount)
        echo "- $blockhash -> $currheight / $stopheight"
    done
    (( nextheight=currheight+waitlen ))
    echo "waiting for height=$nextheight"
done
