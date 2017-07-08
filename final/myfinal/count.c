#include <stdio.h>

int main()
{
	FILE *fp = fopen("ans.csv", "r");

	char str_trash[128];	fscanf(fp, "%s", str_trash);

	int cnt = 0;
	for(int i = 0; i < 25008; ++i) {
		int trash;
		int num;
		fscanf(fp, "%d,%d", &trash, &num);
		if(num == 0)
			++cnt;
	}
	fclose(fp);

	printf("1 - %d / 25008 = %f\n", cnt, ( (double)cnt ) / 25008 );

	return 0;
}
