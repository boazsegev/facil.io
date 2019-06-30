#include "fio.h"

int main(int argc, char const *argv[]) {
  if (argc < 2)
    return -1;
  fio_url_s u = fio_url_parse(argv[1], strlen(argv[1]));
  fprintf(stderr,
          "Parsed URL:\n"
          "\tscheme:\t %.*s\n"
          "\tuser:\t%.*s\n"
          "\tpass:\t%.*s\n"
          "\thost:\t%.*s\n"
          "\tport:\t%.*s\n"
          "\tpath:\t%.*s\n"
          "\tquery:\t%.*s\n"
          "\ttarget:\t%.*s\n",
          (int)u.scheme.len, u.scheme.buf, (int)u.user.len, u.user.buf,
          (int)u.password.len, u.password.buf, (int)u.host.len, u.host.buf,
          (int)u.port.len, u.port.buf, (int)u.path.len, u.path.buf,
          (int)u.query.len, u.query.buf, (int)u.target.len, u.target.buf);
  return 0;
}
