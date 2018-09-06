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
    status = idle;
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
        MPI_Status mpi_status;
        order_makekids order;
        MPI_Recv(&order,2+MAX_CHILDREN,MPI_INT,MPI_ANY_SOURCE,ORDER_MAKEKIDS,MPI_COMM_WORLD,&mpi_status);
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

		if (status == blocked)
		{
            if (actionRand >= 995)
			{
                debug_log("Chcem, ale nie mogem! Jestem zablokowany!\n");
			}
			continue;
       	}
        else if (actionRand>=995)
            organizeMeeting();
//            continue;
        else if(actionRand>=990)
            endMeeting();
//            continue;
        else if (actionRand>=985 && acceptorToken!=NONE)
            ;//pass_acceptor();
   	 }

}

void *Opornik::listen_starter(void * arg)
{
	Opornik *op = (Opornik *)arg;
	op->listen();
}

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
				Msg_pass_acceptor msg = {buffer[0], buffer[1], buffer[2], buffer[3], buffer[4]};
				handleAcceptorMsg(mpi_status.MPI_SOURCE, msg);
				break;
			}
			case TAG_ACCEPTOR_CANDIDATE:
			{
				Msg_pass_acceptor msg = {buffer[0], buffer[1], buffer[2], buffer[3], buffer[4]};
				handleACandidateMsg(mpi_status.MPI_SOURCE, msg);
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

void Opornik::handleACandidateMsg(int sender, Msg_pass_acceptor msg)
{
	if (msg.initializator_id == id)
	{
		debug_log("Dostałem zgłoszenie na KANDYDATa do zmiany akceptora!\n");
	}
	else
	{
    	if (msg.distance == msg.target_distance && sender != parent && parent != NONE)
    	{
			debug_log("Przekazuję kandydata w GÓRĘ\n");
			// warto przekazać jedynie w górę; więc jeśli otrzymane od rodzica, to ignore
			msg.distance += 1;
        	MPI_Send(&msg, sizeof(msg)/sizeof(int), MPI_INT, parent, TAG_ACCEPTOR_CANDIDATE, MPI_COMM_WORLD);
    	}
    	else if (msg.distance > msg.target_distance)
    	{
			debug_log("Przekazuję kandydata w DÓŁ i potencjalnie w GÓRĘ\n");
        	// W górę; jeśli nie dostaliśmy od rodzica
        	msg.distance += 1;
        	if (parent != -1 && sender != parent)
        	{
            	MPI_Send(&msg, sizeof(msg)/sizeof(int), MPI_INT, parent, TAG_ACCEPTOR_CANDIDATE, MPI_COMM_WORLD);
        	}

        	// I w dół
        	msg.distance -= 2; // 2, ponieważ zwiększyliśmy na potrzeby wysłania do rodzica
        	if (children.size() > 0)
        	{
            	for (int i = 0; i < children.size(); i++)
            	{
					if (children[i] != sender)
					{
                		MPI_Send(&msg, sizeof(msg)/sizeof(int), MPI_INT, children[i], TAG_ACCEPTOR_CANDIDATE, MPI_COMM_WORLD);
					}
				}
        	}
    	}
    	else if (msg.distance < msg.target_distance)
    	{
        	if (sender == parent) // Dostałem tę wiadomość od rodzica (wiadomość idzie w dół)
        	{
            	 debug_log("Nikt niżej nie będzie mógł przekazać kandydata, a wiadomość dostałem od rodzica! Ignoruję wiadomość.\n");
        	}
        	else // Wiadomość idzie w górę, aby dotrzeć na inną gałąź
        	{
            	msg.distance += 1;
            	if (parent != -1)
            	{
                	MPI_Send(&msg, sizeof(msg)/sizeof(int), MPI_INT, parent, TAG_PASS_ACCEPTOR, MPI_COMM_WORLD);
            	}
    	    }
   	 	}
	}
}

void Opornik::handleAcceptorMsg(int sender, Msg_pass_acceptor msg)
{
	if (msg.distance == msg.target_distance && msg.initializator_id != id && status != busy)
	{
		// Uwaga! Możemy dostać to samo zgłoszenie kilka razy (sąsiedzi rozprowadzają je przez rodzica), niby status.busy częściowo rozwiązuje problem TODO
		status = busy;
		msg.candidate_id = id;
		msg.distance = (sender == parent) ? msg.distance + 1 : msg.distance - 1; 
		debug_log("(from %d) Jestem DOBRYM KANDYDATEM na akceptora, muszę o tym dać znać! dist = %d\n", sender, msg.distance);
		MPI_Send(&msg, sizeof(msg)/sizeof(int), MPI_INT, sender, TAG_ACCEPTOR_CANDIDATE, MPI_COMM_WORLD);
		debug_log("Wysłałem swoją kandydaturę do %d\n", sender);
		// Można by przekazać jeszcze wyżej i przeskoczyć na inne gałęzie lub do sąsiadów, ale nie robimy sobie konkurencji
	}
	else if (msg.distance > msg.target_distance)
	{
		// w dół i górę, ponieważ przez podanie do góry możemy dotrzeć do innej gałęzi
		debug_log("Muszę przekazać prośbę o zwolnienie akceptora W DÓŁ oraz potencjalnie W GÓRĘ! dist = %d\n", msg.distance);

		// W górę
		msg.distance += 1;
		if (parent != -1 && sender != parent)
        {
            MPI_Send(&msg, sizeof(msg)/sizeof(int), MPI_INT, parent, TAG_PASS_ACCEPTOR, MPI_COMM_WORLD);
        }

		// I w dół
		msg.distance -= 2; // 2, ponieważ zwiększyliśmy na potrzeby wysłania do rodzica
		if (children.size() > 0)
        {
        	for (int i = 0; i < children.size(); i++)
            {
				if (children[i] != sender)
                {
            		MPI_Send(&msg, sizeof(msg)/sizeof(int), MPI_INT, children[i], TAG_PASS_ACCEPTOR, MPI_COMM_WORLD);
				}
			}
        }
	}
	else if (msg.distance < msg.target_distance || msg.distance == msg.target_distance) // 2-gi warunek ponieważ potencjalny kandydat jest busy i nie może przyjąć kandydatury
	{
		if (sender == parent) // Dostałem tę wiadomość od rodzica (wiadomość idzie w dół)
		{
			debug_log("Nikt niżej nie będzie mógł zostać akceptorem, a wiadomość dostałem od rodzica! Ignoruję wiadomość. dist = %d\n", msg.distance);	
		}
		else // Wiadomość idzie w górę, aby dotrzeć na inną stronę drzewa
		{
			debug_log("Muszę przekazać prośbę o zwolnienie akceptora W GÓRĘ! dist = %d\n", msg.distance);
			msg.distance += 1;
			if (parent != -1)
            {
                MPI_Send(&msg, sizeof(msg)/sizeof(int), MPI_INT, parent, TAG_PASS_ACCEPTOR, MPI_COMM_WORLD);
            }
		}
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
				msg = {clock, id, NONE, 1, 1}; // distance = 1, bo przekazujemy w górę
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
			if (parent != -1)
            {
				msg = {clock, id, NONE, 1, -1};  // distance = 1, bo przekazujemy w górę

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
				msg = {clock, id, NONE, 1, 0};  // distance = 1, bo przekazujemy w górę

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
    if(id==meeting)//TODO: pomyśleć czy spotkanie musi się zacząć?
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
            if(meeting==NONE) // THEN: zgódź się :D
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

    bcast.waitingForResponse=sendTo.size();
    switch(tag)//inicjalizacja bufora broadcastu i wybór odbiorców
    {
        case INVITATION_MSG:
        {
            meetingInfo* sumaric=(meetingInfo*)bcast.buffer;
            //Send only to children
            sendTo.remove(parent);
            bcast.waitingForResponse=sendTo.size();

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
                        debug_log("Na moje spotkanie przyjdzie %d oporników i użyjemy zasobu %d\n",info->participants,info->haveResource);
                    else{
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
                        debug_log("Otrzymałem zasób %d\n",res->haveResource);
                    else
                        debug_log("Wszystkie zasoby są pozajmowane\n");
                break;
            }
            case ENDOFMEETING:
                resources.push_back(busyResource);
                busyResource=NONE;
                //TODO: timeout spotkaniowy "Następnie rozchodzą się i przez pewien czas nie biorą udziału w zebraniach."
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

