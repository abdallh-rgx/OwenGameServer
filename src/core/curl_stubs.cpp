extern "C" {

int Curl_http2_may_switch(void*) { return 0; }
int Curl_http2_switch(void*) { return 0; }
int Curl_http2_switch_at(void*, int) { return 0; }
int Curl_http2_request_upgrade(void*, void*) { return 0; }
int Curl_http2_upgrade(void*, void*) { return 0; }
int Curl_cf_h2_proxy_insert_after(void*, void*) { return 0; }

int BrotliDecoderGetErrorCode(void*) { return 0; }
int BrotliDecoderDecompressStream(void*, void*, void*, void*, void*, void*) { return 0; }
int BrotliDecoderDestroyInstance(void*) { return 0; }
int BrotliDecoderCreateInstance(void*, void*, void*) { return 0; }

void* ZSTD_createDStream(void) { return 0; }
int   ZSTD_freeDStream(void*) { return 0; }
int   ZSTD_isError(unsigned long) { return 0; }
unsigned long ZSTD_decompressStream(void*, void*, void*) { return 0; }

}
