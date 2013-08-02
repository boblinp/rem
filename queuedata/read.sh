#!/bin/bash
#this is a bash script to read logdata about TCP connection
#2006-4-18,lianlinjiang,BUAA-CSE
echo "read logdata about queue connection......"
touch queuedata_rem.txt
rm queuedata_rem.txt
touch queuedata_rem.txt
i=0
while [ "$i" -le "30000" ]
do
insmod /root/AQM/rem/queuedata/seqfile_queuedata_rem.ko queue_array_count=$i
cat /proc/data_seq_file >> queuedata_rem.txt
rmmod seqfile_queuedata_rem
i=$[$i + 40]
done
echo "read logdata succed! pelease see queuedata.txt"
