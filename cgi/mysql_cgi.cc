#include <iostream>
#include <string>
#include <unistd.h>
#include "mysql.h"
#include "comm.hpp"

bool InsertSql(std::string sql)
{
    MYSQL *conn = mysql_init(nullptr);
    mysql_set_character_set(conn, "utf8"); //统一编码格式
    if (mysql_real_connect(conn, "127.0.0.1", "http_test", "btjz.88ka.cn", "http_test", 3306, NULL, 0) == nullptr)
    {
        std::cerr << "connect error!" << std::endl;
        return false;
    }
    std::cerr << "connect success ... " << std::endl;

    if (mysql_query(conn, sql.c_str())) // 成功为0， 失败非0
    {
        std::cerr << sql << " is ERROR" << std::endl;
        mysql_close(conn);
        return false;
    }
    else
    {
        std::cerr << sql << " is SUCCESS" << std::endl;
        mysql_close(conn);
        return true;
    }
}

int main()
{
    std::cerr << ".................................................." << std::endl;

    std::string query_string; // from httpserver
    if (GetQueryString(query_string))
    {
        //处理数据
        std::string name;
        std::string passwd;
        CutQuery(query_string, "&", name, passwd);
        std::string tag_name;
        std::string sql_name;
        CutQuery(name, "=", tag_name, sql_name);
        std::string tag_passwd;
        std::string sql_passwd;
        CutQuery(passwd, "=", tag_passwd, sql_passwd);
        std::string sql = "insert into user(name, passwd) values(\'";
        sql += sql_name;
        sql += "\',\'";
        sql += sql_passwd;
        sql += "\')";
        //插入数据库
        std::string res;
        if (InsertSql(sql))
        {
            res = "<h3>" + sql_name + " login was successful!</h3>";
        }
        else
        {
            res = "<h3>" + sql_name + " login was error!</h3>";
        }

        //fd: 1
        std::cout << "<html>";
        std::cout << "<head><meta charset=\"utf-8\"></head>";
        std::cout << "<body>";
        std::cout << res;
        std::cout << "</body>";
        std::cout << "</html>";
    }
    else
    {
        return 1; // 获取参数失败
    }

    std::cerr << "++++++++++++++++++++++++++++++++++++++++++++++++++" << std::endl;
    return 0;
}