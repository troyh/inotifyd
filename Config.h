#include <string>

using namespace std;

class Config 
{
	string m_obj;
	bool m_recurse;
	string m_log;
public:
	Config() : m_recurse(false) {}
	~Config();
	
	void addWatch(const char* objname,bool recursive,const char* logfile);
};
