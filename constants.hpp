#ifndef constants_hpp
#define constants_hpp

#define MAX_CHILDREN 4
#define MAX_BUFFER_SIZE 10000

#define NUM_RESOURCES 6
#define NUM_ACCEPTORS 4
#define NUM_CONSPIR 16

/*value used to indicate null ids*/
#define NONE -1
#define TRUE 1
#define FALSE 2

struct msgBcastInfo{
    int uniqueTag;
    int respondTo;
    int waitingForResponse;
    int msgSize;
    int buffer[MAX_BUFFER_SIZE];
    bool operator ==(const msgBcastInfo& x) {
        return x.uniqueTag==uniqueTag;
    }
};

struct MeetingInfo{
    int meeting;
    int participants;
    int priority;
    int acceptors[NUM_ACCEPTORS];
};

struct AcceptorInfo //ogólnie, to zamieniłbym nazwy acceptorInfo z acceptorToken
{
	int id;
	int participants;
	MeetingInfo meetingInfo[NUM_CONSPIR]; // statycznie? dynamicznie? TODO
};


enum status_enum
{
    idle = 0, //zamiast średników stosuje się przecinki
    busy = 1,
    blocked = 2
};

enum acceptor_enum
{
	notAcceptor = 0,
	isAcceptor = 1,
	findingCandidates = 2,
	passingToken = 3,
	candidate = 4,
	accepted = 5
};



#endif
