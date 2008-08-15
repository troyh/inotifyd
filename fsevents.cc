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

void output_tokens(vector<string>& tokens)
{
	if (!tokens.empty())
	{
		uint32_t mask=strtol(tokens[2].c_str(),NULL,10);

		cout << tokens[1] << '\t' << tokens[0] << '\t' << tokens[3] << '\t';
		for (size_t i=0;i<(sizeof(flags)/sizeof(flags[0]));++i)
		{
			if (flags[i].bit & mask)
				cout << flags[i].str << ' ';
		}
		cout << endl;
	}
}


int main(int argc,char* argv[])
{
	// TODO: fail if no argv[1]
	if (argc<2) 
	{
		cout << "Usage: fsevents <logfile> [since_time]" << endl << endl;
		return -1;
	}
	string logfile=argv[1];
	time_t since=0;
	if (argc>=3)
		since=strtol(argv[2],NULL,10);
	string line;
	vector<string> last_tokens;
	
	for (ifstream logf(logfile.c_str(),ios::in); getline(logf,line); )
	{
		vector<string> tokens;

		string token;
		istringstream ss(line);
		while (getline(ss,token,'\t'))
		{
			tokens.push_back(token);
		}
		
		time_t t=strtol(tokens[1].c_str(),NULL,10);
		if (t<=since)
			continue;
		
		if (last_tokens.empty() ||
		    (last_tokens[0]==tokens[0] && last_tokens[2]==tokens[2] && last_tokens[3]==tokens[3]))
		{
			// Redundant event
		}
		else 
		{
			output_tokens(last_tokens);
		}
		
		last_tokens=tokens;
	}
	
	output_tokens(last_tokens);
	
	return 0;
}