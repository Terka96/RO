#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>
#include <time.h>
#include "struct_const.h"
#include "opornik.hpp"

int main(int argc, char **argv)
{
    srand(time(NULL));
    Opornik root = Opornik(0,NULL);
    root.makeKids(16);

    int rank, size;

    MPI_Init(&argc, &argv);

    MPI_Comm_size(MPI_COMM_WORLD, &size);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    //Populate tree with threads
    root.run(rank);

    MPI_Finalize();
}
