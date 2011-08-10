
#include "openCLUtilities.h"
#include "RGBAUtilities.h"

#include <iostream>
using namespace std;

// OpenCL variables
int err, gpu;                            // error code returned from api calls

size_t *globalWorksize;                  // global domain size for our calculation
size_t *localWorksize;                       // local domain size for our calculation

cl_device_id device_id;             // compute device id 
cl_context context;                 // compute context
cl_command_queue commands;          // compute command queue
cl_program program;                 // compute program
cl_kernel kernel;                   // compute kernel
cl_sampler sampler;
cl_mem input;                       // device memory used for the input array
cl_mem output;                      // device memory used for the output array
int width;
int height;                  //input and output image specs

void cleanKill(int errNumber){
    clReleaseMemObject(input);
	clReleaseMemObject(output);
	clReleaseProgram(program);
    clReleaseSampler(sampler);
	clReleaseKernel(kernel);
	clReleaseCommandQueue(commands);
	clReleaseContext(context);
    exit(errNumber);
}

int imgTest(int argc, char ** argv){
    
    read_png_file((char*)"rgba.png");
    
//    cout << "Size of image " << getImageSize() << endl;
//
//    float *buffer = new float[getImageSize()];    
//    memcpy(buffer, upcastToFloatAndNormalize(getImage(), getImageSize()), getImageSizeInFloats());
//    
//    //buffer is now populated with array of normalized floats
//    setImage(downcastToByteAndDenormalize(buffer, getImageSize()));

    
    uint8 *buffer = new uint8[getImageSizeInFloats()];    
    memcpy(buffer, upcastToFloatAndNormalize(getImage(), getImageSize()), getImageSizeInFloats());
    
    //buffer is now populated with array of normalized floats
    setImage(downcastToByteAndDenormalize((float*)buffer, getImageSize()));
    
    
    //setImage(downCastToByte(denorm(norm(upcastToFloat(getImage(), getImageSize()),getImageSize()), getImageSize()),getImageSize()));
    
    
    // write image back
    //setImage(buffer);
    write_png_file((char*)"outRGBA.png");
    
    //SaveImage((char*)"outRGBA.png", buffer, width, height);
    
    system("open outRGBA.png");
    return 1;
}

//#define USINGFREEIMAGE

int main(int argc, char *argv[])
{	    
	// Connect to a compute device
	//
	gpu = 1;
	if(clGetDeviceIDs(NULL, gpu ? CL_DEVICE_TYPE_GPU : CL_DEVICE_TYPE_CPU, 1, &device_id, NULL) != CL_SUCCESS)  {
		cout << "Failed to create a device group!" << endl;
        cleanKill(EXIT_FAILURE);
    }
	
	// Create a compute context 
	//
	if(!(context = clCreateContext(0, 1, &device_id, NULL, NULL, &err))){
		cout << "Failed to create a compute context!" << endl;
        cleanKill(EXIT_FAILURE);
    }
	
	// Create a command commands
	//
	if(!(commands = clCreateCommandQueue(context, device_id, 0, &err))) {
		cout << "Failed to create a command commands!" << endl;
        cleanKill(EXIT_FAILURE);
    }
    
    // Load kernel source code
    //
	char *source = load_program_source("sobel_opt1.cl");
    if(!source)
    {
        cout << "Error: Failed to load compute program from file!" << endl;
        cleanKill(EXIT_FAILURE);
    }
    
	// Create the compute program from the source buffer
	//
	if(!(program = clCreateProgramWithSource(context, 1, (const char **) &source, NULL, &err))){
		cout << "Failed to create compute program!" << endl;
        cleanKill(EXIT_FAILURE);
    }
    
	// Build the program executable
	//
	err = clBuildProgram(program, 0, NULL, NULL, NULL, NULL);
	if (err != CL_SUCCESS)
	{
		size_t len;
		char buffer[2048];
		cout << "Error: Failed to build program executable!" << endl;
		clGetProgramBuildInfo(program, device_id, CL_PROGRAM_BUILD_LOG, sizeof(buffer), buffer, &len);
		cout << buffer << endl;
        cleanKill(EXIT_FAILURE);
	}
    
    if(!doesGPUSupportImageObjects){
        cleanKill(EXIT_FAILURE);
    }
	
	// Create the compute kernel in the program we wish to run
	//
	kernel = clCreateKernel(program, "sobel", &err);
    
	if (!kernel || err != CL_SUCCESS){
		cout << "Failed to create compute kernel!" << endl;
        cleanKill(EXIT_FAILURE);
    }
	
    #ifdef USINGFREEIMAGE
        getGPUUnitSupportedImageFormats(context);
    #endif
    
    //  specify the image format that the images are represented as... 
    //  by default to support OpenCL they must support 
    //  format.image_channel_data_type = CL_UNORM_INT8;
    //  i.e. Each channel component is a normalized unsigned 8-bit integer value.
    //
    //	format.image_channel_order = CL_RGBA;
    //
    //  format is collected with the LoadImage function
    cl_image_format format; 
    
    
    #ifdef USINGFREEIMAGE
        /*----------->     FREE IMAGE REQUIRED     <-----------*/
        input = FreeImageLoadImage(context, (char*)"rgba.png", width, height, format);
    #else
        input = LoadImage(context, (char*)"rgba.png", width, height, format);
    #endif
    
    //  create output buffer object, to store results
    output = clCreateImage2D(context, 
                             CL_MEM_WRITE_ONLY, 
                             &format, 
                             width, 
                             height,
                             0, 
                             NULL, 
                             &err);

    if(there_was_an_error(err)){
        cout << "Output Image Buffer creation error!" << endl;
        cleanKill(EXIT_FAILURE);
    }    
    
    
    //  if either input of output are empty, crash and burn
	if (!input || !output ){
		cout << "Failed to allocate device memory!" << endl;
        cleanKill(EXIT_FAILURE);
	}
    
    
    // Create sampler for sampling image object 
    sampler = clCreateSampler(context,
                              CL_FALSE, // Non-normalized coordinates 
                              CL_ADDRESS_CLAMP_TO_EDGE, 
                              CL_FILTER_NEAREST, 
                              &err);
    
    if(there_was_an_error(err)){
        cout << "Error creating CL sampler object." << endl;
        cleanKill(EXIT_FAILURE);
    }
    
    
	// Set the arguments to our compute kernel
	//
	err  = clSetKernelArg(kernel, 0, sizeof(cl_mem), &input);
	err |= clSetKernelArg(kernel, 1, sizeof(cl_mem), &output);
    err |= clSetKernelArg(kernel, 2, sizeof(cl_sampler), &sampler); 
    err |= clSetKernelArg(kernel, 3, sizeof(cl_int), &width);
    err |= clSetKernelArg(kernel, 4, sizeof(cl_int), &height);
    
    if(there_was_an_error(err)){
        cout << "Error: Failed to set kernel arguments! " << err << endl;
        cleanKill(EXIT_FAILURE);
    }    
    
	// Get the maximum work group size for executing the kernel on the device
	//
    //err = clGetKernelWorkGroupInfo(kernel, device_id, CL_KERNEL_WORK_GROUP_SIZE, sizeof(int) * 4 + sizeof(float) * 2, &localWorksize, NULL);

	//err = clGetKernelWorkGroupInfo(kernel, device_id, CL_KERNEL_WORK_GROUP_SIZE, sizeof(unsigned short)* getImageLength()*getImageWidth(), &local, NULL);

//	if (err != CL_SUCCESS)
//	{
//        cout << print_cl_errstring(err) << endl;
//        if(err == CL_INVALID_VALUE){
//            cout << "if param_name is not valid, or if size in bytes specified by param_value_size "
//            << "is less than the size of return type as described in the table above and "
//            << "param_value is not NULL." << endl;
//        }
//		cout << "Error: Failed to retrieve kernel work group info!" << err << endl;
//		cleanKill(EXIT_FAILURE);
//	}
	
	// Execute the kernel over the entire range of our 1d input data set
	// using the maximum number of work group items for this device
	//
//    localWorksize = new size_t[2];
//    globalWorksize = new size_t[2];

    
    // THIS IS POORLY DOCUMENTED ELSEWHERE!
    // each independed image object steam needs its own local & global data spec
    // thus while I use 1 input and 1 output object local[0] for local input
    // and local[1] for local output
    
//    localWorksize[0] = 1;
//    localWorksize[1] = localWorksize[0];
//    globalWorksize[0] = width*height;
//    globalWorksize[1] = globalWorksize[0];
    
    size_t localWorksize[2] = { 16, 16 };
    size_t globalWorksize[2] =  { RoundUp(localWorksize[0], width),
        RoundUp(localWorksize[1], height) };
    
    
    //  Start up the kernels in the GPUs
    //
	err = clEnqueueNDRangeKernel(commands, kernel, 2, NULL, globalWorksize, localWorksize, 0, NULL, NULL);
    
	if (there_was_an_error(err))
	{
        cout << print_cl_errstring(err) << endl;
		cout << "Failed to execute kernel!, " << err << endl;
        cleanKill(EXIT_FAILURE);
	}
	
	// Wait for the command commands to get serviced before reading back results
	//
	clFinish(commands);
    
	// Read back the results from the device to verify the output
	//
    #ifdef USINGFREEIMAGE
        /*----------->     FREE IMAGE REQUIRED     <-----------*/
        char* buffer = new char[width * height * 4];
    #else
        uint8* buffer = new uint8[getImageSizeInFloats()];    
    #endif
    
    
    size_t origin[3] = { 0, 0, 0 };
    size_t region[3] = { width, height, 1};
    
    cl_command_queue queue = clCreateCommandQueue(
                                                  context, 
                                                  device_id, 
                                                  0, 
                                                  &err);
    
    err = clEnqueueReadImage(queue, output,
                             CL_TRUE, origin, region, 0, 0, buffer, 0, NULL, NULL);
    
    
    #ifdef USINGFREEIMAGE
        /*----------->     FREE IMAGE REQUIRED     <-----------*/
        FreeImageSaveImage((char*)"outRGBA.png", buffer, width, height);
    #else
        SaveImage((char*)"outRGBA.png", buffer, width, height);
    #endif
    
    
    
    cout << "RUN FINISHED SUCCESSFULLY!" << endl;
    system("open outRGBA.png");
    
    // Shutdown and cleanup
	//
	cleanKill(EXIT_SUCCESS);
	return 1;
}