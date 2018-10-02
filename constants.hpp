#ifndef constants_hpp
#define constants_hpp

#define MAX_CHILDREN 4
#define MAX_BUFFER_SIZE 12800
#define MAX_MEETINGS_PARTICIPANTS 12

#define NUM_RESOURCES 3
#define NUM_ACCEPTORS 4
#define NUM_CONSPIR 16

/*value used to indicate null ids*/
#define NONE -1
#define TRUE 1
#define FALSE 2

#define MEETING_CLOCK_TIMEOUT 1000

#define MIN_CONSOLE_LOG 2
#define MIN_FILE_LOG 0

struct msgBcastInfo {
	int clock;
	int uniqueTag;
	int respondTo;
	int waitingForResponse;
	int msgSize;
	int buffer[64];
	bool operator == (const msgBcastInfo& x) {
		return x.uniqueTag == uniqueTag;
	}
};

struct MeetingInfo {
	int participants;
    int meetingClk;
	int acceptors[NUM_ACCEPTORS];
};

struct AcceptorInfo { //ogólnie, to zamieniłbym nazwy acceptorInfo z acceptorToken
	int id;
	int participants;
	MeetingInfo meetingInfo[NUM_CONSPIR]; // statycznie? dynamicznie? TODO
};


enum status_enum {
	idle = 0, //zamiast średników stosuje się przecinki
	busy = 1,
	blocked = 2
};

enum acceptor_enum {
	notAcceptor = 0,
	isAcceptor = 1,
	findingCandidates = 2,
	passingToken = 3,
	candidate = 4,
	accepted = 5
};

enum log_enum {
    debug = 0,
    trace = 1,
    info = 2,
    error = 3
};


#endif
