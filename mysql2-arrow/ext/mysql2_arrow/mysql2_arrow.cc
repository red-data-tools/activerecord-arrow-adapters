#include "mysql2_arrow.hpp"

namespace mysql2_arrow {
  VALUE mMysql2Arrow;
  VALUE eMysql2Error;
}

extern "C" void
Init_mysql2_arrow()
{
  mysql2_arrow::mMysql2Arrow = rb_define_module("Mysql2Arrow");
  mysql2_arrow::eMysql2Error = rb_path2class("Mysql2::Error");
  mysql2_arrow::init_mysql2_result_extension();
}
