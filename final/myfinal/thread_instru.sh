for(( j=0; j<3; j=j+1 ))
do
	for i in 10 30 50 70 100 200
	do
		echo -n "$i	"
		((perf stat -e instructions:u -v ./hw4 -data ./ -output submission.csv -tree 70 -thread $i) 2>&1) | \
			grep -v "instructions\:u\:" | \
			grep "instructions\:u" | \
			sed 's/instructions:u//g' | \
			tr -d '\n' | \
			tr -d ' '
		echo ""
	done
done
