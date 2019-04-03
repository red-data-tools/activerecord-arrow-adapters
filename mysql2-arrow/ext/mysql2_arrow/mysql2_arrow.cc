#include "mysql2_arrow.hpp"
#include "red-arrow.hpp"

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

  auto mArrow = rb_const_get_at(rb_cObject, rb_intern("Arrow"));
  auto cTable = rb_const_get_at(mArrow, rb_intern("Table"));
  rb_define_method(cTable, "raw_records",
                   reinterpret_cast<rb::RawMethod>(red_arrow::table_raw_records),
                   0);
}
