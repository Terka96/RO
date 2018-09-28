#ifndef MESSAGES_HPP
#define MESSAGES_HPP
#include"constants.hpp"

#define ORDER_MAKEKIDS 1
#define ACK_MAKEKIDS 2
#define INIT_RESOURES 3
#define INVITATION_MSG 4
#define RESOURCE_GATHER 5
#define ENDOFMEETING 6

#define ASKFORACCEPTATION 7
#define SHAREACCEPTOR 8
#define ACCEPT 9

#define TAG_FIND_LOWEST0 100
#define TAG_FIND_LOWEST1 101
#define TAG_FIND_LOWEST2 102
#define TAG_FIND_LOWEST3 103

#define TAG_PASS_ACCEPTOR 200
#define TAG_ACCEPTOR_CANDIDATE 201
#define TAG_ACCEPTOR_RESPONSE 202

/*zamówienie u konspiratora zrobienia dzieci w ilości count, przyjęcia postawy konspiratora i zapamiętania przełożonego i sąsiadów*/
struct order_makekids{
    int parent;
    int count;
    int neighbors[MAX_CHILDREN];
};

/*inicjalizacja/dystrybucja roli akceptora i zasobów*/
struct init_resources{
    int acceptorTokenId;
    int resourceCount;
    int resourceIds[NUM_RESOURCES];
};

/*zaproszenie na spotkanie*/
struct meetingInvitation{
    int clock;
    int uniqueTag;
    int meetingId;
    int participants;
    int haveResource;
};

/*zebranie zasobu*/
struct resourceGatherMsg{
    int clock;
    int uniqueTag;
    int haveResource;
};

/*zakończenie spotkania*/
struct endOfMeeting{
    int clock;
    int uniqueTag;
    int meetingId;
};

/**/
struct askForAcceptation{
    int clock;
    int meeting;
    int participants;
};

/**/
struct shareAcceptor{
    int clock;
    int acceptorToken;
    int meeting;
};

/**/
struct accept{
    int clock;
    int meeting;
    int decision;
};


// Wiadomość generowana do zmiany akceptora
struct Msg_pass_acceptor
{
	int clock; // zegar
	int initializator_id; // Id akceptora, który chce zostać zmieniony
	int candidate_id; // Id opornika, który może zostać nowym akceptorem (wiadomośc zwrotna)
	int distance;	// Aktualna różnica wysokości pomiędzy akceptorem, a kandydatem
	int target_distance; // -1: chcemy przekazać w dół, 0: ten sam poziom, 1: w górę
	int failure;
	int sender; // id opornika, od którego dostaliśmy wiadomość
	int tokenId; // id Tokena do przekazania
	int counter; // liczba oporników na spotkaniach
	int complete; // Czy nowy akceptor już wszystko ustawił
};

#endif // MESSAGES_H
