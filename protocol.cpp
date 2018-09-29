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

//TODO: ogarnąć zegary!

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
		
		// Licznik lamporta 
		// TODO Piotr, może będziesz musiał jeszcze gdzieś u siebie to zrobić, nie wiem jak działa ten Twój protokół
		
		clock = clock > buffer[0] ? clock + 1 : buffer[0] + 1;

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
                Msg_pass_acceptor_final *msg = (Msg_pass_acceptor_final *) buffer;
                handleAResponseMsg(mpi_status.MPI_SOURCE, *msg);
                break;
            }
            case ASKFORACCEPTATION:
            {
                askForAcceptation *a = (askForAcceptation *) buffer;
                if(parent!=NONE && parent!=mpi_status.MPI_SOURCE)
                    MPI_Ibsend(a,3, MPI_INT, parent, ASKFORACCEPTATION, MPI_COMM_WORLD,&req);
                    for(int i=0;i<children.size();i++)
                        if(children[i]!=mpi_status.MPI_SOURCE)
                            MPI_Ibsend(a,3, MPI_INT, children[i], ASKFORACCEPTATION, MPI_COMM_WORLD,&req);
                    if(acceptorToken!=NONE)
                    {
                        shareAcceptor s;
                        s.acceptorClk=clock;
                        s.acceptorToken=acceptorToken;
                        clock++;
                        s.clock=clock;
                        s.meeting=a->meeting;
                        MPI_Ibsend(&s,4, MPI_INT, id, SHAREACCEPTOR, MPI_COMM_WORLD,&req);
                    }
                break;
            }
            case SHAREACCEPTOR:
            {
                shareAcceptor *s = (shareAcceptor *) buffer;
                if(parent!=NONE && parent!=mpi_status.MPI_SOURCE)
                    MPI_Ibsend(s,4, MPI_INT, parent, SHAREACCEPTOR, MPI_COMM_WORLD,&req);
                    for(int i=0;i<children.size();i++)
                        if(children[i]!=mpi_status.MPI_SOURCE)
                            MPI_Ibsend(s,4, MPI_INT, children[i], SHAREACCEPTOR, MPI_COMM_WORLD,&req);
                    if(acceptorToken!=NONE)
                    {
                        if(acceptorToken==0)
                        {
                            accept *a = (accept *) buffer;
                            a->decision=TRUE;
                            a->meeting=s->meeting;
                            if(parent!=NONE && parent!=mpi_status.MPI_SOURCE)
                                MPI_Ibsend(a,3, MPI_INT, parent, ACCEPT, MPI_COMM_WORLD,&req);
                                for(int i=0;i<children.size();i++)
                                    if(children[i]!=mpi_status.MPI_SOURCE)
                                        MPI_Ibsend(a,3, MPI_INT, children[i], ACCEPT, MPI_COMM_WORLD,&req);}
                        debug_log("Ustalanie :D");
                    }
                break;
            }
            case ACCEPT:
            {
                accept *a = (accept *) buffer;
                if(parent!=NONE && parent!=mpi_status.MPI_SOURCE)
                    MPI_Ibsend(a,3, MPI_INT, parent, ACCEPT, MPI_COMM_WORLD,&req);
                    for(int i=0;i<children.size();i++)
                        if(children[i]!=mpi_status.MPI_SOURCE)
                            MPI_Ibsend(a,3, MPI_INT, children[i], ACCEPT, MPI_COMM_WORLD,&req);
                if(acceptorToken!=NONE)
                    debug_log("remove meeting");
                if(a->decision==TRUE)
                    if(a->meeting==id)
                    {
                        debug_log("moje spotkanie jest zaakceptowane");
                        duringMyMeeting=true;
                    }
                    else if(a->meeting==meeting)
                    {
                        debug_log("idę na spotkanie");
                    }
                else //a->decision==FALSE
                {
                        if(a->meeting==id)
                        {
                            debug_log("moje spotkanie jest odrzucone");
                            duringMyMeeting=true;
                        }
                        else if(a->meeting==meeting)
                        {
                            debug_log("ehh nie wyszło, jestem wolny");
                            meeting=NONE;
                        }
                }

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

void Opornik::getAcceptation(int p)
{
    askForAcceptation a;
    a.meeting=id;
    a.participants=p;

    MPI_Ibsend(&a,3, MPI_INT, id, ASKFORACCEPTATION, MPI_COMM_WORLD,&req);
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
        MPI_Ibsend(buffer,bcast.msgSize,MPI_INT,i,tag,MPI_COMM_WORLD,&req);

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
                        participantsOnMymeeting=info->participants;
                        debug_log("Na moje spotkanie przyjdzie %d oporników i użyjemy zasobu %d\n",info->participants,info->haveResource);
                        getAcceptation(participantsOnMymeeting);
                    }
                    else
                    {
                        debug_log("Jest %d chętnych na spotkanie, ale nie mamy zasobu\n",info->participants);
                        participantsOnMymeeting=info->participants;
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
                        getAcceptation(participantsOnMymeeting);
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
                participantsOnMymeeting=0;
                duringMyMeeting=false;
                debug_log("Wszyscy poszli już do domu po moim spotkaniu\n");
                break;
        }
    else
        MPI_Ibsend(buffer,bcast->msgSize,MPI_INT,bcast->respondTo,tag,MPI_COMM_WORLD,&req);
}
