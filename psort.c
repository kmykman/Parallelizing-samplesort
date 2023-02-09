#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/time.h>

int checking(unsigned int *, long);
int compare(const void *, const void *);

sem_t sem;
pthread_mutex_t lock;

int no_of_thread;
pthread_barrier_t barrier_phase1;
pthread_barrier_t barrier_phase2;

// global variables
long size;  // size of the array
unsigned int * intarr; // array of random integers
unsigned int * position_phase1;  // store indices number in phase 1 and 2
unsigned int * pivot_phase1;     // store element from each thread in phase 2
unsigned int * pivot_phase2;     // store pivot element
unsigned int * partition[512];  // maximum number of thread is 512
unsigned int * partition_pos;
int position = 0;  // temp storing position
int pp1_pos = 0;   // size of array position_phase1

typedef struct thread {   
	pthread_t thread_id;       
	int thread_number;                  // each thread has unique thread_number, start from 0 to number of working thread - 1, base on create sequence
	int bucket_size;                    // size of number_in_thread
	unsigned int *number_in_thread;     // number contained in each thread
} bucket;

void *thread_func(void *thread_pointer);

int main (int argc, char **argv){
	long i, j;
	struct timeval start, end;

	if ((argc != 2) && (argc != 3)){       // invalid input
		printf("Usage: seq_sort <number> [<no_of_workers>]\n");
		exit(0);
	}
	
	if ((argc == 3) && (atol(argv[2]) <= 1)){       // invalid when user requires 1 working thread
		printf("Usage: [<no_of_workers>] should be larger than 1\n");
		exit(0);
	}
	
	if (argc == 2){
		if (atol(argv[1]) < 4){           // if number of sample is smaller than number of working thread, number of working thread equals to number of sample (such as n = 3, p = 4)
			no_of_thread = atol(argv[1]);
		}
		else if (atol(argv[1]) >=4 ){
			no_of_thread = 4;
		}
	}
	else if (argc == 3){
		if (atol(argv[2]) >= 508){        // invalid if required working thread is larger than Workbench2 limit
			printf("Exceed maximum number of thread\n");
			exit(0);
		}
		if (atol(argv[2]) > atol(argv[1])){      // if number of sample is smaller than input number of working thread, number of working thread equals to number of sample (such as n = 3, p = 6)
			no_of_thread = atol(argv[1]);
		}
		else if (atol(argv[2]) <= atol(argv[1])){
			no_of_thread = atol(argv[2]);
		}
	}
	
	size = atol(argv[1]);
	intarr = (unsigned int *)malloc(size*sizeof(unsigned int));
	if (intarr == NULL) {perror("malloc"); exit(0); }
	
	// set the random seed for generating a fixed random
	// sequence across different runs
	char * env = getenv("RANNUM");  //get the env variable
	if (!env)                       //if not exists
		srandom(3230);
	else
		srandom(atol(env));
	
	
	for (i=0; i<size; i++){
		intarr[i] = random();
	}
	
	
	// measure the start time
	gettimeofday(&start, NULL);
	
	pthread_barrier_init(&barrier_phase1, NULL, no_of_thread + 1);
	pthread_barrier_init(&barrier_phase2, NULL, no_of_thread + 1);
	
	bucket thread[no_of_thread];         // for the struct thread
	
	sem_init(&sem, 0, 0);
	pthread_mutex_init(&lock, NULL);
	
	int div = size / no_of_thread;        // see if number of working thread is divisible to size (for assigning roughly the same size to each thread) 
	
	// assigning spaces to array
	partition_pos = (unsigned int *)malloc(513*sizeof(unsigned int));
	for (int j = 0; j < no_of_thread; j++){
		thread[j].number_in_thread = malloc(sizeof (unsigned int *) * (div + 1));
		partition[j] = (unsigned int *) malloc(size * sizeof (unsigned int));
	}
	position_phase1 = (unsigned int *)malloc((no_of_thread + 1)*sizeof(unsigned int));
	pivot_phase1 = (unsigned int *)malloc((no_of_thread*no_of_thread + 1)*sizeof(unsigned int));
	pivot_phase2 = (unsigned int *)malloc((no_of_thread + 1)*sizeof(unsigned int));
	
	
	// creating working thread and set assign unique number to each thread as thread_number in struct, starting from 0
	for (i=0; i < no_of_thread; i++){
		thread[i].thread_number = i;
		pthread_create(&(thread[i].thread_id), NULL, thread_func, &thread[i]);
	}
	
	// calculate local indices number in phase 1 and store them in array position_phase1
	for (int n = 0; n < no_of_thread; n++){
		if (pp1_pos >= thread[0].bucket_size){
			break;
		}
		else if ((pp1_pos == 0) || ((int) ((n * size) / (no_of_thread * no_of_thread)) != position_phase1[pp1_pos - 1])){
			position_phase1[pp1_pos] = (int) ((n * size) / (no_of_thread * no_of_thread));
			pp1_pos++;        // indicate the size of position_phase1
		}
		else if ((int) ((n * size) / (no_of_thread * no_of_thread)) == position_phase1[pp1_pos - 1]){
			continue;
		}
	}
	
	
	
	pthread_barrier_wait(&barrier_phase1);    // wait for each thread to finish
	pthread_barrier_wait(&barrier_phase2);    // indicating thread has finished staged process
	
	
	memset(position_phase1,0,pp1_pos);       // reset value in array position_phase1 for reusing
	
	// calculate indices number in phase 2 and store them in array position_phase1
	for (int p = 1; p < no_of_thread; p++){
		if ((p * no_of_thread + ((int) no_of_thread / 2)) >= (no_of_thread * pp1_pos)){
			break;
		}
		else{
			position_phase1[p - 1] = p * no_of_thread + ((int) no_of_thread / 2) - 1;
		}
	}
	
	// sort combined array from thread in phase 2
	qsort(pivot_phase1, no_of_thread * no_of_thread, sizeof(unsigned int), compare);
	
	position = 0;
	
	// get pivot values and store in array pivot_phase2
	for (int n = 0; n < no_of_thread - 1; n++){
		pivot_phase2[position] = pivot_phase1[position_phase1[n]];
		position++;
	}
	
	
	int pos = 0;             // current position in array number_in_thread
	int pi_pos = 0;          // current position in array partition[thread]
	int thread_no = 0;
	
	
	// separating thread numbers by pivot values into array partition[512], partition[0] is to thread 0, partition[1] is to thread 1, etc.
	for (int thr = 0; thr < no_of_thread; thr++){
		while(pos < thread[thr].bucket_size){
			while (thread[thr].number_in_thread[pos] <= pivot_phase2[pi_pos]){
				partition[thread_no][partition_pos[thread_no]] = thread[thr].number_in_thread[pos];
				pos++;                            // move to next position in array number_in_thread
				partition_pos[thread_no]++;       // size of each array in pointer of array partition
				if (pos >= thread->bucket_size){
					break;
				}
			}
			pi_pos++;
			if (pi_pos >= (no_of_thread - 1)){             // for last thread
				while (pos < thread[thr].bucket_size){
					partition[no_of_thread - 1][partition_pos[no_of_thread - 1]] = thread[thr].number_in_thread[pos];
					pos++;
					partition_pos[no_of_thread - 1]++;
					if (pos >= thread->bucket_size){
						break;
					}
				}
				break;
			}
			if (pos >= thread[thr].bucket_size){
				break;
			}
			thread_no++;
			if (thread_no >= no_of_thread){
				break;
			}
		}
		pos = 0;
		pi_pos = 0;
		thread_no = 0;
	}
	
	sem_post(&sem);    // inform the threads that main thread has done
	
	// wait for threads to exit
	for (int j = 0; j < no_of_thread; j++){
		if (pthread_join(thread[j].thread_id, NULL) != 0){
			printf("thread %d: %s\n", thread[j].thread_number, strerror(errno));
		}
	}
	
	
	// combine each thread-sorted array into intarr
	int final_pos = 0;
	for (int x = 0;x<no_of_thread;x++){
		for (int y = 0; y < partition_pos[x]; y++){
			intarr[final_pos] = partition[x][y];
			final_pos++;
		}
	}
	
	
	// free all used array
	free(pivot_phase2);
	free(pivot_phase1);
	free(partition_pos);
	free(position_phase1);
	for (int j = 0; j < no_of_thread; j++){
		free(thread[j].number_in_thread);
		free(partition[j]);
	}
	
	pthread_barrier_destroy(&barrier_phase1);
	pthread_barrier_destroy(&barrier_phase2);
	sem_destroy(&sem);
	pthread_mutex_destroy(&lock);
	
	
	// measure the end time
	gettimeofday(&end, NULL);
	
	
	
	if (!checking(intarr, size)){
		printf("The array is not in sorted order!!\n");
	}
	
	printf("Total elapsed time: %.4f s\n", (end.tv_sec - start.tv_sec)*1.0 + (end.tv_usec - start.tv_usec)/1000000.0);
	  
	free(intarr);
	return 0;
}

int compare(const void * a, const void * b){
	return (*(unsigned int *)a>*(unsigned int *)b) ? 1 : ((*(unsigned int *)a==*(unsigned int *)b) ? 0 : -1);
}


int checking(unsigned int * list, long size){
	long i;
	printf("First : %d\n", list[0]);
	printf("At 25%%: %d\n", list[size/4]);
	printf("At 50%%: %d\n", list[size/2]);
	printf("At 75%%: %d\n", list[3*size/4]);
	printf("Last  : %d\n", list[size-1]);
	for (i=0; i<size-1; i++){
		if (list[i] > list[i+1]){
		  return 0;
		}
	}
	return 1;
}

void *thread_func(void *thread_pointer){
	bucket *thread = (bucket *)thread_pointer;
	
	int div = size / no_of_thread;
	int current = 0;
	int current_max = 0;
	int remainder = size % no_of_thread;
	int add = 0;
	
	// see if there is extra element in this thread due to indivisible
	if (remainder > thread->thread_number){
		current = div * thread->thread_number + thread->thread_number;
	}
	else{
		current = div * thread->thread_number + remainder;
	}
	
	// see if there is extra element in this thread due to indivisible
	if (remainder > thread->thread_number){
		current_max = current + div;
	}
	else{
		current_max = current + div - 1;
	}
	
	int counter = current_max - current + 1;
	thread->bucket_size = counter;
	
	// get own partition for each thread and put in array number_in_thread under struct based on position in intarr
	for (int i=0; i<counter; i++){
		thread->number_in_thread[i] = intarr[current];
		current++;
	}
	
	// local sort each partition
	qsort(thread->number_in_thread, thread->bucket_size, sizeof(unsigned int), compare);
	
	// wait for main thread to do current stage
	pthread_barrier_wait(&barrier_phase1);
	
	// put element in array pivot_phase1 for local sorting
	for (int n = 0; n < pp1_pos; n++){
		pthread_mutex_lock(&lock);       // use lock beacause pivot_phase1 and position are global and shared to all thread
		pivot_phase1[position] = thread->number_in_thread[position_phase1[n]];
		position++;
		pthread_mutex_unlock(&lock);
		if (n >= thread->bucket_size - 1){
			break;
		}
	}
	pthread_barrier_wait(&barrier_phase2);    // inform main thread that have done placing element in array
	
	sem_wait(&sem);     // wait for main thread to finish
	
	qsort(partition[thread->thread_number], partition_pos[thread->thread_number], sizeof(unsigned int), compare);    // local sorting for each thread
	
	sem_post(&sem);     // wake up one of the thread that is in waiting queue
	pthread_exit(NULL);
}
