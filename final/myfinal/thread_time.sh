#!/bin/bash
make 2> /dev/null
gcc diff.c -o diff

echo "thread_num	real		user		error"
for i in 10 50 200 1000 5000
do
	for(( j=0; j<1; j=j+1 ))
	do
		echo -n "$i		"
		(time (./hw4 -data ./ -output submission.csv -tree 200 -thread $i) 2>&1) | \
			grep -v "sys" | \
			sed 's/real//g' | \
			sed 's/user//g' | \
			tr -d '\n'
		echo -n "	"
		./diff
	done
done
