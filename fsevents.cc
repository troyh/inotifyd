#include <stdlib.h>
#include <string>
#include <vector>
#include <sstream>
#include <fstream>
#include <iostream>
#include <linux/inotify.h>

struct flagtypes {
	uint32_t bit;
	const char* str;
};

flagtypes flags[]={
	{IN_ACCESS       ,"ACCESS"			},
	{IN_MODIFY       ,"MODIFY"          },
	{IN_ATTRIB       ,"ATTRIB"          },
	{IN_CLOSE_WRITE  ,"CLOSE_WRITE"     },
	{IN_CLOSE_NOWRITE,"CLOSE_NOWRITE"   },
	{IN_OPEN         ,"OPEN"            },
	{IN_MOVED_FROM   ,"MOVED_FROM"      },
	{IN_MOVED_TO     ,"MOVED_TO"        },
	{IN_CREATE       ,"CREATE"          },
	{IN_DELETE       ,"DELETE"          },
	{IN_DELETE_SELF  ,"DELETE_SELF"     },
	{IN_MOVE_SELF    ,"MOVE_SELF"       },
	{IN_UNMOUNT      ,"UNMOUNT"         },
	{IN_Q_OVERFLOW   ,"Q_OVERFLOW"      },
	{IN_IGNORED      ,"IGNORED"         },
	{IN_ISDIR      	 ,"ISDIR"         },
};

using namespace std;

int main(int argc,char* argv[])
{
	// TODO: fail if no argv[1]
	string logfile=argv[1];
	string line;
	
	for (ifstream logf(logfile.c_str(),ios::in); getline(logf,line); )
	{
		vector<string> tokens;
		int i=0;
		string token;
		istringstream ss(line);
		while (getline(ss,token,'\t'))
		{
			tokens.push_back(token);
		}
	
		uint32_t mask=strtol(tokens[2].c_str(),NULL,10);
		
		cout << tokens[1] << '\t' << tokens[0] << '\t' << tokens[3] << '\t';
		for (size_t i=0;i<(sizeof(flags)/sizeof(flags[0]));++i)
		{
			if (flags[i].bit & mask)
				cout << flags[i].str << ' ';
		}
		cout << endl;
		
	}
	
	return 0;
}