#ifndef	__CTRL_HTTPCURL_H_INCLUDED__
#define	__CTRL_HTTPCURL_H_INCLUDED__

#include "curl/curl.h"

//#define CURL_DEBUG

class HttpCurl
{

private:
	static size_t callbackWriteStr(char *ptr, size_t size, size_t nmemb, std::string *stream)
	{
		int dataLength = size * nmemb;
		stream->append(ptr, dataLength);
#ifdef CURL_DEBUG
		std::cout << "curl write call back:" << *stream << std::endl;
#endif
		return dataLength;
	};
	static size_t callbackWriteVectorUB(char *ptr, size_t size, size_t nmemb, std::vector<unsigned char> *stream)
	{
		int dataLength = size * nmemb;
		stream->insert(stream->end(), ptr, ptr + dataLength);
		return dataLength;
	};

public:
	static int Get( std::string uri, std::vector<unsigned char>& iRecv )
	{
		/* get state */
		{
			CURL *curl;
			CURLcode ret;

			curl = curl_easy_init();
			if (curl == NULL) {
				std::cerr << "curl_easy_init() failed" << std::endl;
				return 1;
			}
#ifdef CURL_DEBUG
			std::cout << "curl request url:" << uri << std::endl;
#endif
#if 1			// encode uri
			char enc_uri[1024] = {0};
			int dst = 0;
			for (int src = 0; src < uri.size(); src++) {
				if (dst >= (sizeof(enc_uri) - 4)) {
					std::cerr << "uri encode fail. request = " << uri << std::endl;
					return 1;
				}
				char c = uri[src];
				if (c == ' ')
				{
					enc_uri[dst++] = '%';
					enc_uri[dst++] = '2';
					enc_uri[dst++] = '0';
//					enc_uri[dst++] = "0123456789abcdef"[c & 0xf0 >> 4];
//					enc_uri[dst++] = "0123456789abcdef"[c & 0xf];
				}else{
					enc_uri[dst++] = c;
				}
			}
#ifdef CURL_DEBUG
			std::cout << "curl request enc url:" << std::string(enc_uri) << std::endl;
#endif
#endif
			// Request Http GET
//			curl_easy_setopt(curl, CURLOPT_URL, enc_uri);
			curl_easy_setopt(curl, CURLOPT_URL, uri.c_str());
			curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, (void *)callbackWriteVectorUB);
			curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1);
			curl_easy_setopt(curl, CURLOPT_WRITEDATA, &iRecv);
			curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, 750);
			ret = curl_easy_perform(curl);
			curl_easy_cleanup(curl);

			if (ret != CURLE_OK) {
				std::cerr << "curl_easy_perform() failed. request = " << uri << std::endl;
				return 1;
			}
//			std::cout << iRecv << std::endl;
		}
		return 0;
	};
};

#endif	// __CTRL_HTTP_H_INCLUDED__
