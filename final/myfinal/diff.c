#include <stdio.h>

int ans_dataset[25008];
int main()
{
	FILE *fp = fopen("ans.csv", "r");

	char str_trash[128];
	fscanf(fp, "%s", str_trash);
	int trash;
	for(int i = 0; i < 25008; ++i) {
		fscanf(fp, "%d,%d", &trash, &(ans_dataset[i]));
		// printf("%d\t", trash);
	}
	fclose(fp);

	int errcnt = 0;
	int myerr0 = 0, myerr1 = 0;
	fp = fopen("submission.csv", "r");
	fscanf(fp, "%s", str_trash);
	for(int i = 0; i < 25008; ++i) {
		int myans;
		fscanf(fp, "%d,%d", &trash, &myans);
		if(myans != ans_dataset[i]) {
			++errcnt;
			if(myans == 0) {
				++myerr0;
			} else {
				++myerr1;
			}
		}
	}
	fclose(fp);

	printf("%f\n", ( (double) (25008-errcnt) ) / 25008 );
	// printf("1 - %d / 25008 = %f\n", errcnt, ( (double) (25008-errcnt) ) / 25008 );
	// printf("myerror is 0 : %d\n", myerr0);
	// printf("myerror is 1 : %d\n", myerr1);

	return 0;
}
