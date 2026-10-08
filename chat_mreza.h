/*
 * Copyright (c) 2026 Ivica Jocic
 * SrBinOS Chat - Mrezni sloj (Zaglavlje - Sinhronizovano sa Paukom)
 */

 #ifndef CHAT_MREZA_H
 #define CHAT_MREZA_H
 
 #include <errno.h>
 #include <stdbool.h>
 #include <stddef.h>
 #include <stdint.h>
 #include <inet/addr.h>
#include <inet/tcp.h>
 
 #include <mbedtls/ssl.h>
 #include <mbedtls/entropy.h>
 #include <mbedtls/ctr_drbg.h>
 #include <mbedtls/x509_crt.h>
 
 #undef GEMINI_HOST
 #define GEMINI_HOST "api.groq.com"
 #define GEMINI_PORT 443
 // Struktura preuzeta direktno iz Pauk arhitekture
 typedef struct {
    tcp_t *tcp_service;
     tcp_conn_t *tcp_conn;
     mbedtls_ssl_context ssl;
     mbedtls_ssl_config conf;
     mbedtls_ctr_drbg_context ctr_drbg;
     mbedtls_entropy_context entropy;
     mbedtls_x509_crt ca_cert;
 } pauk_tls_connection_t;
 
 typedef pauk_tls_connection_t *chat_mreza_handle_t;
 
 // Deklaracije funkcija uskladjene sa tvojim Pauk API-jem
 errno_t chat_otvori_https_vezu(const char *hostname, int port, chat_mreza_handle_t *tls_conn);
 errno_t chat_posalji_https(chat_mreza_handle_t tls_conn, const void *data, size_t len);
 errno_t chat_primi_https(chat_mreza_handle_t tls_conn, void *buffer, size_t size, size_t *received);
 void chat_oslobodi_https(chat_mreza_handle_t tls_conn);
 mbedtls_time_t mbedtls_platform_time(mbedtls_time_t *t);
 #endif /* CHAT_MREZA_H */
 