/* b04902053 鄭淵仁 */

// includes/*{{{*/

#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
/*}}}*/

const int NLINE = 25150;
const int NTEST = 25008;
const int NCHOOSE = 2000;

typedef struct node {/*{{{*/
	int id, dim;
	struct node *left, *right;
} Node;/*}}}*/

typedef struct {/*{{{*/
	int tree;
	Node *root;
} Tree_Root;/*}}}*/

double training_dataset[25150][35];
double testing_dataset[25008][34];
int output_dataset[400][25008];

void free_node(Node *root)/*{{{*/
{
	if(root == NULL) {
		return;
	}
	if(root->left != NULL)
		free_node(root->left);
	if(root->right != NULL)
		free_node(root->right);
	free(root);
	return;
}/*}}}*/

int search(int id, Node *root)/*{{{*/
{
	if( testing_dataset[id][root->dim] < training_dataset[root->id][root->dim] ) {
		if(root->left == NULL) {
			return training_dataset[root->id - 1][34];
		}
		return search(id, root->left);
	}
	if(root->right == NULL) {
		return training_dataset[root->id][34];
	}
	return search(id, root->right);
}/*}}}*/
void *run_search(void *tree_root)/*{{{*/
{
	int t = ((Tree_Root*)tree_root)->tree;
	Node *root = ((Tree_Root*)tree_root)->root;
	// TODO: maybe I can add threads here
	for(int i = 0; i < NTEST; ++i) {
		output_dataset[t][i] = search(i, root);
	}
	return ((void*)0);
}/*}}}*/

int cmp(const void *a, const void *b, void *c) {/*{{{*/
	return ( training_dataset[*(int*)a][*(int*)c]
				> training_dataset[*(int*)b][*(int*)c] );
}/*}}}*/
void find_best_cut(int beg, int end, int indices[], int *ret_minid, int *ret_mindim, double *ret_leftgini, double *ret_rightgini)/*{{{*/
{
	double mingini = 2 * (end - beg);

	for(int dim = 1; dim < 34; ++dim) {
		qsort_r(indices + beg, end - beg, sizeof(int), cmp, (void *)&dim);

		// count total number of 1 -> tot_cnt1/*{{{*/
		double tot_cnt1 = 0;
		for(int i = beg; i < end; ++i) {
			tot_cnt1 += training_dataset[indices[i]][34];
		}/*}}}*/

		double cnt1 = 0;	// number of 1 before cut

		// cut between (i-1) & i
		for(int i = beg + 1; i < end; ++i) {
			cnt1 += training_dataset[indices[i - 1]][34];

			double res_cnt1 = tot_cnt1 - cnt1;
			double left_len = i - beg, right_len = end - i;
			double leftgini = (cnt1/left_len) * (1.0-cnt1/left_len);
			double rightgini = (res_cnt1/right_len) * (1.0 - res_cnt1/right_len);
			double gini = 2*(  left_len*leftgini + right_len*rightgini   );

			if(gini < mingini) {
				*ret_minid = i;
				*ret_mindim = dim;
				mingini = gini;
				*ret_leftgini = leftgini;
				*ret_rightgini = rightgini;
			}
		}
	}

	qsort_r(indices + beg, end - beg, sizeof(int), cmp, (void *)ret_mindim);

	return;
}/*}}}*/
void cut(int beg, int end, Node *root, int indices[])/*{{{*/
{
	int minid, mindim;
	double leftgini, rightgini;
	find_best_cut(beg, end, indices, &minid, &mindim, &leftgini, &rightgini);
	// printf("[beg end leftgini rightgini] = %d\t%d\t%f\t%f\t\n", beg, end, leftgini, rightgini);

	root->id = indices[minid];
	root->dim = mindim;
	if(leftgini == 0) {
		root->left = NULL;
	} else {
		root->left = (Node *)malloc(sizeof(Node));
		cut(beg, minid, root->left, indices);
	}

	if(rightgini == 0) {
		root->right = NULL;
	} else {
		root->right = (Node *)malloc(sizeof(Node));
		cut(minid, end, root->right, indices);
	}
	return ;
}/*}}}*/
void *build_tree(void *root)/*{{{*/
{
	int *indices = (int *)malloc(NLINE * sizeof(int));

	srand( (unsigned) pthread_self() );
	for(int i = 0; i < NCHOOSE; ++i)
		indices[i] = rand() % NCHOOSE;
	srand( (unsigned) time(NULL) );
	for(int i = 0; i < NCHOOSE; ++i)
		indices[i] += rand() % NCHOOSE;

	cut(0, NCHOOSE, (Node*)root, indices);
	free(indices);
	return ((void*)0);
}/*}}}*/

void *input_testingdata(void *testing_data)/*{{{*/
{
	char *data_name = (char *)testing_data;
	FILE *ofp = fopen(data_name, "r");
	for(int i = 0; i < NTEST; ++i)
		for(int j = 0; j < 34; ++j) {
			fscanf(ofp, "%lf", &(testing_dataset[i][j]));
		}
	fclose(ofp);
	return ((void*)0);
}/*}}}*/

int main(int argc, char *argv[])
{
	// argvs/*{{{*/
	char training_data[128] = "../data/training_data";
	char testing_data[128] = "../data/testing_data";
	char output_data[128] = "./submission.csv";
	int tree_number = 50;
	int thread_number = 50;

	for(int i = 1; i < argc; i += 2) {
		if( !strcmp(argv[i], "-data") ) {
			int len = strlen(argv[i + 1]);
			if(argv[i + 1][len - 1] == '/')
				argv[i + 1][len - 1] = '\0';
			strcpy(training_data, argv[i + 1]);
			strcat(training_data, "/training_data");
			strcpy(testing_data, argv[i + 1]);
			strcat(testing_data, "/testing_data");
		} else if ( !strcmp(argv[i], "-output") ) {
			strcpy(output_data, argv[i + 1]);
		} else if ( !strcmp(argv[i], "-tree") ) {
			tree_number = atoi(argv[i + 1]);
		} else if ( !strcmp(argv[i], "-thread") ) {
			thread_number = atoi(argv[i + 1]);
		}
	}
	/*}}}*/

	pthread_t tid_testingdata;
	--thread_number;
	pthread_create(&tid_testingdata, NULL, input_testingdata, (void *)testing_data);

	// input datas/*{{{*/
	FILE *ofp = fopen(training_data, "r");
	for(int i = 0; i < NLINE; ++i)
		for(int j = 0; j < 35; ++j) {
			fscanf(ofp, "%lf", &(training_dataset[i][j]));
		}
	fclose(ofp);
	/*}}}*/

	// Build tree/*{{{*/
	Node root[tree_number];
	pthread_t tid[thread_number];
	int t = 0;
	while(t < tree_number) {
		int th;
		for(th = 0; th < thread_number && t < tree_number; ++th, ++t) {
			pthread_create(tid + th, NULL, build_tree, root + t);
		}
		for(int j = 0; j < th; ++j) {
			void *rval;
			pthread_join(tid[j], &rval);
		}
	}/*}}}*/

	void *rval;
	pthread_join(tid_testingdata, &rval);
	++thread_number;

	// Search in tree/*{{{*/
	Tree_Root tree_root[tree_number];
	t = 0;
	while(t < tree_number) {
		int th;
		for(th = 0; th < thread_number && t < tree_number; ++th, ++t) {
			tree_root[t].tree = t;
			tree_root[t].root = root + t;
			pthread_create(tid + th, NULL, run_search, (void *)(tree_root + t));
		}
		for(int j = 0; j < th; ++j) {
			void *rval;
			pthread_join(tid[j], &rval);
		}
	}
	/*}}}*/

	// output "output_dataset"/*{{{*/
	ofp = fopen(output_data, "w");
	fprintf(ofp, "id,label\n");
	for(int i = 0; i < NTEST; ++i) {
		int sum = 0;
		for(int t = 0; t < tree_number; ++t)
			sum += output_dataset[t][i];
		sum = (sum * 2 > tree_number * 1)? 1 : 0;
		fprintf(ofp, "%d,%d\n", i, sum);
	}
	fclose(ofp);/*}}}*/

	// frees/*{{{*/
	for(int t = 0; t < tree_number; ++t) {
		free_node( root[t].left );
		free_node( root[t].right);
	}
	/*}}}*/

	return 0;
}
