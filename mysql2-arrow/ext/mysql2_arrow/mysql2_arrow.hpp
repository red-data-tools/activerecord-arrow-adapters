#pragma once

#include <ruby.hpp>
#include <ruby/encoding.h>

#ifdef HAVE_MYSQL_H
#include <mysql.h>
#include <mysql_com.h>
#include <errmsg.h>
#include <mysqld_error.h>
#else
#include <mysql/mysql.h>
#include <mysql/mysql_com.h>
#include <mysql/errmsg.h>
#include <mysql/mysqld_error.h>
#endif

#if defined(__GNUC__) && (__GNUC__ >= 3)
#define RB_MYSQL_NORETURN __attribute__ ((noreturn))
#define RB_MYSQL_UNUSED __attribute__ ((unused))
#else
#define RB_MYSQL_NORETURN
#define RB_MYSQL_UNUSED
#endif

#include <mysql2/client.h>
#include <mysql2/statement.h>
#include <mysql2/result.h>

#define GET_RESULT(self) \
  mysql2_result_wrapper *wrapper; \
  Data_Get_Struct(self, mysql2_result_wrapper, wrapper);

namespace mysql2_arrow {
  extern VALUE mMysql2Arrow;
  extern VALUE eMysql2Error;

  void init_mysql2_result_extension();
}
