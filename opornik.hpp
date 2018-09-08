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
    void resourceGather();
    void endMeeting();
   	void pass_acceptor();

   	static void *live_starter(void * arg);  
   	static void *listen_starter(void * arg);
   	void live();
   	void listen();

	void handleAcceptorMsg(int sender, Msg_pass_acceptor msg);
	void handleACandidateMsg(int sender, Msg_pass_acceptor msg);
	void handleAResponseMsg(int sender, Msg_pass_acceptor msg);

	void acceptorMsgSend(Msg_pass_acceptor msg, int sender); // Odpowiedź, jeśli jesteśmy dobrym kandydatem na akceptora
	void basicAcceptorSend(Msg_pass_acceptor msg, int sender, int tag); // Odpowiedź wykorzystywana w każdym z handleA*. (przekazuje wiadomośc dalej w drzewie, bo dany opornik jest nieznaczący)

    void receiveForwardMsg(int*,int,int);
    void receiveResponseMsg(int*,int,msgBcastInfo*);
    void sendForwardMsg(int*,int,int,int);
    void sendResponseMsg(int*,int,msgBcastInfo*);

	void setStatus(status_enum);


    int id;
    int size;
    int parent;
    int clock;
	int acceptorToken;
    acceptor_enum acceptorStatus;
	status_enum status;
    int meeting;                         //przechowuje id spotkania w którym uczestniczy
    int tagGeneratorCounter;             //licznik do generowania unikalnego id
    int busyResource;
    int meetingTimeout;
    bool duringMyMeeting;
    std::vector<int> neighbors;
    std::vector<int> children;
    std::vector<int> resources;
    std::list<msgBcastInfo> bcasts;

	std::vector<Msg_pass_acceptor> passAcceptorMsg_vector; // kolejka otrzymanych próśb o zmianę akceptora, podczas gdy byliśmy "busy"

    int inline debug_log(const char* format, ...);
};

#endif
