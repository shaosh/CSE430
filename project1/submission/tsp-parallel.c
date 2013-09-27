/*
Version 1.0
Run properly. 
1. Matrix of cost, initialized perm correct
2. Min calculation correct
3. Output correct
4. No optimazation on perm after initialization
Version 1.1
1. Add sum() to calculate the cost of each route
2. Add nextperm() to find next permutation
3. Permutation is good 
4. Need to find a way to store the best routes in case there are multiple choices. 
Version 1.2
1. min citynum and **cost become global variable
2. Add partsum() to calculate the part of the total cost of the route
Version 1.3
1. int nextperm() changed to void nextperm()
2. In nextperm() do all the calculation to find the shortest path
3. Need to add: a. output structure
					   b. more test cases
Version 1.4
1. Added output structure
Version 1.5
1. Added list for each permuation (01, 02, 03, ... , 0citynum-1) for multi-threads
2. Need to add time stamp and test omp.h
Version 1.6
1. Multithread
2. Add time stamp (sec)
3. Correct the nextperm iteration so that we can get the correct lowest cost
Version 1.7
1. Add greedy to calculate the very first min.
Version 1.8
1. Correct some minor bugs in list and greedy
Version 1.9
1. add omp parallel directive
Version 2.0
1. fscanf_s is changed to fscanf for gcc
*/
#include<stdio.h>
#include<string.h>
#include<stdlib.h>
#include<omp.h>
#include<time.h>

int main(int,char**);
void shortest(char*);
void nextperm(int*, int);
int sum(int*);
int partsum(int*, int);
void output_insert(int*, int, int);
void greedy();

struct node{
	int output[200];
	int routecost;
	struct node* next;
};

struct node **list;
int min=0,citynum;
int **cost, **costgreedy;

int main(int argc, char* argv[]){	
	if (argc!=2)
		printf("Error: invalid commands!");
	else 
		shortest(argv[1]);		//open command file for read
}

void shortest(char* filename){
	//------------------------------------------variable declaration--------------------------------------------
	int i, j, k, count;
	int **perm;
	FILE *p=fopen(filename, "r");
	struct node *newnode, *pointer;
	clock_t begin, end;
	double  timediff;
	//------------------------------------------memory allocation and matrix initiation----------------------
	begin = clock();
	newnode=(struct node*)malloc(sizeof(struct node));
	if(newnode==0){
		printf("Error: out of memory\n");
		return;
	}
 
	fscanf(p,"%d", &citynum);
	cost=(int**)malloc(sizeof(int*)*citynum);
	costgreedy=(int**)malloc(sizeof(int*)*citynum);
	#pragma omp parallel for schedule(dynamic) num_threads(4)
	for(i=0;i<citynum;i++){
//		printf( "cost allocation%d executed by thread %d\n", i, omp_get_thread_num() );///////////////////////////////////////////////////////////////////////////////////////
		cost[i]=(int*)malloc(sizeof(int)*citynum);
		costgreedy[i]=(int*)malloc(sizeof(int)*citynum);
		for(j=0;j<citynum;j++){
			if(i==j){
				cost[i][j]=0;
				costgreedy[i][j]=0;
			}
		}
	}
			
	if(citynum>1){
		perm=(int**)malloc(sizeof(int*)*(citynum-1));		//not include 0, only 1~citynum-1. Branches from 0.
		list=(struct node**)malloc(sizeof(struct node*)*(citynum-1));
		#pragma omp parallel for  schedule(dynamic) num_threads(4)		
		for(i=0;i<citynum-1;i++){
	//		printf( "Perm allocation %d executed by thread %d\n", i, omp_get_thread_num() );///////////////////////////////////////////////////////////////////////////////////////
			perm[i]=(int*)malloc(sizeof(int)*(citynum+2)); //0->...->0, last spot is for branch sum
			perm[i][0]=0;				//start from city 0
			perm[i][citynum]=0;		//end at city 0
			perm[i][citynum+1]=0;	//initialize route cost to 0
			list[i]=NULL;
		}
		
	}
	
	for(count=0;count<(citynum-1)*citynum;count++){
		fscanf(p,"%d",&i);
		fscanf(p,"%d",&j);
		fscanf(p,"%d",&k);
		cost[i][j]=k;
		costgreedy[i][j]=k;
	}

	//------------------------------------------calculate min cost and output--------------------------------------------
	if(citynum==1){		
		min=0;
		newnode->output[0]=0;		
		newnode->output[1]=0;	
		newnode->routecost=min;	
		list[0]=newnode;
		newnode->next=NULL;
	}
	else if(citynum<1){
		printf("Error: invalid input.");
		return;
	}
	else{
		greedy();
		#pragma omp parallel for schedule(dynamic) num_threads(4)
		for(i=1;i<citynum;i++){			//begin branching for multithread		
//			printf( "Perm calculation%d executed by thread %d\n", i, omp_get_thread_num() );///////////////////////////////////////////////////////////////////////////////////////
			perm[i-1][1]=i;
			perm[i-1][citynum+1]+=cost[0][perm[i-1][1]];
			if(perm[i-1][citynum+1]<min){							//if current cost sum<min, continue, otherwise go on to next i
				#pragma omp parallel for schedule(dynamic) num_threads(4)				
				for(j=2;j<citynum;j++){			//initialize each spot in each branch ascendingly, e.g. 012345, 021345, 031245, 041235,051234
//					printf( "Branch %d executed by thread %d\n", j, omp_get_thread_num() );///////////////////////////////////////////////////////////////////////////////////////
					if(j>i)
						perm[i-1][j]=j;
					else
						perm[i-1][j]=j-1;
					perm[i-1][citynum+1]+=cost[perm[i-1][j-1]][perm[i-1][j]];
				}
				
				if(perm[i-1][citynum+1]<=min){		//add the cost from last city to city 0
					perm[i-1][citynum+1]+=cost[perm[i-1][citynum-1]][0];
					if(perm[i-1][citynum+1]<=min){
						#pragma omp critical	
						{
							min=perm[i-1][citynum+1];
						}
						output_insert(perm[i-1], min, i-1);						
					}
				}
	//------------------------------------------optimization--------------------------------------------
				if(citynum>3)
					nextperm(perm[i-1], i-1);
			}
		}
//------------------------------------------print test--------------------------------------------

		end = clock();
		timediff= (double)(end - begin) / CLOCKS_PER_SEC;
		printf("n = %d", citynum);
		printf("\n%lf seconds\n", timediff);
/*
		printf("\ncost matrix:\n");
		for(i=0;i<citynum;i++){
			for(j=0;j<citynum;j++){
				printf("%d ", cost[i][j]);
			}
			printf("\n");
		}
		
		printf("\nperm:\n");
		for(i=0;i<citynum-1;i++){
			for(j=0;j<citynum+2;j++)
				printf("%d ",perm[i][j]);
			printf("\n");
		}
*/
		printf("\nmin cost=%d\n",min);

		printf("list of best routes:\n");
		for(i=0;i<citynum-1;i++){
			if(list[i]!=NULL && list[i]->routecost==min){
				pointer=list[i];			
				while(pointer!=NULL){
					for(j=0;j<=citynum;j++)
						printf("%d ", pointer->output[j]);
//					printf("routecost = %d\n", pointer->routecost);	
					printf("\n");
					pointer=pointer->next;
				}
			}
		}
		printf("-------------------------End-------------------------\n");
	}
}

void nextperm(int* route, int insertindex){
	int i, j, temp, minpos, flag=-1;

	//--------test-----------

	while(flag!=0)	//((route[1]!=citynum-1&&route[2]!=citynum-1)||(route[1]==citynum-1&&route[2]!=citynum-2))
	{	
		flag=0;
		for(i=citynum-1;i>2;){
			if(route[i-1]<route[i]){						//look for neighboring digits ab in which a<b

				flag=1;
				minpos=i;											//look for the smallest digit which is larger than route[i-1]
				for(j=i;j<citynum;j++){				
					if(route[i-1]<route[j]&&route[j]<route[minpos])
						minpos=j;
				}
				temp=route[i-1];							//swap the route[i-1] with the smallest larger digit  
				route[i-1]=route[minpos];				//swap is necessary for the permuation, otherwise this
				route[minpos]=temp;						//permuation algorithm will have problem
			
				if(partsum(route, i-1)<min){				//if current sum<min, continue to reverse and get the smallest number, 
					for(j=i;j<=(citynum-1+i)/2;j++){	//otherwise just keep the unreversed number(largest number) and go to next iteration
						temp=route[j];							
						route[j]=route[citynum-1-(j-i)];	//reverse route[i] to [citynum-1]
						route[citynum-1-(j-i)]=temp;
					}
					if(sum(route)<=min){
						route[citynum+1]=sum(route);
						output_insert(route, route[citynum+1], insertindex); //add the route to the output list
						
						if(route[citynum+1]<min){
							#pragma omp critical
							min=route[citynum+1];
						}
					}						
				}
				i=citynum-1;
//========================
			}else{
				i--;
			//--------test-----------
	/*		printf("\nafter nextperm:\n");
			for(j=0;j<=citynum;j++)
				printf("%d ",route[j]);
			printf("i=%d \n", i);
	*/		}
		}
	}
}
int sum(int* route){
	int total=0, i;
	for(i=0;i<citynum;i++)
		total+=cost[route[i]][route[i+1]];
	return total;
}

int partsum(int* route, int end){
	int partcost=0, i;
	for (i=0;i<end;i++)
		partcost+=cost[route[i]][route[i+1]];
	return partcost;
}

void output_insert(int* route, int total, int insertindex){
	struct node *newnode, *p, *prev, *waste;
	int i;
	newnode=(struct node*)malloc(sizeof(struct node));
	if(newnode==0){
		printf("Error: out of memory\n");
		free(newnode);
		return;
	}
	for(i=0;i<citynum+1;i++)	//build a new node for the output
		newnode->output[i]=route[i];
	newnode->routecost=total;

	newnode->next=list[insertindex];	//insert the new node into the output linked list
	list[insertindex]=newnode;

	p=list[insertindex];				
	prev=list[insertindex];

	while(p!=NULL){		//delete other routes which the total cost is larger 
		if(p->routecost>newnode->routecost){
			prev->next=p->next;
			waste=p;
			p=p->next;
			free(waste);
			waste=NULL;
		}
		else{
			prev=p;
			p=p->next;
		}		
	}
}

void greedy(){
	int i, j, minpos, greedymin=0, row=0, insertindex;

	for(i=0;i<citynum-1;i++){
		for(j=1;j<citynum;j++){
			if(costgreedy[row][j]>0){
				minpos=j;
				break;
			}
		}		
		for(j=1;j<citynum;j++){
			if(costgreedy[row][j]<costgreedy[row][minpos]&&costgreedy[row][j]>0)
				minpos=j;
		}
		greedymin+=costgreedy[row][minpos];
		row=minpos;
		if(i==0)
			insertindex=minpos;
		for(j=0;j<citynum;j++)
			costgreedy[j][minpos]=-1;		
	}
	greedymin+=costgreedy[minpos][0];	
	min=greedymin;

	for(i=0;i<citynum;i++)
		free(costgreedy[i]);
	free(costgreedy);
	costgreedy=NULL;
	return;
}