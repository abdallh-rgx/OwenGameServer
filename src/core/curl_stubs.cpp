extern "C" {

int Curl_http2_may_switch(void*) { return 0; }
int Curl_http2_switch(void*) { return 0; }
int Curl_http2_switch_at(void*, int) { return 0; }
int Curl_http2_request_upgrade(void*, void*) { return 0; }
int Curl_http2_upgrade(void*, void*) { return 0; }
int Curl_cf_h2_proxy_insert_after(void*, void*) { return 0; }
int Curl_http2_setup(void*, void*) { return 0; }
int Curl_http2_setup_req(void*) { return 0; }
int Curl_http2_done(void*, bool) { return 0; }
int Curl_http2_verfify(void*, void*) { return 0; }
int Curl_h2_http_1_1_error(void*, int) { return 0; }
int Curl_h2_headers_to_string(void*, void*) { return 0; }

void* nghttp2_session_callbacks_new(void) { return 0; }
void  nghttp2_session_callbacks_del(void*) { }
void  nghttp2_session_callbacks_set_send_callback(void*, void*) { }
void  nghttp2_session_callbacks_set_on_frame_recv_callback(void*, void*) { }
void  nghttp2_session_callbacks_set_on_stream_close_callback(void*, void*) { }
void  nghttp2_session_callbacks_set_on_data_chunk_recv_callback(void*, void*) { }
void  nghttp2_session_callbacks_set_on_header_callback(void*, void*) { }
void  nghttp2_session_callbacks_set_on_begin_headers_callback(void*, void*) { }
void  nghttp2_session_callbacks_set_on_invalid_frame_recv_callback(void*, void*) { }
void  nghttp2_session_callbacks_set_error_callback2(void*, void*) { }
void  nghttp2_session_callbacks_set_on_extension_chunk_recv_callback(void*, void*) { }
void  nghttp2_session_callbacks_set_unpack_extension_callback(void*, void*) { }
void  nghttp2_session_callbacks_set_pack_extension_callback(void*, void*) { }
void  nghttp2_session_callbacks_set_on_frame_send_callback(void*, void*) { }
void  nghttp2_session_callbacks_set_on_invalid_header_callback(void*, void*) { }
void  nghttp2_session_callbacks_set_select_padding_callback(void*, void*) { }
void  nghttp2_session_callbacks_set_data_source_read_length_callback(void*, void*) { }

void* nghttp2_option_new(void) { return 0; }
void  nghttp2_option_del(void*) { }
void  nghttp2_option_set_no_auto_window_update(void*, int) { }
void  nghttp2_option_set_peer_max_concurrent_streams(void*, unsigned int) { }
void  nghttp2_option_set_builtin_recv_extension_type(void*, unsigned char) { }
void  nghttp2_option_set_no_rfc9113_leading_and_trailing_ws_validation(void*, int) { }

void* nghttp2_session_client_new3(void**, void*, void*, void*) { return 0; }
int   nghttp2_session_del(void*) { return 0; }
int   nghttp2_session_send(void*) { return 0; }
int   nghttp2_session_recv(void*) { return 0; }
int   nghttp2_session_mem_recv(void*, void*, unsigned long) { return 0; }
int   nghttp2_session_want_read(void*) { return 0; }
int   nghttp2_session_want_write(void*) { return 0; }

int   nghttp2_submit_settings(void*, unsigned char, void*, unsigned long) { return 0; }
int   nghttp2_submit_goaway(void*, unsigned char, int, unsigned int, void*, unsigned long) { return 0; }
int   nghttp2_submit_request(void*, void*, void*, unsigned long, void*, void*) { return 0; }
int   nghttp2_submit_ping(void*, unsigned char, void*) { return 0; }
int   nghttp2_submit_window_update(void*, unsigned char, int, int) { return 0; }
int   nghttp2_submit_rst_stream(void*, unsigned char, int, unsigned int) { return 0; }
int   nghttp2_submit_shutdown_notice(void*) { return 0; }
int   nghttp2_submit_priority(void*, unsigned char, int, void*) { return 0; }

int   nghttp2_session_set_stream_user_data(void*, int, void*) { return 0; }
int   nghttp2_session_set_local_window_size(void*, unsigned char, int, int) { return 0; }
int   nghttp2_session_upgrade2(void*, void*, unsigned long, int, void*) { return 0; }
int   nghttp2_session_get_remote_window_size(void*) { return 0; }
int   nghttp2_session_get_stream_remote_window_size(void*, int) { return 0; }
int   nghttp2_session_get_local_window_size(void*) { return 0; }
int   nghttp2_session_get_stream_local_window_size(void*, int) { return 0; }
void* nghttp2_session_get_stream_user_data(void*, int) { return 0; }
void* nghttp2_session_get_stream(void*, int) { return 0; }
int   nghttp2_session_check_server_session(void*) { return 0; }
int   nghttp2_session_get_hd_deflate_dynamic_table_size(void*) { return 0; }
void* nghttp2_session_get_outbound_queue_size(void*) { return 0; }

const char* nghttp2_strerror(int) { return "nghttp2-disabled"; }
const char* nghttp2_http2_strerror(int) { return "nghttp2-disabled"; }
const char* nghttp2_version(int) { return "0.0.0"; }

int BrotliDecoderGetErrorCode(void*) { return 0; }
int BrotliDecoderDecompressStream(void*, void*, void*, void*, void*, void*) { return 0; }
int BrotliDecoderDestroyInstance(void*) { return 0; }
int BrotliDecoderCreateInstance(void*, void*, void*) { return 0; }
int BrotliDecoderDecompress(unsigned long, const unsigned char*, unsigned long*, unsigned char*) { return 0; }
const char* BrotliDecoderErrorString(int) { return "brotli-disabled"; }

void* ZSTD_createDStream(void) { return 0; }
int   ZSTD_freeDStream(void*) { return 0; }
int   ZSTD_isError(unsigned long) { return 0; }
unsigned long ZSTD_decompressStream(void*, void*, void*) { return 0; }
unsigned long ZSTD_initDStream(void*) { return 0; }
const char*   ZSTD_getErrorName(unsigned long) { return "zstd-disabled"; }

}
