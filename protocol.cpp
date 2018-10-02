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
					log(debug, "ASKFORACCEPTATION clock:%d meeting:%d participants:%d\n", a->clock, a->meeting, a->participants);
					if (parent != NONE && parent != mpi_status.MPI_SOURCE) {
						Ibsend (a, sizeof(askForAcceptation)/sizeof(int), parent, ASKFORACCEPTATION);
					}
					for (int i = 0; i < children.size(); i++)
						if (children[i] != mpi_status.MPI_SOURCE) {
							Ibsend (a, sizeof(askForAcceptation)/sizeof(int), children[i], ASKFORACCEPTATION);
						}
                    knownMeetings[a->meeting].participants = a->participants;
                    if (acceptorStatus == isAcceptor) {
						shareClock(a);
					}
                    else if (acceptorToken != NONE){
                        knownMeetings[a->meeting].meetingClk=a->meetingClk;
                        log(trace, "Zapisuję askForAcceptation \n");
                        askForAcceptation_vector.push_back (a);
                    }
					break;
				}
			case SHAREACCEPTOR: {
					shareAcceptor* s = (shareAcceptor*) buffer;
					if (parent != NONE && parent != mpi_status.MPI_SOURCE) {
						Ibsend (s, sizeof(shareAcceptor)/sizeof(int),  parent, SHAREACCEPTOR);
					}
					for (int i = 0; i < children.size(); i++)
						if (children[i] != mpi_status.MPI_SOURCE) {
							Ibsend (s, sizeof(shareAcceptor)/sizeof(int),  children[i], SHAREACCEPTOR);
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
                    int part=knownMeetings[a->meeting].participants;
					if (parent != NONE && parent != mpi_status.MPI_SOURCE) {
						Ibsend (a, sizeof(accept)/sizeof(int),  parent, ACCEPT);
					}
					for (int i = 0; i < children.size(); i++)
						if (children[i] != mpi_status.MPI_SOURCE) {
							Ibsend (a, sizeof(accept)/sizeof(int),  children[i], ACCEPT);
						}
					if (acceptorToken != NONE) {
                        //wyczyść info
                        knownMeetings[a->meeting].meetingClk = NONE;
                        knownMeetings[a->meeting].participants = 0;
                        for (int i = 0; i < NUM_ACCEPTORS; i++) {
                            knownMeetings[a->meeting].acceptors[i] = NONE;
                        }
                        checkDecisions();
					}
					if (a->decision == TRUE) {
                        freeSlots -= part;
						if (a->meeting == id) {
                            log (info, "Spotkanie (%d) jest zaakceptowane[%d]\n", id, part);
							duringMyMeeting = true;
						}
						else if (a->meeting == meeting) {
							log (info, "Idę na spotkanie\n");
						}
					}
					else { //a->decision==FALSE
						if (a->meeting == id) {
                            log (info, "Spotkanie (%d) jest odrzucone[%d]\n", id, part);
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
	Ibsend (&s, sizeof(shareAcceptor)/sizeof(int),  id, SHAREACCEPTOR);
}

void Opornik::getAcceptation (int p) {
	askForAcceptation a;
	a.meeting = id;
	a.participants = p;
    a.meetingClk=clock;
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
        info.participants = 1;
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
        if(duringMyMeeting){
            end.returnedParticipants = participantsOnMymeeting;
            duringMyMeeting=false;
        }
        else
            end.returnedParticipants = 0;
        participantsOnMymeeting = 0;

		receiveForwardMsg ( (int*) (&end), ENDOFMEETING, id);
	}
}

void Opornik::receiveForwardMsg (int* buffer, int tag, int source) {
	int msgSize;
	switch (tag) {
	case INVITATION_MSG: {
            msgSize = sizeof (meetingInvitation)/sizeof(int);
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
            msgSize = sizeof (resourceGatherMsg)/sizeof(int);
			resourceGatherMsg* res = (resourceGatherMsg*) buffer;
			if (res->haveResource == NONE && !resources.empty() ) {
				res->haveResource = resources.back();
				resources.pop_back();
			}
			break;
		}
	case ENDOFMEETING:
        msgSize = sizeof (endOfMeeting)/sizeof(int);
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
            freeSlots+=end->returnedParticipants;
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
				log (info, "Koniec spotkania (%d)!\n", id);
			}
			busyResource = NONE;
			//log (trace, "Wszyscy poszli już do domu po moim spotkaniu\n");
			break;
		}
	else {
		Ibsend (buffer, bcast->msgSize, bcast->respondTo, tag);
	}
}

void Opornik::checkDecisions() {
	if (acceptorToken != NONE && acceptorStatus == isAcceptor) {
        int minMeetingClk=clock;
        int minAcceptorClk=clock;
        int selectedMeeting=NONE;
        int selectedAcceptor=NONE;
        for(int i=0;i<NUM_CONSPIR;i++)
            if(knownMeetings[i].meetingClk<minMeetingClk && knownMeetings[i].meetingClk!=NONE)
            {
                minMeetingClk=knownMeetings[i].meetingClk;
                selectedMeeting=i;
            }
        for(int i=0;i<NUM_ACCEPTORS;i++)
            if(knownMeetings[selectedAcceptor].acceptors[i]<minAcceptorClk && knownMeetings[selectedMeeting].acceptors[i]!=NONE)
            {
                minAcceptorClk=knownMeetings[selectedMeeting].acceptors[i];
                selectedAcceptor=i;
            }
        if(selectedAcceptor==acceptorToken)
        {
            accept a;
            if (knownMeetings[selectedMeeting].participants <= freeSlots) {
                a.decision = TRUE;
            }
            else {
                a.decision = FALSE;
            }
            a.meeting = selectedMeeting;
            Ibsend (&a, sizeof(accept)/sizeof(int),  id, ACCEPT);
            log (trace, "zdecydowałem\n");
        }

    }
}

void Opornik::Ibsend (void* buf, int count, int dest, int tag) {
	clock++;
    *( int*)buf=clock;
	MPI_Request req;
	//MPI_Status stat;
	MPI_Ibsend (buf, count, MPI_INT, dest, tag, MPI_COMM_WORLD, &req);
	MPI_Request_free (&req);
	//MPI_Wait( &req, &stat );
};
