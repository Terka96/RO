#ifndef MESSAGES_H
#define MESSAGES_H
#include"constants.hpp"

#define ORDER_MAKEKIDS 1
#define ACK_MAKEKIDS 2
#define INIT_RESOURES 3

// 100+ to wiadomości zwykłego typu

// 200+ to wiadomości typu broadcast
#define TAG_PASS_ACCEPTOR 200

/*zamówienie u konspiratora zrobienia dzieci w ilości count, przyjęcia postawy konspiratora i zapamiętania przełożonego i sąsiadów*/
struct order_makekids{
    int parent;
    int count;
    int neighbors[MAX_CHILDREN];
};

/*potwierdzenie ukończenia tworzenia dzieci*/
struct ack_makekids {
    int count;
};

/*inicjalizacja/dystrybucja roli akceptora i zasobów*/
struct init_resources{
    int acceptorTokenId;
    int resourceCount;
    int resourceIds[NUM_RESOURCES];
};

// Wiadomość generowana do zmiany akceptora
struct Msg_pass_acceptor
{
	int clock; // zegar
	int initializaotr_id; // Id akceptora, który chce zostać zmieniony
	int candidate_id; // Id opornika, który może zostać nowym akceptorem (wiadomośc zwrotna)
	int distance;	// Aktualna różnica wysokości pomiędzy akceptorem, a kandydatem
	int target_distance; // -1: chcemy przekazać w dół, 0: ten sam poziom, 1: w górę
};

#endif // MESSAGES_H
