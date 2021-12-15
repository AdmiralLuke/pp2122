#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <mpi.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>


char* getMessage(void){
        char* buffer = malloc(64);
        struct tm* tm_info;
        struct timeval tv;

        gethostname(buffer, 26);
        strcat(buffer, " : ");
        gettimeofday(&tv, NULL);

        tm_info = localtime(&tv.tv_sec);

        strftime(buffer + strlen(buffer), 64 - strlen(buffer), "%d-%m-%Y %H:%M:%S", tm_info);
        snprintf(buffer + strlen(buffer), 64 - strlen(buffer),",%06ld\n", tv.tv_usec);
        return buffer;
}


int main(int argc, char** argv){
	MPI_Init(NULL,NULL);
	int rank;

	/* get rank */
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);

	if(rank == 0)
	{	
		int size;
		int min_usec = 1000000;
		
		/* get number of threads */
		MPI_Comm_size(MPI_COMM_WORLD, &size);
	
		/* string for all messages */
		char receivedMessages[255];
                MPI_Status status;

                for(int i = 1; i < size; i++)
		{
                        MPI_Recv(receivedMessages, 255, MPI_CHAR, i, 0, MPI_COMM_WORLD, &status);
			printf("%s", receivedMessages);
			int last = atoi(&receivedMessages[strlen(receivedMessages)-7]);
			if (last < min_usec) {
        			min_usec = last;
      			}
			
                }
		printf("%d\n", min_usec); 
	} else {	
                MPI_Send(getMessage(), 255, MPI_CHAR, 0, 0, MPI_COMM_WORLD);
	}

	/* wait for output */
	if(MPI_Barrier(MPI_COMM_WORLD) != MPI_SUCCESS){
		printf("Error from MPI_Barrier");
	}		
	printf("Rang %d beendet jetzt!\n", rank);
	MPI_Finalize();
	return 0;

}





