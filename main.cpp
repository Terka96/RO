#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>
#include "opornik.hpp"
#include "constants.hpp"

Opornik *me;

int main(int argc, char **argv)
{
    MPI_Init(&argc, &argv);
    me = new Opornik();
    me->run();
    delete me;
    MPI_Finalize();
}
