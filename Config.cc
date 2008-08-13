#include "Config.h"
#include <iostream>

Config::~Config() {}

void Config::addWatch(const char* objname,bool recursive,const char* logfile) 
{
	m_obj=objname;
	m_recurse=recursive;
	m_log=logfile;
}

