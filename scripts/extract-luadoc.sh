#!/bin/bash
cat $* | awk '/\/\*\*\-\-/ {blk=1}; {if(blk) print $0}; /\*\// {blk=0}'|grep -v "\*\*\-\-" | grep -v "\*\/"
