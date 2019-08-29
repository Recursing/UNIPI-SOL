FAIL=0

for i in `seq 10 59`
    do
            ./test_client folder_$i 1 &
    done

for job in `jobs -p`
do
echo $job
    wait $job || let "FAIL+=1"
done

echo Finished test 1
echo $FAIL fails


for i in `seq 10 39`
    do
            ./test_client folder_$i 2 &
    done

for i in `seq 40 59`
    do
            ./test_client folder_$i 3 &
    done

for job in `jobs -p`
do
echo $job
    wait $job || let "FAIL+=1"
done


echo Finished test 2 and 3
echo $FAIL fails