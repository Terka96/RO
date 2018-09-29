#include "opornik.hpp"
#include <mpi.h>
#include <stdio.h>
#include <time.h>
#include <stdarg.h>
#include <stdio.h>
#include <unistd.h>
#include <string>
#include <iostream>
#include <thread>
#include "constants.hpp"

#define DEBUGLOG 1

int inline Opornik::debug_log (const char* format, ...) {
	int res;
	#ifdef DEBUGLOG
	printf ("Node %d [%d]:", id, clock);
	va_list args;
	va_start (args, format);
	res = vprintf (format, args);
	va_end (args);
	#endif
	return res;
};

Opornik::Opornik() {
	MPI_Comm_size (MPI_COMM_WORLD, &size);
	clock = 0;
	status = idle;
	acceptorToken = notAcceptor;
	busyResource = NONE;
	meeting = NONE;
	duringMyMeeting = false;
	meetingTimeout = 0;
	tagGeneratorCounter = 0;
	participantsOnMymeeting = 0;
	for (int i = 0; i < NUM_CONSPIR; i++) {
		knownMeetings[i].participants = 0;
		knownMeetings[i].priority = 0;
		for (int j = 0; j < NUM_ACCEPTORS; j++) {
			knownMeetings[i].acceptors[j] = NONE;
		}
	}
	makeTree();
	MPI_Barrier (MPI_COMM_WORLD);
	distributeAcceptorsAndResources();
	MPI_Barrier (MPI_COMM_WORLD);
	introduce();
	findLowestKids();
}

void Opornik::makeTree() {
	int rank;
	MPI_Comm_rank (MPI_COMM_WORLD, &rank);
	id = rank;
	srand (time (NULL) + id * size); //give every process (even on same machine) different random seed
	if (id == 0) {
		parent = NONE;
		makeKids (size - 1);
	}
	else {
		MPI_Status mpi_status;
		order_makekids order;
		MPI_Recv (&order, 3 + MAX_CHILDREN, MPI_INT, MPI_ANY_SOURCE, ORDER_MAKEKIDS, MPI_COMM_WORLD, &mpi_status);
		parent = order.parent;
		for (int i = 0; i < MAX_CHILDREN; i++)
			if (order.neighbors[i] != NONE && order.neighbors[i] != id) {
				neighbors.push_back (order.neighbors[i]);
			}
		makeKids (order.count);
	}
}
void Opornik::distributeAcceptorsAndResources() {
	if (id == 0) {
		init_resources* table = new init_resources[size];
		for (int i = 0; i < size; i++) {
			table[i].acceptorTokenId = NONE;
			table[i].resourceCount = 0;
		}
		for (int i = 0; i < NUM_ACCEPTORS; i++) {
			int randValue = rand() % size;
			if (table[randValue].acceptorTokenId != NONE) {
				continue;
			}
			table[randValue].acceptorTokenId = i;
		}
		for (int i = 0; i < NUM_RESOURCES; i++) {
			int randValue = rand() % size;
			/*practically it isn't important if resource is book or dvd, but we can code that information in resource id*/
			if (rand() % 2) {
				table[randValue].resourceIds[table[randValue].resourceCount] = i * 2;
			}
			else {
				table[randValue].resourceIds[table[randValue].resourceCount] = i * 2 + 1;
			}
			table[randValue].resourceCount++;
		}
		for (int i = 1; i < size; i++) {
			//It must be blocking send because after send we are deleting buffer
			MPI_Send (&table[i], 3 + table[i].resourceCount, MPI_INT, i, INIT_RESOURES, MPI_COMM_WORLD);
		}
		if (table[0].acceptorTokenId != notAcceptor) {
			acceptorToken = static_cast<acceptor_enum> (table[0].acceptorTokenId);
		}
		for (int j = 0; j < table[0].resourceCount; j++) {
			resources.push_back (table[0].resourceIds[j]);
		}
		delete[] table;
	}
	else {
		init_resources init;
		MPI_Recv (&init, 3 + NUM_RESOURCES, MPI_INT, 0, INIT_RESOURES, MPI_COMM_WORLD, NULL);
		if (init.acceptorTokenId != notAcceptor) {
			acceptorToken = static_cast<acceptor_enum> (init.acceptorTokenId);
			acceptorInfo = {init.acceptorTokenId, 0, 0};
		}
		for (int i = 0; i < init.resourceCount; i++) {
			resources.push_back (init.resourceIds[i]);
		}
	}
}

void Opornik::makeKids (int count) {
	int grandchildrenCount = 0;
	if (count > 0) {
		int max_childs = (count < MAX_CHILDREN) ? count : MAX_CHILDREN;
		int rand_childs = (random() % max_childs) + 1;
		grandchildrenCount = count - rand_childs;
		//WARNING: STUPID STATIC CLEAR
		int grandchildrenInNodes[MAX_CHILDREN] = {0, 0, 0, 0};
		int childrenNodes[MAX_CHILDREN] = {NONE, NONE, NONE, NONE};
		//Distribution of grandchildren
		for (int i = 0; i < grandchildrenCount; i++) {
			grandchildrenInNodes[random() % rand_childs]++;
		}
		//calculating children nodes ids
		childrenNodes[0] = id + 1;
		for (int i = 1; i < rand_childs; i++) {
			childrenNodes[i] = childrenNodes[i - 1] + grandchildrenInNodes[i - 1] + 1;
		}
		order_makekids order;
		order.parent = id;
		for (int i = 0; i < MAX_CHILDREN; i++) {
			order.neighbors[i] = childrenNodes[i];
		}
		for (int i = 0; i < rand_childs; i++) {
			order.count = grandchildrenInNodes[i];
			debug_log ("Sending order(%d) to node %d\n", grandchildrenInNodes[i], childrenNodes[i]);
			Ibsend (&order, 3 + MAX_CHILDREN, childrenNodes[i], ORDER_MAKEKIDS);
			children.push_back (childrenNodes[i]);
		}
	}
}

// nie wlicza się do zegarów lamporta. Liczenie zaczyna się od właściwego startu symulacji RO
void Opornik::findLowestKids() {
	// inicjalizujemy zmienne, ktore powiedza nam jak wysokie jest drzewo
	int root = 0, level = 0, maxLevel = 0;
	int counter[NUM_CONSPIR + 1] = {}; // tablica przechowuje info o tym, ile jest opornikow o danej wysokosci
	int tempCounter[NUM_CONSPIR + 1] = {};
	lowest = false; // Na poczatku nie podejrzewamy nikogo
	sameLevelNodes = 0;
	if (id == 0) {
		for (int i = 0; i < children.size(); i++) {
			// Wysyłamy wszystkim dzieciom
			Ibsend (&root, 1, children[i], TAG_FIND_LOWEST0);
		}
		// Otrzymujemy wysokość drzewa
		for (int i = 0; i < children.size(); i++) {
			MPI_Recv (&level, 1, MPI_INT, MPI_ANY_SOURCE, TAG_FIND_LOWEST1, MPI_COMM_WORLD, NULL); // Uwaga - zmiana tagu
			MPI_Recv (&tempCounter, NUM_CONSPIR, MPI_INT, MPI_ANY_SOURCE, TAG_FIND_LOWEST3, MPI_COMM_WORLD, NULL); // Musi być inny tag, bo mogą przyjść 2 wiadomości &level pod rząd
			for (int j = 0; j < NUM_CONSPIR; j++) {
				counter[j] += tempCounter[j];
			}
			maxLevel = level > maxLevel ? level : maxLevel;
		}
		// Kanały działają FIFO, więc luz (inaczej jest potrzebne synchro)
		for (int i = 0; i < children.size(); i++) {
			// Wysyłamy wszystkim dzieciom informację o wysokości drzewa...
			Ibsend (&maxLevel, 1, children[i], TAG_FIND_LOWEST2);
			//... oraz counter
			Ibsend (&counter, NUM_CONSPIR, children[i], TAG_FIND_LOWEST2);
		}
	}
	else {
		MPI_Recv (&level, 1, MPI_INT, MPI_ANY_SOURCE, TAG_FIND_LOWEST0, MPI_COMM_WORLD, NULL);
		maxLevel = level; // obecny maksymalny level = level
		// Zwiekszamy wartość wysokości, otrzymaną od rodzica
		level += 1;
		for (int i = 0; i < children.size(); i++) {
			// Wysyłamy wszystkim dzieciom
			Ibsend (&level, 1, children[i], TAG_FIND_LOWEST0);
		}
		// Otrzymujemy wysokość drzewa
		for (int i = 0; i < children.size(); i++) {
			MPI_Recv (&maxLevel, 1, MPI_INT, MPI_ANY_SOURCE, TAG_FIND_LOWEST1, MPI_COMM_WORLD, NULL); // Uwaga - zmiana tagu
			//wczytanie countera od kazdego dziecka i dopisanie do swojego glownego
			MPI_Recv (&tempCounter, NUM_CONSPIR, MPI_INT, MPI_ANY_SOURCE, TAG_FIND_LOWEST3, MPI_COMM_WORLD, NULL);
			for (int j = 0; j < NUM_CONSPIR; j++) {
				counter[j] += tempCounter[j];
			}
		}
		maxLevel = level > maxLevel ? level : maxLevel; // jesli nie mamy dzieci
		// Wysyłamy maxLevel i tablicę countera rodzicowi
		Ibsend (&maxLevel, 1, parent, TAG_FIND_LOWEST1);
		counter[level]++;
		Ibsend (&counter, NUM_CONSPIR, parent, TAG_FIND_LOWEST3);
		// Dostajemy odpowiedź od rodzica i sprawdzamy, czy jesteśmy najniżej w hierarchii oraz zapisujemy ilu konspiratorow jest na tym samym poziomie
		// (zalozenie kanałów FIFO)
		MPI_Recv (&maxLevel, 1, MPI_INT, MPI_ANY_SOURCE, TAG_FIND_LOWEST2, MPI_COMM_WORLD, NULL);
		MPI_Recv (&counter, NUM_CONSPIR, MPI_INT, MPI_ANY_SOURCE, TAG_FIND_LOWEST2, MPI_COMM_WORLD, NULL);
		sameLevelNodes = counter[level];
		lowest = (maxLevel == level);
		if (!lowest) {
			// wysyłamy info dalej, jeśli mamy dzieci
			for (int i = 0; i < children.size(); i++) {
				// Wysyłamy wszystkim dzieciom informację o wysokości drzewa...
				Ibsend (&maxLevel, 1, children[i], TAG_FIND_LOWEST2);
				//... no i counter
				Ibsend (&counter, NUM_CONSPIR, children[i], TAG_FIND_LOWEST2);
			}
		}
	}
	//  printf("%d - %d , level: %d\n", id, sameLevelNodes, level);
}


void Opornik::run() {
	// https://stackoverflow.com/questions/12043057/cannot-convert-from-type-voidclassname-to-type-voidvoid
	std::thread thread_1 (live_starter, this);
	std::thread thread_2 (listen_starter, this);
	thread_1.join();
	thread_2.join();
}


void* Opornik::live_starter (void* arg) {
	Opornik* op = (Opornik*) arg;
	op->live();
}

void Opornik::live() {
	while (true) {
		usleep (10000); //0.1 sec
		int actionRand = rand() % 1001; //promilowy podział prawdopodobieństwa dla pojedynczego procesu co sekundę
		if (status == blocked) {
			if (actionRand >= 995) {
				debug_log ("Chcem, ale nie mogem! Jestem zablokowany!\n");
			}
			continue;
		}
		else if (actionRand >= 995) {
			organizeMeeting();
		}
		else if (actionRand >= 990)
			if (duringMyMeeting) {
				endMeeting();
			}
			else if (actionRand >= 985 && acceptorToken != NONE) {
				pass_acceptor();
			}
	}
}

void* Opornik::listen_starter (void* arg) {
	Opornik* op = (Opornik*) arg;
	op->listen();
}

void Opornik::introduce() {
	std::string info = "Node " + std::to_string (id) + ":Hello, my parent is: " + std::to_string (parent);
	if (resources.size() > 0) {
		info += " I have: ";
		for (int i = 0; i < resources.size(); i++)
			if (resources[i] % 2) {
				info += "book(" + std::to_string (resources[i]) + ") ";
			}
			else {
				info += "dvd(" + std::to_string (resources[i]) + ") ";
			}
	}
	if (neighbors.size() > 0) {
		info += " and my neighbors are";
	}
	for (int i = 0; i < neighbors.size(); i++) {
		info += " " + std::to_string (neighbors[i]);
	}
	if (acceptorToken != NONE) {
		info += "  I'm acceptor(" + std::to_string (acceptorToken) + ")";
	}
	printf ("%s\n", info.c_str() );
}

void Opornik::setStatus (status_enum s) {
	switch (s) {
	case idle: { // Musisz przejrzeć kolejkę otrzymanych próśb inicjalizujących zmianę akceptora (Msg_pass_acceptor)
			debug_log ("Przeglądam kolejkę otrzymanych próśb o zmianę akceptora (%d)...\n", passAcceptorMsg_vector.size() );
			while (passAcceptorMsg_vector.size() != 0) {
				Msg_pass_acceptor msg = passAcceptorMsg_vector.back();
				acceptorMsgSend (msg, msg.sender);
				passAcceptorMsg_vector.pop_back();
			}
			status = idle;
			debug_log ("IDLE\n");
			break;
		}
	}
}
