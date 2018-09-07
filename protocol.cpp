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

void Opornik::listen()
{
    // blocked = true

    try
    {

    int buffer[MAX_BUFFER_SIZE];
    MPI_Status mpi_status;

    while (true)
    {
        MPI_Recv(&buffer, MAX_BUFFER_SIZE, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &mpi_status);

        //debug_log("Dostałem wiadomość typu %d od %d\t", mpi_status.MPI_TAG, mpi_status.MPI_SOURCE);

        switch (mpi_status.MPI_TAG)
        {
			case TAG_PASS_ACCEPTOR:
			{
				Msg_pass_acceptor *msg = (Msg_pass_acceptor *) buffer;
				handleAcceptorMsg(mpi_status.MPI_SOURCE, *msg);
				break;
			}
			case TAG_ACCEPTOR_CANDIDATE:
			{
				Msg_pass_acceptor *msg = (Msg_pass_acceptor *) buffer;
				handleACandidateMsg(mpi_status.MPI_SOURCE, *msg);
				break;
			}
			case TAG_ACCEPTOR_RESPONSE:
            {
                Msg_pass_acceptor *msg = (Msg_pass_acceptor *) buffer;
                handleAResponseMsg(mpi_status.MPI_SOURCE, *msg);
                break;
            }
            case INVITATION_MSG:
            case RESOURCE_GATHER:
            case ENDOFMEETING:
            {
                bool exist=false;
                for(std::list<msgBcastInfo>::iterator x=bcasts.begin();x!=bcasts.end();x++)
                    if(buffer[0]==x->uniqueTag){
                        exist=true;
                        receiveResponseMsg(buffer,mpi_status.MPI_TAG,&(*x));
                        break;
                    }
                if(!exist)
                    receiveForwardMsg(buffer,mpi_status.MPI_TAG,mpi_status.MPI_SOURCE);
                break;
            }
            default:
            {
                debug_log("Otrzymano nieznany typ wiadomości\n");
            }
        }
    }
    }
    catch (std::exception &e)
    {
        debug_log("%s", e.what());
    }
}

void Opornik::basicAcceptorSend(Msg_pass_acceptor msg, int sender, int tag)
{
	if (msg.distance == msg.target_distance && sender != parent && parent != NONE)
	{
		msg.distance += 1;
		MPI_Send(&msg, sizeof(msg)/sizeof(int), MPI_INT, parent, tag, MPI_COMM_WORLD);
	}
	else if (msg.distance > msg.target_distance)
	{
		msg.distance += 1;
		if (parent != -1 && sender != parent)
		{
			MPI_Send(&msg, sizeof(msg)/sizeof(int), MPI_INT, parent, tag, MPI_COMM_WORLD);
		}

		// I w dół
		msg.distance -= 2; // 2, ponieważ zwiększyliśmy na potrzeby wysłania do rodzica
		if (children.size() > 0)
		{
			for (int i = 0; i < children.size(); i++)
			{
				if (children[i] != sender)
				{
					MPI_Send(&msg, sizeof(msg)/sizeof(int), MPI_INT, children[i], tag, MPI_COMM_WORLD);
				}
			}
		}
	}
	else if (msg.distance < msg.target_distance)
	{
		if (sender == parent) // Dostałem tę wiadomość od rodzica (wiadomość idzie w dół)
		{
		}
		else // Wiadomość idzie w górę, aby dotrzeć na inną gałąź
		{
			msg.distance += 1;
			if (parent != -1)
			{
				MPI_Send(&msg, sizeof(msg)/sizeof(int), MPI_INT, parent, tag, MPI_COMM_WORLD);
			}
		}
	}
}

void Opornik::handleAResponseMsg(int sender, Msg_pass_acceptor msg)
{
	if (msg.candidate_id == id)
	{
		if (msg.failure == 0)
		{
			acceptorToken = accepted;
			debug_log("Mogę zostać NOWYM AKCEPTOREM!\n");
		}
		else
		{
			status = idle;
			acceptorToken = notAcceptor;
			debug_log("Zostałem ODRZUCONY na nowego akceptora.\n");
		}
	}
	else
	{
		basicAcceptorSend(msg, sender, TAG_ACCEPTOR_CANDIDATE);
    }
}
void Opornik::handleACandidateMsg(int sender, Msg_pass_acceptor msg)
{
	if (msg.initializator_id == id)
	{
		if (acceptorToken == findingCandidates)
		{
			acceptorToken = passingToken;
			debug_log("Dostałem PIERWSZE zgłoszenie na KANDYDATa do zmiany akceptora!\n Nowym akceptorem zostanie: %d!\n", msg.candidate_id);

			//przekaż dobrą nowinę kandydatowi (wiadomośc zwrotna)
			msg.failure = 0;
            MPI_Send(&msg, sizeof(msg)/sizeof(int), MPI_INT, sender, TAG_ACCEPTOR_RESPONSE, MPI_COMM_WORLD);


		}
		else
		{
			msg.failure = 1;
			debug_log("Dostałem Kolejne zgłoszenie na KANDYDATa do zmiany akceptora. Niestety, nie możesz nim zostać: %d\n", msg.candidate_id);
			MPI_Send(&msg, sizeof(msg)/sizeof(int), MPI_INT, sender, TAG_ACCEPTOR_RESPONSE, MPI_COMM_WORLD);
		}
	}
	else
	{
   	 	basicAcceptorSend(msg, sender, TAG_ACCEPTOR_CANDIDATE);
	}
}

void Opornik::handleAcceptorMsg(int sender, Msg_pass_acceptor msg)
{
	if (msg.distance == msg.target_distance && msg.initializator_id != id && status != busy)
	{
		// Uwaga! Możemy dostać to samo zgłoszenie kilka razy (sąsiedzi rozprowadzają je przez rodzica), niby status.busy częściowo rozwiązuje problem TODO
		status = busy;
		acceptorToken = candidate;
		msg.candidate_id = id;
		msg.distance = (sender == parent) ? msg.distance + 1 : msg.distance - 1;
		debug_log("(from %d) Jestem DOBRYM KANDYDATEM na akceptora, muszę o tym dać znać! dist = %d\n", sender, msg.distance);
		MPI_Send(&msg, sizeof(msg)/sizeof(int), MPI_INT, sender, TAG_ACCEPTOR_CANDIDATE, MPI_COMM_WORLD);
		debug_log("Wysłałem swoją kandydaturę do %d\n", sender);
		// Można by przekazać jeszcze wyżej i przeskoczyć na inne gałęzie lub do sąsiadów, ale nie robimy sobie konkurencji
	}
	else
	{
		basicAcceptorSend(msg, sender, TAG_PASS_ACCEPTOR);
	}
}

void Opornik::pass_acceptor()
{
	debug_log("Nie chcę już być akceptorem!\n");
	status = blocked;
	// losowanie kierunku przekazania akceptora
	int rand = random() % 100;
	int new_acceptor;
	int buffer[MAX_BUFFER_SIZE];

	try
	{
		Msg_pass_acceptor msg;
		// todo: iteracja po całym drzewie i dodanie kandydatów do vectora, następnie wylosowanie kandydata i próba przekazania akceptora
		// trzeba dodać licznik, któr będzie zwiększany gdy wiadomośc pójdzie w górę, zmniejszany kiedy w dół. W ten sposób będzie wiadomo, kto może zostać nowym akceptorem.
		// 0 - ten sam poziom
		// 1 - wyższy
		// -1 - niższy
		// (-inf; +inf)\{-1,0,1} zostają pominięte
		if (rand < 10) //gora
		{
			debug_log("Chcę przekazać akceptora w górę!\n");

			if (parent != -1)
			{
				acceptorToken = findingCandidates;
				msg = {clock, id, NONE, 1, 1, 0}; // distance = 1, bo przekazujemy w górę
				// Wystarczy przekazać w górę
				MPI_Send(&msg, sizeof(msg)/sizeof(int), MPI_INT, parent, TAG_PASS_ACCEPTOR, MPI_COMM_WORLD);
			}
			else
			{
				debug_log("Nie mogę przekazać akceptora w górę, jestem na szczycie!\n");
				status = idle;
			}
		}
		else if (rand < 20) //dol
		{
			acceptorToken = findingCandidates;
			if (parent != -1)
            {
				msg = {clock, id, NONE, 1, -1, 0};  // distance = 1, bo przekazujemy w górę

				// Trzeba przekazać w górę i do dzieci
				debug_log("Chcę przkazać akceptora w dół!\n");
				// TODO UWAGA! Konspirator może być na samym dole, ale nie ma o tym wiedzy. Wtedy pomimo czekania, nie dostanie żadnego kandydata. timeout??
				MPI_Send(&msg, sizeof(msg)/sizeof(int), MPI_INT, parent, TAG_PASS_ACCEPTOR, MPI_COMM_WORLD);
			}
            if (children.size() > 0)
			{
                for (int i = 0; i < children.size(); i++)
				{
                    MPI_Send(&msg, sizeof(msg)/sizeof(int), MPI_INT, children[i], TAG_PASS_ACCEPTOR, MPI_COMM_WORLD);
				}
			}

		}
		else //ten sam poziom
		{
			if (parent != -1)
            {
				acceptorToken = findingCandidates;
				msg = {clock, id, NONE, 1, 0, 0};  // distance = 1, bo przekazujemy w górę

				//Wystarczy przekazać w górę
				debug_log("Chcę przekazać akceptora na swoim poziomie!\n");
				MPI_Send(&msg, sizeof(msg)/sizeof(int), MPI_INT, parent, TAG_PASS_ACCEPTOR, MPI_COMM_WORLD);
			}
            else
            {
            	debug_log("Nie mogę przekazać na ten sam poziom, jestem na szczycie!\n");
				status = idle;
            }

		}
	}
	catch (const std::exception& e)
	{
		// przyk. być może padło na kierunek, gdzie nie ma już więcej elemetów
		debug_log("Mam pecha, nie mogę przekazać akceptora w wylosowanym kierunku. Jeszcze jedna próba!\n");
		pass_acceptor();
	}
}

void Opornik::organizeMeeting(){
    if(meeting==NONE)
    {
        debug_log("Organizuję spotkanie!\n");
        meeting=id;
        meetingInfo info;
        info.uniqueTag=generateUniqueTag();
        info.meetingId=id;
        info.participants=0;
        if(!resources.empty()){
            info.haveResource=resources.back();
            busyResource=resources.back();
            resources.pop_back();
        }
        else
            info.haveResource=NONE;

        receiveForwardMsg((int*)(&info),INVITATION_MSG,id); //TODO: pomyśleć czy tak może być ;)
    }
}

void Opornik::resourceGather(){
    if(busyResource==NONE)
    {
        debug_log("Dajcie mi zasób!\n");
        resourceGatherMsg res;
        res.uniqueTag=generateUniqueTag();
        res.haveResource=NONE;

        receiveForwardMsg((int*)(&res),RESOURCE_GATHER,id); //TODO: pomyśleć czy tak może być ;)
    }
}

void Opornik::endMeeting(){
    if(id==meeting)
    {
        debug_log("Rozejść się!\n");
        endOfMeeting end;
        end.uniqueTag=generateUniqueTag();
        end.meetingId=id;
        receiveForwardMsg((int*)(&end),ENDOFMEETING,id); //TODO: pomyśleć czy tak może być ;)
    }
}

void Opornik::receiveForwardMsg(int* buffer,int tag,int source){
    int msgSize;
    switch(tag)
    {
        case INVITATION_MSG:
        {
            msgSize=4;
            meetingInfo* info=(meetingInfo*)buffer;
            if(meeting==NONE && time(NULL)>=meetingTimeout) // THEN: zgódź się :D
            {
                //debug_log("Zaproszono mnie do spotkania %d\n",info->meetingId);
                meeting=info->meetingId;
            }
            if(info->haveResource==NONE && !resources.empty()){
                    info->haveResource=resources.back();
                    resources.pop_back();
            }
            break;
        }
        case RESOURCE_GATHER:
        {
            msgSize=2;
            resourceGatherMsg* res=(resourceGatherMsg*)buffer;
            if(res->haveResource==NONE && !resources.empty()){
                    res->haveResource=resources.back();
                    resources.pop_back();
            }
            break;
        }
        case ENDOFMEETING:
            msgSize=2;
            break;
    }
    sendForwardMsg(buffer,tag,source,msgSize);
}

void Opornik::receiveResponseMsg(int* buffer,int tag,msgBcastInfo* bcast){
    bcast->waitingForResponse--;
    switch(tag)
    {
        case INVITATION_MSG:
        {
            meetingInfo* info=(meetingInfo*)buffer;
            meetingInfo* sumaric=(meetingInfo*)bcast->buffer;

            sumaric->participants+=info->participants;
            if(info->haveResource!=NONE)
            {
                if(sumaric->haveResource==NONE)
                    sumaric->haveResource=info->haveResource;
                else if(sumaric->haveResource!=info->haveResource)
                    resources.push_back(info->haveResource);
            }

            if(bcast->waitingForResponse<=0)//jeżeli dostałeś już odpowiedzi od wszystkich
            {
                if(meeting==info->meetingId)
                    sumaric->participants++;
                info->participants=sumaric->participants;
                info->haveResource=sumaric->haveResource;
            }
            break;
        }
        case RESOURCE_GATHER:
        {
            resourceGatherMsg* res=(resourceGatherMsg*)buffer;
            resourceGatherMsg* sumaric=(resourceGatherMsg*)bcast->buffer;

            if(res->haveResource!=NONE)
            {
                if(sumaric->haveResource==NONE)
                    sumaric->haveResource=res->haveResource;
                else if(sumaric->haveResource!=res->haveResource)
                    resources.push_back(res->haveResource);
            }

            if(bcast->waitingForResponse<=0)//jeżeli dostałeś już odpowiedzi od wszystkich
                res->haveResource=sumaric->haveResource;
            break;
        }
        case ENDOFMEETING:
        if(bcast->waitingForResponse<=0)//jeżeli dostałeś już odpowiedzi od wszystkich
        {
            //timeout spotkaniowy "Następnie rozchodzą się i przez pewien czas nie biorą udziału w zebraniach."
            meetingTimeout=time(NULL)+5;
            endOfMeeting* end=(endOfMeeting*)buffer;
            if(meeting==end->meetingId)
                meeting=NONE;
            break;
        }
    }
    if(bcast->waitingForResponse<=0)//jeżeli dostałeś już odpowiedzi od wszystkich
    {
        sendResponseMsg(buffer,tag,bcast);
        bcasts.remove(*bcast);
    }
}

void Opornik::sendForwardMsg(int* buffer,int tag,int source,int msgSize){
    std::list<int> sendTo;
    msgBcastInfo bcast;
    bcast.uniqueTag=buffer[0];
    bcast.respondTo=source;
    bcast.msgSize=msgSize;
    for(int i=0;i<children.size();i++)
        if(children[i]!=source)
            sendTo.push_back(children[i]);
    if(parent!=NONE && parent!=source)
        sendTo.push_back(parent);

    switch(tag)//inicjalizacja bufora broadcastu i wybór odbiorców
    {
        case INVITATION_MSG:
        {
            meetingInfo* sumaric=(meetingInfo*)bcast.buffer;
            //Send only to children
            sendTo.remove(parent);

            sumaric->participants=0;
            sumaric->haveResource=NONE;
            break;
        }
        case RESOURCE_GATHER:
        {
            resourceGatherMsg* sumaric=(resourceGatherMsg*)bcast.buffer;
            sumaric->haveResource=NONE;
            break;
        }
        case ENDOFMEETING:
            //Nothing special
            break;
    }
    bcast.waitingForResponse=sendTo.size();
    bcasts.push_back(bcast);

    if(bcast.waitingForResponse==0) //jeżeli to już liść
        receiveResponseMsg(buffer,tag,&bcasts.back());

    for(auto i : sendTo)
        MPI_Send(buffer,bcast.msgSize,MPI_INT,i,tag,MPI_COMM_WORLD);

}

void Opornik::sendResponseMsg(int* buffer,int tag,msgBcastInfo* bcast){
    if(id==bcast->respondTo)//Jeżeli odpowiedź dotarła do inicjatora
        switch (tag)
        {
            case INVITATION_MSG:
            {
                meetingInfo* info=(meetingInfo*)buffer;
                    busyResource=info->haveResource;
                    if(busyResource!=NONE)
                    {
                        debug_log("Na moje spotkanie przyjdzie %d oporników i użyjemy zasobu %d\n",info->participants,info->haveResource);
                        //Temporary! domyślnie duringMyMeeting oznacza że spotkanie zostało zaakceptowane
                        //i wszyscy uczestnicy już to wiedzą (spotykają się)
                        duringMyMeeting=true;
                    }
                    else
                    {
                        debug_log("Jest %d chętnych na spotkanie, ale nie mamy zasobu\n",info->participants);
                        resourceGather();
                    }
                break;
            }
            case RESOURCE_GATHER:
            {
                resourceGatherMsg* res=(resourceGatherMsg*)buffer;
                    busyResource=res->haveResource;
                    if(busyResource!=NONE)
                    {
                        debug_log("Otrzymałem zasób %d\n",res->haveResource);
                        //Temporary!!! domyślnie duringMyMeeting oznacza że spotkanie zostało zaakceptowane
                        //i wszyscy uczestnicy już to wiedzą (spotykają się)
                        duringMyMeeting=true;
                    }
                    else
                    {
                        endMeeting();
                        debug_log("Wszystkie zasoby są pozajmowane\n");
                    }
                break;
            }
            case ENDOFMEETING:
                resources.push_back(busyResource);
                busyResource=NONE;
                duringMyMeeting=false;
                debug_log("Wszyscy poszli już do domu po moim spotkaniu\n");
                break;
        }
    else
        MPI_Send(buffer,bcast->msgSize,MPI_INT,bcast->respondTo,tag,MPI_COMM_WORLD);
}