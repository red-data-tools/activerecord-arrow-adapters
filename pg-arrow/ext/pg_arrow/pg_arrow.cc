#include <ruby.h>

namespace pg_arrow {
  VALUE mPgArrow;
}

extern "C" void
Init_pg_arrow()
{
  pg_arrow::mPgArrow = rb_define_module("PgArrow");
}
