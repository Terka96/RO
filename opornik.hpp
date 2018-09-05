#include <vector>
#include <list>
#ifndef opornik_hpp
#define opornik_hpp
#include"messages.h"

class Opornik{
public:
    Opornik();
    void run();
    void introduce();

private:
    void makeTree();
    void makeKids(int count);
    void distributeAcceptorsAndResources();
    int generateUniqueTag(){tagGeneratorCounter++; return size*tagGeneratorCounter+id;};

    void organizeMeeting();
    void endMeeting();
   	void pass_acceptor();

   	static void *live_starter(void * arg);  
   	static void *listen_starter(void * arg);
   	void live();
   	void listen();

	void handleAcceptorMsg(int sender, Msg_pass_acceptor msg);

    void receiveForwardMsg(int*,int,int);
    void receiveResponseMsg(int*,int,msgBcastInfo*);
    void sendForwardMsg(int*,int,int,int);
    void sendResponseMsg(int*,int,msgBcastInfo*);


    int id;
    int size;
    int parent;
    int clock;
    int acceptorToken;
	bool blocked;
    int meeting;                         //przechowuje id spotkania w którym uczestniczy
    int tagGeneratorCounter;             //licznik do generowania unikalnego id
    std::vector<int> neighbors;
    std::vector<int> children;
    std::vector<int> resources;
    std::list<msgBcastInfo> bcasts;

    int inline debug_log(const char* format, ...);
};

#endif
