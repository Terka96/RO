#ifndef resource_hpp
#define resource_hpp
class Opornik;

class Resource{
public:
	Resource(){};
	Resource(int id, Opornik* o);
	
protected:
	Opornik *owner, *target;
	int id;
	bool available;
};

#endif
