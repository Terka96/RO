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

int inline Opornik::debug_log(const char* format, ...){
    int res;
    #ifdef DEBUGLOG
    printf("Node %d [%d]:",id,clock);
    va_list args;
    va_start (args, format);
    res = vprintf(format, args);
    va_end (args);
    #endif
    return res;
};

Opornik::Opornik(){
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    clock=0;
    blocked = false;
    acceptorToken=NONE;
    meeting=NONE;
    tagGeneratorCounter=0;
    makeTree();
    MPI_Barrier(MPI_COMM_WORLD);
    distributeAcceptorsAndResources();
    MPI_Barrier(MPI_COMM_WORLD);
}

void Opornik::makeTree(){
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    id=rank;

    srand(time(NULL)+id*size);//give every process (even on same machine) different random seed
    if(id==0)
    {
        parent=NONE;
        makeKids(size-1);
    }
    else
    {
        MPI_Status status;
        order_makekids order;
        MPI_Recv(&order,2+MAX_CHILDREN,MPI_INT,MPI_ANY_SOURCE,ORDER_MAKEKIDS,MPI_COMM_WORLD,&status);
        parent=order.parent;
        for(int i=0;i<MAX_CHILDREN;i++)
            if(order.neighbors[i]!=NONE && order.neighbors[i]!=id)
                neighbors.push_back(order.neighbors[i]);
        makeKids(order.count);
    }
}
void Opornik::distributeAcceptorsAndResources(){
    if(id==0){
        init_resources* table = new init_resources[size];
        for(int i=0;i<size;i++){
            table[i].acceptorTokenId=NONE;
            table[i].resourceCount=0;
        }
        for(int i=0;i<NUM_ACCEPTORS;i++){
            int randValue=rand()%size;
            if(table[randValue].acceptorTokenId!=NONE)
                continue;
            table[randValue].acceptorTokenId=i;
        }
        for(int i=0;i<NUM_RESOURCES;i++){
            int randValue=rand()%size;
            /*practically it isn't important if resource is book or dvd, but we can code that information in resource id*/
            if(rand()%2)
                table[randValue].resourceIds[table[randValue].resourceCount]=i*2;
            else
                table[randValue].resourceIds[table[randValue].resourceCount]=i*2+1;
            table[randValue].resourceCount++;
        }
        for(int i=1;i<size;i++){
            MPI_Send(&table[i],2+table[i].resourceCount,MPI_INT,i,INIT_RESOURES,MPI_COMM_WORLD);
        }
        if(table[0].acceptorTokenId!=NONE)
            acceptorToken=table[0].acceptorTokenId;
        for(int j=0;j<table[0].resourceCount;j++)
            resources.push_back(table[0].resourceIds[j]);
        delete[] table;
    }
    else
    {
        init_resources init;
        MPI_Recv(&init,2+NUM_RESOURCES,MPI_INT,0,INIT_RESOURES,MPI_COMM_WORLD,NULL);
        if(init.acceptorTokenId!=NONE)
            acceptorToken=init.acceptorTokenId;
        for(int i=0;i<init.resourceCount;i++)
            resources.push_back(init.resourceIds[i]);
    }
}

void Opornik::makeKids(int count){
    int grandchildrenCount=0;
    if(count>0)
    {
        int max_childs = (count<MAX_CHILDREN) ? count : MAX_CHILDREN;
        int rand_childs = (random()%max_childs) + 1;
        grandchildrenCount=count-rand_childs;
        //WARNING: STUPID STATIC CLEAR
        int grandchildrenInNodes[MAX_CHILDREN]={0,0,0,0};
        int childrenNodes[MAX_CHILDREN]={NONE,NONE,NONE,NONE};
        //Distribution of grandchildren
        for(int i=0;i<grandchildrenCount;i++)
            grandchildrenInNodes[random()%rand_childs]++;
        //calculating children nodes ids
        childrenNodes[0]=id+1;
        for(int i=1;i<rand_childs;i++)
            childrenNodes[i]=childrenNodes[i-1]+grandchildrenInNodes[i-1]+1;

        order_makekids order;
        order.parent=id;
        for(int i=0;i<MAX_CHILDREN;i++)
            order.neighbors[i]=childrenNodes[i];

        for(int i=0;i<rand_childs;i++){
            order.count=grandchildrenInNodes[i];
            debug_log("Sending order(%d) to node %d\n",grandchildrenInNodes[i],childrenNodes[i]);
            MPI_Bsend(&order,2+MAX_CHILDREN,MPI_INT,childrenNodes[i],ORDER_MAKEKIDS,MPI_COMM_WORLD);
            children.push_back(childrenNodes[i]);
        }
    }
}

void Opornik::run(){
	// https://stackoverflow.com/questions/12043057/cannot-convert-from-type-voidclassname-to-type-voidvoid
	std::thread thread_1(live_starter, this);
    std::thread thread_2(listen_starter, this);

    introduce();

	thread_1.join();
	thread_2.join();

}


void *Opornik::live_starter(void *arg)
{
	Opornik *op = (Opornik *)arg;
    	op->live();
}

void Opornik::live()
{
	while (true)
   	{
        usleep(100000);//0.1 sec

		int actionRand=rand()%1001; //promilowy podział prawdopodobieństwa dla pojedynczego procesu co sekundę

		if (blocked)
		{
            if (actionRand >= 995)
			{
                debug_log("Chcem, ale nie mogem! Jestem zablokowany!\n");
			}
			continue;
       	}
        else if (actionRand>=995)
            organizeMeeting();
        else if(actionRand>=990)
            endMeeting();
        else if (actionRand>=985 && acceptorToken!=NONE)
        	pass_acceptor();
   	 }

}

void *Opornik::listen_starter(void * arg)
{
	Opornik *op = (Opornik *)arg;
	op->listen();
}

void Opornik::listen()
{
	// blocked = true;

	int buffer[MAX_BUFFER_SIZE];
	MPI_Status status;

	while (true)
	{
		MPI_Recv(&buffer, MAX_BUFFER_SIZE, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);

		switch (status.MPI_TAG)
		{
			case TAG_PASS_ACCEPTOR:
				{
					Msg_pass_acceptor msg = {buffer[0], buffer[1], buffer[2], buffer[3], buffer[4]};
					handleAcceptorMsg(status.MPI_SOURCE, msg);
					break;
				}
            case INVITATION_MSG:
            case ENDOFMEETING:
                {
                    bool exist=false;
                    for(std::list<msgBcastInfo>::iterator x=bcasts.begin();x!=bcasts.end();x++)
                        if(buffer[0]==x->uniqueTag){
                            exist=true;
                            receiveResponseMsg(buffer,status.MPI_TAG,&(*x));
                            break;
                        }
                    if(!exist)
                        receiveForwardMsg(buffer,status.MPI_TAG,status.MPI_SOURCE);
                    break;
                }
			default:
				{
					debug_log("Otrzymano nieznany typ wiadomości\n");
				}
		}	
	}
}

void Opornik::handleAcceptorMsg(int sender, Msg_pass_acceptor msg)
{
	if (msg.distance == msg.target_distance)
	{
		debug_log("Jestem DOBRYM KANDYDATEM na akceptora, muszę o tym dać znać!\n");
	}
	else if (msg.distance > msg.target_distance)
	{
		debug_log("Muszę przekazać prośbę o zwolnienie akceptora W DÓŁ!\n");
	}
	else // (msg.target_distance > msg.distance)
	{
		if (sender == parent) // Dostałem tę wiadomość od rodzica (wiadomość idzie w dół)
		{
			debug_log("Nikt niżej nie będzie mógł zostać akceptorem, a wiadomość dostałem od rodzica! Ignoruję wiadomość.\n");	
		}
		else // Wiadomość idzie w górę, aby dotrzeć na inną stronę drzewa
		{
			debug_log("Muszę przekazać prośbę o zwolnienie akceptora W GÓRĘ!\n");

		}
	}
}

void Opornik::pass_acceptor()
{
	debug_log("Nie chcę już być akceptorem!\n");

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
				msg = {clock, id, NONE, 1, 1}; // distance = 1, bo przekazujemy w górę
				// Wystarczy przekazać w górę
				MPI_Send(&msg, sizeof(msg)/sizeof(int), MPI_INT, parent, TAG_PASS_ACCEPTOR, MPI_COMM_WORLD);
				// MPI_recv()
			}
			else
			{
				debug_log("Nie mogę przekazać akceptora w górę, jestem na szczycie!\n");
			}
		}
		else if (rand < 20) //dol
		{
			if (parent != -1)
            {
				msg = {clock, id, NONE, 1, -1};  // distance = 1, bo przekazujemy w górę

				// Trzeba przekazać w górę i do dzieci
				debug_log("Chcę przkazać akceptora w dół!\n");
				MPI_Send(&msg, sizeof(msg)/sizeof(int), MPI_INT, parent, TAG_PASS_ACCEPTOR, MPI_COMM_WORLD);
				// MPI_recv()
			}
            if (children.size() > 0)
			{
                for (int i = 0; i < children.size(); i++)
				{
                    MPI_Send(&msg, sizeof(msg)/sizeof(int), MPI_INT, children[i], TAG_PASS_ACCEPTOR, MPI_COMM_WORLD);
					// MPI_recv()

				}
			}

		}
		else //ten sam poziom
		{
			if (parent != -1)
            {
				msg = {clock, id, NONE, 1, 0};  // distance = 1, bo przekazujemy w górę

				//Wystarczy przekazać w górę
				debug_log("Chcę przekazać akceptora na swoim poziomie!\n");
				MPI_Send(&msg, sizeof(msg)/sizeof(int), MPI_INT, parent, TAG_PASS_ACCEPTOR, MPI_COMM_WORLD);
				// MPI_recv()
			}
            else
            {
            	debug_log("Nie mogę przekazać na ten sam poziom, jestem na szczycie!\n");
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
        info.haveResource=NONE;

        receiveForwardMsg((int*)(&info),INVITATION_MSG,id);
    }
}

void Opornik::endMeeting(){
    if(id==meeting)
    {
        debug_log("Rozejść się!\n");
        endOfMeeting end;
        end.uniqueTag=generateUniqueTag();
        end.meetingId=id;
        receiveForwardMsg((int*)(&end),ENDOFMEETING,id);
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
            if(meeting==NONE) // THEN: zgódź się :D
            {
                //debug_log("Zaproszono mnie do spotkania %d\n",info->meetingId);
                if(rand()%4>0)// 75% szans na dołączenie do spotkania
                    meeting=info->meetingId;
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

            if(bcast->waitingForResponse<=0)//jeżeli dostałeś już odpowiedzi od wszystkich
            {
                if(meeting==info->meetingId)
                    sumaric->participants++;
                info->participants=sumaric->participants;
            }
            break;
        }
        case ENDOFMEETING:
        if(bcast->waitingForResponse<=0)//jeżeli dostałeś już odpowiedzi od wszystkich
        {
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
    msgBcastInfo bcast;
    bcast.uniqueTag=buffer[0];
    bcast.respondTo=source;
    bcast.msgSize=msgSize;
    bcast.waitingForResponse=children.size();
    if(parent!=NONE)
        bcast.waitingForResponse++;
    if(source!=id)
        bcast.waitingForResponse--;

    switch(tag)//inicjalizacja bufora broadcastu
    {
        case INVITATION_MSG:
        {
            meetingInfo* sumaric=(meetingInfo*)bcast.buffer;
            sumaric->participants=0;
            break;
        }
        case ENDOFMEETING:
            //Nothing special
            break;
    }
    bcasts.push_back(bcast);

    if(bcast.waitingForResponse==0) //jeżeli to już liść
        receiveResponseMsg(buffer,tag,&bcasts.back());

    for(int i=0;i<children.size();i++)
        if(children[i]!=source)
            MPI_Send(buffer,bcast.msgSize,MPI_INT,children[i],tag,MPI_COMM_WORLD);
    if(parent!=NONE && parent!=source)
        MPI_Send(buffer,bcast.msgSize,MPI_INT,parent,tag,MPI_COMM_WORLD);
}

void Opornik::sendResponseMsg(int* buffer,int tag,msgBcastInfo* bcast){
    if(id==bcast->respondTo)//Jeżeli odpowiedź dotarła do inicjatora
        switch (tag)
        {
            case INVITATION_MSG:
            {
                meetingInfo* info=(meetingInfo*)buffer;
                    debug_log("Na moje spotkanie przyjdzie %d oporników\n",info->participants);
                break;
            }
        case ENDOFMEETING:
                debug_log("Wszyscy poszli już do domu po moim spotkaniu\n");
                break;
        }
    else
        MPI_Send(buffer,bcast->msgSize,MPI_INT,bcast->respondTo,tag,MPI_COMM_WORLD);
}

void Opornik::introduce(){
    std::string info = "Node " +std::to_string(id) + ":Hello, my parent is: " + std::to_string(parent);
    if(resources.size()>0){
       info+= " I have: ";
        for(int i=0;i<resources.size();i++)
            if(resources[i]%2)
                info+= "book(" + std::to_string(resources[i])+") ";
            else
                info+= "dvd(" + std::to_string(resources[i])+") ";
    }
    if(neighbors.size()>0)
        info+= " and my neighbors are";
    for(int i=0; i<neighbors.size();i++)
        info+=" " + std::to_string(neighbors[i]);
    if(acceptorToken!=NONE)
        info+= "  I'm acceptor("+std::to_string(acceptorToken)+")";
    printf("%s\n",info.c_str());
}

