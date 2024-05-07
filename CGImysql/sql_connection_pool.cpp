#include <mysql/mysql.h>
#include <stdio.h>
#include <string>
#include <string.h>
#include <stdlib.h>
#include <list>
#include <pthread.h>
#include <iostream>
#include "sql_connection_pool.h"

using namespace std;

//初始化
connection_pool::connection_pool()
{
	m_CurConn = 0;	//已使用的连接数
	m_FreeConn = 0; //空闲的连接数
}

//单例模式，返回一个连接池
connection_pool *connection_pool::GetInstance()
{
	static connection_pool connPool;
	return &connPool;
}

// mysql_real_connect(MYSQL* mysql, const char* host, const char* user, const char* passwd, const char* db, unsigned int port, const char* unix_socket, unsigned long client_flag)
// 数据库引擎建立连接函数

// mysql：定义的MYSQL变量
// host：MYSQL服务器的地址,决定了连接的类型。如果"host"是NULL或字符串"localhost"，连接将被视为与本地主机的连接,如果操作系统支持套接字（Unix）或命名管道（Windows），将使用它们而不是TCP/IP连接到服务器
// user：登录用户名,如果“user”是NULL或空字符串""，用户将被视为当前用户,在UNIX环境下，它是当前的登录名
// passwd：登录密码
// db：要连接的数据库，如果db为NULL，连接会将该值设为默认的数据库
// port：MYSQL服务器的TCP服务端口，如果"port"不是0，其值将用作TCP/IP连接的端口号
// unix_socket：unix连接方式，如果unix_socket不是NULL，该字符串描述了应使用的套接字或命名管道
// clientflag：Mysql运行为ODBC数据库的标记，一般取0
// 返回值：连接成功，返回连接句柄，即第一个变量mysql；连接失败，返回NULL
//初始化
void connection_pool::init(string url, string User, string PassWord, string DBName, int Port, int MaxConn, int close_log)
{
	m_url = url;					//初始化数据库信息
	m_Port = Port;
	m_User = User;
	m_PassWord = PassWord;
	m_DatabaseName = DBName;
	m_close_log = close_log;

	for (int i = 0; i < MaxConn; i++)			//创建MaxConn条数据库连接
	{
		MYSQL *con = NULL;
		//如果con为NULL则分配一个MYSQL对象句柄，否则将初始化con句柄数据库
		con = mysql_init(con);					//mysql_init(MYSQL* mysql):初始化或分配与mysql_real_connect()相适应的MYSQL对象

		if (con == NULL)						//初始化失败
		{
			LOG_ERROR("MySQL Error");
			exit(1);
		}
		//创建真正的数据库连接
		con = mysql_real_connect(con, url.c_str(), User.c_str(), PassWord.c_str(), DBName.c_str(), Port, NULL, 0);

		if (con == NULL)						//建立连接失败
		{
			LOG_ERROR("MySQL Error");
			exit(1);
		}
		//将数据库对象放入连接池中保存
		connList.push_back(con);				//更新连接池和空闲连接数量
		++m_FreeConn;
	}

	//reserve信号量代表连接池中可用的数据库连接对象
	reserve = sem(m_FreeConn);					//将信号量初始化为最大连接次数

	m_MaxConn = m_FreeConn;
}


//当有请求时，从数据库连接池中返回一个可用连接，更新使用和空闲连接数
MYSQL *connection_pool::GetConnection()
{
	MYSQL *con = NULL;

	if (0 == connList.size())
		return NULL;

	reserve.wait();			//取出连接，信号量原子减1，为0则等待
	
	lock.lock();			//lock互斥锁保证同一时间只有一个线程对容器connlist进行操作
	//从连接池中返回一个数据库连接对象
	con = connList.front();		//得到第一个连接
	connList.pop_front();		//从连接池中弹出该连接
	//空闲连接数
	--m_FreeConn;				
	//已使用连接数
	++m_CurConn;	
	lock.unlock();
	//返回可用连接的指针
	return con;
}

//释放当前使用的连接
bool connection_pool::ReleaseConnection(MYSQL *con)
{
	if (NULL == con)
		return false;

	lock.lock();

	connList.push_back(con);
	++m_FreeConn;
	--m_CurConn;

	lock.unlock();

	reserve.post();		//释放连接原子加1
	return true;
}

//销毁数据库连接池
void connection_pool::DestroyPool()
{

	lock.lock();
	if (connList.size() > 0)
	{
		list<MYSQL *>::iterator it;								//通过迭代器遍历，关闭数据库连接
		for (it = connList.begin(); it != connList.end(); ++it)
		{
			MYSQL *con = *it;
			mysql_close(con);
		}
		m_CurConn = 0;
		m_FreeConn = 0;
		connList.clear();				//清空连接池
	}

	lock.unlock();
}

//当前空闲的连接数
int connection_pool::GetFreeConn()
{
	return this->m_FreeConn;
}

connection_pool::~connection_pool()
{
	DestroyPool();
}

//将连接池的创建与销毁都放在connectionRAII中进行管理
connectionRAII::connectionRAII(MYSQL **SQL, connection_pool *connPool){
	*SQL = connPool->GetConnection();
	
	conRAII = *SQL;
	poolRAII = connPool;
}
connectionRAII::~connectionRAII(){
	poolRAII->ReleaseConnection(conRAII);
}