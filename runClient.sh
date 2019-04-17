#!/bin/bash
# $1 = opearation-number, $2 = put_percent $3 = get_percent
./client $1
ssh rht1 "./client $1 $2 $3" 
ssh rht2 "./client $1 $2 $3"
ssh rht3 "./client $1 $2 $3"
ssh rht4 "./client $1 $2 $3"