#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <string>
#include <errno.h>
#include <iostream>
#include <fstream>
#include <map>
#include <set>
#include <sys/inotify.h>
#include <sys/stat.h>
#include <linux/limits.h>
#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/path.hpp>
#include <libxml/parser.h>
#include <libxml/xpath.h>

using namespace std;

class FileWatch
{
	string m_obj;
	bool m_recurse;
	string m_log;
public:
	FileWatch();
	FileWatch(const char* objname,bool recursive,const char* logfile);
	FileWatch(const FileWatch& fw);
	~FileWatch() {}
	
	string objname() const { return m_obj; }
	bool recursive() const { return m_recurse; }
	string logfile() const { return m_log; }
	
	bool operator<(const FileWatch& fw) const;
	FileWatch& operator=(const FileWatch& fw);
};

FileWatch::FileWatch() 
	: m_recurse(false)
{
}

FileWatch::FileWatch(const FileWatch& fw) 
	: m_obj(fw.m_obj), m_recurse(fw.m_recurse), m_log(fw.m_log)
{
}

FileWatch::FileWatch(const char* objname,bool recursive,const char* logfile) 
	: m_obj(objname), m_recurse(recursive), m_log(logfile)
{
}

bool FileWatch::operator<(const FileWatch& fw) const
{
	return m_obj < fw.m_obj;
}

FileWatch& FileWatch::operator=(const FileWatch& fw) 
{
	m_obj=fw.m_obj;
	m_recurse=fw.m_recurse;
	m_log=fw.m_log;
	return *this;
}

ostream& operator<<(ostream& os, FileWatch& fw) 
{
	os << fw.objname() << " (" << (fw.recursive()?"yes":"no") << ") -> " << fw.logfile();
	return os;
}


namespace fs=boost::filesystem;

static const char* const CONFIG_FILE="/etc/inotify.conf";
static const char* const PIDFILE="/var/run/inotifyd.pid";
const uint32_t EVENTS_WE_CARE_ABOUT=(IN_CLOSE_WRITE|IN_CREATE|IN_DELETE|IN_DELETE_SELF|IN_MODIFY|IN_MOVE_SELF|IN_MOVED_FROM|IN_MOVED_TO);

static bool g_sighup=false;
static bool g_quit=false;

void sig_handler(int signal) 
{
	switch (signal) 
	{
		case SIGHUP:
			g_sighup=true;
			break;
		case SIGINT:
			g_sighup=true;
			g_quit=true;
			break;
		default:
			// cout << "Uncaught signal:" << signal << endl;
			break;
	}
}

int read_config(const char* fname,set<FileWatch>& cfg) 
{
	xmlDocPtr doc;
	xmlNodePtr cur;
	doc=xmlParseFile(fname);
	if (doc==NULL) 
	{
		return 1;
	}
	xmlXPathContextPtr context = xmlXPathNewContext(doc);
	if (context == NULL) {
		return 2;
	}
	xmlXPathObjectPtr obj = xmlXPathEvalExpression((xmlChar*)"//watches/watch", context);
	xmlXPathFreeContext(context);
	if (obj == NULL) {
		return 3;
	}
	if(xmlXPathNodeSetIsEmpty(obj->nodesetval)){
		xmlXPathFreeObject(obj);
		return 4;
	}
	
	xmlNodeSetPtr nodeset=obj->nodesetval;
	for(size_t i = 0; i < nodeset->nodeNr; ++i) // Iterate <watch> elements
	{
		xmlChar* objname=0;
		xmlChar* recursive=0;
		xmlChar* logfile=0;
		
		for (xmlNodePtr node=nodeset->nodeTab[i]->xmlChildrenNode;node;node=node->next)
		{
			if (!xmlStrcmp(node->name,(const xmlChar*)"object"))
			{
				objname=xmlNodeListGetString(doc,node->xmlChildrenNode,1);
				recursive=xmlGetProp(node,(const xmlChar*)"recursive");
			}
			else if (!xmlStrcmp(node->name,(const xmlChar*)"logfile"))
			{
				logfile=xmlNodeListGetString(doc,node->xmlChildrenNode,1);
			}
		}
		
		if (objname && logfile)
		{
			FileWatch watch((char*)objname,!xmlStrcmp(recursive,(const xmlChar*)"yes")?true:false,(char*)logfile);
			cfg.insert(watch);
		}
		else
		{
			// TODO: Report malformed config file
		}
	}
	
	xmlXPathFreeObject(obj);
	xmlFreeDoc(doc);
	xmlCleanupParser();
	
	return 0;
}

size_t watchsubdirectories(int fd,const FileWatch& watch,map<int,FileWatch>& wds)
{
	fs::directory_iterator end_itr;
	for (fs::directory_iterator itr(watch.objname()); itr!=end_itr; ++itr)
	{
		if (fs::is_directory(itr->status()))
		{
			// cout << itr->path().string() << endl;
			int wd=inotify_add_watch(fd,itr->path().string().c_str(),EVENTS_WE_CARE_ABOUT);
			wds.insert(make_pair(wd,FileWatch(itr->path().string().c_str(),watch.recursive(),watch.logfile().c_str())));
			
			FileWatch subwatch(itr->path().string().c_str(),watch.recursive(),watch.logfile().c_str());
			watchsubdirectories(fd,subwatch,wds);
		}
	}
}


int main(int argc,char* argv[])
{
	bool bNoDaemon=false;
	
	// TODO: provide for these arguments:
	// --nodaemon to not run as a daemon
	// --config to specify an alternate config file
	
	set<FileWatch> cfg;
	read_config(CONFIG_FILE,cfg);
	
	// Check config for correctness
	for (set<FileWatch>::iterator itr=cfg.begin(); itr!=cfg.end(); ++itr)
	{
		struct stat statbuf;
		if (stat((*itr).objname().c_str(),&statbuf))
		{
			cerr << "Cannot stat " << (*itr).objname() << endl;
			cfg.erase(*itr);
		}
	}
	
	if (cfg.empty())
	{
		cerr << "No objects to watch. Not running." << endl;
		return 2;
	}
	
	if (!bNoDaemon) 
	{
		int pid=fork();
		if (pid)  // Child started, we can quit
		{
			// Write pid to PIDFILE
			ofstream pidfile(PIDFILE);
			pidfile << pid << endl;
			return 0;
		}
		else if (pid<0) // fork() failed
		{
			cerr << "Unable to daemonize. (" << errno << ")" << endl;
			return 1;
		}
	}
	
	struct sigaction sig_data=
	{
		sig_handler, // sa_handler
		0, // sa_sigaction
		0, // sa_mask
		0, // sa_flags
		0, // sa_restorer (obsolete)
	};
	
	if (sigaction(SIGHUP,&sig_data,NULL))
		throw exception();
	if (sigaction(SIGINT,&sig_data,NULL))
		throw exception();
	
	// TODO: don't allow watching the directory that contains 'logfile' because
	// logging to it will cause more events and more logging and more events and
	// on and on.
	
	while (!g_quit) 
	{
		// Set up watches
		int fd=inotify_init();
		map<int,FileWatch> wds;
		
		try {
			for (set<FileWatch>::iterator seti=cfg.begin(); seti!=cfg.end(); ++seti)
			{
				FileWatch watch=*seti;
				
				// cout << "Logging events to " << watch.logfile() << " for these directories:" << endl;
				// cout << watch.objname() << endl;
				int wd=inotify_add_watch(fd,watch.objname().c_str(),EVENTS_WE_CARE_ABOUT);
				wds.insert(make_pair(wd,watch));
				
				if (fs::is_directory(watch.objname()) && watch.recursive()) 
				{
					watchsubdirectories(fd,watch,wds);
				}
			}
		}
		catch (exception& x) 
		{
			// TODO: log to syslog
			cerr << "Error watching directory:" << x.what() << endl;
		}
		
		g_sighup=false;
	
		while (!g_sighup)
		{
			char buf[16*1024]; // 16K is enough for about 1000 events
			ssize_t n=read(fd,buf,sizeof(buf)); // TODO: use select & epoll
			if (n<0)
			{
				// TODO: write error to log and log to syslog
			}
			else
			{
				time_t now=time(0);
				for(size_t i = 0; i < n; )
				{
					inotify_event* event=(inotify_event*)(&buf[i]);
					
					// Only log events we care about
					if (event->mask & EVENTS_WE_CARE_ABOUT)
					{
						ofstream logf(wds[event->wd].logfile().c_str(),ios::app|ios::out);
						// TODO: log to syslog if can't open logfile
						
						logf << wds[event->wd].objname();
						if (event->len)
						{
							logf << '/' << event->name;
						}
						logf << '\t' << now << '\t' << event->mask << '\t' << event->cookie << endl;
					}
					
					if (event->mask & IN_ISDIR)
					{
						if (event->mask & IN_CREATE) // We have to start watching this new directory too
						{
							if (!event->len)
							{
								// TODO: log this, a new directory was created but we don't know its name
							}
							else
							{
								string newdirname=wds[event->wd].objname() + '/' + event->name;
								int wd=inotify_add_watch(fd,newdirname.c_str(),EVENTS_WE_CARE_ABOUT);
								wds.insert(make_pair(wd,FileWatch(newdirname.c_str(),wds[event->wd].recursive(),wds[event->wd].logfile().c_str())));
							}
						}
						else if (event->mask & (IN_DELETE|IN_DELETE_SELF)) // We should stop watching this directory
						{
							if (event->len)
							{
								// TODO: implement this
							}
						}
					}
				
					i+=sizeof(inotify_event) + event->len;
				}
			}
		}

		// Remove all watches
		if (!wds.empty())
		{
			for (map<int,FileWatch>::iterator itr=wds.begin();itr!=wds.end();++itr) 
			{
				// cout << "Removing watch: " << itr->second << '(' << itr->first << ')' << endl;
				inotify_rm_watch(fd,itr->first);
				wds.erase(itr->first);
			}
		}
		
		close(fd);
	}
	
	remove(PIDFILE);

	return 0;
}
