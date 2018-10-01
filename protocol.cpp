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


void Opornik::listen() {
	// blocked = true
	try {
		int buffer[MAX_BUFFER_SIZE];
		MPI_Status mpi_status;
		while (true) {
			MPI_Recv (&buffer, MAX_BUFFER_SIZE, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &mpi_status);
			//log(debug, "MPI_Recv BUFFER0:%d BUFFER1:%d\n", buffer[0], buffer[1]);
			//debug_log("Dostałem wiadomość typu %d od %d\t", mpi_status.MPI_TAG, mpi_status.MPI_SOURCE);
			// Licznik lamporta
			clock = clock > buffer[0] ? clock + 1 : buffer[0] + 1;
			switch (mpi_status.MPI_TAG) {
			case TAG_PASS_ACCEPTOR: {
					Msg_pass_acceptor* msg = (Msg_pass_acceptor*) buffer;
					handleAcceptorMsg (mpi_status.MPI_SOURCE, *msg);
					break;
				}
			case TAG_ACCEPTOR_CANDIDATE: {
					Msg_pass_acceptor* msg = (Msg_pass_acceptor*) buffer;
					handleACandidateMsg (mpi_status.MPI_SOURCE, *msg);
					break;
				}
			case TAG_ACCEPTOR_RESPONSE: {
					Msg_pass_acceptor_final* msg = (Msg_pass_acceptor_final*) buffer;
					handleAResponseMsg (mpi_status.MPI_SOURCE, *msg);
					break;
				}
			case ASKFORACCEPTATION: {
					askForAcceptation* a = (askForAcceptation*) buffer;
					log(debug, "ASKFORACCEPTATION BEGIN meeting:%d\n", buffer[1]);
					if (parent != NONE && parent != mpi_status.MPI_SOURCE) {
						log(debug, "ASKFORACCEPTATION meeting:%d\n", a->meeting);
						Ibsend (a, sizeof(askForAcceptation)/sizeof(int), parent, ASKFORACCEPTATION);
					}
					for (int i = 0; i < children.size(); i++)
						if (children[i] != mpi_status.MPI_SOURCE) {
							log(debug, "ASKFORACCEPTATION meeting:%d\n", a->meeting);
							Ibsend (a, sizeof(askForAcceptation)/sizeof(int), children[i], ASKFORACCEPTATION);
						}
					if (acceptorStatus == isAcceptor) {
						log(debug, "ASKFORACCEPTATION meeting:%d\n", a->meeting);
						shareClock(a);
					}
                    else if (acceptorToken != NONE){
                        log(trace, "Zapisuję askForAcceptation \n");
                        askForAcceptation_vector.push_back (a);
                    }
					break;
				}
			case SHAREACCEPTOR: {
					shareAcceptor* s = (shareAcceptor*) buffer;
					if (parent != NONE && parent != mpi_status.MPI_SOURCE) {
						Ibsend (s, 4,  parent, SHAREACCEPTOR);
					}
					for (int i = 0; i < children.size(); i++)
						if (children[i] != mpi_status.MPI_SOURCE) {
							Ibsend (s, 4,  children[i], SHAREACCEPTOR);
						}
					if (acceptorToken != NONE) {
						knownMeetings[s->meeting].acceptors[s->acceptorToken] = s->acceptorClk;
						if (acceptorStatus == isAcceptor) {
							checkDecisions();
						}
					}
					log(trace, "Przekazuję akceptora clk, meeting: %d \n", s->meeting);
					break;
				}
			case ACCEPT: {
					accept* a = (accept*) buffer;
					if (parent != NONE && parent != mpi_status.MPI_SOURCE) {
						Ibsend (a, 3,  parent, ACCEPT);
					}
					for (int i = 0; i < children.size(); i++)
						if (children[i] != mpi_status.MPI_SOURCE) {
							Ibsend (a, 3,  children[i], ACCEPT);
						}
					if (acceptorToken != NONE) {
						if (a->decision == TRUE) {
							freeSlots -= knownMeetings[a->meeting].participants;
						}
						//wyczyść info
						knownMeetings[a->meeting].priority = 0;
						knownMeetings[a->meeting].participants = 0;
						for (int i = 0; i < NUM_ACCEPTORS; i++) {
							knownMeetings[a->meeting].acceptors[i] = NONE;
						}
					}
					if (a->decision == TRUE) {
						if (a->meeting == id) {
							log (info, "Moje spotkanie jest zaakceptowane\n");
							duringMyMeeting = true;
						}
						else if (a->meeting == meeting) {
							log (info, "Idę na spotkanie\n");
						}
					}
					else { //a->decision==FALSE
						if (a->meeting == id) {
							log (info, "Moje spotkanie jest odrzucone\n");
							duringMyMeeting = false;
							meeting = NONE;
							resources.push_back (busyResource);
							busyResource = NONE;
							participantsOnMymeeting = 0;
						}
						else if (a->meeting == meeting) {
							log (info, "ehh nie wyszło, jestem wolny\n");
							meeting = NONE;
						}
					}
					break;
				}
			case INVITATION_MSG:
			case RESOURCE_GATHER:
			case ENDOFMEETING: {
					bool exist = false;
					for (std::list<msgBcastInfo>::iterator x = bcasts.begin(); x != bcasts.end(); x++)
						if (buffer[1] == x->uniqueTag) {
							exist = true;
							receiveResponseMsg (buffer, mpi_status.MPI_TAG, & (*x) );
							break;
						}
					if (!exist) {
						receiveForwardMsg (buffer, mpi_status.MPI_TAG, mpi_status.MPI_SOURCE);
					}
					break;
				}
			default: {
					log (error, "Otrzymano nieznany typ wiadomości\n");
				}
			}
		}
	}
	catch (std::exception& e) {
		log (info, "%s", e.what() );
	}
}

void Opornik::shareClock(askForAcceptation* a) {
	log (trace, "Shareuje mój zegar\n");
	shareAcceptor s;
	s.acceptorToken = acceptorToken;
	s.meeting = a->meeting;
	s.acceptorClk = clock;
	Ibsend (&s, 4,  id, SHAREACCEPTOR);
}

void Opornik::getAcceptation (int p) {
	askForAcceptation a;
	a.meeting = id;
	a.participants = p;
	log (debug, "DgetAcceptation meeting: %d\n", a.meeting);
	Ibsend (&a, sizeof(askForAcceptation)/sizeof(int),  id, ASKFORACCEPTATION);
}

void Opornik::organizeMeeting() {
	if (meeting == NONE) {
		log (trace, "Organizuję spotkanie!\n");
		meeting = id;
		meetingInvitation info;
		info.uniqueTag = generateUniqueTag();
		info.meetingId = id;
		info.participants = 0;
		if (!resources.empty() ) {
			info.haveResource = resources.back();
			busyResource = resources.back();
			resources.pop_back();
		}
		else {
			info.haveResource = NONE;
		}
		log (debug, "spotkanie! %d\n", info.meetingId );
		receiveForwardMsg ( (int*) (&info), INVITATION_MSG, id);
	}
}

void Opornik::resourceGather() {
	if (busyResource == NONE) {
		log (trace, "Dajcie mi zasób!\n");
		resourceGatherMsg res;
		res.uniqueTag = generateUniqueTag();
		res.haveResource = NONE;
		receiveForwardMsg ( (int*) (&res), RESOURCE_GATHER, id);
	}
}

void Opornik::endMeeting() {
	if (id == meeting) {
		log (trace, "Rozejść się!\n");
		endOfMeeting end;
		end.uniqueTag = generateUniqueTag();
		end.meetingId = id;
		receiveForwardMsg ( (int*) (&end), ENDOFMEETING, id);
	}
}

void Opornik::receiveForwardMsg (int* buffer, int tag, int source) {
	int msgSize;
	switch (tag) {
	case INVITATION_MSG: {
			msgSize = 5;
			meetingInvitation* info = (meetingInvitation*) buffer;
			if (meeting == NONE && time (NULL) >= meetingTimeout) { // THEN: zgódź się :D
				//debug_log("Zaproszono mnie do spotkania %d\n",info->meetingId);
				meeting = info->meetingId;
				log (debug, "meeting from receiveForwardMsg[212]:%d\n", info->meetingId);
			}
			if (info->haveResource == NONE && !resources.empty() ) {
				info->haveResource = resources.back();
				resources.pop_back();
			}
			break;
		}
	case RESOURCE_GATHER: {
			msgSize = 3;
			resourceGatherMsg* res = (resourceGatherMsg*) buffer;
			if (res->haveResource == NONE && !resources.empty() ) {
				res->haveResource = resources.back();
				resources.pop_back();
			}
			break;
		}
	case ENDOFMEETING:
		msgSize = 3;
		break;
	}
	sendForwardMsg (buffer, tag, source, msgSize);
}

void Opornik::receiveResponseMsg (int* buffer, int tag, msgBcastInfo* bcast) {
	bcast->waitingForResponse--;
	switch (tag) {
	case INVITATION_MSG: {
			meetingInvitation* info = (meetingInvitation*) buffer;
			meetingInvitation* sumaric = (meetingInvitation*) bcast->buffer;
			sumaric->participants += info->participants;
			if (info->haveResource != NONE) {
				if (sumaric->haveResource == NONE) {
					sumaric->haveResource = info->haveResource;
				}
				else if (sumaric->haveResource != info->haveResource) {
					resources.push_back (info->haveResource);
				}
			}
			if (bcast->waitingForResponse <= 0) { //jeżeli dostałeś już odpowiedzi od wszystkich
				if (meeting == info->meetingId) {
					sumaric->participants++;
				}
				info->participants = sumaric->participants;
				info->haveResource = sumaric->haveResource;
			}
			break;
		}
	case RESOURCE_GATHER: {
			resourceGatherMsg* res = (resourceGatherMsg*) buffer;
			resourceGatherMsg* sumaric = (resourceGatherMsg*) bcast->buffer;
			if (res->haveResource != NONE) {
				if (sumaric->haveResource == NONE) {
					sumaric->haveResource = res->haveResource;
				}
				else if (sumaric->haveResource != res->haveResource) {
					resources.push_back (res->haveResource);
				}
			}
			if (bcast->waitingForResponse <= 0) { //jeżeli dostałeś już odpowiedzi od wszystkich
				res->haveResource = sumaric->haveResource;
			}
			break;
		}
	case ENDOFMEETING:
		if (bcast->waitingForResponse <= 0) { //jeżeli dostałeś już odpowiedzi od wszystkich
			//timeout spotkaniowy "Następnie rozchodzą się i przez pewien czas nie biorą udziału w zebraniach."
			meetingTimeout = time (NULL) + 5;
			endOfMeeting* end = (endOfMeeting*) buffer;
			if (meeting == end->meetingId) {
				meeting = NONE;
			}
			break;
		}
	}
	if (bcast->waitingForResponse <= 0) { //jeżeli dostałeś już odpowiedzi od wszystkich
		sendResponseMsg (buffer, tag, bcast);
		bcasts.remove (*bcast);
	}
}

void Opornik::sendForwardMsg (int* buffer, int tag, int source, int msgSize) {
	std::list<int> sendTo;
	msgBcastInfo bcast;
	bcast.uniqueTag = buffer[1];
	bcast.respondTo = source;
	bcast.msgSize = msgSize;
	for (int i = 0; i < children.size(); i++)
		if (children[i] != source) {
			sendTo.push_back (children[i]);
		}
	if (parent != NONE && parent != source) {
		sendTo.push_back (parent);
	}
	switch (tag) { //inicjalizacja bufora broadcastu i wybór odbiorców
	case INVITATION_MSG: {
			meetingInvitation* sumaric = (meetingInvitation*) bcast.buffer;
			//Send only to children
			sendTo.remove (parent);
			sumaric->participants = 0;
			sumaric->haveResource = NONE;
			break;
		}
	case RESOURCE_GATHER: {
			resourceGatherMsg* sumaric = (resourceGatherMsg*) bcast.buffer;
			sumaric->haveResource = NONE;
			break;
		}
	case ENDOFMEETING:
		//Nothing special
		break;
	}
	bcast.waitingForResponse = sendTo.size();
	bcasts.push_back (bcast);
	if (bcast.waitingForResponse == 0) { //jeżeli to już liść
		receiveResponseMsg (buffer, tag, &bcasts.back() );
	}
	for (auto i : sendTo) {
		Ibsend (buffer, bcast.msgSize, i, tag);
	}
}

void Opornik::sendResponseMsg (int* buffer, int tag, msgBcastInfo* bcast) {
	if (id == bcast->respondTo) //Jeżeli odpowiedź dotarła do inicjatora
		switch (tag) {
		case INVITATION_MSG: {
				meetingInvitation* info = (meetingInvitation*) buffer;
				busyResource = info->haveResource;
				if (busyResource != NONE) {
					participantsOnMymeeting = info->participants;
					log (log_enum::trace, "Na moje spotkanie przyjdzie %d oporników i użyjemy zasobu %d\n", info->participants, info->haveResource);
					getAcceptation (participantsOnMymeeting);
				}
				else {
					log (log_enum::trace, "Jest %d chętnych na spotkanie, ale nie mamy zasobu\n", info->participants);
					participantsOnMymeeting = info->participants;
					resourceGather();
				}
				break;
			}
		case RESOURCE_GATHER: {
				resourceGatherMsg* res = (resourceGatherMsg*) buffer;
				busyResource = res->haveResource;
				if (busyResource != NONE) {
					log (trace, "Otrzymałem zasób %d\n", res->haveResource);
					getAcceptation (participantsOnMymeeting);
				}
				else {
					endMeeting();
					log (trace, "Wszystkie zasoby są pozajmowane\n");
				}
				break;
			}
		case ENDOFMEETING:
			if (busyResource != NONE) {
				resources.push_back (busyResource);
				log (info, "Koniec spotkania!\n");
			}
			busyResource = NONE;
			if (acceptorToken != NONE) {
				freeSlots += participantsOnMymeeting;
			}
			participantsOnMymeeting = 0;
			duringMyMeeting = false;
			//log (trace, "Wszyscy poszli już do domu po moim spotkaniu\n");
			break;
		}
	else {
		Ibsend (buffer, bcast->msgSize, bcast->respondTo, tag);
	}
}

void Opornik::checkDecisions() {
	if (acceptorToken != NONE && acceptorStatus == isAcceptor) {
		std::list<int> ready;
		char debug[700] = {};
		char buffer[10];
		for (int j = 0; j < NUM_CONSPIR; j++) {
			snprintf (buffer, sizeof (buffer), "S %d: ", j);
			strcat (debug, buffer);
			bool rd = true;
			//Pierwszy warunek- info od wszystkich akceptorów
			for (int i = 0; i < NUM_ACCEPTORS; i++) {
				if (knownMeetings[j].acceptors[i] == NONE) {
					rd = false;
				}
				else {
					snprintf (buffer, sizeof (buffer), "%d ", i);
					strcat (debug, buffer);
				}
			}
			//drugi warunek- zegar ma najmniejszą wartość
			int lowestClk = clock;
			for (int i = 0; i < NUM_ACCEPTORS; i++)
				if (knownMeetings[j].acceptors[i] < lowestClk) {
					lowestClk = knownMeetings[j].acceptors[i];
				}
			//trzeci warunek- token akceptora najniższy
			for (int i = 0; i < NUM_ACCEPTORS; i++)
				if (knownMeetings[j].acceptors[i] == lowestClk) {
					if (i != acceptorToken) {
						rd = false;
					}
					break;
				}
			if (rd) {
				ready.push_back (j);
			}
			strcat (debug, "| ");
		}
		log (log_enum::debug, "%s \n", debug);
		while (!ready.empty() ) {
			//find maxPriority
			int meetingId = 0;
			int maxPriority = 0;
			for (auto i : ready)
				if (knownMeetings[i].priority > maxPriority) {
					maxPriority = knownMeetings[i].priority;
				}
			//select meetingId
			for (auto i : ready)
				if (knownMeetings[i].priority == maxPriority) {
					meetingId = i;
					log (log_enum::debug, "meetingId from checkDecisions[433]:%d\n", meetingId);
					ready.remove (i);
					break;
				}
			//advance priorities
			for (auto i : ready) {
				knownMeetings[i].priority++;
			}
			accept a;
			if (knownMeetings[meetingId].participants <= freeSlots) {
				a.decision = TRUE;
			}
			else {
				a.decision = FALSE;
			}
			a.meeting = meetingId;
			Ibsend (&a, 3,  id, ACCEPT);
			log (trace, "zdecydowałem\n");
		}
	}
}

void Opornik::Ibsend (void* buf, int count, int dest, int tag) {
	//TODO FIX: pierwsza wartość po clock w strukturze (np. askForAcceptation) jest czasami błędnie zapisywana.
	clock++;
	memcpy (buf, &clock, sizeof (int) );
	MPI_Request req;
	//MPI_Status stat;
	MPI_Ibsend (buf, count, MPI_INT, dest, tag, MPI_COMM_WORLD, &req);
	MPI_Request_free (&req);
	//MPI_Wait( &req, &stat );
};
