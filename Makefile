all: hw2_output.c hw2.cpp
	g++ -pthread hw2_output.c hw2.cpp -o hw2

clean:
	rm -rf hw2