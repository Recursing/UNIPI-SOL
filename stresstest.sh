STARTMILLIS=`date +%s%3N`

# Number of test processes exiting with non 0 status
FAIL=0

# Run first test, 500 concurrent clients for test 1
for i in `seq 100 599`
do
        ./test_client folder_$i 1 &
        pids[${i}]=$!
done

# wait for all stores
for pid in ${pids[*]}
do
    wait $pid || let "FAIL+=1" # Increase fail if $pid returns non 0
done

# Delete all
for i in `seq 100 599`
do
        ./test_client folder_$i 3 &
        newpids[${i}]=$!
done

# Wait for all deletes
for pid in ${newpids[*]}
do
    wait $pid || let "FAIL+=1"
done

# Store all again
for i in `seq 100 599`
do
        ./test_client folder_$i 1 &
        pids[${i}]=$!
done

# wait for all stores
for pid in ${pids[*]}
do
    wait $pid || let "FAIL+=1" 
done

echo Finished storing deleting and restoring 500 test clients
ENDMILLIS=`date +%s%3N`
echo Finished test 1 in $((ENDMILLIS-STARTMILLIS)) ms
echo $FAIL fails in test 1

# Run 500 retrieve tests
for i in `seq 100 599`
do
        ./test_client folder_$i 2 &
        newpids[${i}]=$!
done

# Wait for retrieves to finish checking objects
for pid in ${newpids[*]}
do
    wait $pid || let "FAIL+=1"
done

# Wait retrieves to finish
for pid in ${newpids[*]}
do
    wait $pid || let "FAIL+=1"
done

# Run 500 delete tests
for i in `seq 100 599`
do
        ./test_client folder_$i 3 &
        newpids[${i}]=$!
done

# Wait deletes to finish
for pid in ${newpids[*]}
do
    wait $pid || let "FAIL+=1"
done


echo Finished testing 500 retrieves and deletes
echo Data folder size: `du -sh data`
echo $FAIL total fails

ENDMILLIS=`date +%s%3N`
echo Finished all in $((ENDMILLIS-STARTMILLIS)) ms