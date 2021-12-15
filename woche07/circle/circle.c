#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <mpi.h>


// der Prozess nach dem Prozess mit dem höchsten Rang, ist der Prozess mit
// Rang 0
int upper_process(int rank, int size) {
	if (rank + 1 < size) {
		return rank + 1;
	} 
	return 0;
}

// der Prozess vor dem Prozess 0 ist der Prozess size - 1
int lower_process(int rank, int size) {
	if (rank > 0) {
		return rank - 1;
	}
	return size - 1;
}


int*
init(int N)
{
	// TODO
	int* buf = (int*)malloc(sizeof(int) * N);

	srand(time(NULL));

	for (int i = 0; i < N; i++)
	{
		// Do not modify "% 25"
		buf[i] = rand() % 25;
	}

	return buf;
}


int*
circle(int* buf, int rank, int size, int* data_size, int* rank_size)
{
	MPI_Status status;
	
	// größe des Buffers an den nächsten Prozess senden & vom vorgänger Prozess erhalten
	MPI_Send(data_size, 1, MPI_INT, (int *)upper_process(rank, size), 0, MPI_COMM_WORLD);
	MPI_Recv(rank_size, 1, MPI_INT, (int *)lower_process(rank, size), 0, MPI_COMM_WORLD, &status);

	// Buffer an den nächsten Prozess senden & vom Vorgänger Prozess erhalten
	MPI_Send(buf, *data_size, MPI_INT, (int *)upper_process(rank, size), 0, MPI_COMM_WORLD);
	MPI_Recv(buf, *rank_size, MPI_INT, (int *)lower_process(rank, size), 0, MPI_COMM_WORLD, &status);

	// Größe speichern
	(*data_size) = *rank_size;

	// Synchronisieren
	if (MPI_Barrier(MPI_COMM_WORLD) != MPI_SUCCESS) {
		printf("Error from MPI_Barrier");
	}

	return buf;
}

int
main(int argc, char** argv)
{
	int  N;
	int  rank, size;
	int* buf;
	int termination;

	if (argc < 2)
	{
		if (rank == 0) {
			printf("Arguments error!\nPlease specify a buffer size.\n");
			return EXIT_FAILURE;
		}
	}

	MPI_Init(&argc, &argv);
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	MPI_Comm_size(MPI_COMM_WORLD, &size);

	// Array length
	N   = atoi(argv[1]);
	

	// wenn wir mehr Prozesse als Daten haben
	if (N < size) {
		if (rank == 0) {
			printf("Error: The data, wich will be circeled must be larger than the amount of processes");
		}
		MPI_Finalize();
		return EXIT_FAILURE;
	}


	// Daten in einzelne Blöcke unterteilen
	double data_width = (double)N / (double)size;
	int start = data_width * rank;
	int end = data_width * (rank + 1);
	int data_size = end - start;
	int previous_size = 0;

	buf = init(data_size + 1);
	MPI_Status status;

	// Abbruchskriterium an den letzten Prozess senden
	if (rank == 0) {
		MPI_Send(buf, 1 , MPI_INT, size - 1, 0, MPI_COMM_WORLD);
	} else if (rank == size - 1) {
		MPI_Recv(&termination, 1, MPI_INT, 0, 0, MPI_COMM_WORLD, &status);
	}



	if (rank == 0) {
		printf("\nBEFORE\n");

		for (int i = 0; i < data_size; i++)
		{
			printf("rank %d: %d\n", rank, buf[i]);
		}

		// alle Blöcke der anderen Prozesse ausgeben
		int buf_tmp[data_size + 1];
		for (int i = 1; i < size; i++) {
			int rank_size;
			MPI_Recv(&rank_size, 1, MPI_INT, i, 0, MPI_COMM_WORLD, &status);
			MPI_Recv(buf_tmp, rank_size, MPI_INT, i, 0, MPI_COMM_WORLD, &status);

			// Ausgabe
			for (int j = 0; j < rank_size; j++) {
				printf("rank %d: %d\n", i, buf_tmp[j]);
			}
		}

	} else {
		// Block Daten senden, für Ausgabe (siehe if (rank == 0),...)
		MPI_Send(&data_size, 1, MPI_INT, 0, 0, MPI_COMM_WORLD);
		MPI_Send(buf, data_size, MPI_INT, 0, 0, MPI_COMM_WORLD);
	}

	// Synchronisieren, damit alle Prozesse weitermachen, nachdem alles geprinted wurde
	if (MPI_Barrier(MPI_COMM_WORLD) != MPI_SUCCESS) {
		printf("Error from MPI_Barrier");
	}

	int circle_run = 1;
	// circlen bis Terminierungsbedingung erreicht ist
	while(circle_run) {
		buf = circle(buf, rank, size, &data_size, &previous_size);

		// Schauen, ob terminiert werden kann mittel termination value
		if (rank == size - 1) {
			circle_run = (buf[0] != termination);
		
			for (int i = 0; i < size - 1; i++) {
				MPI_Send(&circle_run, 1, MPI_INT, i, 0, MPI_COMM_WORLD);
			}
		} else {
			MPI_Recv(&circle_run, 1, MPI_INT, size - 1, 0, MPI_COMM_WORLD, &status);
		}

		if (MPI_Barrier(MPI_COMM_WORLD) != MPI_SUCCESS) {
			printf("Error from MPI_Barrier");
		}

	}

	

	if (rank == 0) {
		printf("\nAFTER\n");

		for (int i = 0; i < data_size; i++)
		{
			printf("rank %d: %d\n", rank, buf[i]);
		}

		// alle Blöcke der anderen Prozesse ausgeben
		int buf_tmp[data_size + 1];
		for (int i = 1; i < size; i++) {
			int rank_size;
			MPI_Recv(&rank_size, 1, MPI_INT, i, 0, MPI_COMM_WORLD, &status);
			MPI_Recv(buf_tmp, rank_size, MPI_INT, i, 0, MPI_COMM_WORLD, &status);

			// Ausgabe
			for (int j = 0; j < rank_size; j++) {
				printf("rank %d: %d\n", i, buf_tmp[j]);
			}
		}

	} else {
		// Block Daten senden, für Ausgabe (siehe if (rank == 0),...)
		MPI_Send(&data_size, 1, MPI_INT, 0, 0, MPI_COMM_WORLD);
		MPI_Send(buf, data_size, MPI_INT, 0, 0, MPI_COMM_WORLD);
	}

	// Synchronisieren, damit alle Prozesse weitermachen, nachdem alles geprinted wurde
	if (MPI_Barrier(MPI_COMM_WORLD) != MPI_SUCCESS) {
		printf("Error from MPI_Barrier");
	}

	free(buf);
	MPI_Finalize();

	return EXIT_SUCCESS;
}
