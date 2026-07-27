// Icepi-Zero-640x480 - Raspberry Pi 5 and Lattice ECP5 FPGA
// Copyright (C) 2026  David Dommett, email: david.dommett@gmail.com 

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.

//*************************************************************************
// Compile: g++ Capture-640x480.cpp -o Capture-640x480 `pkg-config --cflags --libs opencv4`
//*************************************************************************

#include <opencv2/opencv.hpp>
#include <iostream>

int main() {
    // Open the default camera (video0) using Video4Linux2 backend
    cv::VideoCapture cap(0, cv::CAP_V4L2);

    if (!cap.isOpened()) 
    {
        std::cerr << "Error: Could not open the camera." << std::endl;
        return -1;
    }

    // Set resolution (Optional)
    cap.set(cv::CAP_PROP_FRAME_WIDTH, 640);
    cap.set(cv::CAP_PROP_FRAME_HEIGHT, 480);

    cv::Mat frame;

    int row = 1;
    int col = 300;
    int lastSeq = -1;
    int errors = 0;

    for (int i=0; i<10000000; i++)
    {
	    // Grab a fresh frame from the camera pipeline
	    cap >> frame;

	    if (frame.empty()) 
	    {
		    std::cerr << "Error: Captured an empty frame." << std::endl;
		    return -1;
	    }

	    unsigned char* raw_data = (unsigned char*)frame.data;
	    int channels = frame.channels();

	    // Calculate 1D index: row * (width_in_bytes) + col * channels
	    int index = row * frame.step + col * channels;
	    int seqID = (int)raw_data[index] << 16;
	    seqID += (int)raw_data[index+1] << 8;
	    seqID += (int)raw_data[index+2];
	    //std::cout << " B " << (int)raw_data[index];
	    //std::cout << " G " << (int)raw_data[index+1];
	    //std::cout << " R " << (int)raw_data[index+2];
	    //std::cout << std::endl;
	    std::cout << seqID << ":" << errors << std::endl;
	    if (lastSeq != -1)
	    {
		if (seqID != (lastSeq + 1))
		{
	    		std::cout << "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!" << std::endl;
			errors += 1;
		}
	    }
	    lastSeq = seqID;
    }
    std::cout << "ERRORS:" << errors << std::endl;
    // Release the camera resource
    cap.release();
    return 0;
}
