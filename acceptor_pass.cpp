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

void Opornik::basicAcceptorSend (Msg_pass_acceptor msg, int sender, int tag) {
	if (msg.distance == msg.target_distance && sender != parent && parent != NONE) {
		msg.distance += 1;
		Ibsend (&msg, sizeof (msg) / sizeof (int), parent, tag);
	}
	else if (msg.distance > msg.target_distance) {
		msg.distance += 1;
		if (parent != -1 && sender != parent) {
			Ibsend (&msg, sizeof (msg) / sizeof (int), parent, tag);
		}
		// I w dół
		msg.distance -= 2; // 2, ponieważ zwiększyliśmy na potrzeby wysłania do rodzica
		if (children.size() > 0) {
			for (int i = 0; i < children.size(); i++) {
				if (children[i] != sender) {
					Ibsend (&msg, sizeof (msg) / sizeof (int), children[i], tag);
				}
			}
		}
	}
	else if (msg.distance < msg.target_distance) {
		// if (sender == parent) // Dostałem tę wiadomość od rodzica (wiadomość idzie w dół)
		if (sender != parent) { // Wiadomość idzie w górę, aby dotrzeć na inną gałąź
			msg.distance += 1;
			if (parent != -1) {
				Ibsend (&msg, sizeof (msg) / sizeof (int), parent, tag);
			}
		}
	}
}

void Opornik::basicAcceptorSend (Msg_pass_acceptor_final final_msg, int sender, int tag) {
	// debug_log("%d %d %d %d %d\n", msg.initializator_id, sender, tag, msg.distance, msg.target_distance);
	if (final_msg.msg.distance == final_msg.msg.target_distance && sender != parent && parent != NONE) {
		final_msg.msg.distance += 1;
		Ibsend (&final_msg, sizeof (final_msg) / sizeof (int), parent, tag);
	}
	else if (final_msg.msg.distance > final_msg.msg.target_distance) {
		final_msg.msg.distance += 1;
		if (parent != -1 && sender != parent) {
			Ibsend (&final_msg, sizeof (final_msg) / sizeof (int), parent, tag);
		}
		// I w dół
		final_msg.msg.distance -= 2; // 2, ponieważ zwiększyliśmy na potrzeby wysłania do rodzica
		if (children.size() > 0) {
			for (int i = 0; i < children.size(); i++) {
				if (children[i] != sender) {
					Ibsend (&final_msg, sizeof (final_msg) / sizeof (int), children[i], tag);
				}
			}
		}
	}
	else if (final_msg.msg.distance < final_msg.msg.target_distance) {
		if (sender == parent) { // Dostałem tę wiadomość od rodzica (wiadomość idzie w dół)
		}
		else { // Wiadomość idzie w górę, aby dotrzeć na inną gałąź
			final_msg.msg.distance += 1;
			if (parent != -1) {
				Ibsend (&final_msg, sizeof (final_msg) / sizeof (int), parent, tag);
			}
		}
	}
}

void Opornik::handleAResponseMsg (int sender, Msg_pass_acceptor_final msg) {
	if (msg.msg.candidate_id == id) {
		// Chyba od razu można przypisać akceptora. Grunt, żeby stary akceptor odblokował się i usunął dopiero po otrzymaniu odp. zwrotnej
		if (msg.msg.failure == 0) {
			// acceptorToken = accepted;
			log (info, "Zostałem NOWYM AKCEPTOREM (%d)!\n", msg.msg.tokenId);
			// Uzupełnienie wartości (nowy akceptor)
			acceptorToken = msg.msg.tokenId;
			acceptorStatus = isAcceptor;
			msg.msg.complete = 1;
            checkDecisions();
			acceptorInfo = msg.acceptorInfo; // przypisanie tablicy informacjami o akceptowaniu spotkań
			//TODO zapisanie liczby uczestników na spotkaniach
			msg.msg.distance = (sender == parent) ? msg.msg.distance + 1 : msg.msg.distance - 1;
			Ibsend (&msg, sizeof (msg) / sizeof (int), sender, TAG_ACCEPTOR_RESPONSE);
		}
		else {
			setStatus (idle);
			acceptorStatus = notAcceptor;
			acceptorToken = NONE;
			log (trace, "Zostałem ODRZUCONY na nowego akceptora.\n");
		}
	}
	else if (msg.msg.initializator_id == id && acceptorStatus != candidate && msg.msg.complete == 1) { // stary akceptor dostał odpowiedź od nowego
		// TODO 1: tutaj być może trzeba poczekać na eventy związane ze spotkaniami. (oporniki mogą kierować wiadomości do starego opornika.)
		// TODO 2: Nie wiem jak działają spotkania, ale jeśli opornik ma status "busy", to nie powinien dawać odpowiedzi, czy jest akceptorem, tylko poczekać do zmiany statusu na "idle".
		log (info, "Uff, już NIE jestem akceptorem. Został nim %d)\n", msg.msg.candidate_id);
		acceptorToken = NONE;
		setStatus (idle);
	}
	else {
		basicAcceptorSend (msg, sender, TAG_ACCEPTOR_RESPONSE);
	}
}
void Opornik::handleACandidateMsg (int sender, Msg_pass_acceptor msg) {
	if (msg.initializator_id == id && acceptorStatus != candidate && msg.failure == 0) { //msg.Failure oznacza, że dany kandydat jest już akceptorem
		candidatesAnswers++;
		msg.distance = (sender == parent) ? msg.distance + 1 : msg.distance - 1;
		if (acceptorStatus == findingCandidates) {
			acceptorStatus = passingToken;
			log (trace, "Dostałem PIERWSZE zgłoszenie na KANDYDATa do zmiany akceptora!\n Nowym akceptorem zostanie: %d!\n", msg.candidate_id);
			//przekaż dobrą nowinę kandydatowi (wiadomośc zwrotna)
			msg.failure = 0;
			Msg_pass_acceptor_final final_msg{msg, acceptorInfo};
			Ibsend (&final_msg, sizeof (final_msg) / sizeof (int), sender, TAG_ACCEPTOR_RESPONSE);
		}
		else {
			msg.failure = 1;
			log (trace, "Dostałem Kolejne zgłoszenie na KANDYDATa do zmiany akceptora. Niestety, nie możesz nim zostać: %d\n", msg.candidate_id);
			Ibsend (&msg, sizeof (msg) / sizeof (int), sender, TAG_ACCEPTOR_RESPONSE);
		}
	}
	else if (msg.initializator_id == id && acceptorStatus != candidate  && msg.failure == 1) {
		if (candidatesAnswers >= sameLevelNodes - 1) { // możliwe, że dostaniemy jakieś stare wiadomości. Nie ma to większego znaczenia, bo spróbujemy przekazać token jeszcze raz. Może się jednak wydawać niezbyt właściwe.
			log (info, "Wszyscy kandydaci na akceptorów również byli akceptorami. Muszę spróbować jeszcze raz\n");
			pass_acceptor (true); //musisz spróbować przekazać jeszcze raz od nowa, bo kandydaci byli zajęci (np. byli także akceptorami)
		}
	}
	else {
		basicAcceptorSend (msg, sender, TAG_ACCEPTOR_CANDIDATE);
	}
}

void Opornik::handleAcceptorMsg (int sender, Msg_pass_acceptor msg) {
	if (msg.distance == msg.target_distance && msg.initializator_id != id) {
		if (status == idle) {
			if (acceptorToken != NONE) {
				msg.failure = 1; // jesteśmy już akceptorem, jesteśmy dobrym kandydatem, ale nie możemy zostać podwójnym akceptorem.
			}
			acceptorMsgSend (msg, sender);
		}
		else {
			msg.sender = sender;
			passAcceptorMsg_vector.push_back (msg);
		}
	}
	else {
		basicAcceptorSend (msg, sender, TAG_PASS_ACCEPTOR);
	}
}

void Opornik::acceptorMsgSend (Msg_pass_acceptor msg, int sender) {
	// Uwaga! Możemy dostać to samo zgłoszenie kilka razy (sąsiedzi rozprowadzają je przez rodzica), niby status.busy częściowo rozwiązuje problem TODO
	msg.candidate_id = id;
	msg.distance = (sender == parent) ? msg.distance + 1 : msg.distance - 1;
	if (acceptorToken == NONE) {
		status = busy;
		acceptorStatus = candidate;
		acceptorToken = msg.tokenId; //to przypisanie, aby podczas przekazywania wlasciwego tokena nie uciekly wiadomosci o organizowanie spotkan
		// Od teraz opornik musi zbierac wszystkie zgloszenia dla akceptora o id tokenId
		log (trace, "(from %d) Jestem DOBRYM KANDYDATEM na akceptora, muszę o tym dać znać do (%d)\n", sender, msg.initializator_id);
	}
	else {
		log (trace, "(from %d) Byłbym DOBRYM KANDYDATEM na akceptora, niestety mam już WŁASNY TOKEN. Dam znać (%d)\n", sender, msg.initializator_id);
	}
	Ibsend (&msg, sizeof (msg) / sizeof (int), sender, TAG_ACCEPTOR_CANDIDATE);
	// Można by przekazać jeszcze wyżej i przeskoczyć na inne gałęzie lub do sąsiadów, ale nie robimy sobie konkurencji
}

void Opornik::pass_acceptor() {
	pass_acceptor (false);
}

void Opornik::pass_acceptor (bool force) {
	log (trace, "Nie chcę już być akceptorem!\n");
	status = blocked;
	// losowanie kierunku przekazania akceptora
	int rand = random() % 100;
	int new_acceptor;
	int buffer[MAX_BUFFER_SIZE];
	bool failure = false;
	candidatesAnswers = 0;
	Msg_pass_acceptor msg;
	// todo: iteracja po całym drzewie i dodanie kandydatów do vectora, następnie wylosowanie kandydata i próba przekazania akceptora
	// trzeba dodać licznik, któr będzie zwiększany gdy wiadomośc pójdzie w górę, zmniejszany kiedy w dół. W ten sposób będzie wiadomo, kto może zostać nowym akceptorem.
	// 0 - ten sam poziom
	// 1 - wyższy
	// -1 - niższy
	// (-inf; +inf)\{-1,0,1} zostają pominięte
	if (rand < 10) { //gora
		log (trace, "Chcę przekazać akceptora w górę!\n");
		if (parent != -1) {
			acceptorStatus = findingCandidates;
			msg = {clock, id, NONE, 1, 1, 0}; // distance = 1, bo przekazujemy w górę
			// Wystarczy przekazać w górę
			Ibsend (&msg, sizeof (msg) / sizeof (int), parent, TAG_PASS_ACCEPTOR);
		}
		else {
			log (trace, "Nie mogę przekazać akceptora w górę, jestem na szczycie!\n");
			failure = true;
		}
	}
	else if (rand < 20) { //dol
		if (!lowest) {
			acceptorStatus = findingCandidates;
			if (parent != -1) {
				msg = {clock, id, NONE, 1, -1, 0};  // distance = 1, bo przekazujemy w górę
				// Trzeba przekazać w górę i do dzieci
				log (trace, "Chcę przkazać akceptora w dół!\n");
				Ibsend (&msg, sizeof (msg) / sizeof (int), parent, TAG_PASS_ACCEPTOR);
			}
			if (children.size() > 0) {
				msg = {clock, id, NONE, -1, -1, 0};
				for (int i = 0; i < children.size(); i++) {
					Ibsend (&msg, sizeof (msg) / sizeof (int), children[i], TAG_PASS_ACCEPTOR);
				}
			}
		}
		else {
			log (trace, "NIE mogę przekazać NIŻEJ, jestem NAJNIŻEJ w hierarchii.\n");
			failure = true;
		}
	}
	else { //ten sam poziom
		if (sameLevelNodes < 2) {
			log (trace, "Nie mogę przekazać na ten sam poziom, jestem JEDYNY na tym szczeblu!\n");
			failure = true;
		}
		else if (parent != -1) {
			acceptorStatus = findingCandidates;
			msg = {clock, id, NONE, 1, 0, 0};  // distance = 1, bo przekazujemy w górę
			//Wystarczy przekazać w górę
			log (trace, "Chcę przekazać akceptora na swoim poziomie!\n");
			Ibsend (&msg, sizeof (msg) / sizeof (int), parent, TAG_PASS_ACCEPTOR);
		}
		else {
			log (trace, "Nie mogę przekazać na ten sam poziom, jestem na szczycie!\n");
			failure = true;
		}
	}
	if (failure && force) {
		status = idle; // po zaimplementowaniu właściwego rozwiązania, to jest do usunięcia
		// pass_acceptor (true); // Tak nie może być, bo dostajemy kilka tysięcy żądań w sekundę
		// TODO tutaj w najgorszym wypadku trzeba dać delay. Najlepiej, jakby był jakiś vector mówiący, że chcieliśmy się zmienić
	}
	else if (!failure || !force) {
		status = idle;
	}
}
