/* *****************************************************************************
Store all functions to the FIO_FUNCTIONS vtable

NOTE: Implementations will need to edit the FIO_FUNCTIONS struct dynamically.
***************************************************************************** */
fio_overridable_functions_s FIO_FUNCTIONS = {
    .fio_fork = fio_fork_overridable,
    .fio_thread_start = fio_thread_start_overridable,
    .fio_thread_join = fio_thread_join_overridable,
    .size_of_thread_t = sizeof(fio_thread_t),
    .TLS_CLIENT =
        {
            .read = fio_tls_read,
            .write = fio_tls_write,
            .flush = fio_tls_flush,
            .free = fio_tls_free,
            .dup = fio_tls_dup_client,
        },
    .TLS_SERVER =
        {
            .read = fio_tls_read,
            .write = fio_tls_write,
            .flush = fio_tls_flush,
            .free = fio_tls_free,
            .dup = fio_tls_dup_server,
        },
    .fio_tls_new = fio_tls_new,
    .fio_tls_dup = fio_tls_dup_master,
    .fio_tls_free = fio_tls_free,
    .fio_tls_cert_add = fio_tls_cert_add,
    .fio_tls_alpn_add = fio_tls_alpn_add,
    .fio_tls_trust = fio_tls_trust,
    .fio_tls_alpn_count = fio_tls_alpn_count,
};
