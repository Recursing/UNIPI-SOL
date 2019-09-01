STARTMILLIS=`date +%s%3N`

# Number of test processes exiting with non 0 status
FAIL=0

# Run first test, 50 concurrent clients for test 1
for i in `seq 10 59`
do
        ./test_client folder_$i 1 &
        pids[${i}]=$!
done

# wait for all stores
for pid in ${pids[*]}
do
    wait $pid || let "FAIL+=1" # Increase fail if $pid returns non 0
done

echo Finished test 1
ENDMILLIS=`date +%s%3N`
echo Finished test 1 in $((ENDMILLIS-STARTMILLIS)) ms
echo $FAIL fails in test 1

# Run 30 retrieve tests
for i in `seq 10 39`
do
        ./test_client folder_$i 2 &
        newpids[${i}]=$!
done

# Run 20 delete tests
for i in `seq 40 59`
do
        ./test_client folder_$i 3 &
        newpids[${i}]=$!
done

# Wait for second part of the test to finish
for pid in ${newpids[*]}
do
    wait $pid || let "FAIL+=1"
done


echo Finished test 2 and 3
echo $FAIL total fails

ENDMILLIS=`date +%s%3N`
echo Finished all in $((ENDMILLIS-STARTMILLIS)) ms
