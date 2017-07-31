
//
//	apt-get install libcv-dev libopencv-dev
//  apt-get install opencv-doc
//

/*
// This file overrides the built-in C++ (simple) runner
// For more information see http://docs.c9.io:8080/#!/api/run-method-run
{
  "script": [
	"set -e",
	"if [ \"$debug\" == true ]; then ",
	"/usr/bin/g++ -ggdb3 -std=c++11 $file -o $file.o `pkg-config --cflags opencv` `pkg-config --libs opencv`",
	"chmod 755 \"$file.o\"",
	"node $HOME/.c9/bin/c9gdbshim.js \"$file.o\" $args",
	"else",
	"/usr/bin/g++ -O3 -std=c++11 $file -o $file.o `pkg-config --cflags opencv` `pkg-config --libs opencv`",
	"chmod 755 $file.o",
	"$file.o $args",
	"fi"
  ],
  "info": "Running $file (C++ OpenCV)",
  "debugger": "gdb",
  "$debugDefaultState": false,
  "env": {},
  "selector": "^.*\\.(cpp|cc)$"
}
//*/


#include <thread>
#include <opencv2/opencv.hpp>
#include "common/perf_log.h"

#include "common/display_st7735_spi.h"
#include "common/display_ili9341_spi.h"
#include "common/display_ili9225_spi.h"
#include "common/display_ssd1306_i2c.h"


int main()
{
	std::vector<DisplayIF*>	display;
	cv::Mat		src	= cv::imread("TestImages/lena_std.tif");

//	display.push_back( new Display_SSD1306_i2c(180) );		// for SSD1306
//	display.push_back( new Display_SSD1306_i2c(180,2) );	// for SH1106
//	display.push_back( new Display_ST7735_spi(180) );		// for KMR1.8
	display.push_back( new Display_ILI9341_spi_TM22(0) );	// for Tianma2.2" Panel
//	display.push_back( new Display_ILI9341_spi_TM24(180) );	// for Tianma2.4" Panel
//	display.push_back( new Display_ILI9341_spi(270) );		// fpr basic ILI9341
//	display.push_back( new Display_ILI9225_spi(180) );		// for basic ILI9225

	////////////////////////////////////////////////////////////////
	// DisplayIF::Init()
	////////////////////////////////////////////////////////////////
	printf( "Hit any key to DisplayIF::Init()\n");
	getchar();
	for( auto it : display )
	{
		it->Init();
	}

	////////////////////////////////////////////////////////////////
	// DisplayIF::DispOn()
	////////////////////////////////////////////////////////////////
	printf( "Hit any key to DisplayIF::DispOn()\n");
	getchar();
	for( auto it : display )
	{
		it->DispOn();
	}

	////////////////////////////////////////////////////////////////
	// DisplayIF::DispOff()
	////////////////////////////////////////////////////////////////
	printf( "Hit any key to DisplayIF::DispClear()\n");
	getchar();
	for( auto it : display )
	{
		it->DispClear();
	}

	////////////////////////////////////////////////////////////////
	// DisplayIF::WriteImageGRAY()
	////////////////////////////////////////////////////////////////
	printf( "Hit any key to DisplayIF::WriteImageGRAY()\n");
	getchar();
	for( auto it : display )
	{
		cv::Mat	img;
		cv::cvtColor( src, img, cv::COLOR_BGR2GRAY );
		cv::resize( img, img, cv::Size(it->GetSize().width,it->GetSize().height), 0, 0, cv::INTER_AREA );
		
		it->WriteImageGRAY( 0, 0, img.data, img.step, img.cols, img.rows );
	}

	////////////////////////////////////////////////////////////////
	// DisplayIF::WriteImageBGRA()
	////////////////////////////////////////////////////////////////
	printf( "Hit any key to DisplayIF::WriteImageBGRA()\n");
	getchar();
	for( auto it : display )
	{
		cv::Mat	img;
		cv::cvtColor( src, img, cv::COLOR_BGR2BGRA );
		cv::resize( img, img, cv::Size(it->GetSize().width,it->GetSize().height), 0, 0, cv::INTER_AREA );

		it->WriteImageBGRA( 0, 0, img.data, img.step, img.cols, img.rows );
	}

	////////////////////////////////////////////////////////////////
	// DisplayIF::DispOff()
	////////////////////////////////////////////////////////////////
	printf( "Hit any key to DisplayIF::DispOff()\n");
	getchar();
	for( auto it : display )
	{
		it->DispOff();
	}

	////////////////////////////////////////////////////////////////
	// DisplayIF::Quit()
	////////////////////////////////////////////////////////////////
	printf( "Hit any key to DisplayIF::DispOff()\n");
	getchar();
	for( auto it : display )
	{
		it->Quit();
	}	

	////////////////////////////////////////////////////////////////
	// DispOn with pre-transfered image.
	////////////////////////////////////////////////////////////////
	printf( "Hit any key to DispOn with pre-transfered image.\n");
	getchar();
	for( auto it : display )
	{
		it->Init();

		int		size	= std::min( it->GetSize().width, it->GetSize().height );
		cv::Mat	img;
		cv::cvtColor( src, img, cv::COLOR_BGR2BGRA );
		cv::resize( img, img, cv::Size(size,size), 0, 0, cv::INTER_AREA );

		it->DispClear();
		it->WriteImageBGRA( (it->GetSize().width-img.cols)/2, (it->GetSize().height-img.rows)/2, img.data, img.step, img.cols, img.rows );
		it->DispOn();
	}

	printf( "Hit any key to DispOff and Quit.\n");
	getchar();
	for( auto it : display )
	{
		it->DispOff();
		it->Quit();
	}	

	////////////////////////////////////////////////////////////////
	// Scroll test
	////////////////////////////////////////////////////////////////
	printf( "Hit any key to Scroll(GRAY) test\n");
	getchar();
	for( auto it : display )
	{
		it->Init();
		it->DispClear();
		it->DispOn();
	}

	for( auto it : display )
	{
		cv::Mat	img;
		cv::cvtColor( src, img, cv::COLOR_BGR2GRAY );
		cv::resize( img, img, cv::Size(it->GetSize().width,it->GetSize().height), 0, 0, cv::INTER_AREA );
		
		for( int y = -img.rows; y < img.rows; y++ )
		{
			it->WriteImageGRAY( y, y, img.data, img.step, img.cols, img.rows );
		}
	}

	for( auto it : display )
	{
		it->DispOff();
		it->Quit();
	}	

	////////////////////////////////////////////////////////////////
	// Scroll test
	////////////////////////////////////////////////////////////////
	printf( "Hit any key to Scroll(BGRA) test\n");
	getchar();
	for( auto it : display )
	{
		it->Init();
		it->DispClear();
		it->DispOn();
	}

	for( auto it : display )
	{
		cv::Mat	img;
		cv::cvtColor( src, img, cv::COLOR_BGR2BGRA );
		cv::resize( img, img, cv::Size(it->GetSize().width,it->GetSize().height), 0, 0, cv::INTER_AREA );

		for( int y = -img.rows; y < img.rows; y++ )
		{
			it->WriteImageBGRA( y, y, img.data, img.step, img.cols, img.rows );
		}
	}

	for( auto it : display )
	{
		it->DispOff();
		it->Quit();
	}

	////////////////////////////////////////////////////////////////
	// Performance test
	////////////////////////////////////////////////////////////////
	printf( "Hit any key to Performance test\n");
	getchar();
	for( auto it : display )
	{
		it->Init();
		it->DispClear();
		it->DispOn();
	}

	for( auto it : display )
	{
		int		loop	= 100;
		cv::Mat	img, gray, bgra;
		cv::resize( src, img, cv::Size(it->GetSize().width,it->GetSize().height), 0, 0, cv::INTER_AREA );
		cv::cvtColor( img, bgra, cv::COLOR_BGR2BGRA );
		cv::cvtColor( img, gray, cv::COLOR_BGR2GRAY );

    	std::chrono::high_resolution_clock::time_point   start	= std::chrono::high_resolution_clock::now();
		for( int i = 0; i < loop; i++ )
		{
			it->DispClear();
			it->WriteImageGRAY( 0, 0, gray.data, gray.step, gray.cols, gray.rows );
			it->DispClear();
			it->WriteImageBGRA( 0, 0, bgra.data, bgra.step, bgra.cols, bgra.rows );
		}
		std::chrono::high_resolution_clock::time_point   end	= std::chrono::high_resolution_clock::now();
 
        double elapsed = std::chrono::duration_cast<std::chrono::seconds>(end-start).count();
		printf( "Framerate Per Sec = %.3f [fps]\n", 4 * loop / elapsed );
	}

	for( auto it : display )
	{
		it->DispOff();
		it->Quit();
	}

	return  0;
}
