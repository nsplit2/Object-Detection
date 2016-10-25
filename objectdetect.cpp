
// arm-linux-gnueabihf-g++ -o objectdetect.arm objectdetect.cpp -lopencv_core -lopencv_imgproc -lopencv_objdetect -lopencv_highgui

#include "opencv2/objdetect/objdetect.hpp"
#include "opencv2/highgui/highgui.hpp"
#include "opencv2/imgproc/imgproc.hpp"

#include <iostream>
#include <vector>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>

using namespace std;
using namespace cv;

char conv[256];
char buf[8192];
size_t size;
char object_cascade_name[256];
CascadeClassifier object_cascade;
int fd;
struct flock lock = {F_WRLCK, SEEK_SET,   0,      0,     0 };
struct flock unlock = {F_UNLCK, SEEK_SET,   0,      0,     0 };
time_t start, end;
string fps_text;
double fps;

RNG rng(12345);

void detect( Mat frame ) {
	std::vector<Rect> objects;
	Mat frame_gray;
	std::vector<uchar> jpegdata;
	std::vector<int> params = vector<int>(2);
	params[0] = 1;
	params[1] = 75;
	
	cvtColor( frame, frame_gray, COLOR_BGR2GRAY );
	equalizeHist( frame_gray, frame_gray );

	object_cascade.detectMultiScale(frame_gray, objects, 2, 2, 0, Size(30, 30) ); // Frames: 366 (8.71 fps) 640x480
	
	for( size_t i = 0; i < objects.size(); i++ ) {
		printf ("* Found %d objects\n", objects.size());
		Rect object_i = objects[i];
		rectangle(frame, object_i, CV_RGB(0, 255,0), 1);
	}

	fd = open ("/dev/shm/frame_processed.jpg", O_RDWR);
	fcntl (fd, F_SETLKW, &lock);
	
	fps_text = format("%.1f fps", fps);
	putText(frame, fps_text, Point(frame.cols-65, 20), FONT_HERSHEY_PLAIN, 1.0, CV_RGB(0,255,0),  1, 1.0);
	
	imwrite ("/dev/shm/frame_processed.jpg", frame);

	fcntl (fd, F_SETLK, &unlock);
	
	close (fd);
}

int main( int argc, char* argv[] ) {
	
	Mat image;
	int frames = 0;
	double seconds;
	char frame[54];
	
	if (argc < 3) {
		printf ("* Usage: %s [haarcascade] [frame]\n",argv[0]);
		return 0;
	}
	
	strcpy(object_cascade_name, argv[1]);
	strcpy(frame, argv[2]);
	
	lock.l_pid = getpid();
	unlock.l_pid = getpid();
	
	memset (&lock, 0, sizeof(lock));
	memset (&unlock, 0, sizeof(unlock));
	lock.l_type = F_WRLCK;
	unlock.l_type = F_SETLK;
	
	while (1) {
		fd = open (frame, O_WRONLY);
		if (fd == -1) {
			printf ("Error opening %s\n", frame);
			return 0;
		}
		// lock
		fcntl (fd, F_SETLKW, &lock);
		
		int source = open(frame, O_RDONLY, 0);
		int dest = open("/dev/shm/frame_copy.jpg", O_TRUNC | O_WRONLY | O_CREAT, 0666);
		while ((size = read(source, buf, 8192)) > 0) {
			write(dest, buf, size);
		}
		close(source);
		close(dest);

		fcntl (fd, F_SETLK, &unlock);
		close (fd);
		
		if (frames == 0)
			time(&start);
		
		image = imread("/dev/shm/frame_copy.jpg", CV_LOAD_IMAGE_COLOR);
		
		detect(image);
		
		time(&end);
		
		frames++;
		seconds = difftime (end, start);
		fps  = frames / seconds;
		
		printf("Frames: %d (%.2f fps)\n", frames, fps);

	}
	
	return 1;

}
