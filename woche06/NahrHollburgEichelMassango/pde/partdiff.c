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
#include <pthread.h>

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
typedef void * (*THREADFUNCPTR)(void *);

struct calculation_arguments
{
	uint64_t N;            /* number of spaces between lines (lines=N+1) */
	uint64_t num_matrices; /* number of matrices */
	double   h;            /* length of a space between two lines */
	double*  M;            /* two matrices with real values */
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

	arguments->M = allocateMemory(arguments->num_matrices * (N + 1) * (N + 1) * sizeof(double));
}

/* ************************************************************************ */
/* initMatrices: Initialize matrix/matrices and some global variables       */
/* ************************************************************************ */
static void
initMatrices(struct calculation_arguments* arguments, struct options const* options)
{
	uint64_t g, i, j; /* local variables for loops */

	uint64_t const N = arguments->N;
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
			for (i = 0; i <= N; i++)
			{
				Matrix[g][i][0] = 1.0 - (h * i);
				Matrix[g][i][N] = h * i;
				Matrix[g][0][i] = 1.0 - (h * i);
				Matrix[g][N][i] = h * i;
			}

			Matrix[g][N][0] = 0.0;
			Matrix[g][0][N] = 0.0;
		}
	}
}

/* struct for thread parameters */
struct thread_arguments{
	int thread_id;

	/* personal rows */
        int row_start;
        int row_end;

        int m1;
        int m2;
        double star;
        int residuum;
        double pih;
        double fpisin;

	/* so there is no need for an options struct */
        int inf_func;
        int termination;

        int term_iteration;
	int N;

	/* typedef will be done later */
	double*** Matrix;
        };

/* ************************************************************************ */
/* thread_ calculate: method used by threads                                */
/* ************************************************************************ */

void *thread_calculate(void *passed_arguments)
{	
	
	struct thread_arguments *arguments;
	arguments = (struct thread_arguments *) passed_arguments;

        int i, j;
        int m1 = arguments->m1;
        int m2 = arguments->m2;
        int fpisin = arguments->fpisin;
        double pih = arguments->pih;
        int inf_func = arguments->inf_func;
        int const N = arguments->N;
        int term_iteration = arguments->term_iteration;
        int termination = arguments->termination;
        double residuum;
        double star;
        double maxresiduum = 0;
	typedef double(*matrix)[N + 1][N + 1];
        matrix Matrix =(matrix) arguments->Matrix;

	/* iterate over given rows */
        for(i = arguments->row_start; i <= arguments->row_end; i++)
        {
                double fpisin_i = 0;

                if(inf_func == FUNC_FPISIN)
                {
                        fpisin_i = fpisin * sin(pih * (double)i);
                }

		/* over all columns */
                for(j = 1; j < N; j++)
                {
                        star = 0.25 * (Matrix[m2][i - 1][j] + Matrix[m2][i][j - 1] + Matrix[m2][i][j + 1] + Matrix[m2][i + 1][j]);

                        if (inf_func == FUNC_FPISIN)
                        {
                                star += fpisin_i * sin(pih * (double)j);
                        }

                        if (termination == TERM_PREC || term_iteration == 1)
                        {
                                residuum    = Matrix[m2][i][j] - star;
                                residuum    = fabs(residuum);
                                maxresiduum = (residuum < maxresiduum) ? maxresiduum : residuum;
                        }


                        Matrix[m1][i][j] = star;

                }

        }
	/* save residuum */
        arguments->residuum = maxresiduum;


	return NULL;
}

/* ************************************************************************ */
/* calculate: solves the equation                                           */
/* ************************************************************************ */
static void
calculate(struct calculation_arguments const* arguments, struct calculation_results* results, struct options const* options)
{
        uint64_t    i;      /* local variables for loops */
        int    m1, m2;      /* used as indices for old and new matrices */
        double maxresiduum; /* maximum residuum value of a slave in iteration */

        int const    N = arguments->N;
        double const h = arguments->h;

        double pih    = 0.0;
        double fpisin = 0.0;

        int term_iteration = options->term_iteration;

        typedef double(*matrix)[N + 1][N + 1];

        matrix Matrix = (matrix)arguments->M;


	/* gives each thread its own struct and parameters to prevent race conditions */
	struct thread_arguments t_arguments[options->number];

	/* thread array for easy adressing */
	pthread_t threads[options->number];

	/* determines how much rows each thread gets to work on */	
	int row_size = (N - 1) / options->number;
	
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

		/* create threads */
		for(i = 0; i < options->number; i++)
		{
			t_arguments[i].thread_id = i;

	
			if(i == 0)
			{	
				/* first has to start with one */
				t_arguments[i].row_start = 1;
			} else
			{
				t_arguments[i].row_start = row_size * i;				
			}

			if(i != options->number - 1)
			{	
				/* start of next thread - 1 */
				t_arguments[i].row_end = row_size *(i + 1) - 1;
			} else{
				/* last gets all remaining rows */
				t_arguments[i].row_end = N - 1;
			} 
		
			t_arguments[i].m1 = m1;
			t_arguments[i].m2 = m2;
			t_arguments[i].pih = pih;
			t_arguments[i].fpisin = fpisin;
			t_arguments[i].inf_func = options->inf_func;
			t_arguments[i].termination = options->termination;;
			t_arguments[i].N = N;
			t_arguments[i].Matrix = (double***)  Matrix;
					
			/* catch errors */
			int rc;
			rc = pthread_create(&threads[i], NULL, thread_calculate, &t_arguments[i]);
			if (rc){
        			printf("ERROR; return code from pthread_create() is %d\n", rc);
         			exit(-1);
       			}	
		}
	
		/* join threads and check thread residuums for maximum */	
		for(i = 0; i < options->number; i++){
			pthread_join(threads[i], NULL);
			maxresiduum = (t_arguments[i].residuum < maxresiduum) ? maxresiduum : t_arguments[i].residuum;
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

/* ************************************************************************ */
/*  main                                                                    */
/* ************************************************************************ */
int
main(int argc, char** argv)
{
	struct options               options;
	struct calculation_arguments arguments;
	struct calculation_results   results;

	askParams(&options, argc, argv);

	initVariables(&arguments, &results, &options);

	allocateMatrices(&arguments);
	initMatrices(&arguments, &options);

	gettimeofday(&start_time, NULL);
	calculate(&arguments, &results, &options);
	gettimeofday(&comp_time, NULL);

	displayStatistics(&arguments, &results, &options);
	displayMatrix(&arguments, &results, &options);

	freeMatrices(&arguments);

	return 0;
}
