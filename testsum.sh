# legge il contenuto di testout.log, calcola e stampa su stdout 
# un sommario dell'esito dei test (clienti lanciati, clienti che
# hanno riportato anomalie, numero di anomalie per batteria di test)

TOTAL_TESTS=`grep "^Run test" testout.log | wc -l`
TOTAL_ERRED=`grep "^Run test" testout.log | grep -v "with 0 errors" | wc -l`
echo "Run $TOTAL_TESTS clients, $TOTAL_ERRED had an error"
for i in `seq 1 3`
do
    TC_NUM=`grep "^Run test $i" testout.log | wc -l`
    ERR_NUM=`grep "^Run test $i" testout.log | grep -v "with 0 errors" | wc -l`
    O_TOTAL=`grep "^Run test $i" testout.log | awk '{SUM+=$8}END{print SUM}'`
    ERR_TOTAL=`grep "^Run test $i" testout.log | awk '{SUM+=$5}END{print SUM}'`
    TEST[${i}]=$TC_NUM

    echo " - Type $i: $TC_NUM total clients, $O_TOTAL total operations"
    echo "    - $ERR_NUM tests with errors, $ERR_TOTAL total errors"
done

TEST1MS=`awk '/Finished test 1 in/ {print $5}' testout.log`
TOTALMS=`awk '/Finished all in/ {print $4}' testout.log`
echo
echo "First batch took $TEST1MS ms"
echo "Total tests time $TOTALMS ms"

echo
echo "Most common rows in testout.log:"

LINES=`grep -v ^$ testout.log | sort | uniq -c | sort -rg | head -n 5`
cat << EOF
$LINES
EOF

killall -SIGUSR1 object_store