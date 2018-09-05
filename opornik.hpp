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
    void organizeMeeting();
    void endMeeting();

    void receiveForwardMsg(int*,int,int);
    void receiveResponseMsg(int*,int,msgBcastInfo*);
    void sendForwardMsg(int*,int,int,int);
    void sendResponseMsg(int*,int,msgBcastInfo*);


    int id;
    int size;
    int parent;
    int clock;
    int acceptorToken;
    int meeting;                         //przechowuje id spotkania w którym uczestniczy
    int tagGeneratorCounter;             //licznik do generowania unikalnego id
    std::vector<int> neighbors;
    std::vector<int> children;
    std::vector<int> resources;
    std::list<msgBcastInfo> bcasts;

    int inline debug_log(const char* format, ...);
};

#endif
