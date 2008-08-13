#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <string>
#include <errno.h>
#include <iostream>
#include <fstream>
#include <map>
#include <sys/inotify.h>
#include <linux/limits.h>
#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/path.hpp>
#include <libxml/parser.h>
#include <libxml/xpath.h>
#include "Config.h"

using namespace std;
namespace fs=boost::filesystem;

// struct flagtypes {
// 	uint32_t bit;
// 	const char* str;
// };
// flagtypes flags[]={
// 	{IN_ACCESS       ,"ACCESS"			},
// 	{IN_MODIFY       ,"MODIFY"          },
// 	{IN_ATTRIB       ,"ATTRIB"          },
// 	{IN_CLOSE_WRITE  ,"CLOSE_WRITE"     },
// 	{IN_CLOSE_NOWRITE,"CLOSE_NOWRITE"   },
// 	{IN_OPEN         ,"OPEN"            },
// 	{IN_MOVED_FROM   ,"MOVED_FROM"      },
// 	{IN_MOVED_TO     ,"MOVED_TO"        },
// 	{IN_CREATE       ,"CREATE"          },
// 	{IN_DELETE       ,"DELETE"          },
// 	{IN_DELETE_SELF  ,"DELETE_SELF"     },
// 	{IN_MOVE_SELF    ,"MOVE_SELF"       },
// 	{IN_UNMOUNT      ,"UNMOUNT"         },
// 	{IN_Q_OVERFLOW   ,"Q_OVERFLOW"      },
// 	{IN_IGNORED      ,"IGNORED"         },
// 	{IN_ISDIR      	 ,"ISDIR"         },
// };

static const char* config_file="/etc/inotify.conf";
static const char* logfile="/var/log/inotify.log";
static string watchdir="/home/troy/";
static bool recursive=true;
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

int read_config(const char* fname,Config& cfg) 
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
			cfg.addWatch((char*)objname,xmlStrcmp(recursive,(const xmlChar*)"yes")?true:false,(char*)logfile);
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

int main(int argc,char* argv[])
{
	Config cfg;
	read_config(config_file,cfg);
	return 0;
	
	int pid=fork();
	if (pid)  // Child started, we can quit
	{
		cout << "Process ID " << pid << endl;
		return 0;
	}
	else if (pid<0) // fork() failed
	{
		cerr << "Unable to daemonize. (" << errno << ")" << endl;
		return 1;
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
		int fd=inotify_init();
		map<int,std::string> wds;
		int wd=inotify_add_watch(fd,watchdir.c_str(),IN_ALL_EVENTS);
		
		wds.insert(make_pair(wd,watchdir));
		if (recursive)
		{
			fs::directory_iterator end_itr;
			for (fs::directory_iterator itr(watchdir); itr!=end_itr; ++itr)
			{
				if (fs::is_directory(itr->status()))
				{
					wd=inotify_add_watch(fd,itr->path().string().c_str(),IN_ALL_EVENTS);
					wds.insert(make_pair(wd,itr->path().string()));
				}
			}
		}
		
		g_sighup=false;
	
		ofstream logf(logfile,ios::app|ios::out);
		// TODO: fail if can't open logfile
		
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
				// TODO: if failed to open or write to log, log error to syslog
				time_t now=time(0);
				for(size_t i = 0; i < n; )
				{
					inotify_event* event=(inotify_event*)(&buf[i]);
					
					logf << wds[event->wd];
					if (event->len)
						logf << '/' << event->name;
					logf << '\t' << now << '\t' << event->mask << '\t' << event->cookie << endl;
				
					i+=sizeof(inotify_event) + event->len;
				}
			}
		}

		// Remove all watches
		if (!wds.empty())
		{
			for (map<int,string>::iterator itr=wds.begin();itr!=wds.end();++itr) 
			{
				// cout << "Removing watch: " << itr->second << '(' << itr->first << ')' << endl;
				inotify_rm_watch(fd,itr->first);
				wds.erase(itr->first);
			}
		}
		
		close(fd);
	}

	return 0;
}
