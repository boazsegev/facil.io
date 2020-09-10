/* *****************************************************************************
Test
***************************************************************************** */
#ifdef TEST
void fio_test(void) {
  /* switch to test data set */
  struct fio___data_s *old = fio_data;
  fio_protocol_s proto = {0};
  protocol_validate(&proto);
  fio_data = fio_malloc(sizeof(*old) + (sizeof(old->info[0]) * 128));
  fio_data->capa = 128;
  fd_data(1).protocol = &proto;
  fprintf(stderr, "Starting facil.io IO core tests.\n");

  fprintf(stderr, "===============\n");
  FIO_NAME_TEST(io, state)();
  fprintf(stderr, "===============\n");
  FIO_NAME_TEST(io, tasks)();
  fprintf(stderr, "===============\n");
  FIO_NAME_TEST(io, env)();
  fprintf(stderr, "===============\n");
  FIO_NAME_TEST(io, protocol)();
  fprintf(stderr, "===============\n");
  FIO_NAME_TEST(io, rw_hooks)();
  fprintf(stderr, "===============\n");
  FIO_NAME_TEST(io, sock)();

  /* free test data set and return normal data set */
  fio_free(fio_data);
  fio_data = old;
  fprintf(stderr, "===============\n");
  fprintf(stderr, "Done.\n");
}
#endif /* TEST */
