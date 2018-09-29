#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>
#include "opornik.hpp"
#include "constants.hpp"

Opornik* me;

int main (int argc, char** argv) {
	int provided;
	MPI_Init_thread (&argc, &argv, MPI::THREAD_MULTIPLE, &provided);
	// https://stackoverflow.com/questions/14836560/thread-safety-of-mpi-send-using-threads-created-with-stdasync/14837206#14837206
	// https://stackoverflow.com/questions/16661888/calling-mpi-functions-from-multiple-threads
	if (provided < MPI_THREAD_MULTIPLE) {
		printf ("ERROR: The MPI library does not have full thread support\n");
		MPI_Abort (MPI_COMM_WORLD, 1);
	}
	try {
		me = new Opornik();
		me->run();
		delete me;
	}
	catch (std::exception& e) {
		std::cout << e.what();
	}
	MPI_Finalize();
}
