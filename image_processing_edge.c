#include <stdio.h>
#include <memory.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <dirent.h>
#include <inttypes.h>
#include <sys/time.h>
#include <stdbool.h>


#define SDRAM_BASE_ADDR 0
#define ALT_VIP_SOFTWARE_RESET_N_BASE 0x00000200   //


#define ALT_AXI_FPGASLVS_OFST (0xC0000000)  // axi_master
#define HW_FPGA_AXI_SPAN (0x40000000)  // Bridge span
#define HW_FPGA_AXI_MASK ( HW_FPGA_AXI_SPAN - 1 )


#define DEMO_VGA_FRAME0_ADDR		0x00000000
#define FR0_FRAME0_OFFSET			0x00000000

#define RED_CHANGE_GRAY 0.2989
#define GREEN_CHANGE_GRAY 0.5870
#define BLUE_CHANGE_GRAY 0.1170


static volatile unsigned long *h2p_memory_addr=NULL;

typedef struct Pixel{
	long Red;
	long Green;
	long Blue;
	long None;
}Pixel;

int pixel_processing( int Red, int Green, int Blue, int None )
{
	int pixel;
	int Red_temp, Green_temp, Blue_temp;

	Red_temp = Red;
	Green_temp = Green;
	Blue_temp = Blue;

	pixel = (Green_temp<<24) | (Blue_temp<<16) | (None<<8) | (Red_temp); 
	
	return pixel;
};


int pixel_processing_gray( int Red, int Green, int Blue, int None )
{
	int pixel;
	int Red_temp, Green_temp, Blue_temp;
	int gray_temp = (int)(Red * RED_CHANGE_GRAY +
						  Green * GREEN_CHANGE_GRAY +
						  Blue * BLUE_CHANGE_GRAY);

	Red_temp = Green_temp =	Blue_temp = gray_temp;

	pixel = (Green_temp<<24) | (Blue_temp<<16) | (None<<8) | (Red_temp); 

	return pixel;

}

int pixel_processing_inversion( int Red, int Green, int Blue, int None )
{
	int pixel;
	int Red_temp, Green_temp, Blue_temp;

	Red_temp = 255-Red;
	Green_temp = 255-Green;
	Blue_temp = 255-Blue;

	pixel = (Green_temp<<24) | (Blue_temp<<16) | (None<<8) | (Red_temp); 
	
	return pixel;
}

int max_directional_color(int *path)
{
	int i = 0;
	int result = path[0];
	for(i = 1; i < 4; i++){
		if( result < path[i] ){
			result = path[i];
		}
	}
	return result;
}

void pixel_processing_edge(FILE *input, int width, int height, volatile unsigned long *output)
{
	int pixel;
	int Red_temp, Green_temp, Blue_temp, None_temp;
	int k = 0, i = 0, j = 0, num = 0, temp;
	long temp1, temp2;
	int moddedValue[4];
	// unsigned char point, buf[4];
	unsigned char buf[4];
	long point;
	// Pixel data[height * width];
	Pixel data[height+20][width+20];
	
	// Get All Pixels RGB
	while(fread(buf, 4, 1, input) != 0){
		if(j == width){
			j = 0;
			i++;
		}
		data[i][j].Red = (long)buf[2]; // Red
		data[i][j].Green = (long)buf[1]; // Green
		data[i][j].Blue = (long)buf[0]; // Blue
		data[i][j].None = (long)buf[3]; // None
		j++;
		num += 1;
	}

	printf("Total number of Pixels : %d \n", num);
	
	// Get Max directional value and average value
		
	for(i = 1 ; i < height-1 ; i++){
		for(j = 1 ; j < width-1 ; j++){
			temp1 = 2*data[i][j-1].Red + 3*data[i][j-1].Green + 4*data[i][j-1].Blue;
			temp2 = 2*data[i][j+1].Red + 3*data[i][j+1].Green + 4*data[i][j+1].Blue;
			moddedValue[0] = 4*temp1 - 4*temp2;

			temp1 = 2*data[i+1][j-1].Red + 3*data[i+1][j-1].Green + 4*data[i+1][j-1].Blue;
			temp2 = 2*data[i-1][j+1].Red + 3*data[i-1][j+1].Green + 4*data[i-1][j+1].Blue;
			moddedValue[1] = 4*temp1 - 4*temp2;
			
			temp1 = 2*data[i+1][j].Red + 3*data[i+1][j].Green + 4*data[i+1][j].Blue;
			temp2 = 2*data[i-1][j].Red + 3*data[i-1][j].Green + 4*data[i-1][j].Blue;
			moddedValue[2] = 4*temp1 - 4*temp2;
			
			temp1 = 2*data[i+1][j+1].Red + 3*data[i+1][j+1].Green + 4*data[i+1][j+1].Blue;
			temp2 = 2*data[i-1][j-1].Red + 3*data[i-1][j-1].Green + 4*data[i-1][j-1].Blue;
			moddedValue[3] = 4*temp1 - 4*temp2;

			temp = max_directional_color(moddedValue);
			point += temp;
		}
		printf("%d\n", i);
	}
	printf("going on...\n");

	// threshold value T = 1.2t which is selected
	int point_value = (int)(point/num * 1);

	// processing edge detect
	for(i = 0; i < height; i++){
		for(j = 0 ; j < width ; j++){
			Red_temp = data[i][j].Red;
			Green_temp = data[i][j].Green;
			Blue_temp = data[i][j].Blue;
			None_temp = data[i][j].None;
			pixel = pixel_processing(Red_temp, Green_temp, Blue_temp, None_temp);
			if(pixel > 255) Red_temp = Green_temp = Blue_temp = 0;
			else if(pixel < 0) Red_temp = Green_temp = Blue_temp = 255;
			else if(pixel < point_value)Red_temp = Green_temp = Blue_temp = 0;
			else Red_temp = Green_temp = Blue_temp = 255;
			
			pixel = pixel_processing(Red_temp, Green_temp, Blue_temp, None_temp);

			*output = pixel;
			output += 1;
		}
	}
	printf("Image Load Complete!\n");
}
	
int main() {


	void *virtual_base;
	void *axi_virtual_base;
	int fd;

	int i, n, adr;

	int num;	

	unsigned char tt[60], buf[4], file[20];
	static volatile unsigned long *output_data = NULL;

	long Red, Green, Blue, None, pixel;

	FILE *fpi;

	printf("\n");
	printf("#########################################################\n");
	printf("#                                                       #\n");
	printf("# Embedded System                                       #\n");
	printf("# Image Processing Project                              #\n");
	printf("#                                                       #\n");
	printf("# Prof. Yongjun Park                                    #\n");
	printf("#                                                       #\n");
	printf("#########################################################\n\n");

	printf("Image name (ex: lena.bmp) : ");
	scanf("%s", file);



	if((fpi = fopen(file, "rb")) == NULL) {

		printf("File not found\n", file);
		exit(0);
	}
 
	if( ( fd = open( "/dev/mem", ( O_RDWR | O_SYNC ) ) ) == -1 ) {
		printf( "ERROR: could not open \"/dev/mem\"...\n" );
		return( 1 );
	}

	axi_virtual_base  = mmap( NULL, HW_FPGA_AXI_SPAN, ( PROT_READ | PROT_WRITE ), MAP_SHARED, fd,ALT_AXI_FPGASLVS_OFST  );	
	if( axi_virtual_base == MAP_FAILED ) {
		printf( "ERROR: axi mmap() failed...\n" );
		close( fd );
		return( 1 );
	}


	h2p_memory_addr=axi_virtual_base + ( ( unsigned long  )( DEMO_VGA_FRAME0_ADDR) & ( unsigned long)( HW_FPGA_AXI_MASK ) );
	

	fread(tt, 54, 1, fpi);
 
	output_data = h2p_memory_addr;
	
	char check_detect;
	printf("Do you want to check edge detect?[YES:y/NO:n]\n");
	printf("Input : ");
	scanf("\n%c", &check_detect);
	if( check_detect == 'y'){
		printf("doing tasks...\n");
		int width = *(int*)&tt[18];
		int height = *(int*)&tt[22];
		pixel_processing_edge(fpi, width, height, output_data);
		printf("Edge Detected!\n");
		fclose(fpi);
		return 0;
	}


	num = 0;
	while( fread(buf, 4, 1, fpi) != 0 ) {


		num += 1;

		Red = (long)buf[2];
		Green = (long)buf[1];
		Blue = (long)buf[0];
		None = (long)buf[3];

//		pixel = pixel_processing(Red, Green, Blue, None);
		pixel = pixel_processing_gray(Red, Green, Blue, None);
//		pixel = pixel_processing_inversion(Red, Green, Blue, None);

		*output_data = pixel;
		output_data += 1;
	}
	
	printf("Total number of Pixels : %d \n", num);
	printf("Image Load Complete!\n");
	fclose(fpi);

	return 0;
}

