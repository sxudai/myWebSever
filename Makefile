.PHONY:clean sever gdbsever

src = $(wildcard ./*.cpp)
obj = $(patsubst %.cpp, %.o, $(src))

sever: $(obj)
	g++ $^ -o $@ -lpthread

gdbsever: $(obj)
	g++ $^ -o $@ -lpthread -g

$(obj):%.o:%.cpp
	g++ -c $< -o $@ 

clean:
	rm -rf *.o