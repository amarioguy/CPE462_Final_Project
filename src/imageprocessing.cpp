/*++
* 
--*/


#include <iostream>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <opencv2/opencv.hpp>
//
// gif-h library, this is public domain software, available here: https://github.com/charlietangora/gif-h
//
#include <gif.h>


/*++
   DELETEME when finished:
   Project outline:
   - Make this work with JPEGs and OpenCV first
     - make basic image manipulation work as in HW6
	 - do image orientation
	 - loop code at speed (GIF max framerate is 50fps, we have 3 frames, base case is 1 frame per second)
	 - command parsing
	 - randomization
   - conversion to/from formats
--*/

/*++
  Resources used:
    OpenCV Mat type indexes: https://gist.github.com/yangcha/38f2fa630e223a8546f9b48ebbb3e61a
	OpenCV documentation (both documentation and tutorials): https://docs.opencv.org/4.5.5/index.html
	gif-h library: https://github.com/charlietangora/gif-h
	Getting a file extension in C++: https://stackoverflow.com/questions/5309471/getting-file-extension-in-c

--*/

typedef enum {
	MODE_COLOR_WHEEL = 0,
	MODE_GIF_OUTPUT,
	MODE_CONVERT_RAW,
	MODE_MAX,
	MODE_UNKNOWN = 0xFFFFFFFF
} img_mode;

using namespace cv; // makes it so any OpenCV methods do not need to be prefixed with "cv::"

//
// Color wheel mode needs these arguments (some are optional, but must be listed in the order specified):
//   input_image - image to be used as input to the color wheel (must be JPEG). Required, must be a file path.
//   angle - how many degrees counter-clockwise you want to rotate the image. Optional, will be 0 unless specified
//   no_equalize_histogram - do not do histogram equalization on the image before creating the output images. 
//   Optional, must be the fifth argument after input_image.
//   framerate - must be a number between 1 and 50 (units are frames per second). Optional, defaults to 1.
// 
// Assumptions:
//   The image is 3 channels (color space is NOT assumed)
//   Alpha channel is ignored.
// 
// Output is always out_ch_1, out_ch_2, out_ch_3 JPG files, the color mixed version out_mixed.jpg, output jpgs colormapped using COLORMAP_HSV (out_1,2,3.jpg), and the out_gif and out_ch_gif GIF files in the directory the program was run in.
//

int main_color_wheel(int argc, char* argv[]) {
	Mat image_in; // one image in, always.
	Mat image_out_temp;
	Mat channel_out[3]; // channels [1,2,3] image out. (TODO: could we determine color space of the image in a future iteration to make these R, G, B?)
	Mat hsv_channel_out[3];
	Mat mixed_image_out; // add a mixed image output
	Mat rotation_matrix, affine_matrix;
	Point2f sourceTriangle[3];
	Point2f destTriangle[3];
	cv::Point center_img;
	cv::String in_string; // input string
	const cv::String out_string_1 = "out_1.jpg";
	const cv::String out_string_2 = "out_2.jpg";
	const cv::String out_string_3 = "out_3.jpg";
	const cv::String out_ch_string_1 = "out_ch_1.jpg";
	const cv::String out_ch_string_2 = "out_ch_2.jpg";
	const cv::String out_ch_string_3 = "out_ch_3.jpg";
	const cv::String out_string_mixed = "out_mixed.jpg";
	const cv::String out_gif_string = "out_gif.gif";
	bool do_histogram_equalization = true;
	bool do_randomize_orientation = false; //ALL randomization is disabled for now.
	bool do_randomize_angle = false;
	bool mix_colors = false; // default to not mixing colors
	bool was_any_transform_done = false; // need to check if any transforms are done to image to choose whether to apply a transformation to original input or to the current running output.
	unsigned int rotation_angle = 90; //default to keeping image angle as is.
	unsigned int framerate = 1; //leave it as 1 frame per second.
	char* input_file_ext;
	//
	// Sanity check the arguments for color wheel mode
	//
	if (argc < 3) {
		// less than 3 arguments means we DEFINITELY didn't get an input image, error out immediately.
		printf("Error: not enough arguments!\n");
		//print_help();
		return -1;
	}
	//
	// Check the file extension of the input, make sure it is JPG/JPEG.
	// Note: while we *could* support TIFF, PNG, etc, and OpenCV *should* not care, to keep the project simple, restrict to JPG.
	// We will error out later if the format is NOT actually JPEG.
	//
	input_file_ext = strrchr(argv[2], '.');
	if ((strncmp(input_file_ext + 1, "jpg", 3) != 0) && (strncmp(input_file_ext + 1, "jpeg", 4) != 0)) {
		//
		// Input image file extension is not 'jpg', error out.
		//
		printf("Error: File does not have .jpg or .jpeg extension! File must have these extensions.\n");
		return -1;
	}
	in_string = (cv::String)argv[2];

	//
	// Optional command parsing disabled for now.
	// 
	
	//if (argc >= 4) {
	//	// parse all the "Optional" parameters here. we always enter this if we have at least one optional parameter passed in.
	//	if (strncmp(argv[3], "no_equalize_histogram", 21) == 0) {
	//		// we've been asked to skip histogram equalization.
	//		do_histogram_equalization = false;
	//	}
	//	else if (strncmp(argv[3], "no_equalize_histogram", 21) != 0) {
	//		//
	//		// we have an invalid parameter where no_equalize_histogram is, exit.
	//		//
	//		printf("Error: ")
	//	}
	//}

	image_in = imread(in_string, cv::IMREAD_COLOR);
	if (image_in.empty()) {
		printf("Error: OpenCV can't parse the input file!\n");
		return -1;
	}

	//
	// The test vector is a BGR8 image, no alpha channel, note that the results of the following might change dependent on source image's color space.
	
	//
	// Orient the image as needed. (To keep the code simple, this must always remain the first transform)
	//
	//
	// if we have an effective angle of 0 (that is, after doing mod 360 on the angle), we know we aren't rotating, so this code should be skipped for speed reasons in that case.
	//
	if ((rotation_angle % 360) != 0) {
		////
		//// NOTE: these triangle points are completely arbitrary and are a variation of the ones used in the tutorial here: https://docs.opencv.org/4.5.5/d4/d61/tutorial_warp_affine.html
		////
		sourceTriangle[0] = Point2f(0, 0);
		sourceTriangle[1] = Point2f(image_in.cols - 1, 0);
		sourceTriangle[2] = Point2f(0, image_in.rows - 1);
		destTriangle[0] = Point2f(0, image_in.rows * 0.6);
		destTriangle[1] = Point2f(image_in.cols * 0.9, image_in.rows * 0.7);
		destTriangle[2] = Point2f(image_in.cols * 0.3, image_in.rows * 0.4);
		rotation_angle = rotation_angle % 360;
		affine_matrix = getAffineTransform(sourceTriangle, destTriangle);
		center_img = Point(image_in.cols / 2, image_in.rows / 2);
		rotation_matrix = cv::getRotationMatrix2D(center_img, (double)rotation_angle, 1.0);
		cv::warpAffine(image_in, image_out_temp, rotation_matrix, image_in.size() );
		if (was_any_transform_done == false) {
			was_any_transform_done = true;
		}
	}


	//
	// Do histogram equalization, if the user requested it.
	//
	if (do_histogram_equalization == true) {
		//cv::equalizeHist(((was_any_transform_done == true) ? image_out_temp : image_in), image_out_temp);
		if (was_any_transform_done == false) {
			was_any_transform_done = true;
		}
	}

	//
	// extract the image channels, and dump them out to the disk as JPG.
	//
	cv::split(image_out_temp, channel_out);

	cv::imwrite(out_ch_string_1, channel_out[0]);
	cv::imwrite(out_ch_string_2, channel_out[1]);
	cv::imwrite(out_ch_string_3, channel_out[2]);


	//
	// apply colormaps to the channels.
	//


	printf("type: %d\n", channel_out[0].type());


	cv::applyColorMap(channel_out[0], hsv_channel_out[0], COLORMAP_HSV);
	cv::applyColorMap(channel_out[1], hsv_channel_out[1], COLORMAP_HSV);
	cv::applyColorMap(channel_out[2], hsv_channel_out[2], COLORMAP_HSV);


	cv::imwrite(out_string_1, hsv_channel_out[0]);
	cv::imwrite(out_string_2, hsv_channel_out[1]);
	cv::imwrite(out_string_3, hsv_channel_out[2]);

	//
	// Mix the channels from earlier into a new BGR8 image.
	//



	//
	// Combine the frames into a GIF.
	//

	return 0;

}

//
// CLI syntax *should* be this:
//
// CPE462_Project.exe [mode] [input image] ...
//
int main(int argc, char *argv[])
{
	img_mode mode = MODE_UNKNOWN;
	int ret = -1; // default to an error state unless we are assured we succeeded.

	//
	// Mode selector:
	// Valid modes are 'color_wheel', 'output_as_gif', 'convert_raw' for now.
	//
	// If we don't have a valid mode selected, since mode is default set to UNKNOWN, exit once we check for modes.
	//
	// Mode descriptions:
	//   Color wheel: the "main" mode of the program, this is where most our actual functionality is, including orientation,
	//   image filtering, etc. (The other modes are basically helpers to convert images as needed)
	//   
	//   Output as GIF: Takes as input one to three images (the frames as JPG) and outputs the GIF of those three frames at a 
	//   specified frame rate.
	//
	//
	
	if (strncmp(argv[1], "color_wheel", 11) == 0) {
		mode = MODE_COLOR_WHEEL;
	}
	else if (strncmp(argv[1], "output_as_gif", 13) == 0) {
		mode = MODE_GIF_OUTPUT;
	}
	else if (strncmp(argv[1], "convert_raw", 11) == 0) {
		mode = MODE_CONVERT_RAW;
	}

	//
	// Check our running mode at this point. If unknown, exit.
	//
	switch (mode) {
		case MODE_UNKNOWN:
		default:
			printf("Error: Unknown running mode specified! Valid modes are [color_wheel, output_as_gif, convert_raw]\n");
			//print_help();
			break;
		case MODE_COLOR_WHEEL:
			printf("Running in color wheel mode\n");
			ret = main_color_wheel(argc, argv);
			break;

	}

	return ret;

	//
	// Temporarily disabling the RAW image code
	//
#if 0
	FILE  *in, *out;
	int   j, k, width, height;
	int ** image_in, ** image_out;
	float sum1, sum2;
	float new_T, old_T, delta_T;
	long count1, count2;
	errno_t err;

	if(argc<5) { printf("ERROR: Insufficient parameters!\n"); return(1);}

	width = atoi(argv[3]);
	height = atoi(argv[4]);

	image_in = (int**) calloc(height, sizeof(int*));
	if(!image_in)
	{
		printf("Error: Can't allocate memmory!\n");
		return(1);
	}

	image_out = (int**) calloc(height, sizeof(int*));
	if(!image_out)
	{
		printf("Error: Can't allocate memmory!\n");
		return(1);
	}

	for (j=0; j<height; j++)
	{
		image_in[j] = (int *) calloc(width, sizeof(int));
		if(!image_in[j])
		{
			printf("Error: Can't allocate memmory!\n");
			return(1);
		}

		image_out[j] = (int *) calloc(width, sizeof(int));
		if(!image_out[j])
		{
			printf("Error: Can't allocate memmory!\n");
			return(1);
		}

	}

	if((err=fopen_s(&in, argv[1],"rb"))!=0)
	{
		printf("ERROR: Can't open in_file!\n");
		return(1);
	}

	if((err=fopen_s(&out, argv[2],"wb"))!=0)
	{
		printf("ERROR: Can't open out_file!\n");
		return(1);
	}

	for (j=0; j<height; j++)
		for (k=0; k<width; k++)
	    	{
			if((image_in[j][k]=getc(in))==EOF)
			{
				printf("ERROR: Can't read from in_file!\n");
				return(1);
		      }
	    	}
      if(fclose(in)==EOF)
	{
		printf("ERROR: Can't close in_file!\n");
		return(1);
	}

#endif

/********************************************************************/
/* Image Processing begins                                          */
/********************************************************************/

#if 0
	for (j=0; j<height; j++)
		for (k=0; k<width; k++)
	    	{
			image_out[j][k]=255-image_in[j][k];
		}
#endif



/********************************************************************/
/* Image Processing ends                                          */
/********************************************************************/

#if 0

	/* save image_out into out_file in RAW format */
	for (j=0; j<height; j++)
		for (k=0; k<width; k++)
	    {
	            if((putc(image_out[j][k],out))==EOF)
	            {
		        	printf("ERROR: Can't write to out_file!\n");
				    return(1);
	            }
		}

    if(fclose(out)==EOF)
	{
		printf("ERROR: Can't close out_file!\n");
		return(1);
	}


	for (j=0; j<height; j++)
	{
		free(image_in[j]);
		free(image_out[j]);
	}
	free(image_in);
	free(image_out);
#endif

    return 0;
}



