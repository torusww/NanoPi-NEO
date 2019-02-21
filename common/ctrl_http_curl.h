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
	static int Get( const char * pszHostName, int port, const char * URI, void* iRecv, void *cb )
	{
		std::string	str;
		int			ret;
		
		str = "http://" + std::string(pszHostName) + ":" +  std::to_string(port) + std::string(URI);
    
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
	std::cout << "curl request url:" << str << std::endl;
#endif
			curl_easy_setopt(curl, CURLOPT_URL, str.c_str());
			curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, cb);
			curl_easy_setopt(curl, CURLOPT_WRITEDATA, iRecv);
			curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, 750);
			ret = curl_easy_perform(curl);
			curl_easy_cleanup(curl);

			if (ret != CURLE_OK) {
				std::cerr << "curl_easy_perform() failed. request = " << str << std::endl;
				return 1;
			}
		//	std::cout << iRecv << std::endl;

		}
		return 0;
	};
	static int Get( const char * pszHostName, int port, const char * URI, std::string& iRecv ) {
		return Get(pszHostName, port, URI, &iRecv, (void *)callbackWriteStr);
	};
	static int Get( const char * pszHostName, int port, const char * URI, std::vector<unsigned char>& iRecv ) {
		return Get(pszHostName, port, URI, &iRecv, (void *)callbackWriteVectorUB);
	};
};

#endif	// __CTRL_HTTP_H_INCLUDED__
