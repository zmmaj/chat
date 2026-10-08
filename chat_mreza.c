/*
 * Copyright (c) 2026 Ivica Jocic
 * SrBinOS Chat - Mrezni sloj (Implementacija - Sinhronizovano sa Paukom)
 */

 #include <stdio.h>
 #include <stdlib.h>
 #include <string.h>
  #include <str.h>
 #include <fibril.h>
 #include <inet/addr.h>
 #include <inet/host.h>
#include <inet/tcp.h>
#include <str_error.h>
 #include <mbedtls/error.h>
 #include <mbedtls/debug.h>
 #include "chat_mreza.h"
 
 #define MAX_RETRIES 3
 #define RETRY_DELAY_MS 100

// --- NOVO: Ručne definicije mrežnih grešaka koje nedostaju u tvom mbedtls_config-u ---
#ifndef MBEDTLS_ERR_NET_SEND_FAILED
#define MBEDTLS_ERR_NET_SEND_FAILED -0x004E
#endif

#ifndef MBEDTLS_ERR_NET_RECV_FAILED
#define MBEDTLS_ERR_NET_RECV_FAILED -0x004C
#endif
static int mbedtls_platform_entropy_poll(void *data, unsigned char *output, size_t len, size_t *olen)
{
	(void) data;
	for (size_t i = 0; i < len; i++) {
		output[i] = (unsigned char)(rand() & 0xFF);
	}
	*olen = len;
	return 0;
}

// Low-level mbedTLS callback za slanje kroz HelenOS TCP
static int tls_send_cb(void *ctx, const unsigned char *buf, size_t len)
{
	pauk_tls_connection_t *conn = (pauk_tls_connection_t *)ctx;
	errno_t rc = tcp_conn_send(conn->tcp_conn, (const void *)buf, len);
	if (rc == EOK) return (int)len;
	if (rc == EAGAIN) return MBEDTLS_ERR_SSL_WANT_WRITE;
	return MBEDTLS_ERR_NET_SEND_FAILED;
}

// Low-level mbedTLS callback za prijem iz HelenOS TCP
static int tls_recv_cb(void *ctx, unsigned char *buf, size_t len)
{
	pauk_tls_connection_t *conn = (pauk_tls_connection_t *)ctx;
	size_t nread = 0;
	errno_t rc = tcp_conn_recv(conn->tcp_conn, (char *)buf, len, &nread);
	if (rc == EOK && nread > 0) return (int)nread;
	if (rc == EAGAIN || (rc == EOK && nread == 0)) return MBEDTLS_ERR_SSL_WANT_READ;
	return MBEDTLS_ERR_NET_RECV_FAILED;
}


static errno_t create_tcp_connection(inet_addr_t addr, uint16_t port, tcp_t **tcp, tcp_conn_t **conn)
{
	errno_t rc;
	int retry_count = 0;
	
	while (retry_count < MAX_RETRIES) {
		rc = tcp_create(tcp);
		if (rc != EOK) return rc;

		inet_ep2_t ep2;
		inet_ep2_init(&ep2);
		ep2.remote.addr = addr;
		ep2.remote.port = port;

		rc = tcp_conn_create(*tcp, &ep2, NULL, NULL, conn);
		
		if (rc == EBUSY) {
			tcp_destroy(*tcp);
			retry_count++;
			fibril_usleep(RETRY_DELAY_MS * 1000); // Zamena za tvoj srbinos_delay_1
			continue;
		} else if (rc != EOK) {
			tcp_destroy(*tcp);
			return rc;
		}
		break;
	}
	
	if (rc != EOK) return rc;
	
	rc = tcp_conn_wait_connected(*conn);
	return rc;
}

// --- GLAVNA FUNKCIJA: Otvaranje HTTPS kanala preko Pauk funkcija ---
errno_t chat_otvori_https_vezu(const char *hostname, int port, chat_mreza_handle_t *tls_conn)
{
	errno_t rc;
	inet_addr_t server_addr;
	tcp_t *tcp_service = NULL;
	tcp_conn_t *tcp_veza = NULL;
printf("chat_otvori_https_vezu-0\n");
	// BEZBEDNOSNA PROVERA: Ako je prosleđeni pokazivač loš, prekidamo pre krahiranja steka
	if (tls_conn == NULL) {
		return EINVAL;
	}
    printf("chat_otvori_https_vezu-1\n");
	// POPRAVKA: Obezbeđujemo stvarne pokazivače na steku umesto NULL-ova!
	char *lokalni_endptr = NULL;
	const char *lokalna_poruka_greske = NULL;

	// Pozivamo funkciju sa tačnim adresama lokalnih pokazivača
	rc = inet_host_plookup_one(hostname, ip_v4, &server_addr, &lokalni_endptr, &lokalna_poruka_greske);
	
	if (rc != EOK) {
		// Ako je došlo do greške, ispisujemo tačan razlog koji nam je funkcija vratila
		printf("[CHAT DNS ERROR]: rc=%d, Poruka: %s\n", (int)rc, 
		       lokalna_poruka_greske ? lokalna_poruka_greske : "Nepoznato");
		return rc; 
	}

	printf("chat_otvori_https_vezu-2\n"); // Sada sigurno stižemo ovde!
	// 2. Spuštamo dobijenu 'server_addr' i ulazni 'port' direktno u tvoj TCP konektor sa retries
	// Kastujemo port u uint16_t kako bi se idealno poklopio sa tvojom create_tcp_connection funkcijom
	rc = create_tcp_connection(server_addr, (uint16_t)port, &tcp_service, &tcp_veza);
	if (rc != EOK) {
		return rc; // Ako TCP spajanje padne, vraćamo sistemski kod greške
	}
    printf("chat_otvori_https_vezu-3\n");
	// --- 2. Pauk TLS Alokacija i Inicijalizacija ---
	pauk_tls_connection_t *conn = calloc(1, sizeof(*conn));
	if (!conn) return ENOMEM;

	conn->tcp_conn = tcp_veza;
	
	mbedtls_ssl_init(&conn->ssl);
	mbedtls_ssl_config_init(&conn->conf);
	mbedtls_ctr_drbg_init(&conn->ctr_drbg);
	mbedtls_entropy_init(&conn->entropy);
	mbedtls_x509_crt_init(&conn->ca_cert);

	mbedtls_entropy_add_source(&conn->entropy, mbedtls_platform_entropy_poll, NULL, 32, MBEDTLS_ENTROPY_SOURCE_STRONG);

	int ret = mbedtls_ctr_drbg_seed(&conn->ctr_drbg, mbedtls_entropy_func, &conn->entropy, NULL, 0);
	if (ret != 0) goto fail;

	ret = mbedtls_ssl_config_defaults(&conn->conf, MBEDTLS_SSL_IS_CLIENT, MBEDTLS_SSL_TRANSPORT_STREAM, MBEDTLS_SSL_PRESET_DEFAULT);
	if (ret != 0) goto fail;

	static int ciphersuites[] = {
#ifdef MBEDTLS_SSL_PROTO_TLS1_3
		MBEDTLS_TLS_AES_256_GCM_SHA384,
		MBEDTLS_TLS_AES_128_GCM_SHA256,
		MBEDTLS_TLS_CHACHA20_POLY1305_SHA256,
#endif
		MBEDTLS_TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256,
		MBEDTLS_TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384,
		MBEDTLS_TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA256,
		MBEDTLS_TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA384,
		MBEDTLS_TLS_RSA_WITH_AES_128_GCM_SHA256,
		MBEDTLS_TLS_RSA_WITH_AES_256_GCM_SHA384,
		MBEDTLS_TLS_RSA_WITH_AES_128_CBC_SHA256,
		MBEDTLS_TLS_RSA_WITH_AES_256_CBC_SHA256,
		0
	};
	mbedtls_ssl_conf_ciphersuites(&conn->conf, ciphersuites);

	static const char *alpn_protos[] = { "http/1.1", NULL };
	mbedtls_ssl_conf_alpn_protocols(&conn->conf, alpn_protos);

	mbedtls_ssl_conf_rng(&conn->conf, mbedtls_ctr_drbg_random, &conn->ctr_drbg);
	mbedtls_ssl_conf_authmode(&conn->conf, MBEDTLS_SSL_VERIFY_NONE);
    printf("chat_otvori_https_vezu-4\n"); 
#ifdef MBEDTLS_SSL_PROTO_TLS1_3
	mbedtls_ssl_conf_min_version(&conn->conf, MBEDTLS_SSL_MAJOR_VERSION_3, MBEDTLS_SSL_MINOR_VERSION_3);
	mbedtls_ssl_conf_max_version(&conn->conf, MBEDTLS_SSL_MAJOR_VERSION_3, MBEDTLS_SSL_MINOR_VERSION_4);
#else
	mbedtls_ssl_conf_max_version(&conn->conf, MBEDTLS_SSL_MAJOR_VERSION_3, MBEDTLS_SSL_MINOR_VERSION_3);
#endif

	ret = mbedtls_ssl_setup(&conn->ssl, &conn->conf);
	if (ret != 0) goto fail;

	mbedtls_ssl_set_hostname(&conn->ssl, hostname);
	mbedtls_ssl_set_bio(&conn->ssl, conn, tls_send_cb, tls_recv_cb, NULL);
    printf("chat_otvori_https_vezu-5\n");
	/* ---------------- HANDSHAKE LOOP ---------------- */
	int handshake_retries = 0;
	while (1) {
		ret = mbedtls_ssl_handshake(&conn->ssl);
		if (ret == 0) break;

		if (ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE) {
			fibril_usleep(10000); 
			if (++handshake_retries > 500) goto fail;
			continue;
		}
		goto fail;
	}
    printf("chat_otvori_https_vezu-6\n");
	*tls_conn = conn;
	return EOK;

fail:
	mbedtls_x509_crt_free(&conn->ca_cert);
	mbedtls_entropy_free(&conn->entropy);
	mbedtls_ctr_drbg_free(&conn->ctr_drbg);
	mbedtls_ssl_config_free(&conn->conf);
	mbedtls_ssl_free(&conn->ssl);
	free(conn);
	return EIO;
}

// Slanje podataka preko TLS-a
errno_t chat_posalji_https(chat_mreza_handle_t tls_conn, const void *data, size_t len)
{
	pauk_tls_connection_t *conn = (pauk_tls_connection_t *)tls_conn;
	size_t remaining = len;
	const unsigned char *ptr = (const unsigned char *)data;
	
	while (remaining > 0) {
		int ret = mbedtls_ssl_write(&conn->ssl, ptr, remaining);
		if (ret > 0) {
			ptr += ret;
			remaining -= ret;
		} else if (ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE) {
			fibril_usleep(10000);
			continue;
		} else {
			return EIO;
		}
	}
	return EOK;
}

// Prijem podataka preko TLS-a
errno_t chat_primi_https(chat_mreza_handle_t tls_conn, void *buffer, size_t size, size_t *received)
{
	pauk_tls_connection_t *conn = (pauk_tls_connection_t *)tls_conn;
	int ret = mbedtls_ssl_read(&conn->ssl, (unsigned char *)buffer, size - 1);
	
	if (ret > 0) {
		*received = ret;
		((char *)buffer)[ret] = '\0';
		return EOK;
	} else if (ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE) {
		*received = 0;
		return EOK;
	}
	return EIO;
}

// Čišćenje resursa
void chat_oslobodi_https(chat_mreza_handle_t tls_conn)
{
	if (!tls_conn) return;
	pauk_tls_connection_t *conn = (pauk_tls_connection_t *)tls_conn;
	mbedtls_x509_crt_free(&conn->ca_cert);
	mbedtls_entropy_free(&conn->entropy);
	mbedtls_ctr_drbg_free(&conn->ctr_drbg);
	mbedtls_ssl_config_free(&conn->conf);
	mbedtls_ssl_free(&conn->ssl);
	free(conn);
}