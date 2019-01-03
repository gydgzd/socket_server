#include "getDate.h"

//#include "sql_conn_cpp.h"  //my sql class

/* get time from mysql server
string getSqlTime()
{
	string date_str;
	string dbname = "websiteman";
	string sql = "select date_format(now(),'%Y-%m-%d %H:%i:%s') time";
	try
	{
	   Csql_conn_cpp  sqlcon;
	   sqlcon.sql_connect("10.1.122.140");
	   sqlcon.setDBName(dbname);
	   sqlcon.sql_execute(sql,1);  //put result into ss.res-> and there might be an exception here either
	   if (sqlcon.m_res->next())
	   {
		   date_str = sqlcon.m_res->getString("time").c_str();  //getString()
	   }
	}
	catch(sql::SQLException &e)
	{
		date_str = getLocalTime("%Y-%m-%d %H:%M:%S");    // get local time when failed
	}
	return date_str;
}
*/
/* 
return the local time, like 
"%Y-%m-%d %H:%M:%S", 
"%Y-%m-%d", 
"%Y" 
*/
string getLocalTime(const char *format)
{
	time_t t = time(0);
	string date_str = "";
	char tmp[32] = {};

#ifdef __linux
	strftime(tmp, sizeof(tmp), format, localtime(&t));
#elif WINVER
	tm timeinfo;
	localtime_s(&timeinfo, &t);
	strftime(tmp, sizeof(tmp), format, &timeinfo);
#endif

	date_str = tmp;
	//puts( tmp );
	return date_str;
}
