/*
 * partdiff - PDE solver for Gauß-Seidel and Jacobi methods
 * Copyright (C) 1997 Thomas Ludwig
 * Copyright (C) 1997 Thomas A. Zochler
 * Copyright (C) 1997 Andreas C. Schmidt
 * Copyright (C) 2007-2010 Julian M. Kunkel
 * Copyright (C) 2010-2021 Michael Kuhn
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/* ************************************************************************ */
/* Include standard header file.                                            */
/* ************************************************************************ */
#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <math.h>
#include <malloc.h>
#include <string.h>
#include <sys/time.h>
#include <mpi.h>

/* ************* */
/* Some defines. */
/* ************* */

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define MAX_INTERLINES    10240
#define MAX_ITERATION     200000
#define MAX_THREADS       1024
#define METH_GAUSS_SEIDEL 1
#define METH_JACOBI       2
#define FUNC_F0           1
#define FUNC_FPISIN       2
#define TERM_PREC         1
#define TERM_ITER         2

struct calculation_arguments
{
	uint64_t N;            /* number of spaces between lines (lines=N+1) */
	uint64_t num_matrices; /* number of matrices */
	double   h;            /* length of a space between two lines */
	double*  M;            /* two matrices with real values */

    // speichert die Anzahl an zu berechnenden Zahlen für die Ränge
    uint64_t ranks;
    int row_start;
    int row_end;
};

struct calculation_results
{
	uint64_t m;
	uint64_t stat_iteration; /* number of current iteration */
	double   stat_precision; /* actual precision of all slaves in iteration */
};

struct options
{
	uint64_t number;         /* Number of threads */
	uint64_t method;         /* Gauss Seidel or Jacobi method of iteration */
	uint64_t interlines;     /* matrix size = interlines*8+9 */
	uint64_t inf_func;       /* inference function */
	uint64_t termination;    /* termination condition */
	uint64_t term_iteration; /* terminate if iteration number reached */
	double   term_precision; /* terminate if precision reached */

    // für Informationen über Ränge und Größe
    int rank;
    int size;
};

/* ************************************************************************ */
/* Global variables                                                         */
/* ************************************************************************ */

/* time measurement variables */
struct timeval start_time; /* time when program started */
struct timeval comp_time;  /* time when calculation completed */

static void
usage(char* name)
{
	printf("Usage: %s [num] [method] [lines] [func] [term] [prec/iter]\n", name);
	printf("\n");
	printf("  - num:       number of threads (1 .. %d)\n", MAX_THREADS);
	printf("  - method:    calculation method (1 .. 2)\n");
	printf("                 %1d: Gauß-Seidel\n", METH_GAUSS_SEIDEL);
	printf("                 %1d: Jacobi\n", METH_JACOBI);
	printf("  - lines:     number of interlines (0 .. %d)\n", MAX_INTERLINES);
	printf("                 matrixsize = (interlines * 8) + 9\n");
	printf("  - func:      interference function (1 .. 2)\n");
	printf("                 %1d: f(x,y) = 0\n", FUNC_F0);
	printf("                 %1d: f(x,y) = 2 * pi^2 * sin(pi * x) * sin(pi * y)\n", FUNC_FPISIN);
	printf("  - term:      termination condition ( 1.. 2)\n");
	printf("                 %1d: sufficient precision\n", TERM_PREC);
	printf("                 %1d: number of iterations\n", TERM_ITER);
	printf("  - prec/iter: depending on term:\n");
	printf("                 precision:  1e-4 .. 1e-20\n");
	printf("                 iterations:    1 .. %d\n", MAX_ITERATION);
	printf("\n");
	printf("Example: %s 1 2 100 1 2 100 \n", name);
}

static void
askParams(struct options* options, int argc, char** argv)
{
	int ret;

	if (argc < 7 || strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "-?") == 0)
	{
		usage(argv[0]);
		exit(0);
	}

	ret = sscanf(argv[1], "%" SCNu64, &(options->number));

	if (ret != 1 || !(options->number >= 1 && options->number <= MAX_THREADS))
	{
		usage(argv[0]);
		exit(1);
	}

	ret = sscanf(argv[2], "%" SCNu64, &(options->method));

	if (ret != 1 || !(options->method == METH_GAUSS_SEIDEL || options->method == METH_JACOBI))
	{
		usage(argv[0]);
		exit(1);
	}

	ret = sscanf(argv[3], "%" SCNu64, &(options->interlines));

	if (ret != 1 || !(options->interlines <= MAX_INTERLINES))
	{
		usage(argv[0]);
		exit(1);
	}

	ret = sscanf(argv[4], "%" SCNu64, &(options->inf_func));

	if (ret != 1 || !(options->inf_func == FUNC_F0 || options->inf_func == FUNC_FPISIN))
	{
		usage(argv[0]);
		exit(1);
	}

	ret = sscanf(argv[5], "%" SCNu64, &(options->termination));

	if (ret != 1 || !(options->termination == TERM_PREC || options->termination == TERM_ITER))
	{
		usage(argv[0]);
		exit(1);
	}

	if (options->termination == TERM_PREC)
	{
		ret = sscanf(argv[6], "%lf", &(options->term_precision));

		options->term_iteration = MAX_ITERATION;

		if (ret != 1 || !(options->term_precision >= 1e-20 && options->term_precision <= 1e-4))
		{
			usage(argv[0]);
			exit(1);
		}
	}
	else
	{
		ret = sscanf(argv[6], "%" SCNu64, &(options->term_iteration));

		options->term_precision = 0;

		if (ret != 1 || !(options->term_iteration >= 1 && options->term_iteration <= MAX_ITERATION))
		{
			usage(argv[0]);
			exit(1);
		}
	}
}

/* ************************************************************************ */
/* initVariables: Initializes some global variables                         */
/* ************************************************************************ */
static void
initVariables(struct calculation_arguments* arguments, struct calculation_results* results, struct options const* options)
{
	arguments->N            = (options->interlines * 8) + 9 - 1;
	arguments->num_matrices = (options->method == METH_JACOBI) ? 2 : 1;
	arguments->h            = 1.0 / arguments->N;

	results->m              = 0;
	results->stat_iteration = 0;
	results->stat_precision = 0;

    // Berechnung wie viele Zeilen welcher Rang berechnet
    int rest = (arguments->N+1) % options->size;
    arguments->ranks = (arguments->N+1) / options->size;

    // gleichmäßige Verteilung des Rests
    int offset = 0;
    for (int i = 0; i < options->rank; i++) {
        if (i < rest) {
            offset++;
        }
    }

    // Startzeile
    arguments->row_start = options->rank * arguments->ranks + offset;
    if (options->rank < rest) {
        offset++;
    }
    // Endzeile
    arguments->row_end = (options->rank + 1) * arguments->ranks + offset;

    // erster Rang
    if (options->rank > 0) {
        arguments->row_start--;
        arguments->ranks++;
    }

    // letzter Rang
    if (options->rank < (options->size-1)) {
        arguments->row_end++;
        arguments->ranks++;
    }

    // die Zeilen an die anderen Ränge verteilen
    if (options->rank < rest) {
        arguments->ranks++;
    }

}

/* ************************************************************************ */
/* freeMatrices: frees memory for matrices                                  */
/* ************************************************************************ */
static void
freeMatrices(struct calculation_arguments* arguments)
{
	free(arguments->M);
}

/* ************************************************************************ */
/* allocateMemory ()                                                        */
/* allocates memory and quits if there was a memory allocation problem      */
/* ************************************************************************ */
static void*
allocateMemory(size_t size)
{
	void* p;

	if ((p = malloc(size)) == NULL)
	{
		printf("Speicherprobleme! (%" PRIu64 " Bytes angefordert)\n", size);
		exit(1);
	}

	return p;
}

/* ************************************************************************ */
/* allocateMatrices: allocates memory for matrices                          */
/* ************************************************************************ */
static void
allocateMatrices(struct calculation_arguments* arguments)
{
	uint64_t const N = arguments->N;

    // nur so viel reservieren wie notwen
	arguments->M = allocateMemory(arguments->num_matrices * arguments->ranks * (N + 1) * (N + 1) * sizeof(double));
}

/* ************************************************************************ */
/* initMatrices: Initialize matrix/matrices and some global variables       */
/* ************************************************************************ */
static void
initMatrices(struct calculation_arguments* arguments, struct options const* options)
{
	uint64_t g, i, j; /* local variables for loops */

	uint64_t const N = arguments->N;
    uint64_t const ranks = arguments->ranks;
	double const   h = arguments->h;

	typedef double(*matrix)[N + 1][N + 1];

	matrix Matrix = (matrix)arguments->M;

	/* initialize matrix/matrices with zeros */
	for (g = 0; g < arguments->num_matrices; g++)
	{
		for (i = 0; i <= N; i++)
		{
			for (j = 0; j <= N; j++)
			{
				Matrix[g][i][j] = 0.0;
			}
		}
	}

	/* initialize borders, depending on function (function 2: nothing to do) */
	if (options->inf_func == FUNC_F0)
	{
		for (g = 0; g < arguments->num_matrices; g++)
		{
			for (i = 0; i < ranks; i++)
			{
                // Erste Zeile wird anders befüllt
                if (options->rank == 0) {
                    for (j = 0; j <= N; j++) {
				        Matrix[g][0][j] = 1.0 - (h * j);
                    }
                }

                // letzte Zeile wird anders befüllt
                if (options->rank == options->size-1) {
                    for (j = 0; j <= N; j++) {
                        Matrix[g][ranks - 1][j] = h * j;
                    }
                }


                Matrix[g][i][0] = 1.0 - (h * (i + arguments->row_start));
				Matrix[g][i][N] = h * (i + arguments->row_start);
				
			}

			Matrix[g][ranks - 1][0] = 0.0;
			Matrix[g][0][N] = 0.0;
		}
	}
}

/* ************************************************************************ */
/* calculate: solves the equation for Jacobi                                */
/* ************************************************************************ */
static void
MPI_jacobi_calculate(struct calculation_arguments const* arguments, struct calculation_results* results, struct options const* options)
{
    // für nachfolgendes hin- und herreichen der Zeilen zwischen den Ränken
    MPI_Request upper[2];
    MPI_Request lower[2];

	int    i, j;        /* local variables for loops */
	int    m1, m2;      /* used as indices for old and new matrices */
	double star;        /* four times center value minus 4 neigh.b values */
	double residuum;    /* residuum of current iteration */
	double maxresiduum; /* maximum residuum value of a slave in iteration */

	int const    N = arguments->N;
    int const ranks = arguments->ranks;
	double const h = arguments->h;

	double pih    = 0.0;
	double fpisin = 0.0;

	int term_iteration = options->term_iteration;

	typedef double(*matrix)[N + 1][N + 1];

	matrix Matrix = (matrix)arguments->M;

	/* initialize m1 and m2 depending on algorithm */
	if (options->method == METH_JACOBI)
	{
		m1 = 0;
		m2 = 1;
	}
	else
	{
		m1 = 0;
		m2 = 0;
	}

	if (options->inf_func == FUNC_FPISIN)
	{
		pih    = M_PI * h;
		fpisin = 0.25 * (2 * M_PI * M_PI) * h * h;
	}

	while (term_iteration > 0)
	{
		maxresiduum = 0;

		/* over all rows */
		for (i = 1; i < ranks; i++)
		{
			double fpisin_i = 0.0;

			if (options->inf_func == FUNC_FPISIN)
			{
				fpisin_i = fpisin * sin(pih * (double)i);
			}

			/* over all columns */
			for (j = 1; j < N; j++)
			{
				star = 0.25 * (Matrix[m2][i - 1][j] + Matrix[m2][i][j - 1] + Matrix[m2][i][j + 1] + Matrix[m2][i + 1][j]);

				if (options->inf_func == FUNC_FPISIN)
				{
					star += fpisin_i * sin(pih * (double)j);
				}

				if (options->termination == TERM_PREC || term_iteration == 1)
				{
					residuum    = Matrix[m2][i][j] - star;
					residuum    = fabs(residuum);
					maxresiduum = (residuum < maxresiduum) ? maxresiduum : residuum;
				}

				Matrix[m1][i][j] = star;
			}
		}

        // isend, irecv = non-blocking events (wichtig für parallelisierung)

        // Zeilen zwischen den Rängen hin- und herschieben

        // Außer dem letzten Rang geben alle ihre letzte Zeile zum nächsten Rang
        if (options->rank != options->size - 1) {
            MPI_Isend(Matrix[ranks - 2], N + 1, MPI_DOUBLE, options->rank + 1, 0, MPI_COMM_WORLD, &lower[0]);
            MPI_Irecv(Matrix[ranks - 1], N + 1, MPI_DOUBLE, options->rank + 1, 0, MPI_COMM_WORLD, &upper[1]);
        }
        // oberste Zeile auch an oberen Rang schieben
        if (options->rank != 0) {
            MPI_Isend(Matrix[1], N + 1, MPI_DOUBLE, options->rank - 1, 0, MPI_COMM_WORLD, &upper[0]);
            MPI_Irecv(Matrix[0], N + 1, MPI_DOUBLE, options->rank - 1, 0, MPI_COMM_WORLD, &lower[1]);
        }

        // Warten bis alles da ist
        if (options->rank != 0) {
            MPI_Wait(&lower[1], NULL);
            MPI_Wait(&upper[0], NULL);
        }
        if (options->rank != options->size - 1) {
            MPI_Wait(&lower[0], NULL);
            MPI_Wait(&upper[1], NULL);
        }

		results->stat_iteration++;
		// results->stat_precision = maxresiduum;
        MPI_Allreduce(&maxresiduum, &(results->stat_precision), 1, MPI_DOUBLE, MPI_MAX, MPI_COMM_WORD);

		/* exchange m1 and m2 */
		i  = m1;
		m1 = m2;
		m2 = i;

		/* check for stopping calculation depending on termination method */
		if (options->termination == TERM_PREC)
		{
			if (maxresiduum < options->term_precision)
			{
				term_iteration = 0;
			}
		}
		else if (options->termination == TERM_ITER)
		{
			term_iteration--;
		}

        MPI_Barrier(MPI_COMM_WORLD);
	}

	results->m = m2;
}

/* ************************************************************************ */
/* calculate: solves the equation                                           */
/* ************************************************************************ */
static void
calculate(struct calculation_arguments const* arguments, struct calculation_results* results, struct options const* options)
{
	int    i, j;        /* local variables for loops */
	int    m1, m2;      /* used as indices for old and new matrices */
	double star;        /* four times center value minus 4 neigh.b values */
	double residuum;    /* residuum of current iteration */
	double maxresiduum; /* maximum residuum value of a slave in iteration */

	int const    N = arguments->N;
	double const h = arguments->h;

	double pih    = 0.0;
	double fpisin = 0.0;

	int term_iteration = options->term_iteration;

	typedef double(*matrix)[N + 1][N + 1];

	matrix Matrix = (matrix)arguments->M;

	/* initialize m1 and m2 depending on algorithm */
	if (options->method == METH_JACOBI)
	{
		m1 = 0;
		m2 = 1;
	}
	else
	{
		m1 = 0;
		m2 = 0;
	}

	if (options->inf_func == FUNC_FPISIN)
	{
		pih    = M_PI * h;
		fpisin = 0.25 * (2 * M_PI * M_PI) * h * h;
	}

	while (term_iteration > 0)
	{
		maxresiduum = 0;

		/* over all rows */
		for (i = 1; i < N; i++)
		{
			double fpisin_i = 0.0;

			if (options->inf_func == FUNC_FPISIN)
			{
				fpisin_i = fpisin * sin(pih * (double)i);
			}

			/* over all columns */
			for (j = 1; j < N; j++)
			{
				star = 0.25 * (Matrix[m2][i - 1][j] + Matrix[m2][i][j - 1] + Matrix[m2][i][j + 1] + Matrix[m2][i + 1][j]);

				if (options->inf_func == FUNC_FPISIN)
				{
					star += fpisin_i * sin(pih * (double)j);
				}

				if (options->termination == TERM_PREC || term_iteration == 1)
				{
					residuum    = Matrix[m2][i][j] - star;
					residuum    = fabs(residuum);
					maxresiduum = (residuum < maxresiduum) ? maxresiduum : residuum;
				}

				Matrix[m1][i][j] = star;
			}
		}

		results->stat_iteration++;
		results->stat_precision = maxresiduum;

		/* exchange m1 and m2 */
		i  = m1;
		m1 = m2;
		m2 = i;

		/* check for stopping calculation depending on termination method */
		if (options->termination == TERM_PREC)
		{
			if (maxresiduum < options->term_precision)
			{
				term_iteration = 0;
			}
		}
		else if (options->termination == TERM_ITER)
		{
			term_iteration--;
		}
	}

	results->m = m2;
}

/* ************************************************************************ */
/*  displayStatistics: displays some statistics about the calculation       */
/* ************************************************************************ */
static void
displayStatistics(struct calculation_arguments const* arguments, struct calculation_results const* results, struct options const* options)
{
	int    N    = arguments->N;
	double time = (comp_time.tv_sec - start_time.tv_sec) + (comp_time.tv_usec - start_time.tv_usec) * 1e-6;

	printf("Berechnungszeit:    %f s\n", time);
	printf("Speicherbedarf:     %f MiB\n", (N + 1) * (N + 1) * sizeof(double) * arguments->num_matrices / 1024.0 / 1024.0);
	printf("Berechnungsmethode: ");

	if (options->method == METH_GAUSS_SEIDEL)
	{
		printf("Gauß-Seidel");
	}
	else if (options->method == METH_JACOBI)
	{
		printf("Jacobi");
	}

	printf("\n");
	printf("Interlines:         %" PRIu64 "\n", options->interlines);
	printf("Stoerfunktion:      ");

	if (options->inf_func == FUNC_F0)
	{
		printf("f(x,y) = 0");
	}
	else if (options->inf_func == FUNC_FPISIN)
	{
		printf("f(x,y) = 2 * pi^2 * sin(pi * x) * sin(pi * y)");
	}

	printf("\n");
	printf("Terminierung:       ");

	if (options->termination == TERM_PREC)
	{
		printf("Hinreichende Genaugkeit");
	}
	else if (options->termination == TERM_ITER)
	{
		printf("Anzahl der Iterationen");
	}

	printf("\n");
	printf("Anzahl Iterationen: %" PRIu64 "\n", results->stat_iteration);
	printf("Norm des Fehlers:   %e\n", results->stat_precision);
	printf("\n");
}

/****************************************************************************/
/** Beschreibung der Funktion displayMatrix:                               **/
/**                                                                        **/
/** Die Funktion displayMatrix gibt eine Matrix                            **/
/** in einer "ubersichtlichen Art und Weise auf die Standardausgabe aus.   **/
/**                                                                        **/
/** Die "Ubersichtlichkeit wird erreicht, indem nur ein Teil der Matrix    **/
/** ausgegeben wird. Aus der Matrix werden die Randzeilen/-spalten sowie   **/
/** sieben Zwischenzeilen ausgegeben.                                      **/
/****************************************************************************/
static void
displayMatrix(struct calculation_arguments* arguments, struct calculation_results* results, struct options* options)
{
	int x, y;

	int const interlines = options->interlines;
	int const N          = arguments->N;

	typedef double(*matrix)[N + 1][N + 1];

	matrix Matrix = (matrix)arguments->M;

	printf("Matrix:\n");

	for (y = 0; y < 9; y++)
	{
		for (x = 0; x < 9; x++)
		{
			printf("%7.4f", Matrix[results->m][y * (interlines + 1)][x * (interlines + 1)]);
		}

		printf("\n");
	}

	fflush(stdout);
}

/*
 * rank und size sind der MPI-Rang und die Größe des Kommunikators
 * from und to stehen für den globalen(!) Bereich von Zeilen für die dieser Prozess zuständig ist
 *
 * Beispiel mit 9 Matrixzeilen und 4 Prozessen:
 * - Rang 0 is verantwortlich für Zeilen 1-2, Rang 1 für 3-4, Rang 2 für 5-6 und Rang 3 für 7
 * - Zeilen 0 und 8 sind nicht inkludiert, weil sie nicht berechnet werden
 * - Jeder Prozess speichert zwei Randzeilen in seiner Matrix
 * - Zum Beispiel: Rang 2 hat vier Zeilen 0-3 aber berechnet nur 1-2 weil 0 und 3 beide Randzeilen für andere Prozesse sind;
 *   Rang 2 ist daher verantwortlich für die globalen Zeilen 5-6
*/
static void
displayMatrixMpi(struct calculation_arguments* arguments, struct calculation_results* results, struct options* options, int rank, int size, int from, int to)
{
  int const elements = 8 * options->interlines + 9;

  int x, y;

  typedef double(*matrix)[to - from + 3][arguments->N + 1];
  matrix Matrix = (matrix)arguments->M;
  int m = results->m;

  MPI_Status status;

  // Die erste Zeile gehört zu Rang 0
  if (rank == 0) {
    from--;
  }

  // Die letzte Zeile gehört zu Rang (size - 1)
  if (rank == size - 1) {
    to++;
  }

  if (rank == 0) {
    printf("Matrix:\n");
  }

  for (y = 0; y < 9; y++)
  {
    int line = y * (options->interlines + 1);

    if (rank == 0)
    {
      // Prüfen, ob die Zeile zu Rang 0 gehört
      if (line < from || line > to)
      {
        // Der Tag wird genutzt, um Zeilen in der richtigen Reihenfolge zu empfangen
        // Matrix[m][0] wird überschrieben, da die Werte nicht mehr benötigt werden
        MPI_Recv(Matrix[m][0], elements, MPI_DOUBLE, MPI_ANY_SOURCE, 42 + y, MPI_COMM_WORLD, &status);
      }
    }
    else
    {
      if (line >= from && line <= to)
      {
        // Zeile an Rang 0 senden, wenn sie dem aktuellen Prozess gehört
        // (line - from + 1) wird genutzt, um die lokale Zeile zu berechnen
        MPI_Send(Matrix[m][line - from + 1], elements, MPI_DOUBLE, 0, 42 + y, MPI_COMM_WORLD);
      }
    }

    if (rank == 0)
    {
      for (x = 0; x < 9; x++)
      {
        int col = x * (options->interlines + 1);

        if (line >= from && line <= to)
        {
          // Diese Zeile gehört zu Rang 0
          printf("%7.4f", Matrix[m][line][col]);
        }
        else
        {
          // Diese Zeile gehört zu einem anderen Rang und wurde weiter oben empfangen
          printf("%7.4f", Matrix[m][0][col]);
        }
      }

      printf("\n");
    }
  }

  fflush(stdout);
}

/* ************************************************************************ */
/*  main                                                                    */
/* ************************************************************************ */
int
main(int argc, char** argv)
{
	struct options               options;
	struct calculation_arguments arguments;
	struct calculation_results   results;

    // Initialisierung von rank und size
    options.size = -1;
    options.rank = -1;

    // MPI Kram initialisieren
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &(options.rank));
    MPI_Comm_size(MPI_COMM_WORLD, &(options.size));

	askParams(&options, argc, argv);

	initVariables(&arguments, &results, &options);

	allocateMatrices(&arguments);
	initMatrices(&arguments, &options);

	gettimeofday(&start_time, NULL);
    if (options.method == METH_JACOBI) {
        MPI_jacobi_calculate(&arguments, &results, &options);
    } else {
        calculate(&arguments, &results, &options);
    }
	gettimeofday(&comp_time, NULL);

    if (options.rank <= 0) {
	    displayStatistics(&arguments, &results, &options);
    }

	// displayMatrix(&arguments, &results, &options);
    if (options.method == METH_JACOBI) {
        displayMatrixMpi(&arguments, &results, &options, options.rank, options.size, arguments.row_start + 1, arguments.row_end - 1);
    } else {
        displayMatrix(&arguments, &results, &options);
    }

    MPI_Finalize();

	freeMatrices(&arguments);

	return 0;
}